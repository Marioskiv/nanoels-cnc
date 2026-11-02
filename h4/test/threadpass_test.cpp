// Tests of threadpass.ino (NanoEls H4 CNC fork): the depth of each thread pass.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <initializer_list>
#include "../threadpass.ino"

int fails = 0, checks = 0;
void check(bool ok, const char* what, long a, long b, long c) {
  checks++;
  if (!ok) { fails++; printf("FAIL %s (%ld %ld %ld)\n", what, a, b, c); }
}

int main() {
  for (long start : {0L, -5000L, 7000L}) {
    for (long depth : {1L, 920L, -812L, 12345L, -100000L}) {
      long end = start + depth;
      for (long n = 1; n <= 60; n++) {
        long prev = start, prevStep = labs(depth) + 1;
        double prevArea = 0;
        for (long k = 1; k <= n; k++) {
          long p = threadPassPos(start, end, k, n);
          // always between the start and the full depth, never back out, the last pass at the full depth
          check((p - start) * (depth > 0 ? 1 : -1) >= 0 && labs(p - start) <= labs(depth), "within", k, n, p);
          check((p - prev) * (depth > 0 ? 1 : -1) >= 0, "never backwards", k, n, p);
          if (k == n) check(p == end, "last pass at the full depth", k, n, p);
          // the depth per pass does not grow (rounding: 1 step)
          long step = labs(p - prev);
          check(step <= prevStep + 1, "smaller and smaller steps", k, n, step);
          // the same material each pass: area of a V of depth h grows with h^2
          double area = double(p - start) * double(p - start);
          if (labs(depth) >= 1000 && k > 1) check(fabs((area - prevArea) - double(depth) * depth / n) <= 2.0 * labs(depth) + 4, "same material", k, n, p);
          prevStep = step; prev = p; prevArea = area;
        }
      }
    }
  }
  check(threadPassPos(0, 1000, 0, 10) == 0, "pass 0", 0, 0, 0);
  // flank infeed: depth * tan(29.5 deg) along the thread, as spindle encoder steps of one lead
  check(threadFlankSync(0, 15000, 2400) == 0 && threadFlankSync(500, 0, 2400) == 0 && threadFlankSync(-5, 15000, 2400) == 0, "flank: nothing", 0, 0, 0);
  check(threadFlankSync(15000, 15000, 2400) == lround(0.5657727781877702 * 2400), "flank: depth = lead", threadFlankSync(15000, 15000, 2400), 0, 0);
  for (long lead : {5000L, 15000L, 25400L, 40000L})
    for (long d = 0; d <= 20000; d += 37) {
      long s = threadFlankSync(d, lead, 2400);
      double mm = s / 2400.0 * lead; // back to du along the thread
      check(fabs(mm - d * 0.5657727781877702) <= lead / 2400.0, "flank: shift = depth * tan 29.5", d, lead, s);
    }
  check(threadPassPos(0, 1000, 1, 4) == 500, "first of 4: half the depth", 0, 0, threadPassPos(0, 1000, 1, 4));
  check(threadPassPos(0, 1000, 5, 0) == 1000, "no passes", 0, 0, 0);
  printf("threadpass: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
