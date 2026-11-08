/*
  cnc_encwatch.cpp - does the chuck follow the C axis? (see cnc_encwatch.h)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "cnc_encwatch.h"
#include <math.h>

static double rawDiff(const cnc_encwatch_t* w, int64_t encCount, int64_t steps) {
  return (double)encCount / w->cfg.countsPerDeg - (double)steps / w->cfg.stepsPerDeg;
}

void cnc_encwatch_reset(cnc_encwatch_t* w, const cnc_encwatch_cfg_t* cfg, int64_t encCount, int64_t steps, uint32_t nowMs) {
  uint32_t events = w->events;
  float worst = w->worstDeg, ratio = w->worstRatio, lastEvent = w->lastEventDeg;
  bool keep = w->cfg.countsPerDeg > 0 && cfg == &w->cfg;  // a restart after an event keeps the counts
  w->cfg = *cfg;
  w->offsetDeg = rawDiff(w, encCount, steps);
  w->lastSteps = steps;
  w->lastMs = nowMs;
  w->speedDegS = 0;
  w->overNow = false;
  w->overSinceMs = nowMs;
  w->diffDeg = 0;
  w->events = keep ? events : 0;
  w->worstDeg = keep ? worst : 0;
  w->worstRatio = keep ? ratio : 0;
  w->lastEventDeg = keep ? lastEvent : 0;
}

bool cnc_encwatch_sample(cnc_encwatch_t* w, int64_t encCount, int64_t steps, uint32_t nowMs) {
  uint32_t dtMs = nowMs - w->lastMs;
  if (dtMs > 0) {
    float v = (float)((double)(steps - w->lastSteps) / w->cfg.stepsPerDeg / (dtMs / 1000.0));
    // smoothed over a few readings; a faster rise follows a start at once (the allowed difference must not lag)
    w->speedDegS = fabsf(v) > fabsf(w->speedDegS) ? v : w->speedDegS + 0.3f * (v - w->speedDegS);
    w->lastSteps = steps;
    w->lastMs = nowMs;
  }
  double d = rawDiff(w, encCount, steps) - w->offsetDeg;
  w->diffDeg = (float)d;
  float allowed = w->cfg.tolDeg + fabsf(w->speedDegS) * w->cfg.lagS;
  float ratio = (float)fabs(d) / allowed;
  if (!w->overNow && ratio > w->worstRatio) w->worstRatio = ratio;
  if (fabs(d) <= allowed) {
    if (!w->overNow && fabs(d) > w->worstDeg) w->worstDeg = (float)fabs(d);
    w->overNow = false;
    return false;
  }
  if (!w->overNow) {
    w->overNow = true;
    w->overSinceMs = nowMs;
    return false;
  }
  if (nowMs - w->overSinceMs < w->cfg.holdMs) return false;
  // an event: counted once, then the difference counts from here (the next skip is a new event)
  w->events++;
  w->lastEventDeg = (float)d;
  w->offsetDeg += d;
  w->overNow = false;
  return true;
}
