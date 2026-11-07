/*
  cnc_runfrom.h - start a stored program from line N (run-from-line) in the GCODE mode of NanoEls H4.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)

  Not a jump: lines 1 .. N-1 go through the interpreter and the planner exactly as when they run, but every planned
  block is taken out at once (nothing moves), as the self-test does. Afterwards the interpreter has the program's
  modes (G90/G91, G20/G21, G93/G94, plane, work offset and G10 / G92 values, F, S, M3, tool, ...) and its position
  at the start of line N; the caller moves the machine there (above the work first) and goes on reading from line N.
  Refused: line N past the end, M2 / M30 before it, an error before it, threading (G33 / G76, needs the spindle)
  before it. No Arduino dependency: tested on a PC (test/runfrom_test.cpp).
*/
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "cnc_core.h"

typedef struct {
  size_t startPos;                  // where line N starts in the text
  double machine[CNC_N_AXIS];       // machine coordinates (G53) at the start of line N
  double highestZ;                  // the highest machine Z before line N (and where the machine is now)
  uint32_t lines;                   // program lines read before line N (without empty ones)
  uint32_t failLine;                // 0, or the line that refused it
  char reason[96];
} cnc_runfrom_t;

// nowZ: the machine Z now (machine coordinates). yield: called now and then (the firmware lets other tasks run).
// True: ready (the interpreter is at the start of line N). False: r->reason says why; the interpreter's position,
// modes and offsets have changed and the caller restores them.
bool cnc_runfrom_prepare(const char* text, size_t length, uint32_t n, double nowZ, cnc_runfrom_t* r, void (*yield)());

// The moves to the start of line N, in machine coordinates (G53, mm, radius): up to the highest Z (rapid), then X and
// the rotary axis (rapid, above the work), then down to Z at plungeFeed mm/min. names: the axis letters
// (X Z C), rotary: whether the third one is used. Three lines into out[3]; the caller runs them (with the program's
// modes saved before and put back after: they set G90 G94 G21 G8).
void cnc_runfrom_approach(const cnc_runfrom_t* r, const char names[CNC_N_AXIS], bool thirdUsed, double plungeFeed,
    char out[3][96]);
