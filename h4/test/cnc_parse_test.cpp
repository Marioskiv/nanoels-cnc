/*
  cnc_parse_test.cpp - checks what the GCODE-mode reader of NanoEls H4 accepts and refuses
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
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
  static uint32_t lineNo = 0;  // every call a new line (a cycle continues only on the same line)
  return cnc_gc_line(line, ++lineNo, msg, 128, &dwell);
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
  expect("G43 H2", CNC_OK, "G43 H");
  expect("%", CNC_OK);    // program start / end mark (Fusion and other posts write it; from a Grbl sender it reaches here)
  expect(" % ", CNC_OK);
  expect("%G0 X1", CNC_ERROR, "unexpected character");
  expect("G18 G1 G49", CNC_OK);  // back to the start modes (the list above set G17 and G80)
  expect("M6 T2", CNC_PAUSE);
  expect("M0", CNC_PAUSE);
  expect("M1", CNC_PAUSE);
  expect("M2", CNC_END);
  expect("M30", CNC_END);
  expect("G4 P1.5", CNC_DWELL);
  // refused
  expect("G2 X1 Z1 I5 F100", CNC_ERROR, "radius to end of arc differs");
  expect("G17 G2 X1 Z1 I1", CNC_ERROR, "X-Z plane");
  expect("G2 X1 Z1 J1", CNC_ERROR, "J word");
  expect("G2 X1 Z1", CNC_ERROR, "without I/K or R");
  expect("G2 X1 Z1 R1 I1", CNC_ERROR, "both R and I/K");
  expect("G1 X1 I1", CNC_ERROR, "without an arc");
  expect("G0 G1 X1", CNC_ERROR, "same modal group");
  expect("G90 G91", CNC_ERROR, "same modal group");
  expect("M3 M5", CNC_ERROR, "same modal group");
  expect("G10 L1 P1 Z2", CNC_ERROR, "only L2 and L20");
  expect("G92", CNC_ERROR, "needs an axis");
  expect("G53 G2 X1 Z1 R1", CNC_ERROR, "G53 only with G0 or G1");
  expect("G80 X1", CNC_ERROR, "G80");
  expect("G76 P1 Z-10 I-1 J0.5 K0.4", CNC_ERROR, "K must be greater than J");
  expect("G1 X1 J1 F100", CNC_ERROR, "J word");
  expect("G71 X1", CNC_ERROR, "without cycles");
  expect("G84 X1 Z-1 R1", CNC_ERROR, "spindle the H4 controls");
  expect("G81 X1 Z-1 R1 F100", CNC_ERROR, "drill along Y");
  expect("G17 G81 X1 R1 F100", CNC_ERROR, "Z value missing");
  expect("G17 G83 X1 Z-1 R1 F100", CNC_ERROR, "Q missing");
  expect("G17 G81 X1 Z-1 R1 P1 F100", CNC_ERROR, "P word without");
  expect("G17 G81 X1 Z3 R1 F100", CNC_ERROR, "R is below");
  expect("G38.2 Z-1", CNC_ERROR, "probing");
  expect("G41 D1", CNC_ERROR, "in computer");
  expect("O100 sub", CNC_ERROR, "O-word");
  expect("G4 P1 X1", CNC_ERROR, "move on one line");
  expect("M6 T1 X1", CNC_ERROR, "move on one line");
  expect("G1 Y5 F100", CNC_ERROR, "no Y axis");
  expect("G1 A5 F100", CNC_ERROR, "no A axis");
  expect("M98 P100", CNC_ERROR, "not supported");
  expect("G4 X1", CNC_ERROR, "needs P");
  expect("G4", CNC_ERROR, "needs P");
  expect("G1 X5 P2", CNC_ERROR, "P word without");
  expect("G1 X#1", CNC_ERROR, "parameters");
  expect("G1 X1 @1", CNC_ERROR, "unexpected character");
  expect("G1 X1.2.3", CNC_ERROR, "bad number");
  expect("G1 X", CNC_ERROR, "without a number");
  expect("G1 X5 F-3", CNC_ERROR, "F must not be negative");
  expect("G1 X5 F0", CNC_ERROR, "F must be positive");
  expectPos(0, 0, 0, "nothing planned by refused lines");

  // moves: the program starts in G1 (old H4 / lathecode lines without G), feed default 120
  expect("X1", CNC_ERROR, "no feed rate");
  expect("F100", CNC_OK);
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

  // G32 / G33 threading: nothing planned, the target and lead are handed over, the feed rate stays
  setup(true, 0);
  expect("G1 X1 F150", CNC_OK);
  uint8_t usedT = cnc_plan_blocks_used();
  expect("G32 Z-10 F1.5", CNC_THREAD);
  {
    int32_t t[3]; float lead;
    cnc_gc_thread(t, &lead);
    checks++;
    if (t[0] != 1200 || t[1] != -4000 || t[2] != 0 || lead != 1.5f || cnc_plan_blocks_used() != usedT) {
      printf("FAIL G32 target %d %d %d lead %g\n", t[0], t[1], t[2], lead); failures++;
    }
  }
  expect("G33 X2 Z-12 K2", CNC_THREAD);
  {
    int32_t t[3]; float lead;
    cnc_gc_thread(t, &lead);
    checks++;
    if (t[0] != 2400 || t[1] != -4800 || lead != 2.0f) { printf("FAIL G33 target %d %d lead %g\n", t[0], t[1], lead); failures++; }
  }
  expect("G20 G33 Z-0.5 K0.1", CNC_THREAD);
  {
    int32_t t[3]; float lead;
    cnc_gc_thread(t, &lead);
    checks++;
    if (t[1] != -5080 || lead < 2.539f || lead > 2.541f) { printf("FAIL G33 inch %d lead %g\n", t[1], lead); failures++; }
  }
  expect("G21", CNC_OK);
  expect("G32 Z-10", CNC_ERROR, "needs F");
  expect("G33 Z-10 F1", CNC_ERROR, "needs K");
  expect("G32 Z-10 K1", CNC_ERROR, "K without");
  expect("G32 C10 Z-10 F1", CNC_ERROR, "X and Z only");
  expect("G32 F1", CNC_ERROR, "needs X or Z");
  expect("G32 G1 Z-10 F1", CNC_ERROR, "same modal group");
  // the next plain move still uses F150, not the lead
  cnc_plan_reset();
  {
    int32_t p0[3] = {2400, -5080, 0}, z0[3] = {0, 0, 0};
    bool n0[3] = {false, false, false};
    cnc_gc_sync(p0, z0, n0);
  }
  expect("X0", CNC_OK);
  checks++;
  if (cnc_plan_last_block_minutes() < 2.0f / 150 * 0.99f || cnc_plan_last_block_minutes() > 2.0f / 150 * 1.01f) {
    printf("FAIL feed after G32: %g min\n", cnc_plan_last_block_minutes()); failures++;
  }

  // U / W: incremental X / Z (Fanuc and the CAM lathe posts); in G7 U is a diameter
  setup(true, 0);
  expect("G0 X10 Z5", CNC_OK);
  expect("G1 U-2 W-5 F100", CNC_OK);
  expectPos(8 * 1200, 0, 0, "U-2 W-5");
  expect("G7 G1 U-4", CNC_OK);
  expectPos(6 * 1200, 0, 0, "G7 U-4 = radius -2");
  expect("G8", CNC_OK);
  expect("G1 X1 U1", CNC_ERROR, "X and U");
  expect("G28 U0", CNC_OK);
  expectPos(0, 0, 0, "G28 U0: X home (machine 0), Z stays");
  expect("G96 S200 M3", CNC_OK, "by hand");

  // M48 / M49 / M50: overrides on / off for the following moves, as LinuxCNC (feed AND rapid override);
  // M51 / M52 / M53 cannot be honoured on this machine and are refused
  setup(true, 0);
  auto overridesOn = []() { cnc_modes_t m; cnc_gc_get_modes(&m); return m.overridesEnabled; };
  auto expectOvr = [&](bool want, const char* what) {
    checks++;
    if (overridesOn() != want) { printf("FAIL %s: overrides %s, want %s\n", what, overridesOn() ? "on" : "off", want ? "on" : "off"); failures++; }
  };
  expectOvr(true, "start (M48)");
  expect("M49", CNC_OK); expectOvr(false, "M49");
  expect("M48", CNC_OK); expectOvr(true, "M48");
  expect("M50 P0", CNC_OK); expectOvr(false, "M50 P0");
  expect("M50 P1", CNC_OK); expectOvr(true, "M50 P1");
  expect("M50 P0", CNC_OK); expect("M50", CNC_OK); expectOvr(true, "M50 without P");
  expect("M49", CNC_OK); expect("M2", CNC_END); expectOvr(true, "M2 turns the overrides on again");
  expect("M51", CNC_ERROR, "spindle speed is set by hand");
  expect("M51 P0", CNC_ERROR, "M51");
  expect("M52 P1", CNC_ERROR, "adaptive feed");
  expect("M53", CNC_ERROR, "feed stop");
  expect("M48 M49", CNC_ERROR, "same modal group");
  // the effect on the planned speed: with overrides at 50 % feed and 25 % rapid, a move under M49 keeps its
  // programmed speed (time at the programmed speed is what cnc_plan_take_all() reports, override included)
  auto blockMinutes = [&](const char* setupLine, const char* moveLine) {
    setup(true, 0);
    cnc_set_overrides(50, 25);
    char msg[128];
    run(setupLine, msg);
    run(moveLine, msg);
    float minutes[4];
    int n = cnc_plan_take_all(minutes, 4);
    cnc_set_overrides(100, 100);
    return n > 0 ? minutes[n - 1] : -1.0f;
  };
  struct { const char *pre, *move; float want; const char* what; } ov[] = {
    {"M48", "G1 X10 F100", 0.2f, "feed 50 % under M48"},              // 10 mm at 50 mm/min
    {"M49", "G1 X10 F100", 0.1f, "feed under M49 = programmed"},       // 10 mm at 100 mm/min
    {"M50 P0", "G1 X10 F100", 0.1f, "feed under M50 P0 = programmed"},
    {"M49", "G0 X7.2", 0.01f, "rapid under M49 = full rapid"},          // 7.2 mm at 720 mm/min
    {"M48", "G0 X7.2", 0.04f, "rapid 25 % under M48"},
    {"M49", "G93 G1 X10 F20", 0.05f, "G93 under M49: 1/F"},
    {"M48", "G93 G1 X10 F20", 0.1f, "G93 with feed override 50 %: twice 1/F"},
  };
  for (auto& o : ov) {
    float m = blockMinutes(o.pre, o.move);
    checks++;
    if (fabsf(m - o.want) > 0.0005f) { printf("FAIL %s: %.4f min, want %.4f\n", o.what, m, o.want); failures++; }
  }
  // the override change on a line is applied before that line's move (LinuxCNC order): "M49 G1 ..." is not scaled
  {
    float m = blockMinutes("G21", "M49 G1 X10 F100");
    checks++;
    if (fabsf(m - 0.1f) > 0.0005f) { printf("FAIL M49 on the move's line: %.4f min, want 0.1\n", m); failures++; }
  }

  printf("parse: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
