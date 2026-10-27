/*
  cnc_gcode.cpp - G-code interpreter for the GCODE mode of NanoEls H4 (X, Z and C)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)

  Reads RS274/NGC the way LinuxCNC does (Grbl reads a subset of the same language), for a lathe with
  X (cross slide), Z (bed) and C (A1, rotary). Written for this fork; checked against LinuxCNC's own
  interpreter (rs274) by h4/test/lcnc_compare.py. Every line is checked completely before anything of it
  is executed; what cannot be executed exactly is refused with the reason, never guessed.

  Modal groups (two codes of one group on a line is an error, as in LinuxCNC):
    motion      G0 G1 G2 G3 G33 (G32 as on the NanoEls H5), G80
    plane       G17 G18 G19 (arcs: G18 only, the plane of X and Z; G17 / G19 need a Y axis)
    distance    G90 G91          arc distance G90.1 G91.1
    feed        G93 G94 G95 (G95: feed per spindle turn, speed measured by the encoder)
    units       G20 G21          diameter G7 G8 (G7: X words are diameters)
    cutter comp G40 (G41 G42 refused: compensate in the CAM)
    tool length G43 H (noted), G43.1 Z, G49
    coordinates G54 G55 G56 G57 G58 G59 G59.1 G59.2 G59.3
    path        G61 G61.1 G64 [P Q]      return G98 G99      spindle G97 (G96 refused)
    non-modal   G4 P  G10 L2 / L20  G28 G28.1 G30 G30.1  G53  G92 G92.1 G92.2 G92.3
    M           M0 M1 M2 M30 M60, M3 M4 M5, M6 (pause for a manual tool change), M61, M7 M8 M9, M48-M53
  Words: N F S T P Q R I K L H D, axes X Z C. Comments are removed before a line arrives here.
  Refused with a reason: Y A B U V W axes, canned cycles (G7x G8x), G38 probing, parameters, expressions
  and O-word subroutines.
*/
#include "cnc_gcode.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

static cnc_machine_t machine;
static cnc_offsets_t offs;
static uint32_t offsChanges = 0;
static double absPos[CNC_N_AXIS];    // tool position, mm/deg, in the planner's coordinates (step 0 = 0)
static double mOff[CNC_N_AXIS];      // machine coordinate of step 0 (homing), mm/deg
static bool lastNeg[CNC_N_AXIS];     // last direction of each motor, for the backlash take-up

// modes
static int motionMode;               // G code * 10 of the motion group, 800 = G80 (no motion mode)
static int plane;                    // 170, 180, 190
static bool incremental, arcIncremental = true;
static int feedMode;                 // 930, 940, 950
static bool inches, diameterMode;
static double feedWord;              // last F: mm (or deg) per minute, or per spindle turn in G95
static double tloZ;                  // G43.1 tool length offset on Z, mm

// a thread move handed to the firmware
static int32_t threadTarget[CNC_N_AXIS];
static float threadLead;

// an arc whose segments do not all fit in the planner at once: continued on the next call
static struct {
  bool active;
  uint32_t lineNo;
  int segments, done;
  double c1, c2, r, a0, da;          // centre (Z, X), radius, start angle, angle per segment
  double start[CNC_N_AXIS], end[CNC_N_AXIS];
  float feed;
  uint8_t condition;
  int result;                        // what the line returns once the arc is planned
  float dwell;
  char msg[96];
} arc;

static const int AX_X = CNC_X, AX_Z = CNC_Z, AX_C = CNC_C;
static const char* AXIS_LETTERS = "XZC";

static int fail(char* msg, int msgLen, const char* text) {
  snprintf(msg, msgLen, "%s", text);
  return CNC_ERROR;
}

static double spu(int i) { return cnc_core_get_settings()->stepsPerUnit[i]; }

// Program origin of an axis in planner coordinates.
static double origin(int i) {
  double o = offs.coord[offs.active][i] - mOff[i];
  if (offs.g92Applied) o += offs.g92[i];
  if (i == AX_Z) o += tloZ;
  return o;
}

void cnc_gc_init(const cnc_machine_t* m) {
  machine = *m;
  cnc_gc_reset_modes();
}

void cnc_gc_set_machine(const cnc_machine_t* m) {
  machine = *m;
}

