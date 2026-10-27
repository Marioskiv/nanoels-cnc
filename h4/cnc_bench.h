/*
  cnc_bench.h - self-test of the GCODE mode of NanoEls H4: can the controller read and plan a program fast enough?
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)

  The program goes through the interpreter and the planner exactly as when it runs, but every planned block is
  taken out at once instead of being executed (nothing moves). Two numbers come out:
    - how many blocks per second this controller plans (measured: CPU time of the interpreter and planner);
    - how many the program needs: the most blocks that fall in any stretch of CNC_BENCH_WINDOW blocks, at their
      programmed speeds (the planner holds 64 blocks, so a short burst of tiny blocks is absorbed, a long one not).
  If the program needs more than the controller gives, the machine slows down (the step buffer runs empty); the
  position stays exact. No Arduino dependency: the same test runs on a PC (test/cnc_bench_pc.cpp).
*/
#pragma once
#include <stddef.h>
#include <stdint.h>

#define CNC_BENCH_WINDOW 32

typedef struct {
  uint32_t lines;            // program lines read
  uint32_t blocks;           // planner blocks (an arc gives many)
  double programMinutes;     // sum of the block times at their programmed speeds (no acceleration)
  double busyUs;             // time spent in the interpreter and planner (yields not counted)
  double neededPerSecond;    // most blocks per second the program asks for (over CNC_BENCH_WINDOW blocks)
  uint32_t neededAtLine;     // where that is
  uint32_t stopLine;         // 0, or the line where the test stopped (error, threading)
  char stopReason[96];
} cnc_bench_t;

// nowUs: a clock in microseconds; yield: called now and then (the firmware lets other tasks run), not timed.
// The interpreter's position, modes and offsets change: the caller restores them afterwards.
void cnc_bench_run(const char* text, size_t length, cnc_bench_t* result, uint64_t (*nowUs)(), void (*yield)());
