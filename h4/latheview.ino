// latheview.ino - how the lathe modes show X and Z (NanoEls H4 CNC fork).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
//
// The H4 keeps its own coordinates inside: Z grows towards the chuck, X grows towards the centre (so with X0 on
// the centre line the outside of the part is negative). Only what the display and the web page SHOW changes here,
// chosen in the settings:
//   Z: LATHE_Z_ISO  - as on lathes (ISO): Z0 on the face, negative towards the chuck (default);
//      LATHE_Z_H4   - as the original H4: positive towards the chuck.
//   X: LATHE_X_DIA  - diameter, positive (default);  LATHE_X_RAD - radius, positive;
//      LATHE_X_H4   - as the original H4: the radius from X0, growing towards the centre.
// Nothing moves differently. The H4 keys stay relative (the arrow gives the direction), so only one thing follows
// the view: with X in diameter, a number typed for X (move, stop, "X0 ahead") is a diameter, i.e. half of it as a
// radius - the way a lathe in diameter mode takes it. numpad + A already takes a diameter in every view.
// The GCODE mode is never affected: it shows the G-code coordinates.

// LATHE_Z_* / LATHE_X_* and latheZView / latheXView are declared in h4.ino (the .ino files after it need them first).

// Is this axis shown in the lathe view now?
bool latheViewOn(Axis* a) {
  return mode != MODE_GCODE && (a == &z || a == &x);
}

// A position or a signed offset (deci-microns, H4 frame) as shown.
long latheViewDu(Axis* a, long du) {
  if (!latheViewOn(a)) return du;
  if (a == &z) return latheZView == LATHE_Z_H4 ? du : -du;
  if (latheXView == LATHE_X_DIA) return -2 * du;
  if (latheXView == LATHE_X_RAD) return -du;
  return du;
}

// A length (a distance between two stops) as shown: a diameter view shows X lengths doubled.
long latheViewLengthDu(Axis* a, long du) {
  return latheViewOn(a) && a == &x && latheXView == LATHE_X_DIA ? 2 * du : du;
}

// A number typed on the keypad for this axis, as a distance in the H4 frame. keyIsDiameter: numpad + A.
long latheTypedDu(Axis* a, long du, bool keyIsDiameter) {
  if (keyIsDiameter || !latheViewOn(a) || a != &x || latheXView != LATHE_X_DIA) return du;
  return du / 2;
}

// The name shown before the X value: the custom character Ø for a diameter, else the axis name.
char latheViewName(Axis* a) {
  return latheViewOn(a) && a == &x && latheXView == LATHE_X_DIA ? (char)customCharDiaCode : a->name;
}

bool latheViewValid(int zv, int xv) {
  return (zv == LATHE_Z_ISO || zv == LATHE_Z_H4) && (xv == LATHE_X_DIA || xv == LATHE_X_RAD || xv == LATHE_X_H4);
}