void cnc_gc_reset_modes() {
  // G1 at the start, not G0: lathecode programs (and the old H4 GCODE mode) move with "X.. Z.." lines
  // without any G word, always at the feed rate.
  motionMode = 10;
  plane = 180;
  incremental = false;
  arcIncremental = true;
  feedMode = 940;
  inches = false;
  diameterMode = false;
  feedWord = 0;  // as LinuxCNC and Grbl: a feed move before any F is refused (no hidden default feed)
  tloZ = 0;
  offs.active = 0;
  arc.active = false;
}

void cnc_gc_sync(const int32_t pos[CNC_N_AXIS], const int32_t machineOffset[CNC_N_AXIS], const bool neg[CNC_N_AXIS]) {
  for (int i = 0; i < CNC_N_AXIS; i++) {
    lastNeg[i] = neg[i];
    absPos[i] = pos[i] / spu(i);
    mOff[i] = machineOffset[i] / spu(i);
  }
  arc.active = false;
  cnc_plan_sync_position(pos);
}

bool cnc_gc_inverse_time() { return feedMode == 930; }
bool cnc_gc_last_neg(int axis) { return lastNeg[axis]; }
void cnc_gc_thread(int32_t target[CNC_N_AXIS], float* leadMm) {
  for (int i = 0; i < CNC_N_AXIS; i++) target[i] = threadTarget[i];
  *leadMm = threadLead;
}
void cnc_gc_get_offsets(cnc_offsets_t* o) { *o = offs; }
void cnc_gc_set_offsets(const cnc_offsets_t* o) { offs = *o; }
uint32_t cnc_gc_offsets_changes() { return offsChanges; }
double cnc_gc_work_position(int axis) { return absPos[axis] - origin(axis); }

static int32_t toSteps(int i, double v) { return (int32_t)llround(v * spu(i)); }

static bool limitOk(int i, int32_t t) {
  return !machine.checkLimits || (t >= machine.minSteps[i] && t <= machine.maxSteps[i]);
}

static bool roomForOne() { return cnc_plan_blocks_free() >= 1 + CNC_N_AXIS; }

// One straight move to an absolute target (planner coordinates). Room must have been checked.
static void planLine(const double target[CNC_N_AXIS], float feed, uint8_t condition, uint32_t lineNo) {
  int32_t t[CNC_N_AXIS];
  const int32_t* pl = cnc_plan_position();
  for (int i = 0; i < CNC_N_AXIS; i++) {
    t[i] = toSteps(i, target[i]);
    int32_t d = t[i] - pl[i];
    if (d != 0 && machine.backlashSteps[i] > 0) {
      bool neg = d < 0;
      if (neg != lastNeg[i]) cnc_plan_buffer_backlash(i, machine.backlashSteps[i], neg, lineNo);
    }
    if (d != 0) lastNeg[i] = d < 0;
  }
  cnc_plan_buffer_line(t, feed, condition, lineNo);  // false only for a zero-length move
  for (int i = 0; i < CNC_N_AXIS; i++) absPos[i] = target[i];
}

// Plans the next segments of the pending arc. True when the whole arc is in the planner.
static bool arcContinue() {
  while (arc.done < arc.segments) {
    if (!roomForOne()) return false;
    arc.done++;
    double t[CNC_N_AXIS];
    if (arc.done == arc.segments) {
      for (int i = 0; i < CNC_N_AXIS; i++) t[i] = arc.end[i];
    } else {
      double a = arc.a0 + arc.da * arc.done;
      double f = (double)arc.done / arc.segments;
      t[AX_Z] = arc.c1 + arc.r * cos(a);
      t[AX_X] = arc.c2 + arc.r * sin(a);
      t[AX_C] = arc.start[AX_C] + (arc.end[AX_C] - arc.start[AX_C]) * f;
    }
    planLine(t, arc.feed, arc.condition, arc.lineNo);
  }
  arc.active = false;
  return true;
}

