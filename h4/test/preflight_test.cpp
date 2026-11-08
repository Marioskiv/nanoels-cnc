/*
  preflight_test.cpp - the check before a program starts (firmware 1.22, h4.ino cncRunPreflight): the whole program
  goes through the interpreter and the planner (cnc_bench_run), nothing moves, and a line that cannot run refuses the
  start with its line number. Here on a PC with the author's machine (X/Z/C, soft limits of the LinuxCNC .ini):
  an error far into a program, a move past a soft limit, an arc past a limit, a Y move from a 3-axis CAM program,
  cutter compensation, a bad arc; notes only (M3, G43 H, M6) do not refuse, and are said before the start (where
  it will wait, the first note); threading ends the check (said so);
  a real program passes.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_bench.h"
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>

static uint64_t fakeUs = 0;
static bool pauseOnM3 = false;  // the H4's setting: M3 / M4 that start the spindle wait for the operator
static uint64_t nowUs() { return fakeUs += 3; }
static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) { failures++; printf("FAIL: %s\n", what); }
}

static void machine() {
  cnc_settings_t s;
  s.stepsPerUnit[CNC_X] = 1200; s.stepsPerUnit[CNC_Z] = 400; s.stepsPerUnit[CNC_C] = 91.733333f;
  s.maxRate[CNC_X] = 12 * 60; s.maxRate[CNC_Z] = 25 * 60; s.maxRate[CNC_C] = 360 * 60;
  s.acceleration[CNC_X] = 100 * 3600; s.acceleration[CNC_Z] = 100 * 3600; s.acceleration[CNC_C] = 720 * 3600;
  s.junctionDeviation = 0.01f;
  s.timerHz = 20000000;
  cnc_core_settings(&s);
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
  for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
  m.rotary[CNC_C] = true;
  m.arcTolerance = 0.002f;
  // soft limits of the author's machine, machine coordinates = program coordinates here (homed, G54 at 0)
  m.pauseOnSpindleStart = pauseOnM3;
  m.checkLimits = true;
  m.minSteps[CNC_X] = (int32_t)lround(-22.039 * 1200); m.maxSteps[CNC_X] = (int32_t)lround(108.961 * 1200);
  m.minSteps[CNC_Z] = -590 * 400; m.maxSteps[CNC_Z] = 7 * 400;
  m.minSteps[CNC_C] = INT32_MIN; m.maxSteps[CNC_C] = INT32_MAX;
  cnc_gc_init(&m);
  cnc_plan_reset();
  int32_t zero[CNC_N_AXIS] = {0, 0, 0};
  bool neg[CNC_N_AXIS] = {false, false, false};
  cnc_gc_sync(zero, zero, neg);
}

static cnc_bench_t run(const std::string& text, bool limits = true) {
  machine();
  if (!limits) {
    cnc_machine_t m;
    memset(&m, 0, sizeof(m));
    m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
    for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
    m.rotary[CNC_C] = true;
    m.arcTolerance = 0.002f;
    cnc_gc_init(&m);
  }
  cnc_bench_t r;
  memset(&r, 0, sizeof(r));
  cnc_bench_run(text.data(), text.size(), &r, nowUs, nullptr);
  return r;
}

static bool refused(const cnc_bench_t& r) { return r.stopLine && strncmp(r.stopReason, "threading", 9) != 0; }

// a long program of good moves (polar style, G93), then 'bad' as line n+1 (the header is lines 1..3)
static std::string longProgram(int n, const char* bad) {
  std::string t = "G21 G90 G18\nG0 X20 Z1 C0\nG93\n";
  for (int i = 4; i <= n; i++) {
    char l[96];
    snprintf(l, sizeof(l), "G1 X%.3f C%.2f F600\n", 20 + 0.001 * (i % 500), (i % 3600) * 0.1);
    t += l;
  }
  t += bad;
  t += "\nG94\nM2\n";
  return t;
}

int main() {
  char what[400];
  // 1. an error far into a long program: refused at that line, before anything moves
  {
    cnc_bench_t r = run(longProgram(5000, "G41 X20 Z0"));
    snprintf(what, sizeof(what), "cutter compensation at line 5001 of 5003: refused at 5001 (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(refused(r) && r.stopLine == 5001 && strstr(r.stopReason, "cutter compensation"), what);
  }
  // 2. a move past the X soft limit (108.961) half way
  {
    cnc_bench_t r = run(longProgram(300, "G0 X150"));
    snprintf(what, sizeof(what), "X150 past the soft limit at line 301: refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(refused(r) && r.stopLine == 301 && strstr(r.stopReason, "outside the limits"), what);
  }
  // 3. Z past the Z+ soft limit (7)
  {
    cnc_bench_t r = run("G21 G90 G18\nG0 X10 Z1\nG1 Z5 F100\nG0 Z12\nM2\n");
    snprintf(what, sizeof(what), "Z12 past Z+ 7 at line 4: refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(refused(r) && r.stopLine == 4, what);
  }
  // 4. an arc whose ends are inside but which bulges past the X limit
  {
    cnc_bench_t r = run("G21 G90 G18 G8\nG0 X100 Z-10\nG3 X100 Z-30 I10 K-10 F200\nM2\n");
    snprintf(what, sizeof(what), "arc bulging past X 108.961 at line 3: refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(refused(r) && r.stopLine == 3, what);
  }
  // 5. a Y move: a 3-axis CAM program sent straight to the H4 (not through the polar converter)
  {
    cnc_bench_t r = run("G21 G90\nG0 X10 Z5\nG1 Z-1 F100\nG1 X20 Y5 F300\nM2\n");
    snprintf(what, sizeof(what), "Y word at line 4: refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(refused(r) && r.stopLine == 4, what);
  }
  // 6. an arc whose radius does not fit its end points
  {
    cnc_bench_t r = run("G21 G90 G18\nG0 X10 Z0\nG2 X20 Z-10 I1 K0 F200\nM2\n");
    snprintf(what, sizeof(what), "bad arc at line 3: refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(refused(r) && r.stopLine == 3, what);
  }
  // 7. notes only (spindle, tool length, tool change): the check does not refuse them (the program says them when it runs)
  {
    cnc_bench_t r = run("G21 G90 G18\nS18000 M3\nT1 M6\nG43 H1\nG0 X10 Z1\nG1 Z-1 F100\nM5\nM2\n");
    snprintf(what, sizeof(what), "M3, M6, G43 H: not refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(!refused(r) && r.lines >= 8, what);
    // ... and says them before the start (1.40): where it will wait (the tool change), the first note, a G43 H
    // before the M3 note that came earlier (the H4 stores no tool lengths)
    snprintf(what, sizeof(what), "said before the start: waits %u at line %u (%s), notes %u, line %u (%s)", r.waits, r.waitLine,
             r.waitReason, r.notes, r.noteLine, r.note);
    check(r.waits == 1 && r.waitLine == 3 && strstr(r.waitReason, "put in tool T1") && r.notes == 2 && r.noteLine == 4 &&
          strstr(r.note, "G43 H"), what);
  }
  // 7b. with "M3 waits for the router" (the author's setting) and an M0: two waits, the first is the M3
  {
    pauseOnM3 = true;
    cnc_bench_t r = run("G21 G90 G18\nS18000 M3\nG0 X10 Z1\nM0\nG1 Z-1 F100\nM5\nM2\n");
    pauseOnM3 = false;
    snprintf(what, sizeof(what), "M3 waits, M0: waits %u, first at line %u (%s), notes %u", r.waits, r.waitLine, r.waitReason, r.notes);
    check(!refused(r) && r.waits == 2 && r.waitLine == 2 && strstr(r.waitReason, "router on") && r.notes == 0, what);
  }
  // 7c. nothing to say: no wait, no note
  {
    cnc_bench_t r = run("G21 G90 G18\nG0 X10 Z1\nG1 Z-1 F100\nG0 Z1\nM2\n");
    snprintf(what, sizeof(what), "a plain program: waits %u, notes %u", r.waits, r.notes);
    check(!refused(r) && r.waits == 0 && r.notes == 0, what);
  }
  // 8. threading: the check ends there and says so (not a refusal); what comes after it is not checked
  {
    cnc_bench_t r = run("G21 G90 G18\nG0 X10 Z2\nG33 Z-10 K1.5\nG0 X12\nG41\nM2\n");
    snprintf(what, sizeof(what), "G33 at line 3: the check ends there, not refused (stopped at %u: %s)", r.stopLine, r.stopReason);
    check(!refused(r) && r.stopLine == 3 && !strncmp(r.stopReason, "threading", 9), what);
  }
  // 9. the polar sample program (converter output) passes, all lines read (its own coordinates: no soft limits here;
  //    with the limits above and no work offset its Z20 is refused, as it should be)
  {
    FILE* fp = fopen("polar_sample.ngc", "rb");
    std::string text;
    if (fp) {
      char buf[65536];
      size_t n;
      while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) text.append(buf, n);
      fclose(fp);
    }
    cnc_bench_t r = run(text, false);
    snprintf(what, sizeof(what), "polar_sample.ngc passes the check (%u lines, stopped at %u: %s)", r.lines, r.stopLine, r.stopReason);
    check(!text.empty() && !r.stopLine && r.lines > 200, what);
  }
  printf("preflight: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
