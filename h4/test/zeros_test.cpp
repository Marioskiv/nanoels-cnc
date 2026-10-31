// Tests of zeros.ino (NanoEls H4 CNC fork): the lathe modes' zero and the GCODE mode's zero stay apart.
// The file is compiled unchanged; Axis, the mode switch and markAxisOrigin are reproduced as h4.ino has them.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>

struct Axis {
  long pos, motorPos, originPos, otherOriginPos, machineOffset;
};
Axis x, z, a1;
volatile bool cncZeroActive = false;
unsigned long keypadTimeUs = 0;
unsigned long micros() { return 0; }
#define MODE_TURN 4
#define MODE_GCODE 9
int mode = -1;
#include "../zeros.ino"

// as h4.ino
void setModeFromLoop(int value) {
  if (mode == value) return;
  mode = value;
  if ((mode == MODE_GCODE) != cncZeroActive) swapZeros(mode == MODE_GCODE);
}
void markAxisOrigin(Axis* a) {
  a->motorPos -= a->pos;
  a->originPos += a->pos;
  a->otherOriginPos += a->pos;
  a->machineOffset += a->pos;
  a->pos = 0;
}
long display(Axis* a) { return a->pos + a->originPos; }
// the H4's Z / X key: zero where the tool stands
void zeroHere(Axis* a) { a->originPos = -a->pos; }
// a fixed place on the machine, in the axis' present pos frame
long place(Axis* a, long machine) { return machine - a->machineOffset; }

int fails = 0, checks = 0;
void check(bool ok, const char* what) {
  checks++;
  if (!ok) {
    fails++;
    printf("FAIL: %s\n", what);
  }
}

// One machine: the lathe tool touches the face at machine 1000, the router's G54 zero is at machine 4000.
void scenario(unsigned seed) {
  srand(seed);
  Axis* a = &z;
  *a = Axis{};
  cncZeroActive = false;
  mode = -1;
  setModeFromLoop(MODE_TURN);
  a->pos = place(a, 1000);
  zeroHere(a);  // lathe touch-off
  setModeFromLoop(MODE_GCODE);
  a->pos = place(a, 4000);
  zeroHere(a);  // G-code touch-off (or G10 L20)
  for (int step = 0; step < 200; step++) {
    int r = rand() % 4;
    if (r == 0) a->pos = place(a, rand() % 9000 - 2000);   // a move
    if (r == 1) markAxisOrigin(a);                         // pitch change, ON, START...
    if (r == 2) setModeFromLoop(rand() % 2 ? MODE_GCODE : MODE_TURN);
    if (r == 3) {  // save to Preferences and start again from them (power off)
      long lathe = latheOriginOf(a), cnc = cncOriginOf(a), pos = a->pos, off = a->machineOffset;
      int m = mode;
      *a = Axis{};
      a->pos = pos;
      a->machineOffset = off;  // the place of the steps' frame (same pos after the restart)
      a->originPos = lathe;
      a->otherOriginPos = cnc;
      cncZeroActive = false;
      mode = -1;
      setModeFromLoop(m);
    }
    // whatever happened, each family reads 0 at its own touch-off place
    long here = a->pos;
    a->pos = place(a, mode == MODE_GCODE ? 4000 : 1000);
    check(display(a) == 0, mode == MODE_GCODE ? "the GCODE zero moved" : "the lathe zero moved");
    a->pos = here;
  }
}

int main() {
  // first start with this firmware: no GCODE zero stored yet -> it equals the lathe zero (as before)
  z = Axis{};
  z.pos = 500;
  z.originPos = -300;
  z.otherOriginPos = z.originPos;  // the default when Preferences have none
  cncZeroActive = false;
  mode = -1;
  setModeFromLoop(MODE_GCODE);
  check(display(&z) == 200, "first start: the GCODE mode keeps the old shared zero");
  setModeFromLoop(MODE_TURN);
  check(display(&z) == 200, "first start: the lathe zero unchanged");
  // a lathe touch-off does not move the GCODE zero, and back
  setModeFromLoop(MODE_TURN);
  z.pos = 800;
  zeroHere(&z);
  setModeFromLoop(MODE_GCODE);
  check(display(&z) == 500, "lathe touch-off moved the GCODE zero");
  zeroHere(&z);
  setModeFromLoop(MODE_TURN);
  check(display(&z) == 0, "GCODE touch-off moved the lathe zero");
  for (unsigned s = 1; s <= 300; s++) scenario(s);
  printf("zeros: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
