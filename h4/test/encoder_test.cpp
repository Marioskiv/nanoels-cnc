/*
  encoder_test.cpp - a simulated chuck encoder against the C axis (work plan B7, for C8 / C9): cnc_encwatch.cpp is fed
  as the firmware would feed it, every millisecond, from a model of the author's machine.
  - C moves as the planner moves it (trapezoid, 360 deg/s, 720 deg/s2, 91.733 steps/deg); the chuck follows through
    belts with backlash (0.2 deg) and stretch (0.15 deg at full acceleration); the step count is read up to 0.1 ms
    after the encoder.
  - The encoder (600 PPR, the E38S6G5) gives quadrature A / B; added: short glitches on both lines (noise of the router
    and the drivers), contact bounce at the edges.
  - The ESP32 pulse counter as the firmware sets it up (A counted on both edges, B gives the direction, a glitch
    filter of N APB clocks of 12.5 ns).
  Checked: with a filter of 5 us no count is lost under heavy noise and nothing is reported while the chuck follows;
  a skipped belt tooth (4.19 deg), a stall and a slowly slipping belt are each reported once and quickly. Shown: how
  many counts the firmware's present filter (25 ns) loses to the same noise.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_encwatch.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) failures++;
}

static const double STEPS_PER_DEG = 91.733333, STATES_PER_DEG = 2400.0 / 360.0;  // 600 PPR, 4 states per cycle
static uint32_t rnd = 1;
static double urand() { rnd = rnd * 1103515245u + 12345u; return ((rnd >> 8) & 0xFFFFFF) / 16777216.0; }

struct Move { double to; double dwellS; };
struct Slip { double atS, deg, overS; };  // a slip of deg degrees of the chuck, spread over overS seconds
struct Scenario {
  const char* name;
  std::vector<Move> moves;
  double backlash = 0.2, stretchAtMaxAcc = 0.15;
  double glitchPerS = 50, glitchMaxUs = 3, bounceP = 0.3;
  double filterUs = 5;
  std::vector<Slip> slips;
  double stallAt = -1, stallS = 0;  // the chuck held still meanwhile (the motor turns on)
};

struct Result { uint32_t events; double firstEventS; double worstDeg, worstRatio; long lostCounts; double endS; };

// motor angle along trapezoid moves (as the planner moves C)
struct Profile {
  std::vector<Move> moves; double v = 360, a = 720;
  size_t i = 0; double from = 0, t0 = 0;  // current move
  double total = 0;
  void segment(size_t k, double& T, double& ta, double& vp) const {
    double d = fabs(moves[k].to - (k ? moves[k - 1].to : 0));
    vp = v; ta = v / a;
    if (a * ta * ta > d) { ta = sqrt(d / a); vp = a * ta; }
    T = 2 * ta + (d - a * ta * ta) / vp;
  }
  // angle and acceleration at time t
  void at(double t, double& ang, double& acc) {
    double start = 0, prev = 0;
    for (size_t k = 0; k < moves.size(); k++) {
      double T, ta, vp; segment(k, T, ta, vp);
      double d = moves[k].to - prev, s = d >= 0 ? 1 : -1;
      if (t < start + T) {
        double u = t - start, x;
        if (u < ta) { x = 0.5 * a * u * u; acc = s * a; }
        else if (u < T - ta) { x = 0.5 * a * ta * ta + vp * (u - ta); acc = 0; }
        else { double r = T - u; x = fabs(d) - 0.5 * a * r * r; acc = -s * a; }
        ang = prev + s * x; return;
      }
      start += T;
      if (t < start + moves[k].dwellS) { ang = moves[k].to; acc = 0; return; }
      start += moves[k].dwellS;
      prev = moves[k].to;
    }
    ang = prev; acc = 0;
  }
  double duration() {
    double s = 0, prev = 0;
    for (size_t k = 0; k < moves.size(); k++) { double T, ta, vp; segment(k, T, ta, vp); s += T + moves[k].dwellS; prev = moves[k].to; }
    (void)prev; return s;
  }
};

struct Edge { double t; int line; int level; };

static Result run(const Scenario& sc, bool verbose = false) {
  Profile p; p.moves = sc.moves;
  double dur = p.duration() + 0.2;
  // 1. the chuck's angle every microsecond -> the encoder's true states -> the raw line changes
  std::vector<Edge> raw;
  double chuck = 0, slip = 0;
  long trueIdx = 0, lastIdx = 0;
  int lineA = 0, lineB = 0;
  const int gA[4] = {0, 0, 1, 1}, gB[4] = {0, 1, 1, 0};  // forward: 00 01 11 10
  double lastMotor = 0;
  for (double t = 0; t < dur; t += 1e-6) {
    double m, acc; p.at(t, m, acc);
    // backlash: the chuck follows the motor only once the play is taken up; stretch: lags with the acceleration
    // (a real chuck cannot jump: it moves at most twice C's top speed, so the stretch that follows a step in the
    // acceleration is taken up over a few ms)
    double target = m - acc / 720.0 * sc.stretchAtMaxAcc, want = chuck;
    if (target > chuck + sc.backlash / 2) want = target - sc.backlash / 2;
    if (target < chuck - sc.backlash / 2) want = target + sc.backlash / 2;
    const double maxStep = 2 * 360 * 1e-6;
    chuck += std::max(-maxStep, std::min(maxStep, want - chuck));
    for (const Slip& s : sc.slips) {
      if (t >= s.atS && t < s.atS + s.overS) slip -= s.deg * 1e-6 / s.overS;
    }
    if (sc.stallAt >= 0 && t >= sc.stallAt && t < sc.stallAt + sc.stallS) slip -= (m - lastMotor);
    lastMotor = m;
    long idx = (long)floor((chuck + slip) * STATES_PER_DEG);
    while (lastIdx != idx) {
      lastIdx += idx > lastIdx ? 1 : -1;
      int st = (int)(((lastIdx % 4) + 4) % 4);
      if (gA[st] != lineA) { lineA = gA[st]; raw.push_back({t, 0, lineA}); }
      if (gB[st] != lineB) { lineB = gB[st]; raw.push_back({t, 1, lineB}); }
    }
  }
  trueIdx = lastIdx;
  // 2. bounce after real edges, glitches anywhere
  std::vector<Edge> ev = raw;
  for (const Edge& e : raw) {
    if (urand() < sc.bounceP) {
      double t1 = e.t + 0.2e-6 + urand() * 1e-6, t2 = t1 + 0.1e-6 + urand() * 0.8e-6;
      ev.push_back({t1, e.line, 1 - e.level}); ev.push_back({t2, e.line, e.level});
    }
  }
  for (int line = 0; line < 2; line++) {
    for (double t = 0; t < dur;) {
      t += -log(1 - urand() + 1e-12) / sc.glitchPerS;
      double w = (0.03 + urand() * (sc.glitchMaxUs - 0.03)) * 1e-6;
      ev.push_back({t, line, -1}); ev.push_back({t + w, line, -2});  // -1: invert from here, -2: back
    }
  }
  std::stable_sort(ev.begin(), ev.end(), [](const Edge& a, const Edge& b) { return a.t < b.t; });
  // 3. the raw level of each line over time (a glitch inverts whatever the real level is meanwhile)
  std::vector<Edge> lev;
  int real[2] = {0, 0}, inv[2] = {0, 0}, shown[2] = {0, 0};
  for (const Edge& e : ev) {
    if (e.level == -1) inv[e.line]++;
    else if (e.level == -2) inv[e.line]--;
    else real[e.line] = e.level;
    int v = real[e.line] ^ (inv[e.line] > 0 ? 1 : 0);
    if (v != shown[e.line]) { shown[e.line] = v; lev.push_back({e.t, e.line, v}); }
  }
  // 4. the pulse counter's filter: a new level counts once it has stayed for filterUs
  double F = sc.filterUs * 1e-6;
  std::vector<Edge> filt;
  for (int line = 0; line < 2; line++) {
    std::vector<Edge> l;
    for (const Edge& e : lev) if (e.line == line) l.push_back(e);
    int f = 0;
    for (size_t k = 0; k < l.size(); k++) {
      double end = k + 1 < l.size() ? l[k + 1].t : 1e9;
      if (end - l[k].t >= F && l[k].level != f) { f = l[k].level; filt.push_back({l[k].t + F, line, f}); }
    }
  }
  std::stable_sort(filt.begin(), filt.end(), [](const Edge& a, const Edge& b) { return a.t < b.t; });
  // 5. decode as the firmware sets the counter up (A both edges, B low reverses) and watch every ms
  cnc_encwatch_cfg_t cfg;
  cfg.countsPerDeg = 1200.0f / 360.0f; cfg.stepsPerDeg = (float)STEPS_PER_DEG;
  cfg.tolDeg = 0.8f; cfg.lagS = 0.002f; cfg.holdMs = 30;
  cnc_encwatch_t w; memset(&w, 0, sizeof(w));
  cnc_encwatch_reset(&w, &cfg, 0, 0, 0);
  long count = 0; int a = 0, b = 0;
  size_t k = 0;
  Result r = {0, -1, 0, 0, 0, dur};
  for (uint32_t ms = 1; ms < dur * 1000; ms++) {
    double t = ms / 1000.0;
    while (k < filt.size() && filt[k].t <= t) {
      const Edge& e = filt[k++];
      if (e.line == 1) { b = e.level; continue; }
      int d = e.level > a ? 1 : -1;  // rising +1, falling -1 ...
      if (!b) d = -d;                // ... reversed while B is low
      a = e.level;
      count += d;
    }
    double m, acc; p.at(t + urand() * 1e-4, m, acc);  // the step count read a little later
    int64_t steps = llround(m * STEPS_PER_DEG);
    if (cnc_encwatch_sample(&w, count, steps, ms)) {
      if (r.firstEventS < 0) r.firstEventS = t;
      if (verbose) printf("     event at %.3f s: %.2f deg\n", t, w.lastEventDeg);
    }
  }
  r.events = w.events; r.worstDeg = w.worstDeg; r.worstRatio = w.worstRatio;
  // counts lost: the counter (2 per 4 states) against the true states
  r.lostCounts = (long)llround(trueIdx / 2.0) - count;
  if (labs(r.lostCounts) <= 1) r.lostCounts = 0;
  return r;
}

int main() {
  char what[240];
  // a program's C: 2-turn rapids back and forth (G0 C720 / C0), short moves, and pauses
  std::vector<Move> prog;
  for (int i = 0; i < 4; i++) { prog.push_back({720, 0.3}); prog.push_back({0, 0.3}); }
  for (int i = 0; i < 20; i++) prog.push_back({(i % 2 ? 10.0 : 25.0) + i, 0.05});
  prog.push_back({0, 0.5});

  Scenario base; base.name = "follows"; base.moves = prog;
  // 1. the chuck follows (backlash, stretch, noise 50 glitches/s per line up to 3 us, bounce): nothing reported
  rnd = 1;
  Result r = run(base);
  snprintf(what, sizeof(what), "chuck follows, noise + bounce, filter 5 us: 0 events (%u), 0 counts lost (%ld), worst %.2f deg = %.0f %% of allowed",
           r.events, r.lostCounts, r.worstDeg, r.worstRatio * 100);
  check(r.events == 0 && r.lostCounts == 0 && r.worstRatio < 0.9, what);
  // 2. ten times the noise: still nothing
  Scenario noisy = base; noisy.glitchPerS = 500; rnd = 2;
  r = run(noisy);
  snprintf(what, sizeof(what), "500 glitches/s per line, filter 5 us: 0 events (%u), 0 counts lost (%ld)", r.events, r.lostCounts);
  check(r.events == 0 && r.lostCounts == 0, what);
  // 3. the firmware's filter today (2 APB clocks = 25 ns): glitches get through; shown, and checked that they cost counts
  Scenario today = noisy; today.filterUs = 0.025; rnd = 2;
  r = run(today);
  snprintf(what, sizeof(what), "the same with today's 25 ns filter: %ld counts lost (%.2f deg), %u false events: the filter must be raised for C8",
           r.lostCounts, r.lostCounts * 0.3, r.events);
  check(r.lostCounts != 0, what);
  // 4. one belt tooth skipped on the 86 T chuck pulley (4.19 deg) during a rapid: reported once, within 50 ms
  Scenario skip = base; skip.slips.push_back({1.0, 360.0 / 86, 0.002}); rnd = 3;
  r = run(skip, true);
  snprintf(what, sizeof(what), "a skipped tooth (4.19 deg) at 1.0 s: 1 event (%u) at %.3f s", r.events, r.firstEventS);
  check(r.events == 1 && r.firstEventS >= 1.0 && r.firstEventS < 1.05, what);
  // 5. the chuck stalls for 0.1 s while C turns (fast: 36 deg missed): reported once
  Scenario stall = base; stall.stallAt = 0.8; stall.stallS = 0.1; rnd = 4;
  r = run(stall, true);
  snprintf(what, sizeof(what), "a 0.1 s stall in a rapid: reported (%u events, first at %.3f s)", r.events, r.firstEventS);
  check(r.events >= 1 && r.firstEventS >= 0.8 && r.firstEventS < 0.9, what);
  // 6. a belt slipping slowly (0.3 deg/s): reported once the difference passes the allowed one
  Scenario creep = base; creep.slips.push_back({0.5, 3.0, 10}); rnd = 5;
  r = run(creep, true);
  snprintf(what, sizeof(what), "a slowly slipping belt (0.3 deg/s from 0.5 s): reported (%u events, first at %.2f s)", r.events, r.firstEventS);
  check(r.events >= 1 && r.firstEventS > 0.5 && r.firstEventS < 6, what);
  // 7. standing still with very heavy noise: nothing
  Scenario rest; rest.name = "rest"; rest.moves = {{0, 5}}; rest.glitchPerS = 2000; rnd = 6;
  r = run(rest);
  snprintf(what, sizeof(what), "at rest, 2000 glitches/s per line: 0 events (%u), 0 counts lost (%ld)", r.events, r.lostCounts);
  check(r.events == 0 && r.lostCounts == 0, what);
  printf("encoder: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
