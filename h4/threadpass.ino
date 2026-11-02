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

// Flank infeed (an option, threadFlank): besides going deeper, each pass moves along the thread by its depth from
// the start times tan 29.5 degrees, in the direction of the cut, so the tool cuts with its leading edge (less chatter,
// a better finish, less wear; the trailing flank is cleaned by the 0.5 degree left). The tool still starts at the
// same Z: the pass starts that much later on the spindle instead (the helix is the same, shifted). Returned in
// encoder steps, to add to the pass's spindle sync. depthDu: depth of this pass from the start; leadDu: travel per
// spindle turn (pitch * starts).
#define THREAD_FLANK_TAN 0.5657727781877702 // tan(29.5 degrees)
long threadFlankSync(long depthDu, long leadDu, long encoderSteps) {
  if (leadDu <= 0 || depthDu <= 0 || encoderSteps <= 0) return 0;
  return lround(double(depthDu) * THREAD_FLANK_TAN / double(leadDu) * double(encoderSteps));
}
