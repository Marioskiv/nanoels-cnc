/*
  cnc_core.cpp - motion planner and step generator for the GCODE mode of NanoEls H4
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

  Differences from Grbl: three axes (X, Z, C) instead of X, Y, Z; a 32-bit step timer, so no
  prescaler and no 16-bit limit; backlash take-up blocks; no spindle, laser, parking, homing or
  probing; feed / rapid overrides as Grbl 1.1, switched off per block by M49 / M50 P0 (as LinuxCNC);
  G61.1 exact stop per block. The planner and the segment generator run in one task; the interrupt only
  reads the segment buffer, exactly as in Grbl. On the dual-core ESP32 the hand-over of a segment
  is fenced with a memory barrier.
*/
#ifdef ARDUINO
#include <Arduino.h>
#define CNC_ISR_ATTR IRAM_ATTR
#endif
#include "cnc_core.h"
#include <math.h>
#include <string.h>

#define CNC_BARRIER() __sync_synchronize()

static cnc_settings_t settings;

// ============================================================================ planner

static cnc_block_t block_buffer[CNC_BLOCK_BUFFER_SIZE];
static uint8_t block_buffer_tail;     // block executed now
static uint8_t block_buffer_head;     // next block to be written
static uint8_t next_buffer_head;
static uint8_t block_buffer_planned;  // first block after the optimally planned ones

typedef struct {
  int32_t position[CNC_N_AXIS];       // planner position of the tool, steps
  float previous_unit_vec[CNC_N_AXIS];
  float previous_nominal_speed;
  bool previous_was_backlash;
  bool previous_exact_stop;           // G61.1: the previous block ends at rest
} planner_t;
static planner_t pl;
static float last_block_minutes;  // length / nominal speed of the last planned block (no acceleration)

static void st_update_plan_block_parameters();

static uint8_t plan_next_block_index(uint8_t i) { i++; return i == CNC_BLOCK_BUFFER_SIZE ? 0 : i; }
static uint8_t plan_prev_block_index(uint8_t i) { if (i == 0) i = CNC_BLOCK_BUFFER_SIZE; return i - 1; }

static float convert_delta_vector_to_unit_vector(float* vector) {
  float magnitude = 0.0f;
  for (int i = 0; i < CNC_N_AXIS; i++) magnitude += vector[i] * vector[i];
  magnitude = sqrtf(magnitude);
  float inv = 1.0f / magnitude;
  for (int i = 0; i < CNC_N_AXIS; i++) vector[i] *= inv;
  return magnitude;
}

// Largest value along the unit vector that keeps every axis within its own limit.
static float limit_value_by_axis_maximum(const float* max_value, const float* unit_vec) {
  float limit = CNC_SOME_LARGE_VALUE;
  for (int i = 0; i < CNC_N_AXIS; i++) {
    if (unit_vec[i] != 0.0f) {
      float v = fabsf(max_value[i] / unit_vec[i]);
      if (v < limit) limit = v;
    }
  }
  return limit;
}

void cnc_core_settings(const cnc_settings_t* s) { settings = *s; }
const cnc_settings_t* cnc_core_get_settings() { return &settings; }

// See Grbl planner.c for the full description of the two-pass plan.
static void planner_recalculate() {
  uint8_t block_index = plan_prev_block_index(block_buffer_head);
  if (block_index == block_buffer_planned) return;

  float entry_speed_sqr;
  cnc_block_t* next;
  cnc_block_t* current = &block_buffer[block_index];

  // The last block always ends at rest.
  current->entry_speed_sqr = fminf(current->max_entry_speed_sqr, 2 * current->acceleration * current->millimeters);

  block_index = plan_prev_block_index(block_index);
  if (block_index == block_buffer_planned) {
    if (block_index == block_buffer_tail) st_update_plan_block_parameters();
  } else {
    while (block_index != block_buffer_planned) {
      next = current;
      current = &block_buffer[block_index];
      block_index = plan_prev_block_index(block_index);
      if (block_index == block_buffer_tail) st_update_plan_block_parameters();
      if (current->entry_speed_sqr != current->max_entry_speed_sqr) {
        entry_speed_sqr = next->entry_speed_sqr + 2 * current->acceleration * current->millimeters;
        current->entry_speed_sqr = entry_speed_sqr < current->max_entry_speed_sqr ? entry_speed_sqr : current->max_entry_speed_sqr;
      }
    }
  }

  next = &block_buffer[block_buffer_planned];
  block_index = plan_next_block_index(block_buffer_planned);
  while (block_index != block_buffer_head) {
    current = next;
    next = &block_buffer[block_index];
    if (current->entry_speed_sqr < next->entry_speed_sqr) {
      entry_speed_sqr = current->entry_speed_sqr + 2 * current->acceleration * current->millimeters;
      if (entry_speed_sqr < next->entry_speed_sqr) {
        next->entry_speed_sqr = entry_speed_sqr;
        block_buffer_planned = block_index;
      }
    }
    if (next->entry_speed_sqr == next->max_entry_speed_sqr) block_buffer_planned = block_index;
    block_index = plan_next_block_index(block_index);
  }
}

