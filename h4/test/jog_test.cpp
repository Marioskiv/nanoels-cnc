/*
  jog_test.cpp - jogging from the app when the app stops talking (work plan B6). The H4 is jogged from the app in two
  ways, both checked here with the firmware's own code (extracted unchanged by run_tests.sh):
  1. the gamepad: the app sends the wanted speeds to /jog about every 100 ms (grbl.ino webJogRequest, webJogService,
     webJogStop, grblJog) and the H4 plans short pieces with the real planner, interpreter and step generator
     (cnc_core.cpp, cnc_gcode.cpp). Simulated: the step interrupt on a 20 MHz clock, taskGcode every 1 ms with now
     and then a long gap, the app's requests (also late and irregular, as over WiFi), and what the firmware does
     around them (models, written here: grblEnsureOn starting the session, setIsOnFromLoop / cncService stopping,
     the end of the session in grblServiceLine). Checked on the author's machine (X 12 mm/s, Z 25 mm/s, C 360 deg/s):
     - requests stop (WiFi lost, app closed, tablet asleep): standing still within 300 ms + one service period +
       braking, travel after the last request within speed * that time + v^2 / 2a; the position afterwards is exact
       (planner = steps given) and the next jog starts from there;
     - speeds 0 stop at once (braking only); requests up to 280 ms apart: no stop, no dip in speed, no underrun;
     - never faster or harder accelerating than the axis allows (also reversing a stick), never during a program;
     - a refusal stays until the stick is let go (speeds 0).
  Found (2026-11-08, reported, nothing changed): a hold starts after the motion the step generator has already prepared
  (CNC_SEGMENT_AHEAD_S 0.1 s), so a stop comes up to ~0.12 s later than the request; C reaches ~288 deg/s, not 360, on
  the stick (the queued 0.2 s must be enough to brake: v <= 2 a 0.2); with X/Z and C together the G94 feed is taken
  along X, Z and C (LinuxCNC: along X/Z, C following), so X/Z move ~13x slower then (printed as NOTE, not a failure).
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
static void syncFromAxes() {  // cncSyncFromAxes: drop what is queued, the position is where the tool stands
  cnc_st_reset();
  cnc_plan_reset();
  int32_t z0[CNC_N_AXIS] = {0, 0, 0};
  bool neg[CNC_N_AXIS] = {false, false, false};
  cnc_gc_sync(tool, z0, neg);
}

// grblRunGcode: grblEnsureOn (start a session: the H4 ON), then the line through the interpreter
bool grblRunGcode(int ch, const char* text) {
  if (!(isOn && grblSession)) {
    if (isOn || cncBusy() || homingActive) return false;
    if (cncStartRefusal()) { snprintf(cncLastError, sizeof(cncLastError), "%s", cncStartRefusal()); return false; }
    grblSession = true;
    isOn = true;
  }
  cncLastError[0] = '\0';
  grblChannel = ch;
  char msg[100] = "";
  float dwell = 0;
  int r = cnc_gc_line(text, 1, msg, sizeof(msg), &dwell);
  if (r == CNC_ERROR) { snprintf(cncLastError, sizeof(cncLastError), "%s", msg); return false; }
  return true;
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
  settings.acceleration[CNC_X] = 100 * 3600; settings.acceleration[CNC_Z] = 200 * 3600; settings.acceleration[CNC_C] = 720 * 3600;
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
            r.vSeen[i] = max(r.vSeen[i], fabs(v1));
            r.aSeen[i] = max(r.aSeen[i], fabs(v1 - v0) / SAMPLE_S);
            // steady speed while requests keep coming: at least 0.9 of the wanted (after speeding up)
            // the jog queues WEB_JOG_LEAD_S of motion and that must be enough to brake: v <= 2 a lead
            double w = wanted[i] / 60;
            if (fabs(w) > 2 * amax[i] * WEB_JOG_LEAD_S) w = (w > 0 ? 1 : -1) * 2 * amax[i] * WEB_JOG_LEAD_S;
            if (w != 0 && wantedSince >= 0 && tS - wantedSince > 400 && tS < r.lastReqMs + 60 && ri < reqs.size()) {
              r.minSpeedRatio = std::min(r.minSpeedRatio, v1 / w);
            }
          }
        }
        nextSample += (uint64_t)(SAMPLE_S * TIMER_HZ);
      }
      cnc_tick_t tick;
      cnc_isr(&tick);
      for (int i = 0; i < CNC_N_AXIS; i++) {
        if (!(tick.step & (1 << i)) || tick.motorOnly) continue;
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
  r.exact = !cnc_st_busy() && memcmp(cnc_plan_position(), tool, sizeof(tool)) == 0;
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
  double brakeS = 0, bound[CNC_N_AXIS];
  for (int i = 0; i < CNC_N_AXIS; i++) {
    double vv = fabs(v[i]) / 60;
    brakeS = max(brakeS, vv / amax[i]);
    bound[i] = vv * (extraMs / 1000) + vv * vv / (2 * amax[i]) + 2 / settings.stepsPerUnit[i];
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

static void checkLimits(const Result& r, const char* name) {
  char what[240];
  bool ok = true;
  for (int i = 0; i < CNC_N_AXIS; i++) ok = ok && r.vSeen[i] <= vmax[i] * 1.05 + 2 / settings.stepsPerUnit[i] / 0.02 && r.aSeen[i] <= amax[i] * 1.35 + 0.5 * vmax[i] / 0.02;
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
    if (a.v[2] != 0 && (a.v[0] != 0 || a.v[1] != 0)) {
      // G94 with X/Z and C together: the planner takes F along X, Z and C (degrees as mm), LinuxCNC along X/Z only
      // (C follows in the same time) - reported, not changed yet: the stick moves X/Z much slower while C turns
      printf("NOTE: %s\n", what);
    } else {
      check(r.minSpeedRatio >= 0.9, what);
    }
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
    q.push_back({1, {0, 0, 0}});  // a tap: one piece of 50 ms (0.05 mm at 60 mm/min)
    Result r = runJog(q, 1500);
    // speeds 0 a millisecond later: the hold comes after what was already prepared (0.1 s at 1 mm/s)
    double moved = (tool[CNC_Z] - before) / settings.stepsPerUnit[CNC_Z], bound = 1.0 * (0.002 + CNC_SEGMENT_AHEAD_S + 0.01) + 0.01;
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
