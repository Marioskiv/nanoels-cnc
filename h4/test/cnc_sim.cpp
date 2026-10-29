/*
  cnc_sim.cpp - runs the GCODE-mode motion core of NanoEls H4 on a PC and checks what it would do
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later

  The program is read line by line exactly as the firmware does (cnc_gc_line, cnc_st_prep_buffer),
  and the step interrupt is called on simulated time. Every step is recorded and checked:
    - the tool goes through every programmed point exactly (steps) and never leaves the straight
      line between them by more than one step per axis;
    - no axis exceeds its maximum rate, no axis accelerates harder than allowed (measured over
      windows of steps, so Bresenham's step spacing does not count);
    - the step buffer never runs empty while motion is still queued (no stutter);
    - backlash take-up moves the motor and not the tool;
    - a feed hold stops inside the path and the resume reaches the same end point.
  Usage: cnc_sim [options] program.ngc
    --task-gap-ms N   longest pause of the task that feeds the step buffer (default 50)
    --hold-at S       start a feed hold after S seconds, resume 0.5 s after it has stopped
    --backlash X,Z,C  backlash in steps
    --machine marios|h4default|fast   (fast: 100 000 steps/s on every axis, near the interrupt's limit)
    --ovr S:F:R       at S seconds set the feed override to F % and the rapid override to R % (repeatable)
    --delayed         step output as the H4 interrupt does it: a step computed by one interrupt is put out at the
                      start of the next one; checks that "not busy" never comes while a step is still pending
    --pulse-us P --dir-us D  with --delayed: the step pins as the H4 drives them (cncOnTimer): a step is put out at
                      the start of an interrupt and held P us, the direction pins of the next step are set when
                      that pulse ends, and the next interrupt waits until they have been set D us. Checks that on
                      every axis a direction change comes at least D us before its step and never during a pulse,
                      and that two steps of one axis are at least 2 P us apart (pulse and pause), also when a
                      direction wait made a step late; --dirhold-us H: a direction changes at least H us after the
                      axis' last pulse (LinuxCNC DIRHOLD). Defaults: P 4, D 5, H 5 (the firmware's defaults).
    --stop-at S       STOP after S seconds, as the firmware does it: a feed hold, and when the motion has stopped
                      the rest of the program is dropped. Checks that it stops on the path (within the speed and
                      acceleration limits) and that a new move from there reaches its target exactly.
    --reach-times     prints "R <line> <seconds>" when the tool reaches each planned point (g93_timing.py)
    --stops all|none  checks the motion at the end of every programmed move (where the next point belongs to
                      another line; the pieces of an arc are one move): "all" = it stops there (G61.1), "none" =
                      it goes on without stopping (G64 on a path without sharp corners). The step generator
                      reports the speed at which it finishes each block and starts the next one: a stop is 0 at
                      both. Shown as well: the time from the last step before the point to the first step after
                      it (several ms after a stop, a fraction of a ms when the tool goes through at speed).
*/
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

struct Target { int32_t p[CNC_N_AXIS]; uint32_t line; };

// Every planned point (arcs are planned as many short lines): the tool must pass through each of them.
static std::vector<Target>* traceTargets = nullptr;
static int32_t traceFrom[CNC_N_AXIS] = {0, 0, 0};
static void traceTarget(const int32_t p[CNC_N_AXIS], uint32_t line) {
  const int32_t* last = traceTargets->empty() ? traceFrom : traceTargets->back().p;
  if (memcmp(last, p, sizeof(int32_t) * CNC_N_AXIS) == 0) return;
  Target t; memcpy(t.p, p, sizeof(t.p)); t.line = line;
  traceTargets->push_back(t);
}

static const uint32_t TIMER_HZ = 20000000;

// --stops: what the step generator reports when it starts and finishes each block
struct BlockEvent { uint32_t line; uint8_t condition; bool start; float speed; };
static std::vector<BlockEvent> blockEvents;
static void blockEvent(uint32_t line, uint8_t condition, bool start, float speed) {
  BlockEvent e; e.line = line; e.condition = condition; e.start = start; e.speed = speed;
  blockEvents.push_back(e);
}