static void plan_reset_buffer() {
  block_buffer_tail = 0;
  block_buffer_head = 0;
  next_buffer_head = 1;
  block_buffer_planned = 0;
}

void cnc_plan_reset() {
  memset(&pl, 0, sizeof(pl));
  plan_reset_buffer();
}

void cnc_plan_sync_position(const int32_t steps[CNC_N_AXIS]) {
  for (int i = 0; i < CNC_N_AXIS; i++) pl.position[i] = steps[i];
}

const int32_t* cnc_plan_position() { return pl.position; }
float cnc_plan_last_block_minutes() { return last_block_minutes; }

static void plan_discard_current_block() {
  if (block_buffer_head != block_buffer_tail) {
    uint8_t block_index = plan_next_block_index(block_buffer_tail);
    if (block_buffer_tail == block_buffer_planned) block_buffer_planned = block_index;
    block_buffer_tail = block_index;
  }
}


static cnc_block_t* plan_get_current_block() {
  if (block_buffer_head == block_buffer_tail) return NULL;
  return &block_buffer[block_buffer_tail];
}

static float plan_get_exec_block_exit_speed_sqr() {
  uint8_t block_index = plan_next_block_index(block_buffer_tail);
  if (block_index == block_buffer_head) return 0.0f;
  return block_buffer[block_index].entry_speed_sqr;
}

uint8_t cnc_plan_blocks_free() {
  if (block_buffer_head >= block_buffer_tail) return (CNC_BLOCK_BUFFER_SIZE - 1) - (block_buffer_head - block_buffer_tail);
  return block_buffer_tail - block_buffer_head - 1;
}

uint8_t cnc_plan_blocks_used() {
  if (block_buffer_head >= block_buffer_tail) return block_buffer_head - block_buffer_tail;
  return CNC_BLOCK_BUFFER_SIZE - (block_buffer_tail - block_buffer_head);
}

// Overrides in percent, as Grbl 1.1: the feed of every feed move (G1-G3, G93, G95) and the speed of rapids.
static uint8_t feed_override = 100;
static uint8_t rapid_override = 100;

static float plan_compute_profile_nominal_speed(const cnc_block_t* block) {
  float nominal_speed = block->programmed_rate;
  if (block->condition & CNC_COND_NO_OVERRIDE) {  // M49 / M50 P0, as LinuxCNC: neither feed nor rapid override
    if (!(block->condition & CNC_COND_RAPID) && nominal_speed > block->rapid_rate) nominal_speed = block->rapid_rate;
  } else if (block->condition & CNC_COND_RAPID) {
    nominal_speed *= 0.01f * rapid_override;
  } else {
    if (!(block->condition & CNC_COND_BACKLASH)) nominal_speed *= 0.01f * feed_override;
    if (nominal_speed > block->rapid_rate) nominal_speed = block->rapid_rate;
  }
  return nominal_speed > CNC_MINIMUM_FEED_RATE ? nominal_speed : CNC_MINIMUM_FEED_RATE;
}

static void plan_compute_profile_parameters(cnc_block_t* block, float nominal_speed, float prev_nominal_speed) {
  block->max_entry_speed_sqr = nominal_speed > prev_nominal_speed ? prev_nominal_speed * prev_nominal_speed : nominal_speed * nominal_speed;
  if (block->max_entry_speed_sqr > block->max_junction_speed_sqr) block->max_entry_speed_sqr = block->max_junction_speed_sqr;
}

