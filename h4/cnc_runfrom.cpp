/*
  cnc_runfrom.cpp - start a stored program from line N (see cnc_runfrom.h)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)
*/
#include "cnc_runfrom.h"
#include "cnc_bench.h"
#include "cnc_gcode.h"
#include <stdio.h>
#include <string.h>

bool cnc_runfrom_prepare(const char* text, size_t length, uint32_t n, double nowZ, cnc_runfrom_t* r, void (*yield)()) {
  memset(r, 0, sizeof(*r));
  r->highestZ = nowZ;
  char line[256], msg[96];
  float minutes[CNC_BLOCK_BUFFER_SIZE];
  size_t pos = 0;
  uint32_t lineNo = 0;
  if (n < 1) {
    snprintf(r->reason, sizeof(r->reason), "no line number");
    return false;
  }
  while (lineNo + 1 < n) {
    if (pos >= length) {
      snprintf(r->reason, sizeof(r->reason), "the program has only %lu lines", (unsigned long)lineNo);
      return false;
    }
    size_t len;
    pos = cnc_bench_line(text, length, pos, line, sizeof(line), &len);
    lineNo++;
    if (len == 0 || (len == 1 && line[0] == '%')) continue;
    r->lines++;
    while (true) {
      float dwell = 0;
      int res = cnc_gc_line(line, lineNo, msg, sizeof(msg), &dwell);
      cnc_plan_take_all(minutes, CNC_BLOCK_BUFFER_SIZE);  // nothing moves
      double z = cnc_gc_machine_position(CNC_Z);
      if (z > r->highestZ) r->highestZ = z;
      if (res == CNC_WAIT || res == CNC_DWELL_MORE) continue;  // the same line goes on (planner full, cycle)
      if (res == CNC_ERROR) {
        r->failLine = lineNo;
        snprintf(r->reason, sizeof(r->reason), "line %lu: %.70s", (unsigned long)lineNo, msg);
        return false;
      }
      if (res == CNC_THREAD || res == CNC_THREAD_MORE) {
        r->failLine = lineNo;
        snprintf(r->reason, sizeof(r->reason), "threading at line %lu comes before it (start the program from the beginning)", (unsigned long)lineNo);
        return false;
      }
      if (res == CNC_END) {
        r->failLine = lineNo;
        snprintf(r->reason, sizeof(r->reason), "the program ends at line %lu (M2 / M30)", (unsigned long)lineNo);
        return false;
      }
      break;  // done, a pause (M0 / M1 / M6 / M3) or a dwell: not waited for here
    }
    if (yield && (lineNo % 64) == 0) yield();
  }
  if (pos >= length) {
    snprintf(r->reason, sizeof(r->reason), "the program has only %lu lines", (unsigned long)lineNo);
    return false;
  }
  r->startPos = pos;
  for (int i = 0; i < CNC_N_AXIS; i++) r->machine[i] = cnc_gc_machine_position(i);
  return true;
}

void cnc_runfrom_approach(const cnc_runfrom_t* r, const char names[CNC_N_AXIS], bool thirdUsed, double plungeFeed,
    char out[3][96]) {
  snprintf(out[0], 96, "G90 G94 G21 G8 G53 G0 %c%.6f", names[CNC_Z], r->highestZ);
  if (thirdUsed) snprintf(out[1], 96, "G53 G0 %c%.6f %c%.6f", names[CNC_X], r->machine[CNC_X], names[2], r->machine[2]);
  else snprintf(out[1], 96, "G53 G0 %c%.6f", names[CNC_X], r->machine[CNC_X]);
  snprintf(out[2], 96, "G53 G1 %c%.6f F%.1f", names[CNC_Z], r->machine[CNC_Z], plungeFeed);
}