static std::string stripComments(const std::string& raw) {
  std::string out;
  bool inParen = false;
  for (char c : raw) {
    if (inParen) { if (c == ')') inParen = false; continue; }
    if (c == '(') { inParen = true; continue; }
    if (c == ';') break;
    out += c;
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '\r' || out.back() == '\t' || out.back() == '\n')) out.pop_back();
  size_t b = out.find_first_not_of(" \t");
  return b == std::string::npos ? "" : out.substr(b);
}

int main(int argc, char** argv) {
  const char* file = NULL;
  double taskGapMs = 50, holdAt = -1;
  int32_t backlash[CNC_N_AXIS] = {0, 0, 0};
  const char* machineName = "marios";
  struct Ovr { double at; int feed, rapid; bool done; };
  std::vector<Ovr> ovrs;
  bool delayed = false;
  const char* stopsMode = NULL;
  double pulseUs = 4, dirUs = 5, holdUs = 5, stopAt = -1;
  bool reachTimes = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--task-gap-ms")) taskGapMs = atof(argv[++i]);
    else if (!strcmp(argv[i], "--hold-at")) holdAt = atof(argv[++i]);
    else if (!strcmp(argv[i], "--backlash")) sscanf(argv[++i], "%d,%d,%d", &backlash[0], &backlash[1], &backlash[2]);
    else if (!strcmp(argv[i], "--machine")) machineName = argv[++i];
    else if (!strcmp(argv[i], "--delayed")) delayed = true;
    else if (!strcmp(argv[i], "--stops")) stopsMode = argv[++i];
    else if (!strcmp(argv[i], "--reach-times")) reachTimes = true;
    else if (!strcmp(argv[i], "--pulse-us")) pulseUs = atof(argv[++i]);
    else if (!strcmp(argv[i], "--dir-us")) dirUs = atof(argv[++i]);
    else if (!strcmp(argv[i], "--dirhold-us")) holdUs = atof(argv[++i]);
    else if (!strcmp(argv[i], "--stop-at")) stopAt = atof(argv[++i]);
    else if (!strcmp(argv[i], "--ovr")) { Ovr o; o.done = false; sscanf(argv[++i], "%lf:%d:%d", &o.at, &o.feed, &o.rapid); ovrs.push_back(o); }
    else file = argv[i];
  }
  if (!file) { fprintf(stderr, "usage: cnc_sim [options] program.ngc\n"); return 2; }

  cnc_settings_t s;
  if (!strcmp(machineName, "fast")) {
    // near the step-rate limit of the H4 interrupt (4 us pulse: 125 000 steps/s): 100 000 steps/s on every axis
    s.stepsPerUnit[CNC_X] = 3200; s.stepsPerUnit[CNC_Z] = 3200; s.stepsPerUnit[CNC_C] = 200;
    for (int i = 0; i < CNC_N_AXIS; i++) { s.maxRate[i] = 100000.0f / s.stepsPerUnit[i] * 60; s.acceleration[i] = 2000000.0f / s.stepsPerUnit[i] * 3600; }
  } else if (!strcmp(machineName, "h4default")) {
    // The values of the unchanged H4 sketch: Z 800 steps / 2 mm, X 2400 steps / 1.25 mm, A1 300 steps / 2 deg.
    s.stepsPerUnit[CNC_X] = 2400 / 1.25f; s.stepsPerUnit[CNC_Z] = 800 / 2.0f; s.stepsPerUnit[CNC_C] = 300 / 2.0f;
    s.maxRate[CNC_X] = 3 * 2400 / s.stepsPerUnit[CNC_X] * 60; s.maxRate[CNC_Z] = 6 * 800 / s.stepsPerUnit[CNC_Z] * 60;
    s.maxRate[CNC_C] = 3200 / s.stepsPerUnit[CNC_C] * 60;
    s.acceleration[CNC_X] = 10 * 2400 / s.stepsPerUnit[CNC_X] * 3600; s.acceleration[CNC_Z] = 30 * 800 / s.stepsPerUnit[CNC_Z] * 3600;
    s.acceleration[CNC_C] = 16000 / s.stepsPerUnit[CNC_C] * 3600;
  } else {
    // The author's machine (LinuxCNC .ini 2026-10-25): X 1200 steps/mm 12 mm/s 100 mm/s2; C 91.733 steps/deg
    // 360 deg/s 720 deg/s2; Z (not measured yet) 400 steps/mm 25 mm/s 200 mm/s2.
    s.stepsPerUnit[CNC_X] = 1200; s.stepsPerUnit[CNC_Z] = 400; s.stepsPerUnit[CNC_C] = 91.733333f;
    s.maxRate[CNC_X] = 12 * 60; s.maxRate[CNC_Z] = 25 * 60; s.maxRate[CNC_C] = 360 * 60;
    s.acceleration[CNC_X] = 100 * 3600; s.acceleration[CNC_Z] = 200 * 3600; s.acceleration[CNC_C] = 720 * 3600;
  }
  if (delayed) {
    // as the firmware (cncStepRateLimit): no axis faster than pulse + the longer of pulse and direction set-up
    double limit = 1e6 / (pulseUs + (holdUs + dirUs > pulseUs ? holdUs + dirUs : pulseUs));
    for (int i = 0; i < CNC_N_AXIS; i++) if (s.maxRate[i] / 60 * s.stepsPerUnit[i] > limit) s.maxRate[i] = (float)(limit / s.stepsPerUnit[i] * 60);
  }
  s.junctionDeviation = 0.01f;
  s.timerHz = TIMER_HZ;
  cnc_core_settings(&s);

  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
  for (int i = 0; i < CNC_N_AXIS; i++) { m.active[i] = true; m.backlashSteps[i] = backlash[i]; }
  m.rotary[CNC_C] = true;
  m.defaultFeed = 120;
  cnc_gc_init(&m);
  cnc_plan_reset();
  cnc_st_reset();
  int32_t zero[CNC_N_AXIS] = {0, 0, 0}, pos0[CNC_N_AXIS] = {0, 0, 0};
  bool neg0[CNC_N_AXIS] = {false, false, false};
  cnc_gc_sync(pos0, zero, neg0);

  FILE* f = fopen(file, "r");
  if (!f) { perror(file); return 2; }
  std::vector<std::string> lines;
  char buf[4096];
  while (fgets(buf, sizeof(buf), f)) lines.push_back(buf);
  fclose(f);

  // Simulation state
  uint64_t now = 0, nextIsr = 0, nextTask = 0;
  int32_t tool[CNC_N_AXIS] = {0, 0, 0}, motor[CNC_N_AXIS] = {0, 0, 0};
  std::vector<Target> targets;
  traceTargets = &targets;
  cnc_plan_trace = traceTarget;
  if (stopsMode) cnc_st_block_event = blockEvent;
  size_t lineIdx = 0, targetIdx = 0;
  int32_t from[CNC_N_AXIS] = {0, 0, 0};
  int errors = 0, underruns = 0, pauses = 0, warnings = 0;
  double worstDev = 0;      // steps off the straight line
  uint64_t steps[CNC_N_AXIS] = {0, 0, 0};
  bool waitingIdle = false; int pendingAction = CNC_OK; float dwellLeft = 0;
  bool holdRequested = false, holdDone = false; uint64_t holdStopAt = 0;
  // speed / acceleration measurement from position samples (inside one move only: at a corner the
  // axis speeds change at once by design, limited by the junction deviation)
  struct Sample { int32_t p[CNC_N_AXIS]; size_t block; };
  std::vector<Sample> samples;
  const double SAMPLE_S = 0.05;
  uint64_t nextSample = 0;
  double maxRateSeen[CNC_N_AXIS] = {0, 0, 0}, maxAccSeen[CNC_N_AXIS] = {0, 0, 0};
  double gapTicks = taskGapMs * TIMER_HZ / 1000.0;
  uint32_t rnd = 12345;
  double idealMin = 0;  // sum of the programmed block times (G93: 1/F)
  bool idleSinceStart = true;
  cnc_tick_t pend; bool pendValid = false; int pendingWhileIdle = 0;
  // --stops: time of the step that reached a move's end point, then the gap to the next step
  uint64_t endStepAt = 0; bool atMoveEnd = false;
  // --delayed: the pin timing model (times in us)
  double pinStep[CNC_N_AXIS] = {-1e9, -1e9, -1e9}, pinDir[CNC_N_AXIS] = {-1e9, -1e9, -1e9};
  int pinDirNeg[CNC_N_AXIS] = {-1, -1, -1};
  double dirSetAt = -1e9, minSpacing = 1e9, minDirLead = 1e9, minDirHold = 1e9, maxLateUs = 0, pulseEndAny = -1e9;
  int pinErrors = 0, dirChanges = 0;
  bool maxFeedOver100 = false;
  for (Ovr& o : ovrs) if (o.feed > 100) maxFeedOver100 = true;
  // --stop-at
  bool stopRequested = false, stopped = false; int32_t stopPos[CNC_N_AXIS] = {0, 0, 0};
  size_t stopLine = 0;
  std::vector<double> gapsMs;  // per move end, in the order the tool reaches them

  auto done = [&]() { return lineIdx >= lines.size() && !waitingIdle && !cnc_st_busy(); };

  while (!done()) {
    if (now > (uint64_t)TIMER_HZ * 36000) {  // 10 hours of machine time: something never ends
      printf("  did not finish: line %zu of %zu, busy %d, hold %d/%d, stop %d/%d, blocks %d\n", lineIdx, lines.size(), cnc_st_busy(),
             cnc_hold_active(), cnc_hold_complete(), stopRequested, stopped, cnc_plan_blocks_used());
      errors++;
      break;
    }
    // ---- the feeding task
    if (now >= nextTask) {
      // what the firmware reads as "motion finished" must mean every step was given
      if (pendValid && (!cnc_st_busy() || cnc_hold_complete())) pendingWhileIdle++;
      if (holdAt >= 0 && !holdRequested && now >= (uint64_t)(holdAt * TIMER_HZ)) { cnc_hold_start(); holdRequested = true; }
      if (stopAt >= 0 && !stopRequested && now >= (uint64_t)(stopAt * TIMER_HZ)) { cnc_hold_start(); stopRequested = true; stopLine = lineIdx; }
      if (stopRequested && !stopped && (cnc_hold_complete() || !cnc_st_busy())) {
        // the firmware's STOP: the rest is dropped (cncSyncFromAxes), the position is where the tool stands
        stopped = true;
        memcpy(stopPos, tool, sizeof(tool));
        cnc_st_reset();
        cnc_plan_reset();
        cnc_gc_reset_modes();
        bool neg[CNC_N_AXIS];
        for (int i = 0; i < CNC_N_AXIS; i++) neg[i] = motor[i] != tool[i];
        int32_t z0[CNC_N_AXIS] = {0, 0, 0};
        cnc_gc_sync(tool, z0, neg);
        for (int i = 0; i < CNC_N_AXIS; i++) cnc_gc_set_slack(i, tool[i] - motor[i]);  // as the firmware's cncSyncFromAxes
        targets.resize(targetIdx);  // what was planned and not done is gone
        memcpy(from, tool, sizeof(from));
        // the operator then sends the tool somewhere: it must get there exactly
        lines.resize(lineIdx);
        lines.push_back("G90 G94 G0 X1 Z1 C0\n");
        lines.push_back("G1 X2 Z0.5 F300\n");
        waitingIdle = false;
      }
      for (Ovr& o : ovrs) {
        if (!o.done && now >= (uint64_t)(o.at * TIMER_HZ)) { cnc_set_overrides((uint8_t)o.feed, (uint8_t)o.rapid); o.done = true; }
      }
      if (holdRequested && !holdDone && cnc_hold_complete()) {
        if (holdStopAt == 0) holdStopAt = now;
        if (now >= holdStopAt + TIMER_HZ / 2) { cnc_hold_resume(); holdDone = true; }
      }
      if (!cnc_hold_active() && !(stopRequested && !stopped)) {
        if (waitingIdle) {
          if (!cnc_st_busy()) {
            if (pendingAction == CNC_DWELL) {
              dwellLeft -= 0.001f;  // the task waits in 1 ms steps
              if (dwellLeft <= 0) waitingIdle = false;
            } else {
              if (pendingAction == CNC_PAUSE) pauses++;  // the operator resumes at once in the simulation
              waitingIdle = false;
              if (pendingAction == CNC_END) lineIdx = lines.size();
            }
          }
        }
        while (!waitingIdle && lineIdx < lines.size()) {
          std::string code = stripComments(lines[lineIdx]);
          if (code.empty() || code == "%") { lineIdx++; continue; }
          char msg[128]; float dwell = 0;
          int32_t before[CNC_N_AXIS];
          memcpy(before, cnc_plan_position(), sizeof(before));
          int r = cnc_gc_line(code.c_str(), (uint32_t)(lineIdx + 1), msg, sizeof(msg), &dwell);
          if (r == CNC_WAIT) break;
          if (r == CNC_DWELL_MORE) {  // a cycle dwells, then the same line goes on
            waitingIdle = true; pendingAction = CNC_DWELL; dwellLeft = dwell;
            break;
          }
          if (r == CNC_THREAD || r == CNC_THREAD_MORE) {
            printf("line %zu: threading (G33 / G76) needs the spindle: not simulated\n", lineIdx + 1);
            errors++; lineIdx = lines.size(); break;
          }
          if (r == CNC_ERROR) {
            printf("line %zu \"%s\": error: %s\n", lineIdx + 1, code.c_str(), msg);
            errors++; lineIdx = lines.size(); break;
          }
          if (msg[0]) { warnings++; }
          // programmed time: the block this line planned (a line that did not move adds nothing; an arc counts
          // its last piece only, so programs with arcs give a rough figure)
          if (memcmp(before, cnc_plan_position(), sizeof(before)) != 0) idealMin += cnc_plan_last_block_minutes();
          lineIdx++;
          if (r == CNC_DWELL || r == CNC_PAUSE || r == CNC_END) { waitingIdle = true; pendingAction = r; dwellLeft = dwell; break; }
        }
      }
      cnc_st_prep_buffer();
      // next run of the task: usually 1 ms, sometimes a long gap (display, keypad, Preferences)
      rnd = rnd * 1103515245 + 12345;
      double gap = ((rnd >> 16) % 100 == 0) ? gapTicks : TIMER_HZ / 1000.0;
      nextTask = now + (uint64_t)gap;
    }

    // ---- the step interrupt
    while (nextIsr <= nextTask) {
      now = nextIsr;
      // position samples every 50 ms, for the speed and acceleration check
      while (nextSample <= now) {
        Sample sm; memcpy(sm.p, tool, sizeof(tool)); sm.block = targetIdx; samples.push_back(sm);
        if (samples.size() >= 3) {
          const Sample& a = samples[samples.size() - 3]; const Sample& b = samples[samples.size() - 2]; const Sample& c = samples.back();
          if (a.block == c.block) {
            double dt = SAMPLE_S;
            for (int i = 0; i < CNC_N_AXIS; i++) {
              double v = fabs((double)(c.p[i] - a.p[i])) / (2 * dt) / s.stepsPerUnit[i] * 60;            // units/min
              double acc = fabs((double)(c.p[i] - 2 * b.p[i] + a.p[i])) / (dt * dt) / s.stepsPerUnit[i] * 3600;  // units/min^2
              // quantisation: every sample is up to half a step off, so a second difference up to 2 steps
              double vq = 1.0 / (2 * dt) / s.stepsPerUnit[i] * 60, aq = 4.0 / (dt * dt) / s.stepsPerUnit[i] * 3600;
              if (v - vq > maxRateSeen[i]) maxRateSeen[i] = v - vq;
              if (acc - aq > maxAccSeen[i]) maxAccSeen[i] = acc - aq;
            }
          }
          if (samples.size() > 3) samples.erase(samples.begin());
        }
        nextSample += (uint64_t)(SAMPLE_S * TIMER_HZ);
      }
      cnc_tick_t tick;
      bool active;
      if (delayed) {
        cnc_tick_t comp;
        memset(&tick, 0, sizeof(tick));
        if (pendValid) tick = pend;
        pendValid = false;
        // pin timing: the pending step goes out now, or later if its direction pins were set less than D us ago
        double tUs = (double)now * 1e6 / TIMER_HZ, outUs = tUs;
        if (tick.step) {
          if (outUs < dirSetAt + dirUs) outUs = dirSetAt + dirUs;
          if (outUs - tUs > maxLateUs) maxLateUs = outUs - tUs;
          for (int i = 0; i < CNC_N_AXIS; i++) {
            if (!(tick.step & (1 << i))) continue;
            double sp = outUs - pinStep[i];
            if (sp < minSpacing) minSpacing = sp;
            if (sp < 2 * pulseUs - 1e-6) { if (pinErrors < 5) printf("  %c: steps %.2f us apart (pulse %.1f us)\n", "XZC"[i], sp, pulseUs); pinErrors++; }
            double lead = outUs - pinDir[i];
            if (lead < minDirLead) minDirLead = lead;
            if (lead < dirUs - 1e-6) { if (pinErrors < 5) printf("  %c: direction set %.2f us before the step\n", "XZC"[i], lead); pinErrors++; }
            pinStep[i] = outUs;
          }
          pulseEndAny = outUs + pulseUs;
        }
        active = cnc_isr(&comp);
        if (active && comp.step) {
          pend = comp; pendValid = true;
          // its direction pins are set when this interrupt's pulse ends (at once if there was no pulse)
          // (a change waits until H us after the last pulse ended: the direction hold)
          double setUs = tick.step ? outUs + pulseUs : tUs;
          bool changed = false;
          for (int i = 0; i < CNC_N_AXIS; i++) {
            if ((comp.step & (1 << i)) && ((comp.dirNeg >> i) & 1) != pinDirNeg[i] && setUs < pulseEndAny + holdUs) setUs = pulseEndAny + holdUs;
          }
          for (int i = 0; i < CNC_N_AXIS; i++) {
            if (!(comp.step & (1 << i))) continue;
            int dn = (comp.dirNeg >> i) & 1;
            if (dn != pinDirNeg[i]) {
              if (setUs < pinStep[i] + pulseUs - 1e-6) { if (pinErrors < 5) printf("  %c: direction changed during a pulse\n", "XZC"[i]); pinErrors++; }
              double hold = setUs - (pinStep[i] + pulseUs);
              if (pinStep[i] > -1e8 && hold < minDirHold) minDirHold = hold;
              if (pinStep[i] > -1e8 && hold < holdUs - 1e-6) { if (pinErrors < 5) printf("  %c: direction changed %.2f us after its pulse\n", "XZC"[i], hold); pinErrors++; }
              pinDirNeg[i] = dn; pinDir[i] = setUs; changed = true; dirChanges++;
            }
          }
          if (changed) dirSetAt = setUs;
        }
        tick.period = comp.period;
      } else {
        active = cnc_isr(&tick);
      }
      if (!active && cnc_st_busy() && cnc_plan_blocks_used() > 0 && !cnc_hold_active() && !idleSinceStart) {
        underruns++;
      }
      if (active) idleSinceStart = false;
      for (int i = 0; i < CNC_N_AXIS; i++) {
        if (!(tick.step & (1 << i))) continue;
        int d = (tick.dirNeg & (1 << i)) ? -1 : 1;
        motor[i] += d;
        if (!tick.motorOnly) {
          tool[i] += d;
          steps[i]++;
        }
      }
      if (tick.step && !tick.motorOnly && atMoveEnd) {  // first step after a move's end point
        gapsMs.push_back((double)(now - endStepAt) * 1000.0 / TIMER_HZ);
        atMoveEnd = false;
      }
      if (tick.step && !tick.motorOnly) {
        // path check against the programmed straight line
        while (targetIdx < targets.size()) {
          const int32_t* b = targets[targetIdx].p;
          int32_t dom = 0; int domAxis = 0;
          for (int i = 0; i < CNC_N_AXIS; i++) { int32_t dd = abs(b[i] - from[i]); if (dd > dom) { dom = dd; domAxis = i; } }
          double sFrac = dom ? (double)(tool[domAxis] - from[domAxis]) / (b[domAxis] - from[domAxis]) : 1;
          double dev = 0;
          for (int i = 0; i < CNC_N_AXIS; i++) {
            double expect = from[i] + sFrac * (b[i] - from[i]);
            double e = fabs(tool[i] - expect);
            if (e > dev) dev = e;
          }
          if (dev > worstDev) worstDev = dev;
          if (dev > 1.01) {
            if (errors < 5) printf("off the line by %.2f steps at line %u (tool %d %d %d)\n", dev, targets[targetIdx].line, tool[0], tool[1], tool[2]);
            errors++;
          }
          if (memcmp(tool, b, sizeof(tool)) == 0) {
            memcpy(from, b, sizeof(from));
            if (reachTimes) printf("R %u %.6f\n", targets[targetIdx].line, (double)now / TIMER_HZ);
            // a move ends here when the next point belongs to another line (arc pieces share their line)
            if (stopsMode && targetIdx + 1 < targets.size() && targets[targetIdx + 1].line != targets[targetIdx].line) {
              atMoveEnd = true; endStepAt = now;
            }
            targetIdx++;
          }
          break;
        }
      }
      if (tick.period == 0) {
        printf("  the step interrupt asked to be called again at once (period 0) at %.6f s, line %zu\n", (double)now / TIMER_HZ, lineIdx);
        errors++;
        lineIdx = lines.size(); waitingIdle = false; cnc_st_reset(); cnc_plan_reset();
        break;
      }
      nextIsr = now + tick.period;
    }
    now = nextTask;
  }

  // ---- results
  bool reachedAll = targetIdx == targets.size();
  const int32_t* fin = targets.empty() ? from : targets.back().p;
  bool endOk = memcmp(tool, fin, sizeof(tool)) == 0;
  printf("%s: %zu lines, %zu moves, %.1f s (programmed %.1f s), worst %.2f step(s) off the line, underruns %d, pauses %d, notes %d\n",
         file, lines.size(), targets.size(), (double)now / TIMER_HZ, idealMin * 60, worstDev, underruns, pauses, warnings);
  for (int i = 0; i < CNC_N_AXIS; i++) {
    printf("  %c: %llu steps, max rate %.1f of %.1f /min, max accel %.0f of %.0f /min2\n", "XZC"[i],
           (unsigned long long)steps[i], maxRateSeen[i], s.maxRate[i], maxAccSeen[i], s.acceleration[i]);
  }
  int fails = errors + underruns + pendingWhileIdle;
  if (pendingWhileIdle) printf("  a step was still pending while the motion counted as finished (%d times)\n", pendingWhileIdle);
  if (pendValid) { printf("  a step was never put out\n"); fails++; }
  if (!reachedAll || !endOk) { printf("  NOT at the end point: tool %d %d %d, expected %d %d %d\n", tool[0], tool[1], tool[2], fin[0], fin[1], fin[2]); fails++; }
  for (int i = 0; i < CNC_N_AXIS; i++) {
    if (maxRateSeen[i] > s.maxRate[i] * 1.02) { printf("  %c faster than its maximum\n", "XZC"[i]); fails++; }
    if (maxAccSeen[i] > s.acceleration[i] * 1.05) { printf("  %c accelerates harder than allowed\n", "XZC"[i]); fails++; }
  }
  for (int i = 0; i < CNC_N_AXIS; i++) {
    int32_t expectMotor = tool[i] - cnc_gc_slack(i);
    if (motor[i] != expectMotor) { printf("  %c motor %d, expected %d\n", "XZC"[i], motor[i], expectMotor); fails++; }
  }
  if (stopsMode) {
    // a move ends where the next block belongs to another line; its end speed and the next block's start speed
    int moveEnds = 0, stopped = 0;
    double maxStopSpeed = 0, minGoSpeed = 1e9;
    for (size_t k = 0; k + 1 < blockEvents.size(); k++) {
      const BlockEvent& e = blockEvents[k];
      if (e.start || (e.condition & CNC_COND_BACKLASH)) continue;
      const BlockEvent* next = NULL;
      for (size_t q = k + 1; q < blockEvents.size(); q++) if (blockEvents[q].start && !(blockEvents[q].condition & CNC_COND_BACKLASH)) { next = &blockEvents[q]; break; }
      if (!next || next->line == e.line) continue;
      moveEnds++;
      double v = e.speed > next->speed ? e.speed : next->speed;
      if (v == 0) { stopped++; if (v > maxStopSpeed) maxStopSpeed = v; } else if (v < minGoSpeed) minGoSpeed = v;
    }
    double gMin = 1e9, gMax = 0;
    for (double g : gapsMs) { if (g < gMin) gMin = g; if (g > gMax) gMax = g; }
    printf("  move ends: %d, at rest (end and next start at speed 0) at %d; slowest pass-through %.3f /min; "
           "time from the last step to the next %.3f ... %.3f ms\n",
           moveEnds, stopped, minGoSpeed < 1e9 ? minGoSpeed : 0.0, gapsMs.empty() ? 0.0 : gMin, gMax);
    bool want = !strcmp(stopsMode, "all");
    if (moveEnds == 0 || (want && stopped != moveEnds) || (!want && stopped != 0)) {
      printf("  expected %s move ends at rest\n", want ? "all" : "no");
      fails++;
    }
  }
  if (delayed) {
    printf("  pins: closest steps of one axis %.2f us (needs %.1f), direction set %.2f us before its step at the least "
           "(needs %.1f) and changed %.2f us after its last pulse at the least (needs %.1f), %d direction changes, a step "
           "late by up to %.2f us\n", minSpacing < 1e9 ? minSpacing : 0.0, 2 * pulseUs, minDirLead < 1e9 ? minDirLead : 0.0,
           dirUs, minDirHold < 1e9 ? minDirHold : 0.0, holdUs, dirChanges, maxLateUs);
    fails += pinErrors;
  }
  if (holdAt >= 0 && holdStopAt) printf("  hold: the tool stood still %.0f ms after the request\n", ((double)holdStopAt / TIMER_HZ - holdAt) * 1000);
  if (stopAt >= 0) {
    printf("  STOP at %.1f s (line %zu): stopped at %d %d %d, then moved on to %d %d %d\n", stopAt, stopLine,
           stopPos[0], stopPos[1], stopPos[2], tool[0], tool[1], tool[2]);
    if (!stopped) { printf("  the STOP did not end\n"); fails++; }
  }
  // the programmed time is the fastest possible: at feed override <= 100 % the motion never takes less
  if (!maxFeedOver100 && stopAt < 0 && idealMin * 60 > (double)now / TIMER_HZ + 0.01) {
    printf("  faster than programmed: %.3f s for %.3f s\n", (double)now / TIMER_HZ, idealMin * 60);
    fails++;
  }
  printf("  %s\n", fails ? "FAIL" : "OK");
  return fails ? 1 : 0;
}
