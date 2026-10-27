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
static size_t benchLine(const char* text, size_t length, size_t pos, char* out, size_t outSize, size_t* outLen) {
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
  float minutes[CNC_BLOCK_BUFFER_SIZE];
  char line[256], msg[96];
  size_t pos = 0;
  uint32_t lineNo = 0;
  uint64_t busyStart = nowUs();

  // Takes the planned blocks out and adds their times to the running window.
  auto drain = [&](uint32_t atLine) {
    int n = cnc_plan_take_all(minutes, CNC_BLOCK_BUFFER_SIZE);
    if (n > CNC_BLOCK_BUFFER_SIZE) n = CNC_BLOCK_BUFFER_SIZE;
    for (int i = 0; i < n; i++) {
      float m = minutes[i];
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
    pos = benchLine(text, length, pos, line, sizeof(line), &n);
    lineNo++;
    if (n == 0 || (n == 1 && line[0] == '%')) continue;
    r->lines++;
    bool stop = false;
    while (true) {
      float dwell = 0;
      int res = cnc_gc_line(line, lineNo, msg, sizeof(msg), &dwell);
      drain(lineNo);
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
  r->busyUs += (double)(nowUs() - busyStart);
}
