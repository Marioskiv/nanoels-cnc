// Lathe screen vs the H4: runs the H4 lathe-mode functions (extracted unchanged from h4.ino by extract.sh) on the PC
// and prints the tool path. Used by compare.py, which checks it against the Lathe page drawing (plans.js).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <vector>
#include <string>
struct Axis { long pos=0, leftStop=LONG_MAX, rightStop=LONG_MIN, speedMax=0, speedManualMove=1, motorSteps=1000, screwPitch=10000, posGlobal=0, originPos=0; bool movingManually=false; };
Axis z, x;
float ENCODER_STEPS_FLOAT = 2400; const long ENCODER_STEPS_INT = 2400;
const long SAFE_DISTANCE_DU = 5000;
#define HALF_PI 1.5707963267948966
long dupr = 0, opDupr = 0; int starts = 1, turnPasses = 1, opDuprSign = 1; float coneRatio = 0; bool auxForward = true;
long opIndex = 0, opSubIndex = 0; bool opIndexAdvanceFlag = false; bool isOn = false;
long spindlePos = 0, spindlePosAvg = 0, spindlePosGlobal = 0; int spindlePosSync = 0;
void beep() {}
void setIsOnFromLoop(bool on) { isOn = on; }
void stepToContinuous(Axis* a, long p) { a->pos = p; a->posGlobal = p + a->originPos; }
// a manual-speed move: at most 20 um per loop, so the order of the moves shows as on the machine
void stepToFinal(Axis* a, long p) { long d = p - a->pos; if (d > 20) d = 20; if (d < -20) d = -20; stepToContinuous(a, a->pos + d); }
void markAxisOrigin(Axis* a) { if (a->leftStop != LONG_MAX) a->leftStop -= a->pos; if (a->rightStop != LONG_MIN) a->rightStop -= a->pos; a->originPos += a->pos; a->pos = 0; }
void markOrigin() { markAxisOrigin(&z); markAxisOrigin(&x); spindlePos = spindlePosAvg = 0; spindlePosSync = 0; }
#define MODE_THREAD 7
int mode = 0;
long auxSafeDistance, startOffset;
#include "modes.inc" // made by extract.sh
long toPos(double mm) { return lround(mm * 1000); }
double disp(Axis* a) { return (a->pos + a->originPos) / 1000.0; }
int main(int argc, char** argv) {
  // mode pitch starts passes cone aux zl zr xl xr startZ startX   (stops: "-" = none; display mm, H4 frame)
  if (argc < 13) { fprintf(stderr, "args\n"); return 2; }
  mode = atoi(argv[1]); dupr = lround(atof(argv[2]) * 10000); starts = atoi(argv[3]); turnPasses = atoi(argv[4]);
  coneRatio = atof(argv[5]); auxForward = atoi(argv[6]);
  auto st = [](const char* s, long def) { return std::string(s) == "-" ? def : toPos(atof(s)); };
  z.leftStop = st(argv[7], LONG_MAX); z.rightStop = st(argv[8], LONG_MIN); x.leftStop = st(argv[9], LONG_MAX); x.rightStop = st(argv[10], LONG_MIN);
  z.pos = toPos(atof(argv[11])); x.pos = toPos(atof(argv[12]));
  // ON (setIsOnFromLoop(true)): markOrigin, then the operation's counters
  markOrigin(); isOn = true; opDuprSign = dupr >= 0 ? 1 : -1; opDupr = dupr; opIndex = 0; opIndexAdvanceFlag = false; opSubIndex = 0;
  printf("[");
  double lz = 1e9, lx = 1e9; bool first = true;
  for (long it = 0; it < 20000000 && isOn; it++) {
    spindlePos += 4; spindlePosAvg += 4; spindlePosGlobal += 4;
    if (spindlePosSync != 0) { spindlePosSync = 0; continue; }
    if (mode == 4 || mode == 7) modeTurn(&z, &x);
    else if (mode == 5) modeTurn(&x, &z);
    else if (mode == 6) modeCut();
    else if (mode == 3) { modeCone(); if (it > 4000000) isOn = false; }
    else if (mode == 8) modeEllipse(&z, &x);
    double dz = disp(&z), dx = disp(&x);
    if (fabs(dz - lz) > 1e-9 || fabs(dx - lx) > 1e-9) { printf("%s[%.3f,%.3f,%ld,%ld]", first ? "" : ",", dz, dx, opIndex, opSubIndex); first = false; lz = dz; lx = dx; }
  }
  printf("]\n");
  return 0;
}
