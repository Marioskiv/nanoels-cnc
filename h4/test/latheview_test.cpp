// Tests of latheview.ino (NanoEls H4 CNC fork): how the lathe modes show Z and X.
// The file is compiled unchanged; Axis and the keypad's arithmetic are reproduced as h4.ino has them.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>

struct Axis {
  char name;
  long pos, originPos;
};
Axis x{'X', 0, 0}, z{'Z', 0, 0}, a1{'C', 0, 0};
#define MODE_TURN 4
#define MODE_GCODE 9
int mode = MODE_TURN;
const int customCharDiaCode = 7;
#define LATHE_Z_ISO 0
#define LATHE_Z_H4 1
#define LATHE_X_DIA 0
#define LATHE_X_RAD 1
#define LATHE_X_H4 2
int latheZView = LATHE_Z_ISO;
int latheXView = LATHE_X_DIA;
#include "../latheview.ino"

// 1 step = 1 deci-micron here, so du and steps are the same numbers.
long shown(Axis* a) { return latheViewDu(a, a->pos + a->originPos); }
// The H4 keys with a number (processNumpadResult): the sign comes from the key.
enum Key { LEFT, RIGHT, UP, DOWN, KEY_Z, KEY_X, KEY_A };
void numpad(Key k, long du) {
  Axis* a = (k == LEFT || k == RIGHT || k == KEY_Z) ? &z : &x;
  int sign = (k == LEFT || k == UP || k == KEY_Z || k == KEY_X || k == KEY_A) ? 1 : -1;
  long pos = a->pos + latheTypedDu(a, du, k == KEY_A) * sign;
  if (k == KEY_Z || k == KEY_X) a->originPos = -pos;
  else if (k == KEY_A) a->originPos = -(a->pos + pos) / 2;
  else a->pos = pos; // a move
}

int fails = 0, checks = 0;
void check(bool ok, const char* what, long got, long want) {
  checks++;
  if (!ok) {
    fails++;
    printf("FAIL %s: got %ld, want %ld\n", what, got, want);
  }
}
#define EQ(got, want, what) check((got) == (want), what, (long)(got), (long)(want))

int main() {
  const long MM = 10000; // deci-microns per mm
  for (int zv = 0; zv <= 1; zv++) {
    for (int xv = 0; xv <= 2; xv++) {
      latheZView = zv;
      latheXView = xv;
      mode = MODE_TURN;
      // A part of Ø 40: the tool touches its outside, numpad 40 + A: X0 on the centre line.
      x = {'X', 1234, 0};
      numpad(KEY_A, 40 * MM);
      long wantX = xv == LATHE_X_DIA ? 40 * MM : xv == LATHE_X_RAD ? 20 * MM : -20 * MM;
      EQ(shown(&x), wantX, "outside of a 40 mm part after numpad 40 + A");
      // Cutting in (towards the centre, H4 +X) by 1 mm of radius: Ø 38 / R 19 / X -19.
      x.pos += 1 * MM;
      EQ(shown(&x), xv == LATHE_X_DIA ? 38 * MM : xv == LATHE_X_RAD ? 19 * MM : -19 * MM, "1 mm deeper");
      // numpad 30 + X: "this is X 30" in the diameter and radius views (diameter touch-off), as on the H4 otherwise.
      numpad(KEY_X, 30 * MM);
      EQ(shown(&x), xv == LATHE_X_H4 ? -30 * MM : 30 * MM, "numpad 30 + X");
      // numpad 4 + up (towards the centre): Ø 26 (4 of diameter) / R 26 (4 of radius) / X -26.
      numpad(UP, 4 * MM);
      EQ(shown(&x), xv == LATHE_X_H4 ? -26 * MM : 26 * MM, "numpad 4 + up");
      numpad(DOWN, 4 * MM);
      EQ(shown(&x), xv == LATHE_X_H4 ? -30 * MM : 30 * MM, "numpad 4 + down");
      // Z: the face touched, Z key: Z0. Then 20 mm towards the chuck (H4 +Z, key left).
      z = {'Z', 555, 0};
      z.originPos = -z.pos;
      EQ(shown(&z), 0, "Z0 on the face");
      numpad(LEFT, 20 * MM);
      EQ(shown(&z), zv == LATHE_Z_ISO ? -20 * MM : 20 * MM, "20 mm towards the chuck");
      // numpad 5 + Z: Z0 5 mm ahead (towards the chuck): the tool is 5 mm in the air in front of the face.
      numpad(KEY_Z, 5 * MM);
      EQ(shown(&z), zv == LATHE_Z_ISO ? 5 * MM : -5 * MM, "numpad 5 + Z");
      // Z numbers are never halved; stop lengths: X doubled only in the diameter view.
      EQ(latheTypedDu(&z, 7 * MM, false), 7 * MM, "a Z number");
      EQ(latheViewLengthDu(&x, 3 * MM), xv == LATHE_X_DIA ? 6 * MM : 3 * MM, "X length between stops");
      EQ(latheViewLengthDu(&z, 3 * MM), 3 * MM, "Z length between stops");
      EQ(latheViewName(&x), xv == LATHE_X_DIA ? 7 : 'X', "X name");
      EQ(latheViewName(&z), 'Z', "Z name");
      // The GCODE mode is never changed by the view.
      mode = MODE_GCODE;
      EQ(latheViewDu(&x, 123), 123, "GCODE X");
      EQ(latheViewDu(&z, 123), 123, "GCODE Z");
      EQ(latheTypedDu(&x, 100, false), 100, "GCODE typed X");
      EQ(latheViewName(&x), 'X', "GCODE X name");
      EQ(latheViewLengthDu(&x, 5), 5, "GCODE length");
      EQ(latheViewDu(&a1, 77), 77, "C axis");
    }
  }
  // The original H4 view is the identity: same numbers as before this file.
  latheZView = LATHE_Z_H4;
  latheXView = LATHE_X_H4;
  mode = MODE_TURN;
  for (long v = -100000; v <= 100000; v += 37) {
    EQ(latheViewDu(&x, v), v, "H4 view X");
    EQ(latheViewDu(&z, v), v, "H4 view Z");
    EQ(latheTypedDu(&x, v, false), v, "H4 view typed X");
  }
  EQ(latheViewValid(0, 0) && latheViewValid(1, 2) && !latheViewValid(2, 0) && !latheViewValid(0, 3) && !latheViewValid(-1, 0), 1, "valid views");
  printf("latheview: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
