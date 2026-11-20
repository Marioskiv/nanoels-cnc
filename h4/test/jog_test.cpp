/*
  jog_test.cpp - jogging from the app when the app stops talking (work plan B6). The H4 is jogged from the app in two
  ways, both checked here with the firmware's own code (extracted unchanged by run_tests.sh):
  1. the gamepad: the app sends the wanted speeds to /jog about every 100 ms (grbl.ino webJogRequest, webJogService,
     webJogStop) and, since firmware 1.47, the step generator's jog (cnc_core.cpp cnc_jog_*) moves every axis on its
     own, as LinuxCNC's jog: each with its own acceleration, braking for its soft limit, taking up its backlash (8c-8e).
     Before 1.47 the speeds became short G1 pieces through the planner (one straight move of all axes, as Grbl's $J=). Simulated: the step interrupt on a 20 MHz clock, taskGcode every 1 ms with now
     and then a long gap, the app's requests (also late and irregular, as over WiFi), and what the firmware does
     around them (models, written here: grblEnsureOn starting the session, setIsOnFromLoop / cncService stopping,
     the end of the session in grblServiceLine). Checked on the author's machine (X 12 mm/s 100 mm/s2, Z 25 mm/s 100 mm/s2, C 360 deg/s 720 deg/s2):
     - requests stop (WiFi lost, app closed, tablet asleep): standing still within 300 ms + one service period +
       braking, travel after the last request within speed * that time + v^2 / 2a; the position afterwards is exact
       (planner = steps given) and the next jog starts from there;
     - speeds 0 stop at once (braking only); requests up to 280 ms apart: no stop, no dip in speed, no underrun;
     - never faster or harder accelerating than the axis allows (also reversing a stick), never during a program;
     - a refusal stays until the stick is let go (speeds 0).
  Found 2026-11-08: a hold starts after the motion the step generator has already prepared (CNC_SEGMENT_AHEAD_S 0.1 s),
  so a stop comes up to ~0.12 s later than the request; C reaches ~288 deg/s, not 360, on the stick (the queued 0.2 s
  must be enough to brake: v <= 2 a 0.2). With X/Z and C together the G94 feed was taken along X, Z and C, so X/Z
  moved ~13x slower: fixed in cnc_gcode.cpp (F along X/Z as LinuxCNC), checked here at full speed.
  When X or Z changed direction while C turned fast (a stick moved suddenly), X/Z jumped in speed at the corner (1.8 x
  their acceleration over 20 ms): Grbl's junction deviation works on the X-Z-C direction with degrees counted as mm.
  Fixed in cnc_core.cpp (firmware 1.36): at a corner each axis jumps by at most its acceleration x 10 ms (what the
  step generator changes in one segment), so over 20 ms an axis may show up to 1.5 x its acceleration, never more.
  2. the arrows on the app's screen: /key with hold=1 every 150 ms (wifi.ino handleKey, releaseHeldKey and the fail-safe
     line of the web task): when the repeats stop, the key is released after 500 ms, also when the keypad task is busy.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

using std::max;

// ---- a little of Arduino
struct String {
  std::string s;
  String() {}
  String(const char* c) : s(c) {}
  String(const std::string& x) : s(x) {}
  const char* c_str() const { return s.c_str(); }
  long toInt() const { return atol(s.c_str()); }
  bool operator==(const char* c) const { return s == c; }
};
static String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }

static const uint32_t TIMER_HZ = 20000000;
static uint64_t now = 0;  // timer ticks
unsigned long millis() { return (unsigned long)(now * 1000 / TIMER_HZ); }
#define constrain(v, a, b) ((v) < (a) ? (a) : (v) > (b) ? (b) : (v))

// ---- what webJogService reads from the rest of the firmware
#define MODE_GCODE 9
int mode = MODE_GCODE;
bool isOn = false;
volatile bool grblSession = false;
volatile bool grblJogging = false;
volatile int grblChannel = -1;
unsigned long grblLastMs = 0;
char cncLastError[100] = "";
bool homingActive = false, homingRequest = false;
static const char* startRefusal = NULL;  // e.g. not homed
const char* cncStartRefusal() { return startRefusal; }
static bool cncStopRequest = false;
static bool programRunning = false;      // a file program (isOn without a Grbl session)
bool cncBusy() { return cnc_st_busy() || cnc_plan_blocks_used() > 0 || cncStopRequest || programRunning; }
static bool nextIsOnFlag = false, nextIsOn = false;
void setIsOnFromTask(bool on) { nextIsOn = on; nextIsOnFlag = true; }

static int32_t tool[CNC_N_AXIS] = {0, 0, 0};
static int32_t motor[CNC_N_AXIS] = {0, 0, 0};       // motor position: differs from the tool by the backlash not taken up
static int32_t backlash[CNC_N_AXIS] = {0, 0, 0};    // steps
static int32_t limLo[CNC_N_AXIS] = {INT32_MIN, INT32_MIN, INT32_MIN}, limHi[CNC_N_AXIS] = {INT32_MAX, INT32_MAX, INT32_MAX};
static bool cncStartRequest = false;                // (the firmware takes the positions before the jog starts)
static void syncFromAxes() {  // cncSyncFromAxes: drop what is queued, the position is where the tool stands
  cnc_st_reset();
  cnc_plan_reset();
  int32_t z0[CNC_N_AXIS] = {0, 0, 0};
  bool neg[CNC_N_AXIS] = {false, false, false};
  cnc_gc_sync(tool, z0, neg);
}

// webJogBegin: grblEnsureOn (start a session: the H4 ON)
bool webJogBegin() {
  if (isOn || cncBusy() || homingActive) return false;
  if (cncStartRefusal()) { snprintf(cncLastError, sizeof(cncLastError), "%s", cncStartRefusal()); return false; }
  grblSession = true;
  isOn = true;
  return true;
}
// webJogAxes: the positions, backlash and limits of the axes
void webJogAxes(cnc_jog_axes_t* ax) {
  for (int i = 0; i < CNC_N_AXIS; i++) {
    ax->pos[i] = tool[i];
    ax->slack[i] = tool[i] - motor[i];
    ax->backlash[i] = backlash[i];
    ax->lo[i] = limLo[i];
    ax->hi[i] = limHi[i];
  }
}

#include "jog_funcs.inc"

// ---- the arrows of the app (wifi.ino)
struct Server {
  std::string code, hold;
  int lastStatus = 0;
  String arg(const char* n) { return String(!strcmp(n, "code") ? code : !strcmp(n, "hold") ? hold : std::string()); }
  void send(int st, const char*, const char*) { lastStatus = st; }
} server;
static std::vector<int> keyEvents;   // what the keypad task got (128 + code: pressed)
static int keypadBusyTries = 0;      // the next tries fail (the keypad task busy)
bool sendKeyEvent(int e) {
  if (keypadBusyTries > 0) { keypadBusyTries--; return false; }
  keyEvents.push_back(e);
  return true;
}
static void vTaskDelay(int) { now += TIMER_HZ / 200; }
#include "key_funcs.inc"

// ---- the test
static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) failures++;
  printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
}

static cnc_settings_t settings;
static double vmax[CNC_N_AXIS], amax[CNC_N_AXIS];  // units/s, units/s2

static void setup() {
  now = (uint64_t)TIMER_HZ * 60;  // a minute after power on
  settings.stepsPerUnit[CNC_X] = 1200; settings.stepsPerUnit[CNC_Z] = 400; settings.stepsPerUnit[CNC_C] = 91.733333f;
  settings.maxRate[CNC_X] = 12 * 60; settings.maxRate[CNC_Z] = 25 * 60; settings.maxRate[CNC_C] = 360 * 60;
  settings.acceleration[CNC_X] = 100 * 3600; settings.acceleration[CNC_Z] = 100 * 3600; settings.acceleration[CNC_C] = 720 * 3600;
  settings.junctionDeviation = 0.01f;
  settings.timerHz = TIMER_HZ;
  cnc_core_settings(&settings);
  for (int i = 0; i < CNC_N_AXIS; i++) { vmax[i] = settings.maxRate[i] / 60; amax[i] = settings.acceleration[i] / 3600; }
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
  for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
  m.rotary[CNC_C] = true;
  m.defaultFeed = 120;
  cnc_gc_init(&m);
  syncFromAxes();
}

// One run: the app sends speeds (units/min) at the times given (ms; speeds 0 0 0 = stick let go) and then nothing.
struct Req { double atMs; float v[3]; };
struct Result {
  double lastReqMs = -1, stillMs = -1;           // last request with a speed; tool standing still from
  double travelAfter[CNC_N_AXIS] = {0, 0, 0};    // units moved after the last request
  double vSeen[CNC_N_AXIS] = {0, 0, 0}, aSeen[CNC_N_AXIS] = {0, 0, 0};
  double minSpeedRatio = 1e9;                    // lowest speed / wanted while requests came (after speeding up)
  uint32_t underruns = 0;
  bool exact = false;                            // planner position = steps given, after the stop
  bool refused = false;
  bool endedOn = false;                          // the H4 still ON at the end
  double longestTickS = 0;                       // longest wait of the step interrupt while it had motion
  std::vector<std::vector<double>> speeds;       // every 20 ms: time (ms), speed X, Z, C (units/s)
};

static Result runJog(const std::vector<Req>& reqs, double endMs, double gapEveryMs = 0, double gapMs = 0) {
  Result r;
  uint32_t under0 = cnc_st_underruns();
  uint64_t start = now, nextTask = now, nextIsr = now, nextSample = now;
  size_t ri = 0;
  int32_t atLast[CNC_N_AXIS] = {0, 0, 0};
  std::vector<std::vector<double>> samples;     // positions every 20 ms
  const double SAMPLE_S = 0.02;
  double nextGap = gapEveryMs;
  uint64_t lastStepAt = now;
  float wanted[3] = {0, 0, 0};
  double wantedSince = -1;
  while ((double)(now - start) * 1000 / TIMER_HZ < endMs) {
    double tMs = (double)(now - start) * 1000 / TIMER_HZ;
    if (now >= nextTask) {
      // the app's requests (the web task)
      while (ri < reqs.size() && reqs[ri].atMs <= tMs) {
        const float* v = reqs[ri].v;
        bool ok = webJogRequest(v[0], v[1], v[2]);
        if (!ok) r.refused = true;
        if (v[0] || v[1] || v[2]) {
          r.lastReqMs = tMs;
          memcpy(atLast, tool, sizeof(atLast));
          if (memcmp(wanted, v, sizeof(wanted))) { memcpy(wanted, v, sizeof(wanted)); wantedSince = tMs; }
        } else {
          memset(wanted, 0, sizeof(wanted));
          r.lastReqMs = tMs;
          memcpy(atLast, tool, sizeof(atLast));
        }
        ri++;
      }
      // loop(): setIsOnFromLoop
      if (nextIsOnFlag) {
        nextIsOnFlag = false;
        if (!nextIsOn) {
          isOn = false;
          if (cncBusy()) cncStopRequest = true;
        }
      }
      // taskGcode: cncService (the stop), grblServiceLine (end of the session), webJogService, prep
      if (cncStopRequest) {
        cnc_hold_start();
        cnc_st_prep_buffer();
        if (cnc_hold_complete() || !cnc_st_busy()) { syncFromAxes(); cncStopRequest = false; }
      } else {
        if (grblSession && !isOn) { grblSession = false; grblJogging = false; }
        if (grblSession && !cncBusy() && millis() - grblLastMs > 300) {
          grblSession = false; grblJogging = false; setIsOnFromTask(false);
        }
        if (grblJogging && !cncBusy()) grblJogging = false;
        while (webJogService()) {}
      }
      cnc_st_prep_buffer();
      double gap = 1;
      if (gapEveryMs > 0 && tMs >= nextGap) { gap = gapMs; nextGap += gapEveryMs; }
      nextTask = now + (uint64_t)(gap * TIMER_HZ / 1000);
    }
    // the step interrupt until the next task run
    while (nextIsr <= nextTask) {
      now = nextIsr;
      if (now >= nextSample) {
        std::vector<double> p(CNC_N_AXIS);
        for (int i = 0; i < CNC_N_AXIS; i++) p[i] = tool[i] / settings.stepsPerUnit[i];
        samples.push_back(p);
        size_t n = samples.size();
        if (n >= 3) {
          double tS = (double)(now - start) / TIMER_HZ * 1000;
          for (int i = 0; i < CNC_N_AXIS; i++) {
            double v1 = (samples[n - 1][i] - samples[n - 2][i]) / SAMPLE_S, v0 = (samples[n - 2][i] - samples[n - 3][i]) / SAMPLE_S;
            if (i == 0) r.speeds.push_back({tS, 0, 0, 0});
            r.speeds.back()[1 + i] = v1;
            r.vSeen[i] = max(r.vSeen[i], fabs(v1));
            r.aSeen[i] = max(r.aSeen[i], fabs(v1 - v0) / SAMPLE_S);
            // steady speed while requests keep coming: at least 0.9 of the wanted (after speeding up)
            double w = wanted[i] / 60;
            // (after the time the axis needs to speed up with its own acceleration, + 0.1 s for the request to arrive)
            double upMs = fabs(w) / amax[i] * 1000 + 100;
            if (w != 0 && wantedSince >= 0 && tS - wantedSince > upMs && tS < r.lastReqMs + 60 && ri < reqs.size()) {
              r.minSpeedRatio = std::min(r.minSpeedRatio, v1 / w);
            }
          }
        }
        nextSample += (uint64_t)(SAMPLE_S * TIMER_HZ);
      }
      cnc_tick_t tick;
      if (cnc_isr(&tick) && tick.period) r.longestTickS = max(r.longestTickS, (double)tick.period / TIMER_HZ);
      for (int i = 0; i < CNC_N_AXIS; i++) {
        if (!(tick.step & (1 << i))) continue;
        motor[i] += (tick.dirNeg & (1 << i)) ? -1 : 1;
        if (tick.motorOnly & (1 << i)) continue;
        tool[i] += (tick.dirNeg & (1 << i)) ? -1 : 1;
        lastStepAt = now;
      }
      nextIsr = now + (tick.period ? tick.period : 1);
    }
    now = nextTask;
  }
  if (lastStepAt > start) r.stillMs = (double)(lastStepAt - start) * 1000 / TIMER_HZ;
  for (int i = 0; i < CNC_N_AXIS; i++) r.travelAfter[i] = fabs((double)(tool[i] - atLast[i]) / settings.stepsPerUnit[i]);
  r.underruns = cnc_st_underruns() - under0;
  // exact: nothing left, the planner where the tool is, and the motor within the backlash of the tool
  r.exact = !cnc_st_busy() && memcmp(cnc_plan_position(), tool, sizeof(tool)) == 0;
  for (int i = 0; i < CNC_N_AXIS; i++) r.exact = r.exact && tool[i] - motor[i] >= 0 && tool[i] - motor[i] <= backlash[i];
  r.endedOn = isOn;
  return r;
}

// requests every periodMs (+ jitter up to jitterMs) from fromMs to toMs with speeds v
static void addReqs(std::vector<Req>& q, double fromMs, double toMs, double periodMs, double jitterMs, float vx, float vz, float vc) {
  unsigned s = 12345;
  for (double t = fromMs; t <= toMs; ) {
    q.push_back({t, {vx, vz, vc}});
    s = s * 1103515245 + 12345;
    t += periodMs + (jitterMs > 0 ? ((s >> 16) % 1000) / 1000.0 * jitterMs : 0);
  }
}

// the stop after the app went away: within 300 ms + a service period + braking, travel within its bound
static void checkStop(const Result& r, const float v[3], double extraMs, const char* name) {
  char what[240];
  extraMs += CNC_SEGMENT_AHEAD_S * 1000 + 20;  // a hold starts after what the step generator has prepared (+ the segment running and the next)
  // the axes brake together (one straight move): in the time of the slowest one, each covering v * t / 2
  double brakeS = 0, bound[CNC_N_AXIS];
  for (int i = 0; i < CNC_N_AXIS; i++) brakeS = max(brakeS, fabs(v[i]) / 60 / amax[i]);
  for (int i = 0; i < CNC_N_AXIS; i++) {
    double vv = fabs(v[i]) / 60;
    bound[i] = vv * (extraMs / 1000 + brakeS / 2) + 2 / settings.stepsPerUnit[i];
  }
  double limitMs = extraMs + brakeS * 1000 + 5;
  snprintf(what, sizeof(what), "%s: standing still %.0f ms after the last request (limit %.0f)", name, r.stillMs - r.lastReqMs, limitMs);
  check(r.stillMs - r.lastReqMs <= limitMs, what);
  for (int i = 0; i < CNC_N_AXIS; i++) {
    if (v[i] == 0) continue;
    snprintf(what, sizeof(what), "%s: %c moved %.3f after the last request (limit %.3f)", name, "XZC"[i], r.travelAfter[i], bound[i]);
    check(r.travelAfter[i] <= bound[i], what);
  }
  snprintf(what, sizeof(what), "%s: position exact afterwards, no underrun (%u), the H4 OFF again", name, r.underruns);
  check(r.exact && r.underruns == 0 && !r.endedOn && !grblSession, what);
}

// speed within 5 % (+ steps), acceleration over 20 ms within 1.5 x (a corner may add a jump of a x 10 ms) + steps
static void checkLimits(const Result& r, const char* name) {
  char what[240];
  bool ok = true;
  for (int i = 0; i < CNC_N_AXIS; i++) ok = ok && r.vSeen[i] <= vmax[i] * 1.05 + 2 / settings.stepsPerUnit[i] / 0.02 && r.aSeen[i] <= amax[i] * 1.5 + 4 / settings.stepsPerUnit[i] / (0.02 * 0.02);
  snprintf(what, sizeof(what), "%s: speed X %.1f Z %.1f C %.0f, accel X %.0f Z %.0f C %.0f within the axis limits", name,
           r.vSeen[0], r.vSeen[1], r.vSeen[2], r.aSeen[0], r.aSeen[1], r.aSeen[2]);
  check(ok, what);
}

int main() {
  setup();
  char what[240];
  const float VX = 12 * 60, VZ = 25 * 60, VC = 360 * 60;  // full stick: the axis limits

  // 1. each axis at full speed for 3 s, then the app goes away (WiFi lost, tablet asleep, app closed)
  struct { const char* name; float v[3]; } axes[] = {
      {"X full speed, app gone", {VX, 0, 0}}, {"Z full speed, app gone", {0, -VZ, 0}}, {"C full speed, app gone", {0, 0, VC}},
      {"X+Z+C together, app gone", {VX, VZ, -VC}}, {"X slow (60 mm/min), app gone", {60, 0, 0}}};
  for (auto& a : axes) {
    std::vector<Req> q;
    addReqs(q, 0, 3000, 100, 0, a.v[0], a.v[1], a.v[2]);
    Result r = runJog(q, 5000);
    checkStop(r, a.v, 300 + 1, a.name);
    checkLimits(r, a.name);
    snprintf(what, sizeof(what), "%s: steady speed while the app sends (lowest %.2f of wanted)", a.name, r.minSpeedRatio);
    check(r.minSpeedRatio >= 0.9, what);
  }

  // 2. stick let go: speeds 0 stop at once (braking only, no 300 ms)
  {
    float v[3] = {0, VZ, 0};
    std::vector<Req> q;
    addReqs(q, 0, 2000, 100, 0, v[0], v[1], v[2]);
    q.push_back({2050, {0, 0, 0}});
    Result r = runJog(q, 4000);
    checkStop(r, v, 1, "Z stick let go (speeds 0)");
  }

  // 3. bad WiFi: requests 100-280 ms apart for 10 s: never stops in between, speed steady, no underrun
  {
    float v[3] = {VX, 0, 0};
    std::vector<Req> q;
    addReqs(q, 0, 10000, 100, 180, v[0], v[1], v[2]);
    Result r = runJog(q, 12000, 97, 40);  // and taskGcode stalls 40 ms every 97 ms (display, flash)
    snprintf(what, sizeof(what), "requests 100-280 ms apart + task stalls of 40 ms: speed steady (lowest %.2f), no underrun (%u)",
             r.minSpeedRatio, r.underruns);
    check(r.minSpeedRatio >= 0.9 && r.underruns == 0, what);
    checkStop(r, v, 300 + 41, "bad WiFi, then gone");
  }

  // 4. a stick reversed at full speed (+X to -X): braking and speeding up within the limits; then gone
  {
    std::vector<Req> q;
    addReqs(q, 0, 1500, 100, 0, VX, 0, 0);
    addReqs(q, 1600, 3000, 100, 0, -VX, 0, 0);
    Result r = runJog(q, 5000);
    checkLimits(r, "X reversed at full speed");
    float v[3] = {VX, 0, 0};
    checkStop(r, v, 301, "X reversed, then gone");
  }

  // 5. the next jog after a stop starts from where the tool stands and reaches the expected place
  {
    int32_t before = tool[CNC_Z];
    std::vector<Req> q;
    q.push_back({0, {0, 60, 0}});
    q.push_back({100, {0, 0, 0}});  // a tap: the stick pushed for one request of the app (100 ms)
    Result r = runJog(q, 1500);
    // speeds 0 after 100 ms: the stop comes after what was already prepared (0.06 s at 1 mm/s)
    double moved = (tool[CNC_Z] - before) / settings.stepsPerUnit[CNC_Z], bound = 1.0 * (0.1 + 0.06 + 0.01) + 0.01;
    snprintf(what, sizeof(what), "a tap of the stick at 60 mm/min: Z moved %.3f mm (limit %.3f), position exact", moved, bound);
    check(r.exact && moved > 0 && moved <= bound, what);
  }

  // 6. during a file program: refused ("A program is running"), nothing moves; the refusal stays until speeds 0
  {
    isOn = true; programRunning = true;
    int32_t p0[CNC_N_AXIS]; memcpy(p0, tool, sizeof(p0));
    std::vector<Req> q;
    addReqs(q, 0, 500, 100, 0, VX, 0, 0);
    Result r = runJog(q, 1000);
    check(r.refused && !strcmp(webJogError, "A program is running") && memcmp(p0, tool, sizeof(p0)) == 0,
          "during a program: refused (A program is running), nothing moved");
    isOn = false; programRunning = false;
    q.clear();
    addReqs(q, 0, 500, 100, 0, VX, 0, 0);
    r = runJog(q, 1000);
    check(r.refused && memcmp(p0, tool, sizeof(p0)) == 0, "the program ended, stick still held: still refused (let it go first)");
    q.clear();
    q.push_back({0, {0, 0, 0}});
    addReqs(q, 100, 1000, 100, 0, VX, 0, 0);
    r = runJog(q, 2000);
    check(!r.refused && tool[CNC_X] != p0[CNC_X] && r.exact && !isOn, "stick let go and pushed again: it jogs");
  }

  // 7. not homed (or any start refusal): refused with the reason, nothing moves
  {
    startRefusal = "Home All first";
    int32_t p0[CNC_N_AXIS]; memcpy(p0, tool, sizeof(p0));
    std::vector<Req> q;
    addReqs(q, 0, 500, 100, 0, 0, VZ, 0);
    Result r = runJog(q, 1000);
    check(r.refused && !strcmp(webJogError, "Home All first") && memcmp(p0, tool, sizeof(p0)) == 0,
          "not homed: refused with the reason, nothing moved");
    startRefusal = NULL;
    q.clear(); q.push_back({0, {0, 0, 0}});
    runJog(q, 200);
  }

  // 8. one minute of jogging with the stick moved around, then gone: exact position, no underrun
  {
    std::vector<Req> q;
    unsigned s = 7;
    for (double t = 0; t < 60000; t += 100) {
      if ((int)t % 2000 == 0) s = s * 1103515245 + 12345;
      float k = ((s >> 16) % 2001) / 1000.0f - 1, k2 = ((s >> 8) % 2001) / 1000.0f - 1;
      q.push_back({t, {VX * k, VZ * k2, VC * k * k2}});
    }
    Result r = runJog(q, 62000);
    checkLimits(r, "one minute, stick moved around");
    snprintf(what, sizeof(what), "one minute, stick moved around: position exact, no underrun (%u), OFF at the end", r.underruns);
    check(r.exact && r.underruns == 0 && !isOn, what);
  }

  // 8b. one axis moving, a second one added (and later let go): the first keeps its speed, no pause (firmware 1.46;
  //     before, the corner braked X almost to a stop), and the second speeds up / slows down with its acceleration
  {
    struct { const char* name; float a[3]; float b[3]; int keep; int add; } cases[] = {
        {"X moving, Z added", {VX / 2, 0, 0}, {VX / 2, VZ / 2, 0}, 0, 1},
        {"Z moving, X added", {0, VZ / 2, 0}, {VX / 2, VZ / 2, 0}, 1, 0},
        {"X moving, C added", {VX / 2, 0, 0}, {VX / 2, 0, VC / 2}, 0, 2}};
    for (auto& c : cases) {
      std::vector<Req> q;
      addReqs(q, 0, 1500, 100, 0, c.a[0], c.a[1], c.a[2]);
      addReqs(q, 1600, 3000, 100, 0, c.b[0], c.b[1], c.b[2]);
      addReqs(q, 3100, 4500, 100, 0, c.a[0], c.a[1], c.a[2]);  // the added axis let go again
      q.push_back({4600, {0, 0, 0}});
      Result r = runJog(q, 6000);
      double want = fabs(c.a[c.keep]) / 60, lo = 1e9;
      for (auto& sp : r.speeds) if (sp[0] > 1000 && sp[0] < 4400) lo = std::min(lo, fabs(sp[1 + c.keep]));
      snprintf(what, sizeof(what), "%s: the moving axis keeps its speed (lowest %.2f of %.2f), and back when let go", c.name, lo, want);
      check(lo >= 0.9 * want, what);
      checkLimits(r, c.name);
      check(r.exact && !isOn, "and it stops exactly");
    }
  }

  // 8c. every axis on its own (firmware 1.47, as LinuxCNC's jog): X held at half speed the whole time while Z and C do
  //     everything else (start, full speed, reverse at full speed, stop, start again): X's speed never changes (every
  //     20 ms sample within 2 % + one step), and no axis accelerates harder than its own limit (+ one step per sample)
  {
    std::vector<Req> q;
    float x = VX / 2;
    addReqs(q, 0, 1000, 100, 0, x, 0, 0);
    addReqs(q, 1100, 2000, 100, 0, x, VZ, 0);
    addReqs(q, 2100, 3000, 100, 0, x, -VZ, VC);
    addReqs(q, 3100, 4000, 100, 0, x, 0, -VC);
    addReqs(q, 4100, 5000, 100, 0, x, VZ / 3, 0);
    addReqs(q, 5100, 6000, 100, 0, x, -VZ, -VC / 2);
    q.push_back({6100, {0, 0, 0}});
    Result r = runJog(q, 8000);
    double lo = 1e9, hi = 0, aMax[CNC_N_AXIS] = {0, 0, 0};
    for (size_t k = 0; k < r.speeds.size(); k++) {
      double t = r.speeds[k][0];
      if (t > 800 && t < 6050) { lo = std::min(lo, r.speeds[k][1]); hi = max(hi, r.speeds[k][1]); }
      if (k > 0) for (int i = 0; i < CNC_N_AXIS; i++) aMax[i] = max(aMax[i], fabs(r.speeds[k][1 + i] - r.speeds[k - 1][1 + i]) / 0.02);
    }
    double w = x / 60, q1 = 1 / settings.stepsPerUnit[CNC_X] / 0.02;
    snprintf(what, sizeof(what), "X held at %.1f mm/s while Z and C start, reverse and stop: X between %.3f and %.3f", w, lo, hi);
    check(lo >= w * 0.98 - q1 && hi <= w * 1.02 + q1, what);
    bool aOk = true;
    for (int i = 0; i < CNC_N_AXIS; i++) aOk = aOk && aMax[i] <= amax[i] * 1.1 + 2 / settings.stepsPerUnit[i] / (0.02 * 0.02);
    snprintf(what, sizeof(what), "and each axis within its own acceleration: X %.0f Z %.0f C %.0f (limits %.0f %.0f %.0f)", aMax[0], aMax[1], aMax[2],
             amax[0], amax[1], amax[2]);
    check(aOk, what);
    check(r.exact && r.underruns == 0 && !isOn, "and it stops exactly, no underrun");
  }

  // 8d. soft limits: X at full speed toward a limit 5 mm away brakes with its own acceleration and stops exactly on it
  //     (LinuxCNC jog), not past it; the other way it moves again
  {
    int32_t lim = tool[CNC_X] + (int32_t)(5 * settings.stepsPerUnit[CNC_X]);
    limHi[CNC_X] = lim;
    std::vector<Req> q;
    addReqs(q, 0, 3000, 100, 0, VX, VZ / 2, 0);
    q.push_back({3100, {0, 0, 0}});
    Result r = runJog(q, 4000);
    double zMin = 1e9;
    for (auto& sp : r.speeds) if (sp[0] > 800 && sp[0] < 3000) zMin = std::min(zMin, sp[2]);
    snprintf(what, sizeof(what), "X at full speed toward a soft limit 5 mm away: stopped %d steps from it (0), Z went on (lowest %.2f of %.2f)",
             lim - tool[CNC_X], zMin, VZ / 2 / 60);
    check(tool[CNC_X] == lim && zMin >= VZ / 2 / 60 * 0.98 && r.exact, what);
    checkLimits(r, "toward the soft limit");
    int32_t p0 = tool[CNC_X];
    q.clear();
    addReqs(q, 0, 500, 100, 0, -VX, 0, 0);
    q.push_back({600, {0, 0, 0}});
    runJog(q, 1500);
    check(tool[CNC_X] < p0, "and away from the limit it moves again");
    limHi[CNC_X] = INT32_MAX;
  }

  // 8e. backlash per axis: Z down then up (0.3 mm backlash): the motor ends the take-up's length below the tool after
  //     the move down and level with it after the move up; X going on meanwhile is not slowed by Z's take-up
  {
    backlash[CNC_Z] = 120;
    std::vector<Req> q;
    addReqs(q, 0, 800, 100, 0, VX / 2, -VZ, 0);
    q.push_back({900, {0, 0, 0}});
    runJog(q, 1800);
    int32_t down = tool[CNC_Z] - motor[CNC_Z];
    q.clear();
    addReqs(q, 0, 800, 100, 0, VX / 2, 0, 0);
    addReqs(q, 900, 2000, 100, 0, VX / 2, VZ, 0);
    q.push_back({2100, {0, 0, 0}});
    Result r = runJog(q, 3000);
    int32_t up = tool[CNC_Z] - motor[CNC_Z];
    double lo = 1e9;
    for (auto& sp : r.speeds) if (sp[0] > 600 && sp[0] < 2050) lo = std::min(lo, sp[1]);
    snprintf(what, sizeof(what), "Z backlash 120 steps: motor below the tool by %d after down, %d after up; X kept %.2f of %.2f", down, up, lo,
             VX / 2 / 60);
    check(down == 120 && up == 0 && lo >= VX / 2 / 60 * 0.98 && r.exact, what);
    backlash[CNC_Z] = 0;
    memcpy(motor, tool, sizeof(motor));
  }

  // 9. one axis, let go, then two axes with one of them reversed (its backlash is taken up first): the jog must run
  //    and stop as any other (found 2026-11-20 on the simulated H4: it moved 0.2 mm and stayed in "Stopping")
  {
    struct { const char* name; float a[3]; float b[3]; } cases[] = {
        {"Z down, then X+ Z+ (Z reverses)", {0, -600, 0}, {600, 600, 0}},
        {"X out, then X- Z+ (X reverses)", {600, 0, 0}, {-600, 600, 0}},
        {"Z+, then X+ Z- (Z reverses)", {0, 600, 0}, {600, -600, 0}}};
    // with backlash on X and Z (take-up blocks): 0.15 mm and 0.3 mm
    cnc_machine_t m;
    memset(&m, 0, sizeof(m));
    m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
    for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
    m.rotary[CNC_C] = true;
    m.defaultFeed = 120;
    m.backlashSteps[CNC_X] = 180;
    m.backlashSteps[CNC_Z] = 120;
    backlash[CNC_X] = 180; backlash[CNC_Z] = 120;
    cnc_gc_init(&m);
    syncFromAxes();
    for (auto& c : cases) {
      std::vector<Req> q;
      addReqs(q, 0, 1000, 100, 0, c.a[0], c.a[1], c.a[2]);
      q.push_back({1100, {0, 0, 0}});
      runJog(q, 1800);
      int32_t p0[CNC_N_AXIS];
      memcpy(p0, tool, sizeof(p0));
      q.clear();
      addReqs(q, 0, 1200, 100, 0, c.b[0], c.b[1], c.b[2]);
      q.push_back({1300, {0, 0, 0}});
      Result r = runJog(q, 3000);
      double mx = (tool[CNC_X] - p0[CNC_X]) / settings.stepsPerUnit[CNC_X], mz = (tool[CNC_Z] - p0[CNC_Z]) / settings.stepsPerUnit[CNC_Z];
      snprintf(what, sizeof(what), "%s: moved X %.3f Z %.3f mm (about 12 each), stopped, exact, not left stopping", c.name, mx, mz);
      check(fabs(fabs(mx) - 12) < 3 && fabs(fabs(mz) - 12) < 3 && r.exact && !cncStopRequest && !isOn, what);
    }
  }

  // 10. the same on the H4's own default machine: X 1920 steps/mm 3.75 mm/s 12.5 mm/s2, Z 400 steps/mm 12 mm/s
  //     60 mm/s2, backlash X 0.15 mm Z 0.65 mm, jog at 120 mm/min, and the longest wait of the step interrupt. (The
  //     stall fixed in firmware 1.46, a segment without a whole step waiting up to 200 s, needs the timing of the
  //     whole firmware to appear: it is checked in h4sim/scenario_test.py.)
  {
    cnc_settings_t d = settings;
    d.stepsPerUnit[CNC_X] = 1920; d.stepsPerUnit[CNC_Z] = 400;
    d.maxRate[CNC_X] = 3.75f * 60; d.maxRate[CNC_Z] = 12 * 60;
    d.acceleration[CNC_X] = 12.5f * 3600; d.acceleration[CNC_Z] = 60 * 3600;
    cnc_core_settings(&d);
    cnc_settings_t keep = settings;
    settings = d;
    for (int i = 0; i < CNC_N_AXIS; i++) { vmax[i] = settings.maxRate[i] / 60; amax[i] = settings.acceleration[i] / 3600; }
    cnc_machine_t m;
    memset(&m, 0, sizeof(m));
    m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
    for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
    m.rotary[CNC_C] = true;
    m.defaultFeed = 120;
    m.backlashSteps[CNC_X] = 288;
    m.backlashSteps[CNC_Z] = 260;
    backlash[CNC_X] = 288; backlash[CNC_Z] = 260;
    cnc_gc_init(&m);
    syncFromAxes();
    float seq[][2] = {{-120, 0}, {120, 120}, {-120, 0}, {120, -120}, {0, 120}, {-120, -120}};
    for (auto& v : seq) {
      std::vector<Req> q;
      addReqs(q, 0, 1200, 100, 0, v[0], v[1], 0);
      q.push_back({1300, {0, 0, 0}});
      int32_t p0[CNC_N_AXIS];
      memcpy(p0, tool, sizeof(p0));
      Result r = runJog(q, 3000);
      double mx = (tool[CNC_X] - p0[CNC_X]) / settings.stepsPerUnit[CNC_X], mz = (tool[CNC_Z] - p0[CNC_Z]) / settings.stepsPerUnit[CNC_Z];
      bool okMove = (v[0] == 0 || fabs(mx) > 1.8) && (v[1] == 0 || fabs(mz) > 1.8);
      snprintf(what, sizeof(what), "default machine, jog X %+.0f Z %+.0f mm/min: moved X %.3f Z %.3f, longest step wait %.3f s, stopped, exact",
               v[0], v[1], mx, mz, r.longestTickS);
      check(okMove && r.longestTickS < 0.05 && r.exact && !cncStopRequest && !isOn, what);
    }
    settings = keep;
    cnc_core_settings(&settings);
    for (int i = 0; i < CNC_N_AXIS; i++) { vmax[i] = settings.maxRate[i] / 60; amax[i] = settings.acceleration[i] / 3600; }
    memset(&m, 0, sizeof(m));
    m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
    for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
    m.rotary[CNC_C] = true;
    m.defaultFeed = 120;
    cnc_gc_init(&m);
    memset(backlash, 0, sizeof(backlash));
    memcpy(motor, tool, sizeof(motor));
    syncFromAxes();
  }

  // ---- the arrows of the app: /key hold=1 every 150 ms
  {
    auto press = [](const char* code, const char* hold) { server.code = code; server.hold = hold; handleKey(); };
    auto webLoop = [](double ms) {  // the web task: the fail-safe every 2 ms
      uint64_t end = now + (uint64_t)(ms * TIMER_HZ / 1000);
      while (now < end) { heldKeyFailSafe(); now += TIMER_HZ / 500; }
    };
    keyEvents.clear();
    unsigned long last = 0;
    for (int i = 0; i < 20; i++) { press("26", "1"); last = millis(); webLoop(150); }   // 3 s held
    check(keyEvents.size() == 1 && keyEvents[0] == 128 + 26, "arrow held 3 s with repeats: pressed once, not released");
    while (keyEvents.size() == 1 && millis() - last < 2000) webLoop(2);
    snprintf(what, sizeof(what), "the app gone while an arrow is held: released %lu ms after the last repeat (fail-safe 500)", millis() - last);
    check(keyEvents.size() == 2 && keyEvents[1] == 26 && millis() - last >= 500 && millis() - last <= 504, what);
    // released by the app (hold=0): at once
    keyEvents.clear();
    press("27", "1"); webLoop(150); press("27", "0");
    check(keyEvents.size() == 2 && keyEvents[1] == 27 && heldKey == 0, "arrow let go on the screen: released at once");
    // the app gone and the keypad task busy for a while: the release still arrives
    keyEvents.clear();
    press("28", "1");
    keypadBusyTries = 6;
    last = millis();
    while (keyEvents.size() == 1 && millis() - last < 2000) webLoop(2);
    check(keyEvents.size() == 2 && keyEvents[1] == 28, "app gone, keypad task busy 6 tries: the release still arrives");
    // another arrow pressed while one is held: the first is released first
    keyEvents.clear();
    press("26", "1"); press("29", "1");
    check(keyEvents.size() == 3 && keyEvents[1] == 26 && keyEvents[2] == 128 + 29, "another arrow: the first one released first");
    webLoop(600);
    check(keyEvents.size() == 4 && keyEvents[3] == 29 && heldKey == 0, "and that one released by the fail-safe too");
  }

  printf("jog: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
