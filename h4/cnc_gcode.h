/*
  cnc_gcode.h - G-code interpreter for the GCODE mode of NanoEls H4 (X, Z and C)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later (it is built together with cnc_core, derived from Grbl)

  The language is RS274/NGC as LinuxCNC reads it (which Grbl follows too), for a lathe: see cnc_gcode.cpp.
  No Arduino dependency: compiled and tested on a PC too.
*/
#pragma once
#include "cnc_core.h"

#define CNC_COORD_SYSTEMS 9  // G54 G55 G56 G57 G58 G59 G59.1 G59.2 G59.3

typedef struct {
  char name[CNC_N_AXIS];             // G-code letters, normally 'X', 'Z', 'C'
  bool active[CNC_N_AXIS];
  bool rotary[CNC_N_AXIS];           // rotary axes are not scaled by G20
  int32_t backlashSteps[CNC_N_AXIS];
  bool checkLimits;                  // refuse moves outside min/max
  int32_t minSteps[CNC_N_AXIS];
  int32_t maxSteps[CNC_N_AXIS];
  float defaultFeed;                 // not used: a feed move needs an F first, as in LinuxCNC and Grbl
  float arcTolerance;                // mm, largest distance of an arc segment from the true arc
  float (*spindleRpm)();             // measured spindle speed, for G95 (NULL: G95 refused)
} cnc_machine_t;

// Offsets kept by the interpreter (machine coordinates, mm or degrees). The firmware stores them.
typedef struct {
  double coord[CNC_COORD_SYSTEMS][CNC_N_AXIS];  // G54 ... G59.3 (G54 is also the H4's own zero)
  double g28[CNC_N_AXIS];
  double g30[CNC_N_AXIS];
  double g92[CNC_N_AXIS];                         // G92 offset (program coordinates are shifted by it)
  bool g92Applied;
  int active;                                     // 0 = G54 ... 8 = G59.3
} cnc_offsets_t;

enum {
  CNC_OK = 0,     // line done (motion is in the planner)
  CNC_ERROR,      // line refused, msg says why; nothing was planned
  CNC_WAIT,       // the planner is too full: call cnc_st_prep_buffer() and try the same line again
  CNC_DWELL,      // wait until motion is done, then wait *dwell seconds
  CNC_PAUSE,      // wait until motion is done, then pause until the operator resumes (M0, M1, M6)
  CNC_END,        // wait until motion is done, then end the program (M2, M30)
  CNC_THREAD,     // wait until motion is done, then run the threading move of cnc_gc_thread() (G32, G33)
  CNC_DWELL_MORE, // wait until motion is done, wait *dwell seconds, then call again with the SAME line (cycles)
  CNC_THREAD_MORE // as CNC_THREAD, then call again with the SAME line (G76)
};

void cnc_gc_init(const cnc_machine_t* m);
void cnc_gc_set_machine(const cnc_machine_t* m);  // as cnc_gc_init, but the modes (G90, G93, F...) stay
// Sets the tool position (steps), the machine coordinate of step 0 of each axis (steps, from homing) and which
// way each axis last moved. Call only when no motion is queued.
void cnc_gc_sync(const int32_t pos[CNC_N_AXIS], const int32_t machineOffset[CNC_N_AXIS], const bool lastNeg[CNC_N_AXIS]);
void cnc_gc_reset_modes();           // G1 G17 G90 G94 G21 G54 ..., start of a program
int cnc_gc_line(const char* line, uint32_t lineNo, char* msg, int msgLen, float* dwell);
bool cnc_gc_inverse_time();          // G93 active
bool cnc_gc_last_neg(int axis);
// Target (steps) and lead (mm per spindle turn) of the last CNC_THREAD line. The caller moves the axes,
// then calls cnc_gc_sync() with the reached position.
void cnc_gc_thread(int32_t target[CNC_N_AXIS], float* leadMm);
// Offsets: read / replace (e.g. from storage at start-up), and a counter that changes whenever a program
// changed them (G10, G28.1, G30.1, G92...), so the firmware knows when to store them.
void cnc_gc_get_offsets(cnc_offsets_t* o);
void cnc_gc_set_offsets(const cnc_offsets_t* o);
uint32_t cnc_gc_offsets_changes();
// Current position in program (work) coordinates, mm or degrees, as the program sees it.
double cnc_gc_work_position(int axis);
