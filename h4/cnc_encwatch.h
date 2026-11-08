/*
  cnc_encwatch.h - does the chuck follow the C axis? A rotary encoder on the chuck compared with the C steps given
  (work plan C8: first only logged, no alarm). Not used by the firmware yet; tested on a PC (test/encoder_test.cpp,
  work plan B7) with a simulated encoder: quadrature with noise, contact bounce, belt backlash and stretch, and slips.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later

  difference = encoder angle - C angle (degrees of the chuck), zeroed by cnc_encwatch_reset(). It may be off by the
  encoder's resolution, the backlash of the belts and their stretch while C accelerates, and by the time between
  reading the encoder and the step count; so the allowed difference is tolDeg + |C speed| * lagS. Longer than holdMs
  over it is an event (the chuck did not follow: a belt tooth skipped, C stalled); the difference then counts from the
  new place again, so one skip is one event. No Arduino dependency.
*/
#pragma once
#include <stdint.h>

typedef struct {
  float countsPerDeg;   // encoder counts per degree of the chuck (600 PPR counted on both edges of A: 1200 / 360)
  float stepsPerDeg;    // C steps per degree of the chuck (the author's machine: 91.733333)
  float tolDeg;         // allowed difference at rest (resolution + backlash + margin)
  float lagS;           // allowed lag: the difference may grow by speed * lagS (belt stretch, reading skew)
  uint32_t holdMs;      // over the allowed difference this long = an event
} cnc_encwatch_cfg_t;

typedef struct {
  cnc_encwatch_cfg_t cfg;
  double offsetDeg;     // difference taken as zero
  int64_t lastSteps;
  uint32_t lastMs;
  float speedDegS;      // C speed, smoothed
  bool overNow;
  uint32_t overSinceMs;
  float diffDeg;        // last difference
  float worstDeg;       // largest |difference| seen (not counting events)
  float worstRatio;     // largest |difference| / allowed seen
  uint32_t events;      // times the chuck did not follow
  float lastEventDeg;   // difference at the last event
} cnc_encwatch_t;

// Starts (or restarts after an event, a STOP, Home All) with the difference now taken as zero.
void cnc_encwatch_reset(cnc_encwatch_t* w, const cnc_encwatch_cfg_t* cfg, int64_t encCount, int64_t steps, uint32_t nowMs);
// One reading, about every millisecond. Returns true when an event starts (the difference was too large for holdMs).
bool cnc_encwatch_sample(cnc_encwatch_t* w, int64_t encCount, int64_t steps, uint32_t nowMs);
