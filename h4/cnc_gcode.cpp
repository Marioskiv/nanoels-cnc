/*
  cnc_gcode.cpp - G-code reader for the GCODE mode of NanoEls H4 (X, Z and C)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)

  Reads one line at a time and puts its motion into the planner. Only what can be executed exactly
  is accepted; anything else is refused with a reason, never guessed:
    G0 G1             rapid / feed move on X, Z and C (the A1 axis); a program starts in G1, so
                      "X.. Z.." lines without a G word move at the feed rate, as before
    G4 P<seconds>     dwell
    G20 G21           inch / mm (C stays in degrees)
    G90 G91           absolute / incremental
    G93 G94           inverse time (F = 1/minutes of each move) / units per minute
    G8 G17 G18 G19 G40 G49 G54 G61 G64 G80   accepted, nothing to do on this machine
    G43               accepted; tool lengths are set by touching off Z (a note is sent)
    G32 X.. Z.. F<lead>   spindle-synchronised threading move (as on the NanoEls H5): F is the lead per
    G33 X.. Z.. K<lead>   spindle turn in the current units; G33 is the LinuxCNC form. Not modal, and F of
                      a G32 line does not change the feed rate. Executed by the H4 with the spindle encoder
    M0 M1 M6          pause until the operator resumes (tool change by hand)
    M2 M30            end of program
    M3 M4 M5 M7 M8 M9 accepted (spindle and coolant are switched by hand)
    N F S T P H       line number, feed, spindle speed, tool, dwell, tool length register
  Refused: arcs (G2 G3), G7 diameter mode, machine coordinates (G28 G30 G53), offsets (G10 G92),
  canned cycles, cutter compensation, any other axis letter.
*/
#include "cnc_gcode.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cnc_machine_t machine;
static int32_t workZero[CNC_N_AXIS];
static double progPos[CNC_N_AXIS];   // programmed position in work units (mm or degrees)
static bool lastNeg[CNC_N_AXIS];     // last direction of each motor, for the backlash take-up

static int motionMode;               // 0 or 1
static bool incremental;
static bool inches;
static bool inverseTime;
static float feedRate;               // units/min (G94)
static int32_t threadTarget[CNC_N_AXIS];
static float threadLead;             // mm per spindle turn

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
  motionMode = 1;
  incremental = false;
  inches = false;
  inverseTime = false;
  feedRate = machine.defaultFeed;
}

void cnc_gc_sync(const int32_t pos[CNC_N_AXIS], const int32_t zero[CNC_N_AXIS], const bool neg[CNC_N_AXIS]) {
  const cnc_settings_t* s = cnc_core_get_settings();
  for (int i = 0; i < CNC_N_AXIS; i++) {
    workZero[i] = zero[i];
    lastNeg[i] = neg[i];
    progPos[i] = (pos[i] - zero[i]) / (double)s->stepsPerUnit[i];
  }
  cnc_plan_sync_position(pos);
}

bool cnc_gc_inverse_time() { return inverseTime; }
bool cnc_gc_last_neg(int axis) { return lastNeg[axis]; }
void cnc_gc_thread(int32_t target[CNC_N_AXIS], float* leadMm) {
  for (int i = 0; i < CNC_N_AXIS; i++) target[i] = threadTarget[i];
  *leadMm = threadLead;
}

static int fail(char* msg, int msgLen, const char* text) {
  snprintf(msg, msgLen, "%s", text);
  return CNC_ERROR;
}