// The self-test (and the PC tests): every planned block leaves the planner as if it had been executed at once.
// Gives the time of each one at its programmed speed, in minutes. Only when no motion is being executed.
int cnc_plan_take_all(float* minutes, int max) {
  int n = 0;
  while (block_buffer_head != block_buffer_tail) {
    cnc_block_t* block = &block_buffer[block_buffer_tail];
    if (n < max) minutes[n] = block->millimeters / plan_compute_profile_nominal_speed(block);
    n++;
    plan_discard_current_block();
  }
  return n;
}

// Adds a block that moves from the planner position by delta_steps. The caller checks that a block is free.
static bool plan_buffer_steps(const int32_t delta_steps[CNC_N_AXIS], float feed, uint8_t condition, uint32_t line) {
  cnc_block_t* block = &block_buffer[block_buffer_head];
  memset(block, 0, sizeof(cnc_block_t));
  block->condition = condition;
  block->line = line;

  float unit_vec[CNC_N_AXIS];
  for (int idx = 0; idx < CNC_N_AXIS; idx++) {
    int32_t d = delta_steps[idx];
    block->steps[idx] = d < 0 ? (uint32_t)(-d) : (uint32_t)d;
    if (block->steps[idx] > block->step_event_count) block->step_event_count = block->steps[idx];
    unit_vec[idx] = d / settings.stepsPerUnit[idx];
    if (d < 0) block->direction_bits |= (1 << idx);
  }
  if (block->step_event_count == 0) return false;

  block->millimeters = convert_delta_vector_to_unit_vector(unit_vec);
  block->acceleration = limit_value_by_axis_maximum(settings.acceleration, unit_vec);
  block->rapid_rate = limit_value_by_axis_maximum(settings.maxRate, unit_vec);

  if (condition & (CNC_COND_RAPID | CNC_COND_BACKLASH)) {
    block->programmed_rate = block->rapid_rate;
  } else {
    block->programmed_rate = feed;
    if (condition & CNC_COND_INVERSE_TIME) block->programmed_rate *= block->millimeters;
  }

  if (block_buffer_head == block_buffer_tail || pl.previous_was_backlash || pl.previous_exact_stop) {
    // Starts from rest. Also after a backlash take-up: the motor stops when the slack is taken up, so
    // the carriage is not hit at speed, then the move accelerates normally. And after a G61.1 block (exact
    // stop, as LinuxCNC: every move of that mode ends at rest).
    block->entry_speed_sqr = 0.0f;
    block->max_junction_speed_sqr = 0.0f;
  } else {
    // Junction speed by the centripetal acceleration of a circle "junction deviation" away from the corner.
    float junction_unit_vec[CNC_N_AXIS];
    float junction_cos_theta = 0.0f;
    for (int idx = 0; idx < CNC_N_AXIS; idx++) {
      junction_cos_theta -= pl.previous_unit_vec[idx] * unit_vec[idx];
      junction_unit_vec[idx] = unit_vec[idx] - pl.previous_unit_vec[idx];
    }
    if (junction_cos_theta > 0.999999f) {
      block->max_junction_speed_sqr = CNC_MINIMUM_JUNCTION_SPEED * CNC_MINIMUM_JUNCTION_SPEED;
    } else if (junction_cos_theta < -0.999999f) {
      block->max_junction_speed_sqr = CNC_SOME_LARGE_VALUE;
    } else {
      convert_delta_vector_to_unit_vector(junction_unit_vec);
      float junction_acceleration = limit_value_by_axis_maximum(settings.acceleration, junction_unit_vec);
      float sin_theta_d2 = sqrtf(0.5f * (1.0f - junction_cos_theta));
      float v = (junction_acceleration * settings.junctionDeviation * sin_theta_d2) / (1.0f - sin_theta_d2);
      block->max_junction_speed_sqr = v > CNC_MINIMUM_JUNCTION_SPEED * CNC_MINIMUM_JUNCTION_SPEED ? v : CNC_MINIMUM_JUNCTION_SPEED * CNC_MINIMUM_JUNCTION_SPEED;
    }
  }

  float nominal_speed = plan_compute_profile_nominal_speed(block);
  last_block_minutes = block->millimeters / nominal_speed;
  plan_compute_profile_parameters(block, nominal_speed, pl.previous_nominal_speed);
  pl.previous_nominal_speed = nominal_speed;
  memcpy(pl.previous_unit_vec, unit_vec, sizeof(unit_vec));
  pl.previous_was_backlash = (condition & CNC_COND_BACKLASH) != 0;
  pl.previous_exact_stop = (condition & CNC_COND_EXACT_STOP) != 0;

  block_buffer_head = next_buffer_head;
  next_buffer_head = plan_next_block_index(block_buffer_head);
  planner_recalculate();
  return true;
}

