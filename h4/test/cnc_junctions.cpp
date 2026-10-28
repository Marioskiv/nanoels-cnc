/*
  cnc_junctions.cpp - measures what the planner does at every corner (junction) of a program: how much each
  axis changes its speed at once there. Grbl's planner (and this port) moves through a corner at a "junction
  speed"; the direction changes in an instant, so each axis jumps from one speed to another. The junction speed
  comes from the junction deviation and the axis accelerations, computed on the vector of all axes together,
  where X and Z are in mm and C in degrees. This tool shows, per axis, what that gives on real programs.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later

  The program is planned as on the machine with a full look-ahead buffer: a block's entry speed is taken when
  the block before it is finished (when the step generator would start it). Per junction and axis:
    jump      speed change at once, in steps/s and in motor turns/s (steps per motor turn given below)
    t_eq      jump / acceleration of the axis: how long the axis would need to make that change at its
              maximum acceleration (ms). Grbl's corners on X / Z give the reference for what is normal.
    cart      the change of the tool's speed relative to the part (mm/s): radial X, tangential X * C (rad),
              axial Z - the real geometric corner, independent of units.
  Measured result (2026-10-28): every jump stays within sqrt(8 * acceleration * junction deviation) of that axis,
  in the axis's own units (X, Z mm; C degrees), because the junction acceleration is limited per axis. --check
  fails when an axis goes past that bound (0.5 % for float rounding).
  Usage: cnc_junctions program.ngc [--machine marios|h4default] [--jd mm] [--csv file] [--check]
*/
#include "../cnc_core.cpp"
#include "../cnc_gcode.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

struct J { uint32_t line; double v; double jump[CNC_N_AXIS]; double r; double cart; bool cMoves, xzMoves; };

