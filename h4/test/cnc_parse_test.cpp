/*
  cnc_parse_test.cpp - checks what the GCODE-mode reader of NanoEls H4 accepts and refuses
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <stdio.h>
#include <string.h>
#include <initializer_list>

static int failures = 0, checks = 0;

static void setup(bool cActive, int32_t backlashX) {
  cnc_settings_t s;
  s.stepsPerUnit[0] = 1200; s.stepsPerUnit[1] = 400; s.stepsPerUnit[2] = 91.733333f;
  s.maxRate[0] = 720; s.maxRate[1] = 1500; s.maxRate[2] = 21600;
  s.acceleration[0] = 360000; s.acceleration[1] = 720000; s.acceleration[2] = 2592000;
  s.junctionDeviation = 0.01f; s.timerHz = 20000000;
  cnc_core_settings(&s);
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
  m.active[0] = m.active[1] = true; m.active[2] = cActive;
  m.rotary[2] = true;
  m.backlashSteps[0] = backlashX;
  m.defaultFeed = 120;
  cnc_gc_init(&m);
  cnc_plan_reset();
  cnc_st_reset();
  int32_t z[3] = {0, 0, 0};
  bool n[3] = {false, false, false};
  cnc_gc_sync(z, z, n);
}

static int run(const char* line, char* msg) {
  float dwell = 0;
  return cnc_gc_line(line, 1, msg, 128, &dwell);
}

static void expect(const char* line, int want, const char* msgPart = NULL) {
  char msg[128];
  int r = run(line, msg);
  checks++;
  if (r != want || (msgPart && !strstr(msg, msgPart))) {
    printf("FAIL \"%s\": got %d \"%s\", want %d%s%s\n", line, r, msg, want, msgPart ? " with " : "", msgPart ? msgPart : "");
    failures++;
  }
}

static void expectPos(int32_t x, int32_t z, int32_t c, const char* what) {
  const int32_t* p = cnc_plan_position();
  checks++;
  if (p[0] != x || p[1] != z || p[2] != c) {
    printf("FAIL %s: planner at %d %d %d, want %d %d %d\n", what, p[0], p[1], p[2], x, z, c);
    failures++;
  }
}

int main() {
  setup(true, 0);
  // accepted, nothing to do
  for (const char* l : {"G21", "G90", "G94", "G17", "G18", "G19", "G40", "G49", "G54", "G61", "G64", "G64 P0.025", "G80",
                        "G8", "G54 G40 G49 G80", "M3 S10000", "M5", "M8", "M9", "T2", "N10", "g21 g90", "S18000 M3"})
    expect(l, CNC_OK);
  expect("G43 H2", CNC_OK, "G43 ignored");
  expect("M6 T2", CNC_PAUSE);
  expect("M0", CNC_PAUSE);
  expect("M1", CNC_PAUSE);
  expect("M2", CNC_END);
  expect("M30", CNC_END);
  expect("G4 P1.5", CNC_DWELL);
  // refused
  expect("G2 X1 Z1 I1", CNC_ERROR, "arcs");
  expect("G3 X1 Z1 R1", CNC_ERROR, "arcs");
  expect("G1 X1 I1", CNC_ERROR, "arcs");
  expect("G7", CNC_ERROR, "diameter");
  expect("G28", CNC_ERROR, "not supported");
  expect("G53 G0 Z0", CNC_ERROR, "not supported");
  expect("G92 X0", CNC_ERROR, "not supported");
  expect("G81 X1 Z-1 R1", CNC_ERROR, "not supported");
  expect("G41", CNC_ERROR, "not supported");
  expect("G1 Y5 F100", CNC_ERROR, "no Y axis");
  expect("G1 A5 F100", CNC_ERROR, "no A axis");
  expect("M98 P100", CNC_ERROR, "not supported");
  expect("G4 X1", CNC_ERROR, "G4 with an axis");
  expect("G4", CNC_ERROR, "needs P");
  expect("G1 X5 P2", CNC_ERROR, "P without G4");
  expect("G1 X#1", CNC_ERROR, "without a number");
  expect("G1 X1 #1", CNC_ERROR, "unexpected character");
  expect("G1 X1.2.3", CNC_ERROR, "bad number");
  expect("G1 X", CNC_ERROR, "without a number");
  expect("G1 X5 F-3", CNC_ERROR, "F must be positive");
  expectPos(0, 0, 0, "nothing planned by refused lines");

  // moves: the program starts in G1 (old H4 / lathecode lines without G), feed default 120
  expect("X1", CNC_OK);
  expectPos(1200, 0, 0, "X1 without G");
  expect("G0 X2 Z-1 C90", CNC_OK);
  expectPos(2400, -400, 8256, "G0 X2 Z-1 C90");
  expect("G91 X1 Z1", CNC_OK);
  expectPos(3600, 0, 8256, "G91 X1 Z1");
  expect("G90 G20 X1", CNC_OK);
  expectPos(30480, 0, 8256, "G20 X1 = 25.4 mm");
  expect("G20 C10", CNC_OK);
  expectPos(30480, 0, 917, "G20 does not scale C");
  expect("G21 G93 G1 X0 C0", CNC_ERROR, "own F");
  expect("G21 G93 G1 X0 C0 F2", CNC_OK);
  expectPos(0, 0, 0, "G93 move");
  expect("G1 X1", CNC_ERROR, "own F");
  expect("G94", CNC_OK);
  expect("G1 X1", CNC_ERROR, "no feed rate");
  expect("G1 X1 F200", CNC_OK);
  expect("X2", CNC_OK);
  expectPos(2400, 0, 0, "modal feed after F200");

  // C not connected
  setup(false, 0);
  expect("G1 C10 F100", CNC_ERROR, "not connected");

  // backlash: a reversal adds a take-up block, the planner position is the tool's
  setup(true, 100);
  expect("G1 X1 F100", CNC_OK);
  uint8_t used1 = cnc_plan_blocks_used();
  expect("G1 X0", CNC_OK);
  checks++;
  if (cnc_plan_blocks_used() != used1 + 2) { printf("FAIL backlash block not added (%d -> %d)\n", used1, cnc_plan_blocks_used()); failures++; }
  expectPos(0, 0, 0, "after reversal");

  printf("parse: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