void (*cnc_plan_trace)(const int32_t target[CNC_N_AXIS], uint32_t line) = 0;
void (*cnc_st_block_event)(uint32_t line, uint8_t condition, bool start, float speed) = 0;

bool cnc_plan_buffer_line(const int32_t target[CNC_N_AXIS], float feed, uint8_t condition, uint32_t line) {
  if (cnc_plan_trace) cnc_plan_trace(target, line);
  int32_t delta[CNC_N_AXIS];
  for (int i = 0; i < CNC_N_AXIS; i++) delta[i] = target[i] - pl.position[i];
  if (!plan_buffer_steps(delta, feed, condition & ~CNC_COND_BACKLASH, line)) return false;
  for (int i = 0; i < CNC_N_AXIS; i++) pl.position[i] = target[i];
  return true;
}

bool cnc_plan_buffer_backlash(int axis, int32_t steps, bool negative, uint32_t line) {
  int32_t delta[CNC_N_AXIS] = {0, 0, 0};
  delta[axis] = negative ? -steps : steps;
  return plan_buffer_steps(delta, 0.0f, CNC_COND_BACKLASH, line);  // the planner position does not change
}

// Re-plans the buffer from the block being executed (after a feed hold, or when an override changed).
static void plan_cycle_reinitialize() {
  st_update_plan_block_parameters();
  block_buffer_planned = block_buffer_tail;
  planner_recalculate();
}

// The nominal speeds changed (override): new speed limits for every planned block, as Grbl's
// plan_update_velocity_profile_parameters().
static void plan_update_velocity_profile_parameters() {
  uint8_t block_index = block_buffer_tail;
  float prev_nominal_speed = CNC_SOME_LARGE_VALUE;
  while (block_index != block_buffer_head) {
    cnc_block_t* block = &block_buffer[block_index];
    float nominal_speed = plan_compute_profile_nominal_speed(block);
    plan_compute_profile_parameters(block, nominal_speed, prev_nominal_speed);
    prev_nominal_speed = nominal_speed;
    block_index = plan_next_block_index(block_index);
  }
  pl.previous_nominal_speed = prev_nominal_speed;
}



// ============================================================================ step segments

#define DT_SEGMENT (1.0f / (CNC_ACCELERATION_TICKS_PER_SECOND * 60.0f))  // min/segment
#define REQ_MM_INCREMENT_SCALAR 1.25f
#define RAMP_ACCEL 0
#define RAMP_CRUISE 1
#define RAMP_DECEL 2
#define RAMP_DECEL_OVERRIDE 3

#define PREP_FLAG_RECALCULATE 0x01
#define PREP_FLAG_DECEL_OVERRIDE 0x08

#define STEP_CONTROL_NORMAL_OP 0x00
#define STEP_CONTROL_END_MOTION 0x01
#define STEP_CONTROL_EXECUTE_HOLD 0x02

// Adaptive Multi-Axis Step Smoothing, as in Grbl: below these step rates the interrupt runs 2, 4 or 8
// times per step of the main axis so that the other axes step evenly.
#define MAX_AMASS_LEVEL 3

typedef struct {
  uint32_t steps[CNC_N_AXIS];
  uint32_t step_event_count;
  uint8_t direction_bits;
  uint8_t condition;
  uint32_t line;
} st_block_t;
static st_block_t st_block_buffer[CNC_SEGMENT_BUFFER_SIZE - 1];

typedef struct {
  uint32_t n_step;
  uint32_t cycles_per_tick;
  uint8_t st_block_index;
  uint8_t amass_level;
} segment_t;
static segment_t segment_buffer[CNC_SEGMENT_BUFFER_SIZE];

typedef struct {
  uint32_t counter[CNC_N_AXIS];
  uint32_t steps[CNC_N_AXIS];
  uint32_t step_count;
  uint8_t exec_block_index;
  st_block_t* exec_block;
  segment_t* exec_segment;
} stepper_t;
static stepper_t st;

