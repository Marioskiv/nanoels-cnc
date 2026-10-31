// zeros.ino - the two zeros of the NanoEls H4 CNC fork: the lathe modes' and the GCODE mode's.
// https://github.com/Marioskiv/nanoels-cnc
//
// The lathe modes use the H4's own zero, touched off on the part with a lathe tool (Z and X keys, numpad + A).
// The GCODE mode (G54) has its own, e.g. the router on the chuck centre for polar work: the tools are not at the
// same place, so a touch-off in one family must never move the other's zero. Both are kept as the H4 keeps its
// zero (originPos: relative to the motor steps, saved in Preferences, no homing needed). originPos is the active
// one, otherOriginPos the other; markAxisOrigin() moves both alike, and they swap when the mode changes between
// a lathe mode and the GCODE mode (setModeFromLoop). Until a GCODE zero was ever stored it equals the lathe one.
//
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)

long latheOriginOf(Axis* a) {
  return cncZeroActive ? a->otherOriginPos : a->originPos;
}

long cncOriginOf(Axis* a) {
  return cncZeroActive ? a->originPos : a->otherOriginPos;
}

// A save to Preferences waits a few seconds after this (as after a key press), so it never sees a half-swapped pair.
void swapZeros(bool cnc) {
  keypadTimeUs = micros();
  Axis* axes[] = {&z, &x, &a1};
  for (Axis* a : axes) {
    long t = a->originPos;
    a->originPos = a->otherOriginPos;
    a->otherOriginPos = t;
  }
  cncZeroActive = cnc;
}
