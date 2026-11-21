/*
  cnc_bench.cpp - self-test of the GCODE mode of NanoEls H4 (see cnc_bench.h)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)
*/
#include "cnc_bench.h"
#include "cnc_core.h"
#include "cnc_gcode.h"
#include <string.h>
#include <stdio.h>

// A line as the firmware reads a stored program: ( ) and ; comments removed, blanks around trimmed.
size_t cnc_bench_line(const char* text, size_t length, size_t pos, char* out, size_t outSize, size_t* outLen) {
  size_t n = 0;
  bool paren = false, semicolon = false;
  while (pos < length && text[pos] != '\n') {
    char c = text[pos++];
    if (paren) {
      if (c == ')') paren = false;
    } else if (c == '(') {
      paren = true;
    } else if (c == ';') {
      semicolon = true;
    } else if (!semicolon && c != '\r' && n + 1 < outSize) {
      out[n++] = c;
    }
  }
  if (pos < length) pos++;  // the '\n'
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
  size_t start = 0;
  while (start < n && (out[start] == ' ' || out[start] == '\t')) start++;
  if (start > 0) memmove(out, out + start, n - start);
  n -= start;
  out[n] = '\0';
  *outLen = n;
  return pos;
}

void cnc_bench_run(const char* text, size_t length, cnc_bench_t* r, uint64_t (*nowUs)(), void (*yield)()) {
  memset(r, 0, sizeof(*r));
  float window[CNC_BENCH_WINDOW];
  int windowCount = 0, windowNext = 0;
  double windowSum = 0;
  cnc_taken_t taken[CNC_BLOCK_BUFFER_SIZE];
  int32_t tool[CNC_N_AXIS];
  for (int i = 0; i < CNC_N_AXIS; i++) r->minSteps[i] = r->maxSteps[i] = tool[i] = cnc_plan_position()[i];
  char line[256], msg[96];
  size_t pos = 0;
  uint32_t lineNo = 0;
  uint64_t busyStart = nowUs();

  // Takes planned blocks out (all = the motion stops here: M0, G4, the end; otherwise the newest stay for the
  // look-ahead, as when the program runs) and adds their times to the estimate and the running window.
  auto drain = [&](uint32_t atLine, bool all) {
    int keep = all ? 0 : CNC_BLOCK_BUFFER_SIZE - 8;
    int n;
    while ((n = cnc_plan_take_timed(taken, CNC_BLOCK_BUFFER_SIZE, keep)) > 0)
    for (int i = 0; i < n; i++) {
      float m = taken[i].nominalMinutes;
      r->minutes += taken[i].minutes;
      for (int k = 0; k < CNC_N_AXIS; k++) {
        if (taken[i].condition & CNC_COND_BACKLASH) break;  // the motor only: the tool stays
        tool[k] += taken[i].delta[k];
        if (tool[k] < r->minSteps[k]) r->minSteps[k] = tool[k];
        if (tool[k] > r->maxSteps[k]) r->maxSteps[k] = tool[k];
      }
      r->blocks++;
      r->programMinutes += m;
      if (windowCount == CNC_BENCH_WINDOW) windowSum -= window[windowNext];
      else windowCount++;
      window[windowNext] = m;
      windowSum += m;
      windowNext = (windowNext + 1) % CNC_BENCH_WINDOW;
      if (windowCount == CNC_BENCH_WINDOW && windowSum > 0) {
        double perSecond = CNC_BENCH_WINDOW / (windowSum * 60);
        if (perSecond > r->neededPerSecond) {
          r->neededPerSecond = perSecond;
          r->neededAtLine = atLine;
        }
      }
    }
  };

  while (pos < length) {
    size_t n;
    pos = cnc_bench_line(text, length, pos, line, sizeof(line), &n);
    lineNo++;
    if (n == 0 || (n == 1 && line[0] == '%')) continue;
    r->lines++;
    bool stop = false;
    while (true) {
      float dwell = 0;
      msg[0] = '\0';
      int res = cnc_gc_line(line, lineNo, msg, sizeof(msg), &dwell);
      // the motion stops before a dwell, a pause for the operator, threading and the end
      bool stops = res == CNC_DWELL || res == CNC_DWELL_MORE || res == CNC_PAUSE || res == CNC_END || res == CNC_THREAD ||
                   res == CNC_THREAD_MORE;
      drain(lineNo, stops);
      if ((res == CNC_DWELL || res == CNC_DWELL_MORE) && dwell > 0) {
        r->dwellSeconds += dwell;
        r->minutes += dwell / 60.0;
      }
      if (res == CNC_PAUSE) {
        if (r->waits++ == 0) {
          r->waitLine = lineNo;
          snprintf(r->waitReason, sizeof(r->waitReason), "%s", msg[0] ? msg : "M0 / M1: program stop");
        }
      } else if (res != CNC_ERROR && msg[0]) {
        bool g43 = strstr(msg, "G43 H") != NULL;
        if (r->notes++ == 0 || (g43 && strstr(r->note, "G43 H") == NULL)) {
          r->noteLine = lineNo;
          snprintf(r->note, sizeof(r->note), "%s", msg);
        }
      }
      if (res == CNC_WAIT || res == CNC_DWELL_MORE) continue;  // the same line goes on (planner full, cycle)
      if (res == CNC_ERROR) {
        r->stopLine = lineNo;
        snprintf(r->stopReason, sizeof(r->stopReason), "%s", msg);
        stop = true;
      } else if (res == CNC_THREAD || res == CNC_THREAD_MORE) {
        r->stopLine = lineNo;
        snprintf(r->stopReason, sizeof(r->stopReason), "threading (G33 / G76) is not part of the test");
        stop = true;
      } else if (res == CNC_END) {
        stop = true;
      }
      break;
    }
    if (stop) break;
    if (yield && (lineNo % 64) == 0) {
      r->busyUs += (double)(nowUs() - busyStart);
      yield();
      busyStart = nowUs();
    }
  }
  drain(lineNo, true);  // the program's last moves (a program without M2 / M30 ends at its last line)
  r->busyUs += (double)(nowUs() - busyStart);
}
