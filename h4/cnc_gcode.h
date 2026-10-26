/*
  cnc_gcode.h - G-code reader for the GCODE mode of NanoEls H4 (X, Z and C)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)

  No Arduino dependency: compiled and tested on a PC too.
*/
#pragma once
#include "cnc_core.h"

typedef struct {
  char name[CNC_N_AXIS];             // G-code letters, normally 'X', 'Z', 'C'
  bool active[CNC_N_AXIS];
  bool rotary[CNC_N_AXIS];           // rotary axes are not scaled by G20
  int32_t backlashSteps[CNC_N_AXIS];
  bool checkLimits;                  // refuse moves outside min/max
  int32_t minSteps[CNC_N_AXIS];
  int32_t maxSteps[CNC_N_AXIS];
  float defaultFeed;                 // mm/min when a G94 program gives no F (the old H4 default)
} cnc_machine_t;

enum {
  CNC_OK = 0,     // line done (motion is in the planner)
  CNC_ERROR,      // line refused, msg says why; nothing was planned
  CNC_WAIT,       // the planner is too full: call cnc_st_prep_buffer() and try the same line again
  CNC_DWELL,      // wait until motion is done, then wait *dwell seconds
  CNC_PAUSE,      // wait until motion is done, then pause until the operator resumes (M0, M1, M6)
  CNC_END,        // wait until motion is done, then end the program (M2, M30)
  CNC_THREAD      // wait until motion is done, then run the threading move of cnc_gc_thread() (G32, G33)
};

void cnc_gc_init(const cnc_machine_t* m);
void cnc_gc_set_machine(const cnc_machine_t* m);  // as cnc_gc_init, but the modes (G90, G93, F...) stay
// Sets the tool position (steps), the work zero (steps) and which way each axis last moved.
// Call only when no motion is queued.
void cnc_gc_sync(const int32_t pos[CNC_N_AXIS], const int32_t workZero[CNC_N_AXIS], const bool lastNeg[CNC_N_AXIS]);
void cnc_gc_reset_modes();           // G0 G90 G94 G21, start of a program
int cnc_gc_line(const char* line, uint32_t lineNo, char* msg, int msgLen, float* dwell);
bool cnc_gc_inverse_time();          // G93 active
bool cnc_gc_last_neg(int axis);
// Target (steps) and lead (mm per spindle turn) of the last CNC_THREAD line. The caller moves the axes,
// then calls cnc_gc_sync() with the reached position.
void cnc_gc_thread(int32_t target[CNC_N_AXIS], float* leadMm);