static volatile uint8_t segment_buffer_tail;
static volatile uint8_t segment_buffer_head;
static uint8_t segment_next_head;

static volatile uint8_t step_control;
static volatile bool exec_active;     // the interrupt is executing a segment
static volatile uint32_t exec_line;
static volatile uint32_t underruns;   // ticks without a segment while blocks were still waiting (stutter)

static cnc_block_t* pl_block;
static st_block_t* st_prep_block;

typedef struct {
  uint8_t st_block_index;
  uint8_t recalculate_flag;
  float dt_remainder;
  float steps_remaining;
  float step_per_mm;
  float req_mm_increment;
  uint8_t ramp_type;
  float mm_complete;
  float current_speed;
  float maximum_speed;
  float exit_speed;
  float accelerate_until;
  float decelerate_after;
} st_prep_t;
static st_prep_t prep;

void cnc_st_reset() {
  memset(&prep, 0, sizeof(prep));
  memset(&st, 0, sizeof(st));
  st.exec_segment = NULL;
  pl_block = NULL;
  segment_buffer_tail = 0;
  segment_buffer_head = 0;
  segment_next_head = 1;
  step_control = STEP_CONTROL_NORMAL_OP;
  exec_active = false;
}

static void st_update_plan_block_parameters() {
  if (pl_block != NULL) {
    prep.recalculate_flag |= PREP_FLAG_RECALCULATE;
    pl_block->entry_speed_sqr = prep.current_speed * prep.current_speed;
    pl_block = NULL;
  }
}

static uint8_t st_next_block_index(uint8_t i) { i++; return i == (CNC_SEGMENT_BUFFER_SIZE - 1) ? 0 : i; }

