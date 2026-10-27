/*
  cnc_core.h - motion planner and step generator for the GCODE mode of NanoEls H4
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc

  Ported from Grbl 1.1 (planner.c, stepper.c):
    Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
    Copyright (c) 2009-2011 Simen Svale Skogsrud
    Copyright (c) 2011 Jens Geisler
  Adapted for the ESP32-S3 and for X / Z / C axes:
    Copyright (c) 2026 Marioskiv

  This program is free software: you can redistribute it and/or modify it under the terms of the
  GNU General Public License as published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version. This program is distributed in the hope that it
  will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  SPDX-License-Identifier: GPL-3.0-or-later

  This file has no Arduino dependency so that it can be compiled and tested on a PC.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifndef CNC_ISR_ATTR
#define CNC_ISR_ATTR
#endif

// Axis indexes. Units: X and Z in mm, C in degrees (as in the H4 firmware: 1 mm = 1 degree).
#define CNC_N_AXIS 3
#define CNC_X 0
#define CNC_Z 1
#define CNC_C 2

#define CNC_BLOCK_BUFFER_SIZE 64     // planner blocks (look-ahead)
#define CNC_SEGMENT_BUFFER_SIZE 24   // step segments, about 0.23 s of motion at 100 segments per second
#define CNC_ACCELERATION_TICKS_PER_SECOND 100
#define CNC_MINIMUM_FEED_RATE 1.0f   // units/min
#define CNC_MINIMUM_JUNCTION_SPEED 0.0f
#define CNC_SOME_LARGE_VALUE 1.0E+38f

// Block conditions
#define CNC_COND_RAPID 0x01        // G0: moves at the rapid rate of the axes
#define CNC_COND_INVERSE_TIME 0x02 // G93: feed is 1/minutes for the whole block
#define CNC_COND_BACKLASH 0x04     // takes up backlash: the motor moves, the tool does not

typedef struct {
  float stepsPerUnit[CNC_N_AXIS];  // steps per mm (X, Z) or per degree (C)
  float maxRate[CNC_N_AXIS];       // units/min
  float acceleration[CNC_N_AXIS];  // units/min^2
  float junctionDeviation;         // mm, as in Grbl ($11)
  uint32_t timerHz;                // step timer clock
} cnc_settings_t;

typedef struct {
  // Fields used by the step segment generator
  uint32_t steps[CNC_N_AXIS];
  uint32_t step_event_count;
  uint8_t direction_bits;          // bit i set = axis i moves negative
  uint8_t condition;

  // Fields used by the motion planner
  float entry_speed_sqr;
  float max_entry_speed_sqr;
  float acceleration;
  float millimeters;               // remaining length of the block (units)
  float max_junction_speed_sqr;
  float rapid_rate;
  float programmed_rate;
  uint32_t line;                   // source line, for reports
} cnc_block_t;

// What the step interrupt has to output on one tick.
typedef struct {
  uint8_t step;      // bit i = step axis i now
  uint8_t dirNeg;    // bit i = axis i direction negative (valid for the stepped axes)
  uint8_t motorOnly; // set: the steps take up backlash (motor position changes, tool position does not)
  uint32_t period;   // timer ticks until the next call
} cnc_tick_t;

// ---- settings
void cnc_core_settings(const cnc_settings_t* s);
const cnc_settings_t* cnc_core_get_settings();

// ---- planner
void cnc_plan_reset();                                  // empties everything and sets the position
void cnc_plan_sync_position(const int32_t steps[CNC_N_AXIS]);
bool cnc_plan_buffer_line(const int32_t target[CNC_N_AXIS], float feed, uint8_t condition, uint32_t line);
extern void (*cnc_plan_trace)(const int32_t target[CNC_N_AXIS], uint32_t line);  // tests: every line target
bool cnc_plan_buffer_backlash(int axis, int32_t steps, bool negative, uint32_t line);
uint8_t cnc_plan_blocks_free();
uint8_t cnc_plan_blocks_used();
const int32_t* cnc_plan_position();
float cnc_plan_last_block_minutes();  // time of the last block at its programmed speed (for reports)

// ---- step segment generator (call often from a task)
void cnc_st_reset();
void cnc_st_prep_buffer();
bool cnc_st_busy();               // steps are queued or being executed
float cnc_st_realtime_rate();     // units/min of the segment being prepared
uint32_t cnc_st_current_line();
uint32_t cnc_st_underruns();      // times the step buffer ran empty while motion was queued (should stay 0)

// ---- feed hold
void cnc_hold_start();            // decelerate to a stop inside the planned path
bool cnc_hold_complete();         // the hold has stopped all motion
void cnc_hold_resume();           // continue the program after a completed hold
bool cnc_hold_active();

// ---- step interrupt: called by the timer, fills out and returns true if a tick was executed
bool cnc_isr(cnc_tick_t* out);