// The arc rules of LinuxCNC (interp_arc.cc): centre from I K (offset or absolute) or from R, its
// tolerances, P = number of turns. In G18 the first coordinate is Z and the second X; G3 turns
// counter-clockwise in that (Z, X) frame.
static int arcCentre(bool cw, bool ikIncremental, double s1, double s2, double e1, double e2, bool hasR, double r,
                     double i1, double i2, double* c1, double* c2, double* radius, char* msg, int msgLen) {
  const double radiusTol = 0.00005 * 25.4;
  const double spiralAbs = inches ? 2 * 0.001 * M_SQRT2 * 25.4 : 2 * 0.01 * M_SQRT2;
  if (hasR) {
    if (s1 == e1 && s2 == e2) return fail(msg, msgLen, "R arc: start and end are the same point");
    double absR = fabs(r);
    double m1 = (s1 + e1) / 2, m2 = (s2 + e2) / 2;
    double half = hypot(m1 - e1, m2 - e2);
    if (half - absR > radiusTol) return fail(msg, msgLen, "arc radius too small to reach the end point");
    if (half / absR > 1 - 1e-12) half = absR;
    double theta = atan2(e2 - s2, e1 - s1) + (((cw && r > 0) || (!cw && r < 0)) ? -M_PI / 2 : M_PI / 2);
    double off = absR * cos(asin(half / absR));
    *c1 = m1 + off * cos(theta);
    *c2 = m2 + off * sin(theta);
    *radius = absR;
    return CNC_OK;
  }
  *c1 = ikIncremental ? s1 + i1 : i1;
  *c2 = ikIncremental ? s2 + i2 : i2;
  double r1 = hypot(*c1 - s1, *c2 - s2), r2 = hypot(*c1 - e1, *c2 - e2);
  if (r1 < radiusTol || r2 < radiusTol) return fail(msg, msgLen, "zero-radius arc");
  double absErr = fabs(r1 - r2), relErr = absErr / (r1 > r2 ? r1 : r2);
  if (absErr > spiralAbs * 100.0 || (relErr > 0.001 && absErr > spiralAbs)) {
    snprintf(msg, msgLen, "radius to end of arc differs from radius to start (%.4f / %.4f mm)", r1, r2);
    return CNC_ERROR;
  }
  *radius = r1;
  return CNC_OK;
}

int cnc_gc_line(const char* line, uint32_t lineNo, char* msg, int msgLen, float* dwell) {
  msg[0] = '\0';
  // An arc of this line still being planned: go on with it.
  if (arc.active) {
    if (arc.lineNo != lineNo) {
      arc.active = false;  // a different line: the arc was given up (stop)
    } else {
      if (!arcContinue()) return CNC_WAIT;
      snprintf(msg, msgLen, "%s", arc.msg);
      *dwell = arc.dwell;
      return arc.result;
    }
  }

  // ---------------------------------------------------------------- read the words
  bool has[26];
  double val[26];
  memset(has, 0, sizeof(has));
  int gCodes[12], nG = 0, mCodes[6], nM = 0;
  const char* p = line;
  while (*p == ' ' || *p == '\t') p++;
  if (*p == '/') p++;  // block delete is off: the line runs (as LinuxCNC by default)
  while (*p) {
    char c = (char)toupper((unsigned char)*p);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { p++; continue; }
    if (c == '#' || c == '[') return fail(msg, msgLen, "parameters and expressions are not supported");
    if (c < 'A' || c > 'Z') {
      snprintf(msg, msgLen, "unexpected character '%c'", *p);
      return CNC_ERROR;
    }
    if (c == 'O') return fail(msg, msgLen, "O-word subroutines and loops are not supported");
    p++;
    char num[24];
    int k = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '[') return fail(msg, msgLen, "parameters and expressions are not supported");
    while ((*p == '-' || *p == '+' || *p == '.' || isdigit((unsigned char)*p)) && k < 23) {
      num[k++] = *p;
      p++;
      while (*p == ' ' || *p == '\t') p++;
    }
    num[k] = '\0';
    if (k == 0) {
      snprintf(msg, msgLen, "letter %c without a number", c);
      return CNC_ERROR;
    }
    char* end;
    double v = strtod(num, &end);
    if (*end != '\0') {
      snprintf(msg, msgLen, "bad number after %c: %s", c, num);
      return CNC_ERROR;
    }
    if (c == 'G') {
      int code10 = (int)lround(v * 10);
      if (fabs(v * 10 - code10) > 1e-6) return fail(msg, msgLen, "bad G code");
      if (nG >= 12) return fail(msg, msgLen, "too many G codes on one line");
      gCodes[nG++] = code10;
    } else if (c == 'M') {
      int code = (int)lround(v);
      if (fabs(v - code) > 1e-6) return fail(msg, msgLen, "bad M code");
      if (nM >= 6) return fail(msg, msgLen, "too many M codes on one line");
      mCodes[nM++] = code;
    } else {
      if (has[c - 'A']) {
        snprintf(msg, msgLen, "two %c words on one line", c);
        return CNC_ERROR;
      }
      has[c - 'A'] = true;
      val[c - 'A'] = v;
    }
  }
