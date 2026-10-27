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
    --machine marios|h4default
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

static std::string stripComments(const std::string& raw) {
  std::string out;
  bool inParen = false;
  for (char c : raw) {
    if (inParen) { if (c == ')') inParen = false; continue; }
    if (c == '(') { inParen = true; continue; }
    if (c == ';') break;
    out += c;
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '\r' || out.back() == '\t')) out.pop_back();
  size_t b = out.find_first_not_of(" \t");
  return b == std::string::npos ? "" : out.substr(b);
}

int main(int argc, char** argv) {
  const char* file = NULL;
  double taskGapMs = 50, holdAt = -1;
  int32_t backlash[CNC_N_AXIS] = {0, 0, 0};
  const char* machineName = "marios";
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--task-gap-ms")) taskGapMs = atof(argv[++i]);
    else if (!strcmp(argv[i], "--hold-at")) holdAt = atof(argv[++i]);
    else if (!strcmp(argv[i], "--backlash")) sscanf(argv[++i], "%d,%d,%d", &backlash[0], &backlash[1], &backlash[2]);
    else if (!strcmp(argv[i], "--machine")) machineName = argv[++i];
    else file = argv[i];
  }
  if (!file) { fprintf(stderr, "usage: cnc_sim [options] program.ngc\n"); return 2; }

  cnc_settings_t s;
  if (!strcmp(machineName, "h4default")) {
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

  auto done = [&]() { return lineIdx >= lines.size() && !waitingIdle && !cnc_st_busy(); };

  while (!done()) {
    // ---- the feeding task
    if (now >= nextTask) {
      if (holdAt >= 0 && !holdRequested && now >= (uint64_t)(holdAt * TIMER_HZ)) { cnc_hold_start(); holdRequested = true; }
      if (holdRequested && !holdDone && cnc_hold_complete()) {
        if (holdStopAt == 0) holdStopAt = now;
        if (now >= holdStopAt + TIMER_HZ / 2) { cnc_hold_resume(); holdDone = true; }
      }
      if (!cnc_hold_active()) {
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
          idealMin += cnc_plan_last_block_minutes();  // (an arc counts its last piece only: a rough figure)
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
      bool active = cnc_isr(&tick);
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
            targetIdx++;
          }
          break;
        }
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
  int fails = errors + underruns;
  if (!reachedAll || !endOk) { printf("  NOT at the end point: tool %d %d %d, expected %d %d %d\n", tool[0], tool[1], tool[2], fin[0], fin[1], fin[2]); fails++; }
  for (int i = 0; i < CNC_N_AXIS; i++) {
    if (maxRateSeen[i] > s.maxRate[i] * 1.02) { printf("  %c faster than its maximum\n", "XZC"[i]); fails++; }
    if (maxAccSeen[i] > s.acceleration[i] * 1.05) { printf("  %c accelerates harder than allowed\n", "XZC"[i]); fails++; }
  }
  for (int i = 0; i < CNC_N_AXIS; i++) {
    int32_t expectMotor = tool[i] - ((backlash[i] > 0 && cnc_gc_last_neg(i)) ? backlash[i] : 0);
    if (motor[i] != expectMotor) { printf("  %c motor %d, expected %d\n", "XZC"[i], motor[i], expectMotor); fails++; }
  }
  printf("  %s\n", fails ? "FAIL" : "OK");
  return fails ? 1 : 0;
}
