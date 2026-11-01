// rollback.cpp - a new firmware is accepted by the sketch itself (NanoEls H4 CNC fork, see safety.ino).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
//
// The Arduino core calls verifyRollbackLater() at start (a weak C function in the core). Returning true leaves a
// new firmware "pending" until safetyLoop() marks it valid after it has run long enough; if it restarts before,
// the bootloader starts the previous firmware. In a .cpp file (not an .ino) so that the Arduino builder does not
// add a C++ declaration of its own in front of it.
extern "C" bool verifyRollbackLater() {
  return true;
}