#define HAS(L) has[(L) - 'A']
#define VAL(L) val[(L) - 'A']
  for (const char* bad = "YABUVWEJ"; *bad; bad++) {
    if (!HAS(*bad)) continue;
    if (*bad == 'J') return fail(msg, msgLen, "J word: arcs are in the X-Z plane (G18), use I and K");
    if (*bad == 'E') return fail(msg, msgLen, "E word is not supported");
    snprintf(msg, msgLen, "this machine has no %c axis", *bad);
    return CNC_ERROR;
  }

  // ---------------------------------------------------------------- sort the codes into groups
  enum { GR_MOTION, GR_PLANE, GR_DIST, GR_ARCDIST, GR_FEED, GR_UNITS, GR_COMP, GR_TOOL, GR_RETRACT, GR_COORD,
         GR_PATH, GR_SPINDLE, GR_DIAM, GR_NONMODAL, GR_COUNT };
  int g[GR_COUNT];
  for (int i = 0; i < GR_COUNT; i++) g[i] = -1;
  for (int n = 0; n < nG; n++) {
    int c = gCodes[n], grp;
    switch (c) {
      case 0: case 10: case 20: case 30: case 320: case 330: case 800: grp = GR_MOTION; break;
      case 170: case 180: case 190: grp = GR_PLANE; break;
      case 900: case 910: grp = GR_DIST; break;
      case 901: case 911: grp = GR_ARCDIST; break;
      case 930: case 940: case 950: grp = GR_FEED; break;
      case 200: case 210: grp = GR_UNITS; break;
      case 400: grp = GR_COMP; break;
      case 430: case 431: case 490: grp = GR_TOOL; break;
      case 980: case 990: grp = GR_RETRACT; break;
      case 540: case 550: case 560: case 570: case 580: case 590: case 591: case 592: case 593: grp = GR_COORD; break;
      case 610: case 611: case 640: grp = GR_PATH; break;
      case 970: grp = GR_SPINDLE; break;
      case 70: case 80: grp = GR_DIAM; break;
      case 40: case 100: case 280: case 281: case 300: case 301: case 530: case 920: case 921: case 922: case 923:
        grp = GR_NONMODAL;
        break;
      case 410: case 420: case 411: case 421:
        return fail(msg, msgLen, "cutter compensation (G41/G42) is not supported: compensate in the CAM (in computer)");
      case 960: return fail(msg, msgLen, "G96 constant surface speed needs a spindle the H4 controls: use G97");
      case 700: case 710: case 720: case 730: case 740: case 760:
      case 810: case 820: case 830: case 840: case 850: case 860: case 870: case 880: case 890:
        snprintf(msg, msgLen, "canned cycle G%g is not supported yet: post the program with the cycles expanded", c / 10.0);
        return CNC_ERROR;
      case 382: case 383: case 384: case 385: return fail(msg, msgLen, "probing (G38) is not supported");
      case 331: return fail(msg, msgLen, "rigid tapping (G33.1) is not supported");
      default:
        snprintf(msg, msgLen, "G%g is not supported", c / 10.0);
        return CNC_ERROR;
    }
    if (g[grp] != -1) {
      snprintf(msg, msgLen, "G%g and G%g are in the same modal group", g[grp] / 10.0, c / 10.0);
      return CNC_ERROR;
    }
    g[grp] = c;
  }
  int mStop = -1, mSpindle = -1, mCool = -1, mTool = -1, mOverride = -1;
  for (int n = 0; n < nM; n++) {
    int c = mCodes[n];
    int* slot;
    switch (c) {
      case 0: case 1: case 2: case 30: case 60: slot = &mStop; break;
      case 3: case 4: case 5: slot = &mSpindle; break;
      case 7: case 8: case 9: slot = &mCool; break;
      case 6: case 61: slot = &mTool; break;
      case 48: case 49: case 50: case 51: case 52: case 53: slot = &mOverride; break;
      default:
        snprintf(msg, msgLen, "M%d is not supported", c);
        return CNC_ERROR;
    }
    if (*slot != -1 && !(slot == &mCool && *slot != 9 && c != 9)) {
      snprintf(msg, msgLen, "M%d and M%d are in the same modal group", *slot, c);
      return CNC_ERROR;
    }
    *slot = c;
  }
  (void)mSpindle;
  (void)mOverride;

  // ---------------------------------------------------------------- the modes this line asks for
  int nPlane = g[GR_PLANE] >= 0 ? g[GR_PLANE] : plane;
  bool nIncr = g[GR_DIST] >= 0 ? g[GR_DIST] == 910 : incremental;
  bool nArcIncr = g[GR_ARCDIST] >= 0 ? g[GR_ARCDIST] == 911 : arcIncremental;
  int nFeedMode = g[GR_FEED] >= 0 ? g[GR_FEED] : feedMode;
  bool nInches = g[GR_UNITS] >= 0 ? g[GR_UNITS] == 200 : inches;
  bool nDiam = g[GR_DIAM] >= 0 ? g[GR_DIAM] == 70 : diameterMode;
  int nCoord = offs.active;
  if (g[GR_COORD] >= 0) {
    int c = g[GR_COORD];
    nCoord = c < 591 ? (c - 540) / 10 : 6 + (c - 591);
  }
  double lin = nInches ? 25.4 : 1.0;  // linear words to mm

  for (int i = 0; i < CNC_N_AXIS; i++) {
    if (HAS(AXIS_LETTERS[i]) && !machine.active[i]) {
      snprintf(msg, msgLen, "axis %c is not connected", AXIS_LETTERS[i]);
      return CNC_ERROR;
    }
  }
  bool anyAxis = HAS('X') || HAS('Z') || HAS('C');
  int nonModal = g[GR_NONMODAL];
  bool g53 = nonModal == 530;
  bool axesForNonModal = nonModal == 100 || nonModal == 280 || nonModal == 300 || nonModal == 920;
  int explicitMotion = g[GR_MOTION];
  if (axesForNonModal && explicitMotion >= 0 && explicitMotion != 800) {
    return fail(msg, msgLen, "axis words used by G10/G28/G30/G92 and by a motion on the same line");
  }
  if (g[GR_TOOL] == 431) {
    if (!HAS('Z') || HAS('X') || HAS('C')) return fail(msg, msgLen, "G43.1 needs a Z word only");
  }
  int motion = explicitMotion >= 0 ? explicitMotion : motionMode;
  bool thread = motion == 320 || motion == 330;
  bool moving = anyAxis && !axesForNonModal && g[GR_TOOL] != 431;
  if (explicitMotion == 800 && moving) return fail(msg, msgLen, "axis words with G80 (no motion mode)");
  if (moving && motion == 800) return fail(msg, msgLen, "axis words without a motion mode (G0, G1, G2, G3)");
  if (g53 && !(motion == 0 || motion == 10)) return fail(msg, msgLen, "G53 only with G0 or G1");
  if (g53 && nIncr) return fail(msg, msgLen, "G53 with G91");
  bool isArc = moving && (motion == 20 || motion == 30);
  if (isArc && nPlane != 180) return fail(msg, msgLen, "arcs only in the X-Z plane (G18): this machine has no Y axis");
  if (isArc && !HAS('R') && !HAS('I') && !HAS('K')) return fail(msg, msgLen, "arc without I/K or R");
  if (isArc && HAS('R') && (HAS('I') || HAS('K'))) return fail(msg, msgLen, "arc with both R and I/K");
  bool dwellCode = nonModal == 40;
  if (dwellCode && !HAS('P')) return fail(msg, msgLen, "G4 needs P (seconds)");
  if (dwellCode && VAL('P') < 0) return fail(msg, msgLen, "G4 P must not be negative");
  if (dwellCode && moving) return fail(msg, msgLen, "G4 and a move on one line");
  if (mTool == 6 && moving) return fail(msg, msgLen, "M6 and a move on one line");
  if (HAS('F') && VAL('F') < 0) return fail(msg, msgLen, "F must not be negative");
  if (HAS('F') && VAL('F') == 0 && !thread && !(nFeedMode == 930 && !moving)) return fail(msg, msgLen, "F must be positive");
  if ((HAS('I') || HAS('R')) && !isArc) return fail(msg, msgLen, "I or R without an arc");
  if (HAS('K') && !isArc && !(thread && motion == 330)) return fail(msg, msgLen, "K without an arc or G33");
  if (HAS('L') && nonModal != 100) return fail(msg, msgLen, "L word without G10");
  if (HAS('P') && !dwellCode && nonModal != 100 && !isArc && g[GR_PATH] != 640) return fail(msg, msgLen, "P word without G4, G10, G64 or an arc");

  // F: per minute, inverse time or per turn
  double nFeedWord = feedWord;
  if (HAS('F') && !thread) {
    nFeedWord = VAL('F') * (nFeedMode == 930 ? 1.0 : lin);
  } else if (g[GR_FEED] == 940 || g[GR_FEED] == 950) {
    nFeedWord = 0;  // as LinuxCNC: G94 / G95 clear the feed rate, the next feed move must state its F
  }

  // ---------------------------------------------------------------- tentative state: restored on refusal
  cnc_offsets_t saveOffs = offs;
  double saveTlo = tloZ;
  offs.active = nCoord;
  if (g[GR_TOOL] == 490) tloZ = 0;
  if (g[GR_TOOL] == 431) tloZ = VAL('Z') * lin;