int cnc_gc_line(const char* line, uint32_t lineNo, char* msg, int msgLen, float* dwell) {
  msg[0] = '\0';
  // Words: letter + number. Spaces are ignored, as in RS274/NGC.
  char letters[32];
  double values[32];
  int n = 0;
  const char* p = line;
  while (*p) {
    char c = (char)toupper((unsigned char)*p);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { p++; continue; }
    if (c < 'A' || c > 'Z') {
      snprintf(msg, msgLen, "unexpected character '%c'", *p);
      return CNC_ERROR;
    }
    p++;
    char num[24];
    int k = 0;
    while (*p == ' ' || *p == '\t') p++;
    while ((*p == '-' || *p == '+' || *p == '.' || isdigit((unsigned char)*p)) && k < 23) {
      if (*p != ' ') num[k++] = *p;
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
    if (n >= 32) return fail(msg, msgLen, "too many words on one line");
    letters[n] = c;
    values[n] = v;
    n++;
  }

  // Modal and non-modal codes of this line; nothing changes before the whole line is checked.
  int newMotion = motionMode;
  bool newIncremental = incremental, newInches = inches, newInverse = inverseTime;
  bool dwellCode = false, g43 = false, g64 = false, motionCode = false;
  int threadCode = 0;  // 32 or 33
  int mAction = 0;  // 1 pause, 2 end
  bool hasAxis[CNC_N_AXIS] = {false, false, false};
  double axisValue[CNC_N_AXIS] = {0, 0, 0};
  bool hasF = false, hasP = false, hasK = false;
  double fValue = 0, pValue = 0, kValue = 0;

  for (int w = 0; w < n; w++) {  // a G33 anywhere on the line makes K its lead
    if (letters[w] == 'G' && fabs(values[w] - 33) < 1e-6) threadCode = 33;
  }
  for (int w = 0; w < n; w++) {
    char c = letters[w];
    double v = values[w];
    if (c == 'G') {
      int code10 = (int)lround(v * 10);
      if (fabs(v * 10 - code10) > 1e-6) return fail(msg, msgLen, "bad G code");
      switch (code10) {
        case 0: newMotion = 0; motionCode = true; break;
        case 10: newMotion = 1; motionCode = true; break;
        case 40: dwellCode = true; break;
        case 200: newInches = true; break;
        case 210: newInches = false; break;
        case 900: newIncremental = false; break;
        case 910: newIncremental = true; break;
        case 930: newInverse = true; break;
        case 940: newInverse = false; break;
        case 430: g43 = true; break;
        case 640: g64 = true; break;  // path tolerance P: the planner keeps the exact path anyway
        case 320: threadCode = 32; break;
        case 330: threadCode = 33; break;
        case 80: case 170: case 180: case 190: case 400: case 490: case 540: case 610: case 800:
          break;
        case 20: case 30: return fail(msg, msgLen, "arcs (G2/G3) are not supported: post the program with lines only");
        case 70: return fail(msg, msgLen, "diameter mode (G7) is not supported");
        default:
          snprintf(msg, msgLen, "G%g is not supported", v);
          return CNC_ERROR;
      }
    } else if (c == 'M') {
      int code = (int)lround(v);
      if (fabs(v - code) > 1e-6) return fail(msg, msgLen, "bad M code");
      switch (code) {
        case 0: case 1: case 6: mAction = 1; break;
        case 2: case 30: mAction = 2; break;
        case 3: case 4: case 5: case 7: case 8: case 9: break;
        default:
          snprintf(msg, msgLen, "M%d is not supported", code);
          return CNC_ERROR;
      }
    } else if (c == 'F') {
      hasF = true; fValue = v;
    } else if (c == 'P') {
      hasP = true; pValue = v;
    } else if (c == 'N' || c == 'S' || c == 'T' || c == 'H') {
      // line number, spindle speed, tool number, tool length register: nothing to do
    } else if (c == 'K' && threadCode == 33) {
      hasK = true; kValue = v;
    } else if (c == 'I' || c == 'J' || c == 'K' || c == 'R') {
      return fail(msg, msgLen, "arcs are not supported: post the program with lines only");
    } else {
      int axis = -1;
      for (int i = 0; i < CNC_N_AXIS; i++) if (machine.name[i] == c) axis = i;
      if (axis < 0) {
        snprintf(msg, msgLen, "this machine has no %c axis", c);
        return CNC_ERROR;
      }
      if (!machine.active[axis]) {
        snprintf(msg, msgLen, "axis %c is not connected (ACTIVE_A1 = false)", c);
        return CNC_ERROR;
      }
      hasAxis[axis] = true;
      axisValue[axis] = v;
    }
  }

  bool anyAxis = hasAxis[0] || hasAxis[1] || hasAxis[2];
  if (dwellCode && anyAxis) return fail(msg, msgLen, "G4 with an axis word");
  if (dwellCode && !hasP) return fail(msg, msgLen, "G4 needs P (seconds)");
  if (hasP && !dwellCode && !g64) return fail(msg, msgLen, "P without G4");
  if (motionCode && dwellCode) return fail(msg, msgLen, "G0/G1 and G4 on one line");
  if (hasF && !(fValue > 0) && !(newInverse && !anyAxis)) return fail(msg, msgLen, "F must be positive");

  if (threadCode) {
    if (motionCode || dwellCode || mAction) return fail(msg, msgLen, "G32/G33 must be alone on its line");
    if (hasAxis[CNC_C]) return fail(msg, msgLen, "G32/G33 moves X and Z only");
    if (!hasAxis[CNC_X] && !hasAxis[CNC_Z]) return fail(msg, msgLen, "G32/G33 needs X or Z");
    if (threadCode == 32 && hasK) return fail(msg, msgLen, "G32 takes the lead as F");
    if (threadCode == 33 && !hasK) return fail(msg, msgLen, "G33 needs K (lead per spindle turn)");
    if (threadCode == 32 && !hasF) return fail(msg, msgLen, "G32 needs F (lead per spindle turn)");
    double lead = (threadCode == 32 ? fValue : kValue) * (newInches ? 25.4 : 1.0);
    if (!(lead > 0) || lead > 25.4) return fail(msg, msgLen, "thread lead must be above 0 and at most 25.4 mm");
    const cnc_settings_t* s = cnc_core_get_settings();
    double prog[CNC_N_AXIS];
    int32_t target[CNC_N_AXIS];
    for (int i = 0; i < CNC_N_AXIS; i++) {
      prog[i] = progPos[i];
      if (hasAxis[i]) prog[i] = newIncremental ? progPos[i] + axisValue[i] * (newInches ? 25.4 : 1.0) : axisValue[i] * (newInches ? 25.4 : 1.0);
      target[i] = workZero[i] + (int32_t)llround(prog[i] * s->stepsPerUnit[i]);
      if (machine.checkLimits && hasAxis[i] && (target[i] < machine.minSteps[i] || target[i] > machine.maxSteps[i])) {
        snprintf(msg, msgLen, "%c%.4f is outside the limits", machine.name[i], prog[i]);
        return CNC_ERROR;
      }
    }
    // Valid: the modes of the line apply, the feed rate does not change (F is the lead).
    incremental = newIncremental;
    inches = newInches;
    inverseTime = newInverse;
    for (int i = 0; i < CNC_N_AXIS; i++) {
      threadTarget[i] = target[i];
      progPos[i] = prog[i];
    }
    threadLead = (float)lead;
    return CNC_THREAD;
  }

  // Motion: check everything before anything is planned.
  int32_t target[CNC_N_AXIS];
  double newProg[CNC_N_AXIS];
  bool move = anyAxis;
  float feed = 0;
  uint8_t condition = 0;
  if (move) {
    const cnc_settings_t* s = cnc_core_get_settings();
    for (int i = 0; i < CNC_N_AXIS; i++) {
      double scale = (newInches && !machine.rotary[i]) ? 25.4 : 1.0;
      newProg[i] = progPos[i];
      if (hasAxis[i]) newProg[i] = newIncremental ? progPos[i] + axisValue[i] * scale : axisValue[i] * scale;
      target[i] = workZero[i] + (int32_t)llround(newProg[i] * s->stepsPerUnit[i]);
      if (machine.checkLimits && hasAxis[i] && (target[i] < machine.minSteps[i] || target[i] > machine.maxSteps[i])) {
        snprintf(msg, msgLen, "%c%.4f is outside the limits", machine.name[i], newProg[i]);
        return CNC_ERROR;
      }
    }
    if (newMotion == 0) {
      condition = CNC_COND_RAPID;
    } else if (newInverse) {
      if (!hasF) return fail(msg, msgLen, "G93: every G1 move needs its own F");
      feed = (float)fValue;
      condition = CNC_COND_INVERSE_TIME;
    } else {
      double f = hasF ? fValue * (newInches ? 25.4 : 1.0) : feedRate;
      if (!(f > 0)) return fail(msg, msgLen, "no feed rate (F)");
      feed = (float)f;
    }
    // One block for the move, plus one per axis that reverses with backlash.
    if (cnc_plan_blocks_free() < 1 + CNC_N_AXIS) return CNC_WAIT;
  }

  // The line is valid: apply it.
  motionMode = newMotion;
  incremental = newIncremental;
  inches = newInches;
  bool leftInverse = inverseTime && !newInverse;
  inverseTime = newInverse;
  if (hasF && !newInverse) feedRate = (float)(fValue * (newInches ? 25.4 : 1.0));
  if (leftInverse && !hasF) feedRate = 0;  // after G93 a G94 move must state its own F, as in LinuxCNC

  if (move) {
    const int32_t* plPos = cnc_plan_position();
    for (int i = 0; i < CNC_N_AXIS; i++) {
      int32_t d = target[i] - plPos[i];
      if (d == 0 || machine.backlashSteps[i] <= 0) {
        if (d != 0) lastNeg[i] = d < 0;
        continue;
      }
      bool neg = d < 0;
      if (neg != lastNeg[i]) cnc_plan_buffer_backlash(i, machine.backlashSteps[i], neg, lineNo);
      lastNeg[i] = neg;
    }
    cnc_plan_buffer_line(target, feed, condition, lineNo);  // false only for a zero-length move
    for (int i = 0; i < CNC_N_AXIS; i++) progPos[i] = newProg[i];
  }

  if (g43) snprintf(msg, msgLen, "G43 ignored: touch off Z for every tool");
  if (dwellCode) {
    *dwell = (float)pValue;
    return CNC_DWELL;
  }
  if (mAction == 1) return CNC_PAUSE;
  if (mAction == 2) return CNC_END;
  return CNC_OK;
}