int main(int argc, char** argv) {
  const char* file = NULL; const char* machine = "marios"; const char* csv = NULL; float jd = 0.01f; bool check = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--machine")) machine = argv[++i];
    else if (!strcmp(argv[i], "--jd")) jd = atof(argv[++i]);
    else if (!strcmp(argv[i], "--csv")) csv = argv[++i];
    else if (!strcmp(argv[i], "--check")) check = true;
    else file = argv[i];
  }
  if (!file) { fprintf(stderr, "usage: cnc_junctions program.ngc\n"); return 2; }
  cnc_settings_t s;
  double motorStepsPerTurn[CNC_N_AXIS];
  if (!strcmp(machine, "h4default")) {
    s.stepsPerUnit[CNC_X] = 2400 / 1.25f; s.stepsPerUnit[CNC_Z] = 800 / 2.0f; s.stepsPerUnit[CNC_C] = 300 / 2.0f;
    s.maxRate[CNC_X] = 3 * 2400 / s.stepsPerUnit[CNC_X] * 60; s.maxRate[CNC_Z] = 6 * 800 / s.stepsPerUnit[CNC_Z] * 60;
    s.maxRate[CNC_C] = 3200 / s.stepsPerUnit[CNC_C] * 60;
    s.acceleration[CNC_X] = 10 * 2400 / s.stepsPerUnit[CNC_X] * 3600; s.acceleration[CNC_Z] = 30 * 800 / s.stepsPerUnit[CNC_Z] * 3600;
    s.acceleration[CNC_C] = 16000 / s.stepsPerUnit[CNC_C] * 3600;
    motorStepsPerTurn[0] = 2400; motorStepsPerTurn[1] = 800; motorStepsPerTurn[2] = 300;
  } else {
    // the author's machine: X 1200 steps/mm (1600 steps per motor turn, belt 20:30, 2 mm screw), 12 mm/s,
    // 100 mm/s2; Z 400 steps/mm (800 per turn); C 91.733 steps/deg (3200 per motor turn, 10.32:1), 360 deg/s, 720 deg/s2
    s.stepsPerUnit[CNC_X] = 1200; s.stepsPerUnit[CNC_Z] = 400; s.stepsPerUnit[CNC_C] = 91.733333f;
    s.maxRate[CNC_X] = 12 * 60; s.maxRate[CNC_Z] = 25 * 60; s.maxRate[CNC_C] = 360 * 60;
    s.acceleration[CNC_X] = 100 * 3600; s.acceleration[CNC_Z] = 200 * 3600; s.acceleration[CNC_C] = 720 * 3600;
    motorStepsPerTurn[0] = 1600; motorStepsPerTurn[1] = 800; motorStepsPerTurn[2] = 3200;
  }
  s.junctionDeviation = jd;
  s.timerHz = 20000000;
  cnc_core_settings(&s);
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
  for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
  m.rotary[CNC_C] = true;
  m.defaultFeed = 100;
  m.arcTolerance = 0.002f;
  cnc_gc_init(&m);
  cnc_plan_reset();
  cnc_st_reset();
  int32_t z0[3] = {0, 0, 0};
  bool n0[3] = {false, false, false};
  cnc_gc_sync(z0, z0, n0);

  std::vector<J> js;
  bool havePrev = false;
  float prevU[CNC_N_AXIS];
  double pos[CNC_N_AXIS] = {0, 0, 0};  // position at the end of the previous block, units
  auto unitOf = [&](const cnc_block_t* b, float* u) {
    float mag = 0;
    for (int i = 0; i < CNC_N_AXIS; i++) {
      u[i] = (b->direction_bits & (1 << i) ? -1.0f : 1.0f) * b->steps[i] / s.stepsPerUnit[i];
      mag += u[i] * u[i];
    }
    mag = sqrtf(mag);
    for (int i = 0; i < CNC_N_AXIS; i++) u[i] /= mag;
  };
  // the tail block starts now: its entry speed is the speed through the junction with the block before
  auto startTail = [&]() {
    cnc_block_t* b = plan_get_current_block();
    if (!b) return;
    float u[CNC_N_AXIS];
    unitOf(b, u);
    bool backlash = b->condition & CNC_COND_BACKLASH;
    if (havePrev && !backlash) {
      J j; j.line = b->line; j.v = sqrt(b->entry_speed_sqr); j.r = fabs(pos[CNC_X]);
      double dv[CNC_N_AXIS];
      for (int i = 0; i < CNC_N_AXIS; i++) { dv[i] = j.v * (u[i] - prevU[i]) / 60.0; j.jump[i] = fabs(dv[i]); }  // units/s
      j.cart = sqrt(dv[CNC_X] * dv[CNC_X] + pow(j.r * dv[CNC_C] * M_PI / 180, 2) + dv[CNC_Z] * dv[CNC_Z]);
      j.cMoves = fabs(u[CNC_C]) > 1e-6 || fabs(prevU[CNC_C]) > 1e-6;
      j.xzMoves = fabs(u[CNC_X]) + fabs(u[CNC_Z]) > 1e-6 || fabs(prevU[CNC_X]) + fabs(prevU[CNC_Z]) > 1e-6;
      js.push_back(j);
    }
    if (!backlash) {
      memcpy(prevU, u, sizeof(u)); havePrev = true;
      for (int i = 0; i < CNC_N_AXIS; i++) pos[i] += (b->direction_bits & (1 << i) ? -1.0 : 1.0) * b->steps[i] / s.stepsPerUnit[i];
    } else havePrev = false;  // a backlash block starts and ends at rest
  };
  auto finishTail = [&]() { plan_discard_current_block(); startTail(); };

  FILE* f = fopen(file, "r");
  if (!f) { perror(file); return 2; }
  char buf[4096];
  uint32_t lineNo = 0;
  bool started = false;
  while (fgets(buf, sizeof(buf), f)) {
    lineNo++;
    std::string l;
    bool paren = false;
    for (char* c = buf; *c; c++) {
      if (paren) { if (*c == ')') paren = false; continue; }
      if (*c == '(') { paren = true; continue; }
      if (*c == ';' || *c == '\n' || *c == '\r') break;
      l += *c;
    }
    if (l.find_first_not_of(" \t") == std::string::npos || l == "%") continue;
    char msg[128]; float dwell = 0; int r;
    while (true) {
      while (cnc_plan_blocks_free() < 2 && cnc_plan_blocks_used()) { if (!started) { startTail(); started = true; } finishTail(); }
      r = cnc_gc_line(l.c_str(), lineNo, msg, sizeof(msg), &dwell);
      if (r == CNC_DWELL_MORE) continue;
      if (r != CNC_WAIT) break;
      if (!started && cnc_plan_blocks_used()) { startTail(); started = true; }
      if (cnc_plan_blocks_used()) finishTail();
      else break;
    }
    if (r == CNC_ERROR) { printf("line %u: %s\n", lineNo, msg); return 1; }
    // a dwell, pause or end waits for the machine to stop: every block runs out, the next starts at rest
    if (r == CNC_DWELL || r == CNC_PAUSE || r == CNC_END) {
      if (!started && cnc_plan_blocks_used()) { startTail(); started = true; }
      while (cnc_plan_blocks_used()) finishTail();
      havePrev = false; started = false;
      if (r == CNC_END) break;
    }
  }
  if (!started && cnc_plan_blocks_used()) startTail();
  while (cnc_plan_blocks_used()) finishTail();

  if (csv) {
    FILE* o = fopen(csv, "w");
    fprintf(o, "line,v,jumpX_mm_s,jumpZ_mm_s,jumpC_deg_s,r_mm,cart_mm_s\n");
    for (const J& j : js) fprintf(o, "%u,%.4f,%.5f,%.5f,%.5f,%.4f,%.5f\n", j.line, j.v, j.jump[0], j.jump[1], j.jump[2], j.r, j.cart);
    fclose(o);
  }
  // summary: per axis the largest jump and the 99th percentile, in steps/s, motor turns/s and t_eq
  printf("%s: %zu junctions (machine %s, junction deviation %.3f)\n", file, js.size(), machine, jd);
  const char* nm = "XZC";
  for (int a = 0; a < CNC_N_AXIS; a++) {
    std::vector<double> v;
    uint32_t worstLine = 0; double worst = 0, worstR = 0;
    for (const J& j : js) { v.push_back(j.jump[a]); if (j.jump[a] > worst) { worst = j.jump[a]; worstLine = j.line; worstR = j.r; } }
    if (v.empty()) continue;
    std::sort(v.begin(), v.end());
    double p99 = v[(size_t)(0.99 * (v.size() - 1))], p50 = v[v.size() / 2];
    double sps = s.stepsPerUnit[a], acc = s.acceleration[a] / 3600.0;
    printf("  %c jump: max %8.1f steps/s = %.3f motor turns/s, t_eq %6.2f ms (line %u, r %.2f mm) | p99 %8.1f steps/s, t_eq %5.2f ms | median %.1f steps/s\n",
           nm[a], worst * sps, worst * sps / motorStepsPerTurn[a], 1000 * worst / acc, worstLine, worstR, p99 * sps, 1000 * p99 / acc, p50 * sps);
  }
  int over = 0;
  for (int a = 0; a < CNC_N_AXIS; a++) {
    double bound = sqrt(8.0 * (s.acceleration[a] / 3600.0) * jd);  // units/s
    for (const J& j : js)
      if (j.jump[a] > bound * 1.005) { if (over < 5) printf("  %c jumps %.4f units/s at line %u: over the bound %.4f\n", nm[a], j.jump[a], j.line, bound); over++; }
  }
  if (check) printf("  bound check: %s\n", over ? "FAIL" : "OK (every axis within sqrt(8 a jd))");
  std::vector<double> c;
  uint32_t cl = 0; double cw = 0, cr = 0;
  for (const J& j : js) { c.push_back(j.cart); if (j.cart > cw) { cw = j.cart; cl = j.line; cr = j.r; } }
  if (!c.empty()) {
    std::sort(c.begin(), c.end());
    printf("  tool speed change at the part: max %.2f mm/s (line %u, r %.2f mm) | p99 %.2f mm/s | median %.3f mm/s\n", cw, cl, cr, c[(size_t)(0.99 * (c.size() - 1))], c[c.size() / 2]);
  }
  return check && over ? 1 : 0;
}