#define REFUSE(text) do { offs = saveOffs; tloZ = saveTlo; return fail(msg, msgLen, text); } while (0)
#define REFUSE_MSG() do { offs = saveOffs; tloZ = saveTlo; return CNC_ERROR; } while (0)
#define WAIT() do { offs = saveOffs; tloZ = saveTlo; return CNC_WAIT; } while (0)

  // a program value of an axis word in mm, honouring G7 and G20
  auto wordValue = [&](int i) {
    double v = VAL(AXIS_LETTERS[i]);
    if (i == AX_X && nDiam) v /= 2;
    if (!machine.rotary[i]) v *= lin;
    return v;
  };
  // -> planner coordinate, honouring G90/G91
  auto wordTarget = [&](int i) {
    double v = wordValue(i);
    return nIncr ? absPos[i] + v : v + origin(i);
  };

  // ---------------------------------------------------------------- non-modal: G10, G28/G30, G92
  bool changedOffsets = false;
  double interTarget[CNC_N_AXIS], homeTarget[CNC_N_AXIS];
  bool homeMove = false;
  if (nonModal == 100) {
    if (!HAS('L') || !HAS('P')) REFUSE("G10 needs L and P");
    int L = (int)lround(VAL('L'));
    int P = (int)lround(VAL('P'));
    if (L != 2 && L != 20) REFUSE("G10: only L2 and L20 (coordinate systems) are supported");
    if (HAS('R')) REFUSE("G10 R (rotation) is not supported");
    if (P < 0 || P > CNC_COORD_SYSTEMS) REFUSE("G10 P must be 0 to 9");
    int cs = P == 0 ? offs.active : P - 1;
    for (int i = 0; i < CNC_N_AXIS; i++) {
      if (!HAS(AXIS_LETTERS[i])) continue;
      double v = wordValue(i);
      if (L == 2) {
        offs.coord[cs][i] = v;
      } else {
        // L20: the tool's current point gets the value v in system cs
        double other = (offs.g92Applied ? offs.g92[i] : 0) + (i == AX_Z ? tloZ : 0);
        offs.coord[cs][i] = absPos[i] + mOff[i] - other - v;
      }
    }
    changedOffsets = true;
  } else if (nonModal == 280 || nonModal == 300) {
    const double* stored = nonModal == 280 ? offs.g28 : offs.g30;
    for (int i = 0; i < CNC_N_AXIS; i++) {
      bool w = HAS(AXIS_LETTERS[i]);
      interTarget[i] = w ? wordTarget(i) : absPos[i];
      homeTarget[i] = (w || !anyAxis) ? stored[i] - mOff[i] : interTarget[i];
    }
    homeMove = true;
  } else if (nonModal == 281 || nonModal == 301) {
    double* stored = nonModal == 281 ? offs.g28 : offs.g30;
    for (int i = 0; i < CNC_N_AXIS; i++) stored[i] = absPos[i] + mOff[i];
    changedOffsets = true;
  } else if (nonModal == 920) {
    if (!anyAxis) REFUSE("G92 needs an axis word");
    if (!offs.g92Applied) {
      for (int i = 0; i < CNC_N_AXIS; i++) offs.g92[i] = 0;
    }
    offs.g92Applied = true;
    for (int i = 0; i < CNC_N_AXIS; i++) {
      if (!HAS(AXIS_LETTERS[i])) continue;
      double base = offs.coord[offs.active][i] - mOff[i] + (i == AX_Z ? tloZ : 0);
      offs.g92[i] = absPos[i] - base - wordValue(i);
    }
    changedOffsets = true;
  } else if (nonModal == 921) {
    for (int i = 0; i < CNC_N_AXIS; i++) offs.g92[i] = 0;
    offs.g92Applied = false;
    changedOffsets = true;
  } else if (nonModal == 922) {
    offs.g92Applied = false;
    changedOffsets = true;
  } else if (nonModal == 923) {
    offs.g92Applied = true;
    changedOffsets = true;
  }

  // ---------------------------------------------------------------- motion target and checks
  double target[CNC_N_AXIS];
  for (int i = 0; i < CNC_N_AXIS; i++) target[i] = absPos[i];
  if (moving) {
    for (int i = 0; i < CNC_N_AXIS; i++) {
      if (!HAS(AXIS_LETTERS[i])) continue;
      target[i] = g53 ? wordValue(i) - mOff[i] : wordTarget(i);
    }
  }
  for (int i = 0; i < CNC_N_AXIS; i++) {
    if (moving && HAS(AXIS_LETTERS[i]) && !limitOk(i, toSteps(i, target[i]))) {
      snprintf(msg, msgLen, "%c%.4f is outside the limits", AXIS_LETTERS[i], target[i] - origin(i));
      REFUSE_MSG();
    }
    if (homeMove && (!limitOk(i, toSteps(i, interTarget[i])) || !limitOk(i, toSteps(i, homeTarget[i])))) {
      snprintf(msg, msgLen, "G28/G30: %c outside the limits", AXIS_LETTERS[i]);
      REFUSE_MSG();
    }
  }

  // feed of the move
  float feed = 0;
  uint8_t condition = 0;
  bool rapid = moving && motion == 0;
  if (moving && !rapid && !thread) {
    if (nFeedMode == 930) {
      if (!HAS('F')) REFUSE("G93: every feed move needs its own F");
      feed = (float)VAL('F');
      condition = CNC_COND_INVERSE_TIME;
    } else if (nFeedMode == 950) {
      if (!(nFeedWord > 0)) REFUSE("no feed (F) for G95");
      if (!machine.spindleRpm) REFUSE("G95 needs the spindle encoder");
      float rpm = machine.spindleRpm();
      if (rpm < 1) WAIT();  // the spindle is not turning yet (the program can be stopped)
      feed = (float)(nFeedWord * rpm);
    } else {
      if (!(nFeedWord > 0)) REFUSE("no feed rate (F)");
      feed = (float)nFeedWord;
    }
  }
  if (rapid) condition = CNC_COND_RAPID;

  // arc geometry, in the (Z, X) frame
  double c1 = 0, c2 = 0, radius = 0, a0 = 0, sweep = 0;
  int segments = 0;
  if (isArc) {
    bool cw = motion == 20;
    int turns = HAS('P') ? (int)lround(VAL('P')) : 1;
    if (turns < 1) REFUSE("arc P must be 1 or more");
    double k = HAS('K') ? VAL('K') * lin : 0, ii = HAS('I') ? VAL('I') * lin : 0;
    if (!nArcIncr) {  // G90.1: the centre in program coordinates
      k += origin(AX_Z);
      ii += origin(AX_X);
    }
    int r = arcCentre(cw, nArcIncr, absPos[AX_Z], absPos[AX_X], target[AX_Z], target[AX_X], HAS('R'),
                      HAS('R') ? VAL('R') * lin : 0, k, ii, &c1, &c2, &radius, msg, msgLen);
    if (r != CNC_OK) REFUSE_MSG();
    a0 = atan2(absPos[AX_X] - c2, absPos[AX_Z] - c1);
    double a1 = atan2(target[AX_X] - c2, target[AX_Z] - c1);
    sweep = a1 - a0;  // counter-clockwise
    if (cw) {
      if (sweep >= -1e-12) sweep -= 2 * M_PI;
      sweep -= 2 * M_PI * (turns - 1);
    } else {
      if (sweep <= 1e-12) sweep += 2 * M_PI;
      sweep += 2 * M_PI * (turns - 1);
    }
    double tol = machine.arcTolerance > 0 ? machine.arcTolerance : 0.002;
    double segAngle = radius > tol ? 2 * acos(1 - tol / radius) : M_PI / 2;
    if (segAngle > M_PI / 4) segAngle = M_PI / 4;
    segments = (int)ceil(fabs(sweep) / segAngle);
    if (segments < 1) segments = 1;
    for (int s = 1; s < segments; s++) {
      double a = a0 + sweep * s / segments;
      if (!limitOk(AX_Z, toSteps(AX_Z, c1 + radius * cos(a))) || !limitOk(AX_X, toSteps(AX_X, c2 + radius * sin(a)))) {
        REFUSE("the arc goes outside the limits");
      }
    }
    if (condition == CNC_COND_INVERSE_TIME) feed *= segments;  // every segment its share of the time
  }

  // threading move: its target is handed over to the firmware
  if (thread) {
    if (!moving) REFUSE("G32/G33 needs X or Z");
    if (HAS('C')) REFUSE("G32/G33 moves X and Z only");
    double lead;
    if (motion == 330) {
      if (!HAS('K')) REFUSE("G33 needs K (lead per spindle turn)");
      lead = VAL('K') * lin;
    } else {
      if (!HAS('F')) REFUSE("G32 needs F (lead per spindle turn)");
      lead = VAL('F') * lin;
    }
    if (!(lead > 0) || lead > 25.4) REFUSE("thread lead must be above 0 and at most 25.4 mm");
    threadLead = (float)lead;
    for (int i = 0; i < CNC_N_AXIS; i++) threadTarget[i] = toSteps(i, target[i]);
  }

  // room in the planner (an arc continues on later calls)
  if (moving && !thread && !isArc && !roomForOne()) WAIT();
  if (homeMove && cnc_plan_blocks_free() < 2 * (1 + CNC_N_AXIS)) WAIT();

  // ---------------------------------------------------------------- execute (nothing is refused from here)
  plane = nPlane;
  incremental = nIncr;
  arcIncremental = nArcIncr;
  feedMode = nFeedMode;
  inches = nInches;
  diameterMode = nDiam;
  feedWord = nFeedWord;
  if (explicitMotion >= 0 && explicitMotion != 320 && explicitMotion != 330) motionMode = explicitMotion;
  if (changedOffsets) offsChanges++;

  int result = CNC_OK;
  if (g[GR_TOOL] == 430) snprintf(msg, msgLen, "G43 H: tool lengths are not stored yet, touch off Z for every tool");
  if (mTool == 6) result = CNC_PAUSE;
  if (dwellCode) {
    *dwell = (float)VAL('P');
    result = CNC_DWELL;
  }
  if (homeMove) {
    planLine(interTarget, 0, CNC_COND_RAPID, lineNo);
    planLine(homeTarget, 0, CNC_COND_RAPID, lineNo);
  }
  if (thread) {
    for (int i = 0; i < CNC_N_AXIS; i++) absPos[i] = target[i];
    result = CNC_THREAD;
  } else if (isArc) {
    arc.active = true;
    arc.lineNo = lineNo;
    arc.segments = segments;
    arc.done = 0;
    arc.c1 = c1;
    arc.c2 = c2;
    arc.r = radius;
    arc.a0 = a0;
    arc.da = sweep / segments;
    for (int i = 0; i < CNC_N_AXIS; i++) {
      arc.start[i] = absPos[i];
      arc.end[i] = target[i];
    }
    arc.feed = feed;
    arc.condition = condition;
  } else if (moving) {
    planLine(target, feed, condition, lineNo);
  }
  if (mStop == 0 || mStop == 1 || mStop == 60) result = CNC_PAUSE;
  if (mStop == 2 || mStop == 30) result = CNC_END;
  if (isArc) {
    arc.result = result;
    arc.dwell = *dwell;
    snprintf(arc.msg, sizeof(arc.msg), "%s", msg);
    if (!arcContinue()) return CNC_WAIT;
  }
  return result;
#undef REFUSE
#undef REFUSE_MSG
#undef WAIT
#undef HAS
#undef VAL
}
