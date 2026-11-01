// threadpass.ino - how deep each pass of a thread goes (NanoEls H4 CNC fork).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
//
// A threading tool cuts with its whole flank, so the material taken by a pass grows with the depth: with the same
// depth every pass the last passes cut far more than the first ones. Here the depth after pass k of n is
// total * sqrt(k / n): every pass takes about the same amount of material, the depth per pass gets smaller
// towards the end (the usual "constant area" infeed). The last pass ends exactly at the full depth.

// Position of the cross slide for pass k (1..n) between the start (outside) and the end (full depth).
long threadPassPos(long start, long end, long k, long n) {
  if (n <= 0 || k >= n) return end;
  if (k <= 0) return start;
  return start + lround((end - start) * sqrt(double(k) / double(n)));
}
