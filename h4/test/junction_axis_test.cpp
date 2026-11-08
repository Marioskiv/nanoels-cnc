/*
  junction_axis_test.cpp - at a corner between two moves no axis may change its speed at once by more than its
  acceleration x 10 ms (cnc_core.cpp, firmware 1.36). Grbl's junction deviation alone works on the direction of all
  axes together, where C degrees count as mm, so a fast C hid a reversal of X or Z and X jumped by ~2 mm/s.
  Random programs of straight moves in X, Z and C (G94 and G93, short and long, sharp and gentle corners, C alone, X/Z
  alone) run through the real planner and step generator on the author's machine and two others; the step generator
  reports the speed at which every block starts, which is the speed at the corner. Checked at every corner and for
  every axis: speed x |change of the axis's share of the direction| <= acceleration x 10 ms (+0.1 %).
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

struct Block { int32_t delta[CNC_N_AXIS]; uint32_t line; };
static std::vector<Block> planned;  // the moves in planner order (line = index + 1)
static std::vector<std::pair<uint32_t, float>> starts;  // (line, speed units/min) when a block starts
static void onBlock(uint32_t line, uint8_t, bool start, float speed) {
  if (start) starts.push_back(std::make_pair(line, speed));
}

static int checks = 0, failures = 0;
static double worstRatio = 0;

static void unitVec(const int32_t d[CNC_N_AXIS], const cnc_settings_t& s, double u[CNC_N_AXIS]) {
  double l = 0;
  for (int i = 0; i < CNC_N_AXIS; i++) { u[i] = d[i] / s.stepsPerUnit[i]; l += u[i] * u[i]; }
  l = sqrt(l);
  for (int i = 0; i < CNC_N_AXIS; i++) u[i] /= l;
}

// one random program on one machine: n moves, then run until the step generator has finished
static void run(const cnc_settings_t& s, unsigned seed, int n, const char* machine) {
  cnc_core_settings(&s);
  cnc_plan_reset();
  cnc_st_reset();
  planned.clear();
  starts.clear();
  cnc_st_block_event = onBlock;
  unsigned r = seed * 2654435761u + 7;
  auto rnd = [&]() { r = r * 1103515245u + 12345u; return (r >> 8) & 0xffff; };
  int32_t pos[CNC_N_AXIS] = {0, 0, 0};
  int next = 0;
  float dir[CNC_N_AXIS] = {1, 1, 1};
  while (next < n || cnc_st_busy() || cnc_plan_blocks_used() > 0) {
    while (next < n && cnc_plan_blocks_free() > 1) {
      Block b;
      int kind = rnd() % 6;
      for (int i = 0; i < CNC_N_AXIS; i++) {
        if (rnd() % 5 == 0) dir[i] = -dir[i];  // reversals
        double mm = (kind == 0 ? 0.05 : kind == 1 ? 0.5 : 5.0) * (rnd() % 1000) / 1000.0;
        if (i == CNC_C) mm *= (kind == 2 ? 60 : 8);  // C in degrees: often much more than X/Z
        b.delta[i] = (int32_t)lround(dir[i] * mm * s.stepsPerUnit[i]);
      }
      if (kind == 3) b.delta[CNC_X] = b.delta[CNC_Z] = 0;      // C alone
      if (kind == 4) b.delta[CNC_C] = 0;                         // X/Z alone
      if (b.delta[0] == 0 && b.delta[1] == 0 && b.delta[2] == 0) b.delta[CNC_X] = 1;
      int32_t target[CNC_N_AXIS];
      for (int i = 0; i < CNC_N_AXIS; i++) target[i] = pos[i] + b.delta[i];
      b.line = (uint32_t)planned.size() + 1;
      bool inverse = rnd() % 2;
      float feed = inverse ? 600.0f + rnd() % 30000 : 50.0f + rnd() % 3000;  // G93 1/min, or G94 units/min
      if (cnc_plan_buffer_line(target, feed, inverse ? CNC_COND_INVERSE_TIME : 0, b.line)) {
        planned.push_back(b);
        memcpy(pos, target, sizeof(pos));
      }
      next++;
    }
    cnc_st_prep_buffer();
    cnc_tick_t t;
    for (int k = 0; k < 2000; k++) if (!cnc_isr(&t)) break;
  }
  cnc_st_block_event = 0;
  // every block that started with a speed: compare with the block before it
  for (auto& st : starts) {
    uint32_t line = st.first;
    if (line < 2 || line > planned.size()) continue;
    double u0[CNC_N_AXIS], u1[CNC_N_AXIS];
    unitVec(planned[line - 2].delta, s, u0);
    unitVec(planned[line - 1].delta, s, u1);
    for (int i = 0; i < CNC_N_AXIS; i++) {
      double jump = st.second * fabs(u1[i] - u0[i]);                       // units/min, at once
      double allowed = s.acceleration[i] * CNC_JUNCTION_AXIS_JUMP_MIN;     // units/min
      checks++;
      if (allowed > 0 && jump / allowed > worstRatio) worstRatio = jump / allowed;
      if (jump > allowed * 1.001 + 1e-6) {
        failures++;
        if (failures <= 5) printf("FAIL %s seed %u line %u: %c jumps %.3f units/min at the corner, allowed %.3f\n",
                                  machine, seed, line, "XZC"[i], jump, allowed);
      }
    }
  }
}

int main() {
  cnc_settings_t marios, h4def, fast;
  memset(&marios, 0, sizeof(marios));
  marios.stepsPerUnit[CNC_X] = 1200; marios.stepsPerUnit[CNC_Z] = 400; marios.stepsPerUnit[CNC_C] = 91.733333f;
  marios.maxRate[CNC_X] = 12 * 60; marios.maxRate[CNC_Z] = 25 * 60; marios.maxRate[CNC_C] = 360 * 60;
  marios.acceleration[CNC_X] = 100 * 3600; marios.acceleration[CNC_Z] = 100 * 3600; marios.acceleration[CNC_C] = 720 * 3600;
  marios.junctionDeviation = 0.01f; marios.timerHz = 20000000;
  h4def = marios;
  h4def.stepsPerUnit[CNC_X] = 1920; h4def.stepsPerUnit[CNC_Z] = 400; h4def.stepsPerUnit[CNC_C] = 150;
  h4def.maxRate[CNC_X] = 3 * 60; h4def.maxRate[CNC_Z] = 12 * 60; h4def.maxRate[CNC_C] = 1280;
  h4def.acceleration[CNC_X] = 12.5f * 3600; h4def.acceleration[CNC_Z] = 60 * 3600; h4def.acceleration[CNC_C] = 107 * 3600;
  fast = marios;
  for (int i = 0; i < CNC_N_AXIS; i++) { fast.maxRate[i] *= 3; fast.acceleration[i] *= 10; }
  fast.junctionDeviation = 0.05f;
  for (unsigned seed = 1; seed <= 40; seed++) {
    run(marios, seed, 400, "marios");
    run(h4def, seed, 400, "h4default");
    run(fast, seed, 400, "fast");
  }
  printf("junction axis: %d checks, %d failures (largest jump %.3f of the allowed)\n", checks, failures, worstRatio);
  return failures ? 1 : 0;
}
