/*
  runfrom_test.cpp - start a stored program from line N (firmware 1.31, cnc_runfrom.cpp): for every line N of the
  sample programs, the moves planned from line N on are the same as in a full run, after the machine was elsewhere
  and went to the start of line N with the approach moves (G53, above the work first) and the program's modes were
  put back. Also: refusals (past the end, after M2, threading before N, an error before N), the highest Z, G91 / G20 /
  G93 / G92 / G10 / G55 programs.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_bench.h"
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include "../cnc_runfrom.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) { failures++; printf("FAIL: %s\n", what); }
}

struct Planned { uint32_t line; int32_t t[CNC_N_AXIS]; };
static std::vector<Planned> trace;
static void onPlan(const int32_t target[CNC_N_AXIS], uint32_t line) {
  Planned p; p.line = line; memcpy(p.t, target, sizeof(p.t)); trace.push_back(p);
}

static const int32_t MOFF[CNC_N_AXIS] = {-2000, -4000, 0};  // machine = pos + offset (steps)
static void machine(const int32_t pos[CNC_N_AXIS]) {
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
  cnc_gc_init(&m);
  cnc_plan_reset();
  bool neg[CNC_N_AXIS] = {false, false, false};
  cnc_gc_sync(pos, MOFF, neg);
}

static void take() { float m[CNC_BLOCK_BUFFER_SIZE]; cnc_plan_take_all(m, CNC_BLOCK_BUFFER_SIZE); }

// runs lines from pos (numbered from firstLine) to the end, nothing moves; false on an error
static bool runRest(const std::string& t, size_t pos, uint32_t firstLine) {
  char line[256], msg[96];
  uint32_t no = firstLine - 1;
  while (pos < t.size()) {
    size_t len;
    pos = cnc_bench_line(t.data(), t.size(), pos, line, sizeof(line), &len);
    no++;
    if (len == 0) continue;
    while (true) {
      float dwell;
      int r = cnc_gc_line(line, no, msg, sizeof(msg), &dwell);
      take();
      if (r == CNC_WAIT || r == CNC_DWELL_MORE) continue;
      if (r == CNC_ERROR) { printf("error line %u: %s\n", no, msg); return false; }
      if (r == CNC_END) return true;
      break;
    }
  }
  return true;
}

static std::vector<Planned> from(const std::vector<Planned>& all, uint32_t n) {
  std::vector<Planned> o;
  for (auto& p : all) if (p.line >= n) o.push_back(p);
  return o;
}

// full run vs run from line n (the machine elsewhere before): same moves from line n on
static bool sameFrom(const std::string& t, uint32_t n, const char* name, bool quiet = false) {
  int32_t start[CNC_N_AXIS] = {0, 0, 0};
  machine(start);
  trace.clear();
  runRest(t, 0, 1);
  std::vector<Planned> full = from(trace, n);
  machine(start);
  cnc_runfrom_t r;
  if (!cnc_runfrom_prepare(t.data(), t.size(), n, (start[CNC_Z] + MOFF[CNC_Z]) / 400.0, &r, nullptr)) {
    if (!quiet) printf("%s line %u: refused: %s\n", name, n, r.reason);
    return false;
  }
  cnc_modes_t saved;
  cnc_gc_get_modes(&saved);
  // the machine is elsewhere (e.g. stopped, jogged away): the interpreter and the planner are put there
  int32_t elsewhere[CNC_N_AXIS] = {37 * 1200, 15 * 400, 4000};
  bool neg[CNC_N_AXIS] = {false, false, false};
  cnc_gc_sync(elsewhere, MOFF, neg);
  char lines[3][96], msg[96];
  const char names[CNC_N_AXIS] = {'X', 'Z', 'C'};
  cnc_runfrom_approach(&r, names, true, 100, lines);
  trace.clear();
  for (int i = 0; i < 3; i++) {
    float dwell;
    int res;
    do { res = cnc_gc_line(lines[i], 0, msg, sizeof(msg), &dwell); take(); } while (res == CNC_WAIT);
    if (res != CNC_OK) { printf("%s line %u: approach '%s' refused: %s\n", name, n, lines[i], msg); return false; }
  }
  // the approach ends exactly at the start of line n
  bool at = true;
  for (int i = 0; i < CNC_N_AXIS; i++) at = at && fabs(cnc_gc_machine_position(i) - r.machine[i]) < 1e-6;
  if (!at && !quiet) for (int i = 0; i < CNC_N_AXIS; i++) printf("  axis %d: at %.6f, wanted %.6f (%s)\n", i, cnc_gc_machine_position(i), r.machine[i], lines[i]);
  cnc_gc_set_modes(&saved);
  trace.clear();
  runRest(t, r.startPos, n);
  std::vector<Planned> rest = from(trace, n);
  bool same = at && rest.size() == full.size();
  for (size_t i = 0; same && i < rest.size(); i++)
    for (int k = 0; k < CNC_N_AXIS; k++) same = same && abs(rest[i].t[k] - full[i].t[k]) <= 1;
  if (!same && !quiet) printf("%s line %u: at %d, %zu moves vs %zu in the full run\n", name, n, at, rest.size(), full.size());
  return same;
}

static std::string readFile(const char* f) {
  FILE* fp = fopen(f, "rb");
  std::string s;
  if (!fp) return s;
  char b[65536]; size_t k;
  while ((k = fread(b, 1, sizeof(b), fp)) > 0) s.append(b, k);
  fclose(fp);
  return s;
}

int main() {
  char what[200];
  cnc_plan_trace = onPlan;
  // 1. every line of the polar sample (converter output, G93) and of a mixed program
  std::string polar = readFile("polar_sample.ngc");
  uint32_t lines = 0;
  for (char c : polar) if (c == '\n') lines++;
  int bad = 0, tried = 0, firstBad = 0;
  for (uint32_t n = 2; n < lines; n++) {  // the last line is M2
    tried++;
    if (!sameFrom(polar, n, "polar_sample", true)) { if (!bad) firstBad = n; bad++; }
  }
  snprintf(what, sizeof(what), "polar_sample.ngc: every line %d of %d: same moves as the full run (first bad: %d)", tried - bad, tried, firstBad);
  check(!polar.empty() && bad == 0, what);
  if (firstBad) sameFrom(polar, firstBad, "polar_sample");
  std::string mixed =
      "(mixed)\nG21 G90 G18 G94\nG10 L2 P2 X1 Z-2\nG55\nG0 X30 Z5 C0\nG1 Z-1 F200\nG91 G1 X-5\nG90\nG2 X20 Z-6 R5\n"
      "G92 X0\nG1 X3 Z-7\nG92.1\nG20\nG1 X1 Z-0.3 F10\nG21\nG93\nG1 X10 C90 F2\nG1 X12 C180 F3\nG94 F150\nG0 Z8\nG54\n"
      "G1 X5 Z1\nM5\nM30\n";
  bad = 0; tried = 0; firstBad = 0;
  for (uint32_t n = 2; n <= 23; n++) {
    tried++;
    if (!sameFrom(mixed, n, "mixed", true)) { if (!bad) firstBad = n; bad++; }
  }
  snprintf(what, sizeof(what), "mixed program (G55, G10, G91, G92, G20, G93, arcs): every line %d of %d same (first bad: %d)", tried - bad, tried, firstBad);
  check(bad == 0, what);
  if (firstBad) sameFrom(mixed, firstBad, "mixed");
  // 2. refusals
  int32_t zero[CNC_N_AXIS] = {0, 0, 0};
  cnc_runfrom_t r;
  machine(zero);
  std::string shortp = "G21 G90\nG0 X1\nG0 X2\n";
  check(!cnc_runfrom_prepare(shortp.data(), shortp.size(), 40, 0, &r, nullptr) && strstr(r.reason, "has only 3 lines"), "line past the end: refused");
  machine(zero);
  check(!cnc_runfrom_prepare(mixed.data(), mixed.size(), 25, 0, &r, nullptr) && strstr(r.reason, "ends at line 24"), "after M30: refused");
  std::string thr = "G21 G90 G18\nG0 X10 Z2\nG33 Z-10 K1.5\nG0 X12\nG0 Z2\n";
  machine(zero);
  check(!cnc_runfrom_prepare(thr.data(), thr.size(), 5, 0, &r, nullptr) && strstr(r.reason, "threading at line 3"), "threading before N: refused");
  std::string err = "G21 G90\nG0 X10 Z2\nG41 X3\nG0 X1\n";
  machine(zero);
  check(!cnc_runfrom_prepare(err.data(), err.size(), 4, 0, &r, nullptr) && r.failLine == 3, "an error before N: refused with its line");
  // 3. the highest Z: the program's highest point before N, or the machine's Z if higher
  std::string hz = "G21 G90\nG0 Z12\nG0 X5\nG1 Z-2 F100\nG1 X8\nG1 X9\n";
  machine(zero);
  bool ok = cnc_runfrom_prepare(hz.data(), hz.size(), 6, (0 + MOFF[CNC_Z]) / 400.0, &r, nullptr);
  snprintf(what, sizeof(what), "highest Z before line 6 = 12 (G54 at machine 0) (%.3f), start Z = -2 (%.3f)", r.highestZ, r.machine[CNC_Z]);
  check(ok && fabs(r.highestZ - 12) < 1e-6 && fabs(r.machine[CNC_Z] + 2) < 1e-6, what);
  machine(zero);
  ok = cnc_runfrom_prepare(hz.data(), hz.size(), 6, 50, &r, nullptr);
  check(ok && r.highestZ == 50, "the machine higher than the program: stays at its own Z");
  printf("run from line: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