void cnc_st_prep_buffer() {
  if (step_control & STEP_CONTROL_END_MOTION) return;

  while (segment_buffer_tail != segment_next_head) {
    if (pl_block == NULL) {
      pl_block = plan_get_current_block();
      if (pl_block == NULL) return;

      if (prep.recalculate_flag & PREP_FLAG_RECALCULATE) {
        prep.recalculate_flag = 0;
      } else {
        prep.st_block_index = st_next_block_index(prep.st_block_index);
        st_prep_block = &st_block_buffer[prep.st_block_index];
        st_prep_block->direction_bits = pl_block->direction_bits;
        st_prep_block->condition = pl_block->condition;
        st_prep_block->line = pl_block->line;
        for (int idx = 0; idx < CNC_N_AXIS; idx++) st_prep_block->steps[idx] = pl_block->steps[idx] << MAX_AMASS_LEVEL;
        st_prep_block->step_event_count = pl_block->step_event_count << MAX_AMASS_LEVEL;

        prep.steps_remaining = (float)pl_block->step_event_count;
        prep.step_per_mm = prep.steps_remaining / pl_block->millimeters;
        prep.req_mm_increment = REQ_MM_INCREMENT_SCALAR / prep.step_per_mm;
        prep.dt_remainder = 0.0f;

        if ((step_control & STEP_CONTROL_EXECUTE_HOLD) || (prep.recalculate_flag & PREP_FLAG_DECEL_OVERRIDE)) {
          prep.current_speed = prep.exit_speed;
          pl_block->entry_speed_sqr = prep.exit_speed * prep.exit_speed;
          prep.recalculate_flag &= ~PREP_FLAG_DECEL_OVERRIDE;
        } else {
          prep.current_speed = sqrtf(pl_block->entry_speed_sqr);
        }
        if (cnc_st_block_event) cnc_st_block_event(pl_block->line, pl_block->condition, true, prep.current_speed);
      }

      // Velocity profile of the block (see the drawing in Grbl stepper.c).
      prep.mm_complete = 0.0f;
      float inv_2_accel = 0.5f / pl_block->acceleration;
      if (step_control & STEP_CONTROL_EXECUTE_HOLD) {
        prep.ramp_type = RAMP_DECEL;
        float decel_dist = pl_block->millimeters - inv_2_accel * pl_block->entry_speed_sqr;
        if (decel_dist < 0.0f) {
          prep.exit_speed = sqrtf(pl_block->entry_speed_sqr - 2 * pl_block->acceleration * pl_block->millimeters);
        } else {
          prep.mm_complete = decel_dist;
          prep.exit_speed = 0.0f;
        }
      } else {
        prep.ramp_type = RAMP_ACCEL;
        prep.accelerate_until = pl_block->millimeters;
        float exit_speed_sqr = plan_get_exec_block_exit_speed_sqr();
        prep.exit_speed = sqrtf(exit_speed_sqr);
        float nominal_speed = plan_compute_profile_nominal_speed(pl_block);
        float nominal_speed_sqr = nominal_speed * nominal_speed;
        float intersect_distance = 0.5f * (pl_block->millimeters + inv_2_accel * (pl_block->entry_speed_sqr - exit_speed_sqr));

        if (pl_block->entry_speed_sqr > nominal_speed_sqr) {
          prep.accelerate_until = pl_block->millimeters - inv_2_accel * (pl_block->entry_speed_sqr - nominal_speed_sqr);
          if (prep.accelerate_until <= 0.0f) {
            prep.ramp_type = RAMP_DECEL;
            prep.exit_speed = sqrtf(pl_block->entry_speed_sqr - 2 * pl_block->acceleration * pl_block->millimeters);
            prep.recalculate_flag |= PREP_FLAG_DECEL_OVERRIDE;
          } else {
            prep.decelerate_after = inv_2_accel * (nominal_speed_sqr - exit_speed_sqr);
            prep.maximum_speed = nominal_speed;
            prep.ramp_type = RAMP_DECEL_OVERRIDE;
          }
        } else if (intersect_distance > 0.0f) {
          if (intersect_distance < pl_block->millimeters) {
            prep.decelerate_after = inv_2_accel * (nominal_speed_sqr - exit_speed_sqr);
            if (prep.decelerate_after < intersect_distance) {
              prep.maximum_speed = nominal_speed;
              if (pl_block->entry_speed_sqr == nominal_speed_sqr) {
                prep.ramp_type = RAMP_CRUISE;
              } else {
                prep.accelerate_until -= inv_2_accel * (nominal_speed_sqr - pl_block->entry_speed_sqr);
              }
            } else {
              prep.accelerate_until = intersect_distance;
              prep.decelerate_after = intersect_distance;
              prep.maximum_speed = sqrtf(2.0f * pl_block->acceleration * intersect_distance + exit_speed_sqr);
            }
          } else {
            prep.ramp_type = RAMP_DECEL;
          }
        } else {
          prep.accelerate_until = 0.0f;
          prep.maximum_speed = prep.exit_speed;
        }
      }
    }

    segment_t* prep_segment = &segment_buffer[segment_buffer_head];
    prep_segment->st_block_index = prep.st_block_index;

    float dt_max = DT_SEGMENT;
    float dt = 0.0f;
    float time_var = dt_max;
    float mm_var;
    float speed_var;
    float mm_remaining = pl_block->millimeters;
    float minimum_mm = mm_remaining - prep.req_mm_increment;
    if (minimum_mm < 0.0f) minimum_mm = 0.0f;

    do {
      switch (prep.ramp_type) {
        case RAMP_DECEL_OVERRIDE:
          speed_var = pl_block->acceleration * time_var;
          if (prep.current_speed - prep.maximum_speed <= speed_var) {
            mm_remaining = prep.accelerate_until;
            time_var = 2.0f * (pl_block->millimeters - mm_remaining) / (prep.current_speed + prep.maximum_speed);
            prep.ramp_type = RAMP_CRUISE;
            prep.current_speed = prep.maximum_speed;
          } else {
            mm_remaining -= time_var * (prep.current_speed - 0.5f * speed_var);
            prep.current_speed -= speed_var;
          }
          break;
        case RAMP_ACCEL:
          speed_var = pl_block->acceleration * time_var;
          mm_remaining -= time_var * (prep.current_speed + 0.5f * speed_var);
          if (mm_remaining < prep.accelerate_until) {
            mm_remaining = prep.accelerate_until;
            time_var = 2.0f * (pl_block->millimeters - mm_remaining) / (prep.current_speed + prep.maximum_speed);
            if (mm_remaining == prep.decelerate_after) prep.ramp_type = RAMP_DECEL;
            else prep.ramp_type = RAMP_CRUISE;
            prep.current_speed = prep.maximum_speed;
          } else {
            prep.current_speed += speed_var;
          }
          break;
        case RAMP_CRUISE:
          mm_var = mm_remaining - prep.maximum_speed * time_var;
          if (mm_var < prep.decelerate_after) {
            time_var = (mm_remaining - prep.decelerate_after) / prep.maximum_speed;
            mm_remaining = prep.decelerate_after;
            prep.ramp_type = RAMP_DECEL;
          } else {
            mm_remaining = mm_var;
          }
          break;
        default:  // RAMP_DECEL
          speed_var = pl_block->acceleration * time_var;
          if (prep.current_speed > speed_var) {
            mm_var = mm_remaining - time_var * (prep.current_speed - 0.5f * speed_var);
            if (mm_var > prep.mm_complete) {
              mm_remaining = mm_var;
              prep.current_speed -= speed_var;
              break;
            }
          }
          time_var = 2.0f * (mm_remaining - prep.mm_complete) / (prep.current_speed + prep.exit_speed);
          mm_remaining = prep.mm_complete;
          prep.current_speed = prep.exit_speed;
      }
      dt += time_var;
      if (dt < dt_max) {
        time_var = dt_max - dt;
      } else {
        if (mm_remaining > minimum_mm) {
          dt_max += DT_SEGMENT;
          time_var = dt_max - dt;
        } else {
          break;
        }
      }
    } while (mm_remaining > prep.mm_complete);

    float step_dist_remaining = prep.step_per_mm * mm_remaining;
    float n_steps_remaining = ceilf(step_dist_remaining);
    float last_n_steps_remaining = ceilf(prep.steps_remaining);
    prep_segment->n_step = (uint32_t)(last_n_steps_remaining - n_steps_remaining);

    if (prep_segment->n_step == 0) {
      if (step_control & STEP_CONTROL_EXECUTE_HOLD) {
        // Less than one step left to decelerate: the hold ends here.
        step_control |= STEP_CONTROL_END_MOTION;
        return;
      }
    }

    dt += prep.dt_remainder;
    float inv_rate = dt / (last_n_steps_remaining - step_dist_remaining);  // min/step

    float cycles_f = ceilf((float)settings.timerHz * 60.0f * inv_rate);
    uint32_t cycles = cycles_f > 4.0e9f ? 4000000000UL : (uint32_t)cycles_f;
    uint32_t level1 = settings.timerHz / 8000, level2 = settings.timerHz / 4000, level3 = settings.timerHz / 2000;
    if (cycles < level1) {
      prep_segment->amass_level = 0;
    } else {
      if (cycles < level2) prep_segment->amass_level = 1;
      else if (cycles < level3) prep_segment->amass_level = 2;
      else prep_segment->amass_level = 3;
      cycles >>= prep_segment->amass_level;
      prep_segment->n_step <<= prep_segment->amass_level;
    }
    prep_segment->cycles_per_tick = cycles;

    // Hand the segment to the interrupt: everything it reads is written before the head moves.
    CNC_BARRIER();
    segment_buffer_head = segment_next_head;
    if (++segment_next_head == CNC_SEGMENT_BUFFER_SIZE) segment_next_head = 0;

    pl_block->millimeters = mm_remaining;
    prep.steps_remaining = n_steps_remaining;
    prep.dt_remainder = (n_steps_remaining - step_dist_remaining) * inv_rate;

    if (mm_remaining == prep.mm_complete) {
      if (mm_remaining > 0.0f) {
        // End of a hold inside the block: keep the rest of it for the resume.
        step_control |= STEP_CONTROL_END_MOTION;
        return;
      }
      if (cnc_st_block_event) cnc_st_block_event(pl_block->line, pl_block->condition, false, prep.current_speed);
      pl_block = NULL;
      plan_discard_current_block();
    }
  }
}

bool cnc_st_busy() {
  return segment_buffer_head != segment_buffer_tail || exec_active || block_buffer_head != block_buffer_tail;
}

float cnc_st_realtime_rate() { return cnc_st_busy() ? prep.current_speed : 0.0f; }
uint32_t cnc_st_current_line() { return exec_line; }
uint32_t cnc_st_underruns() { return underruns; }

// ============================================================================ overrides

void cnc_set_overrides(uint8_t feed, uint8_t rapid) {
  if (feed < CNC_FEED_OVERRIDE_MIN) feed = CNC_FEED_OVERRIDE_MIN;
  if (feed > CNC_FEED_OVERRIDE_MAX) feed = CNC_FEED_OVERRIDE_MAX;
  if (rapid != 100 && rapid != 50 && rapid != 25) rapid = 100;
  if (feed == feed_override && rapid == rapid_override) return;
  feed_override = feed;
  rapid_override = rapid;
  plan_update_velocity_profile_parameters();
  // In a hold the rest is re-planned on resume; otherwise at once, from the block being executed.
  if (!(step_control & STEP_CONTROL_EXECUTE_HOLD)) plan_cycle_reinitialize();
}

uint8_t cnc_feed_override() { return feed_override; }
uint8_t cnc_rapid_override() { return rapid_override; }

// ============================================================================ feed hold

void cnc_hold_start() {
  if (step_control & STEP_CONTROL_EXECUTE_HOLD) return;
  step_control |= STEP_CONTROL_EXECUTE_HOLD;
  st_update_plan_block_parameters();  // re-profile the executing block with a deceleration to zero
}

bool cnc_hold_active() { return (step_control & STEP_CONTROL_EXECUTE_HOLD) != 0; }

bool cnc_hold_complete() {
  if (!(step_control & STEP_CONTROL_EXECUTE_HOLD)) return false;
  // Stopped when the interrupt has nothing left: either the hold ended (END_MOTION) or the program ran out.
  bool drained = segment_buffer_head == segment_buffer_tail && !exec_active;
  return drained && ((step_control & STEP_CONTROL_END_MOTION) || block_buffer_head == block_buffer_tail);
}

void cnc_hold_resume() {
  if (!cnc_hold_complete()) return;
  // As Grbl does on cycle stop + cycle start: re-plan the rest from zero speed and continue.
  plan_cycle_reinitialize();
  step_control = STEP_CONTROL_NORMAL_OP;
}

// ============================================================================ interrupt

CNC_ISR_ATTR bool cnc_isr(cnc_tick_t* out) {
  if (st.exec_segment == NULL) {
    if (segment_buffer_head != segment_buffer_tail) {
      CNC_BARRIER();
      st.exec_segment = &segment_buffer[segment_buffer_tail];
      st.step_count = st.exec_segment->n_step;
      if (st.exec_block == NULL || st.exec_block_index != st.exec_segment->st_block_index) {
        st.exec_block_index = st.exec_segment->st_block_index;
        st.exec_block = &st_block_buffer[st.exec_block_index];
        for (int i = 0; i < CNC_N_AXIS; i++) st.counter[i] = st.exec_block->step_event_count >> 1;
        exec_line = st.exec_block->line;
      }
      for (int i = 0; i < CNC_N_AXIS; i++) st.steps[i] = st.exec_block->steps[i] >> st.exec_segment->amass_level;
      exec_active = true;
      if (st.step_count == 0) {
        // A segment without steps (very slow motion): only its time passes.
        out->step = 0;
        out->dirNeg = st.exec_block->direction_bits;
        out->motorOnly = 0;
        out->period = st.exec_segment->cycles_per_tick;
        st.exec_segment = NULL;
        if (segment_buffer_tail + 1 == CNC_SEGMENT_BUFFER_SIZE) segment_buffer_tail = 0;
        else segment_buffer_tail = segment_buffer_tail + 1;
        return true;
      }
    } else {
      if (exec_active && block_buffer_head != block_buffer_tail && !(step_control & STEP_CONTROL_EXECUTE_HOLD)) underruns = underruns + 1;
      exec_active = false;
      out->step = 0;
      out->period = settings.timerHz / 1000;  // idle: look again in 1 ms
      return false;
    }
  }

  uint8_t bits = 0;
  for (int i = 0; i < CNC_N_AXIS; i++) {
    st.counter[i] += st.steps[i];
    if (st.counter[i] > st.exec_block->step_event_count) {
      bits |= (1 << i);
      st.counter[i] -= st.exec_block->step_event_count;
    }
  }
  out->step = bits;
  out->dirNeg = st.exec_block->direction_bits;
  out->motorOnly = (st.exec_block->condition & CNC_COND_BACKLASH) ? 1 : 0;
  out->period = st.exec_segment->cycles_per_tick;

  if (--st.step_count == 0) {
    st.exec_segment = NULL;
    if (segment_buffer_tail + 1 == CNC_SEGMENT_BUFFER_SIZE) segment_buffer_tail = 0;
    else segment_buffer_tail = segment_buffer_tail + 1;
  }
  return true;
}
