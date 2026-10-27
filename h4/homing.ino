// homing.ino - home switches, soft limits and driver alarm for the GCODE mode of the NanoEls H4 CNC fork.
// https://github.com/Marioskiv/nanoels-cnc
//
// Works like LinuxCNC with the same meanings: HOME_SEARCH_VEL (sign = towards the switch), HOME_LATCH_VEL
// (opposite sign: leave the switch slowly; same sign: back off, then approach slowly), HOME_FINAL_VEL,
// HOME_OFFSET (machine coordinate of the switch point), HOME (where to go after homing), MIN_LIMIT and
// MAX_LIMIT (soft limits in machine coordinates, active once homed). Home All: Z, then X (or X, then Z),
// then A1 (C) takes machine 0 where it stands (as HOME_SEARCH_VEL = 0 in LinuxCNC).
//
// Inputs on terminal A2: A21 home switch X, A22 home switch Z, A23 driver alarm (see CNC.md for the wiring).
// loop() reads them on every pass (cncInputsLoop), so a switch stops the axis within a fraction of a
// millisecond. A home switch that is hit outside homing, or a driver alarm, stops all motion at once
// (no deceleration, as a limit in LinuxCNC); the positions stay exact because every step given is counted.
//
// With all of it switched off (the defaults) nothing here acts and the H4 behaves as the original.
//
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)

const unsigned long INPUT_DEBOUNCE_US = 500; // a level must stay this long to count
bool inStable[3] = {true, true, true};       // debounced levels (pull-ups: high when nothing is connected)
bool inLast[3] = {true, true, true};
unsigned long inChangedUs[3] = {0, 0, 0};
bool prevHomeX = false, prevHomeZ = false, prevAlarm = false;

bool homingConfigured() {
  return HOME_X_USE || HOME_Z_USE || ALARM_USE || LIMIT_X_USE || LIMIT_Z_USE;
}

float axisStepsPerMm(Axis* a) {
  return a->motorSteps / (a->screwPitch / 10000.0);
}

bool switchUsed(Axis* a) {
  return a == &x ? HOME_X_USE : (a == &z ? HOME_Z_USE : false);
}

bool switchActive(Axis* a) {
  return a == &x ? inHomeX : (a == &z ? inHomeZ : false);
}

float homeSearchVel(Axis* a) {
  return a == &x ? HOME_X_SEARCH_VEL : HOME_Z_SEARCH_VEL;
}

// Soft limits of an axis in its pos steps; false when they do not apply (not used or not homed).
bool softLimitSteps(Axis* a, long* lo, long* hi) {
  bool use = a == &x ? LIMIT_X_USE : (a == &z ? LIMIT_Z_USE : false);
  if (!use || !a->homed) return false;
  float spm = axisStepsPerMm(a);
  float mn = a == &x ? LIMIT_X_MIN : LIMIT_Z_MIN;
  float mx = a == &x ? LIMIT_X_MAX : LIMIT_Z_MAX;
  *lo = lround(mn * spm) - a->machineOffset;
  *hi = lround(mx * spm) - a->machineOffset;
  return true;
}

bool softLimitsActive() {
  long lo, hi;
  return softLimitSteps(&x, &lo, &hi) || softLimitSteps(&z, &lo, &hi);
}

// After a limit or an alarm stopped the machine: no new legacy moves until taskGcode has reset the planner.
bool motionBlocked() {
  return cncFaultStop || (ALARM_USE && inAlarm);
}

// Jog (keys, handwheels): not past the soft limits and not further into a pressed home switch.
int cncClampJog(Axis* a, long posCopy, int delta) {
  if (homingActive) return 0; // the homing task moves the axes now
  long lo, hi;
  if (softLimitSteps(a, &lo, &hi)) {
    if (posCopy + delta > hi) delta = max(0L, hi - posCopy);
    if (posCopy + delta < lo) delta = min(0L, lo - posCopy);
  }
  if (switchUsed(a) && switchActive(a) && !homingActive) {
    int towards = homeSearchVel(a) > 0 ? 1 : -1;
    if (delta * towards > 0) delta = 0;
  }
  return delta;
}

// Why a G-code program may not start now, or NULL.
const char* cncStartRefusal() {
  if (ALARM_USE && inAlarm) return "Driver alarm";
  if (HOME_X_USE && HOME_X_IS_LIMIT && inHomeX) return "X on its switch";
  if (HOME_Z_USE && HOME_Z_IS_LIMIT && inHomeZ) return "Z on its switch";
  if (homingActive) return "Homing";
  if (HOME_REQUIRED && ((HOME_X_USE && !x.homed) || (HOME_Z_USE && !z.homed))) return "Home All first";
  return NULL;
}

// Stops an axis driven by moveAxis() where it is (called from loop(), which also runs moveAxis()).
void stopAxisNow(Axis* a) {
  a->pendingPos = 0;
  a->continuous = false;
}

void cncFault(const char* text, bool unhome) {
  cncFaultText = text;
  cncFaultStop = true; // the step interrupt stops now
  stopAxisNow(&x);
  stopAxisNow(&z);
  stopAxisNow(&a1);
  cncThreadState = CNC_THREAD_NONE;
  homingAbort = true;
  if (unhome) z.homed = x.homed = a1.homed = false;
  if (isOn) setIsOnFromLoop(false);
  cncStatusText = text;
  cncLog(text);
  beepFlag = true;
}

// Called on every pass of loop(), holding motionMutex.
void cncInputsLoop() {
  uint32_t levels = REG_READ(GPIO_IN_REG);
  unsigned long now = micros();
  const int pins[3] = {IN_HOME_X, IN_HOME_Z, IN_ALARM};
  uint8_t raw = 0;
  for (int i = 0; i < 3; i++) {
    bool level = (levels >> pins[i]) & 1;
    if (level != inLast[i]) {
      inLast[i] = level;
      inChangedUs[i] = now;
    } else if (level != inStable[i] && now - inChangedUs[i] >= INPUT_DEBOUNCE_US) {
      inStable[i] = level;
    }
    if (inStable[i]) raw |= 1 << i;
  }
  inRaw = raw;
  if (!homingConfigured() || PULSE_2_USE) return;
  // Normally open switch to GND: pressed = low. Normally closed: pressed (or a broken wire) = high.
  inHomeX = HOME_X_USE && (inStable[0] == HOME_X_NC);
  inHomeZ = HOME_Z_USE && (inStable[1] == HOME_Z_NC);
  inAlarm = ALARM_USE && (inStable[2] == !ALARM_WHEN_LOW);

  Axis* watch = homingWatchAxis;
  if (watch != NULL && !homingHit && switchActive(watch) == homingWantActive) {
    stopAxisNow(watch);
    homingHitPos = watch->pos;
    homingHit = true;
  }
  if (inAlarm && !prevAlarm) {
    cncFault("Driver alarm", true); // a driver that stopped has lost its position
  } else if (inHomeX && !prevHomeX && HOME_X_IS_LIMIT && watch != &x) {
    cncFault("Limit X", false);
  } else if (inHomeZ && !prevHomeZ && HOME_Z_IS_LIMIT && watch != &z) {
    cncFault("Limit Z", false);
  }
  prevAlarm = inAlarm;
  prevHomeX = inHomeX;
  prevHomeZ = inHomeZ;
}

// ------------------------------------------------------------------ Home All (own task, started from the page)

bool homingWaitMotion(Axis* a) {
  while (a->pendingPos != 0) {
    if (homingAbort || emergencyStop != ESTOP_NONE) return false;
    vTaskDelay(1);
  }
  return !homingAbort;
}

// Moves the axis in direction dir at vel mm/s until its switch is active (wantActive) or released,
// at most maxMm. True when the switch was reached; homingHitPos is the position there.
bool homingMoveUntil(Axis* a, int dir, float vel, bool wantActive, float maxMm) {
  float spm = axisStepsPerMm(a);
  homingHit = false;
  if (switchActive(a) == wantActive) {
    homingHitPos = a->pos;
    return true;
  }
  a->speedMax = max(1L, lround(fabs(vel) * spm));
  long steps = lround(maxMm * spm);
  if (!stepToFinal(a, a->pos + dir * steps)) return false;
  homingWantActive = wantActive;
  homingWatchAxis = a;
  while (!homingHit) {
    if (homingAbort || emergencyStop != ESTOP_NONE) break;
    if (a->pendingPos == 0) {
      vTaskDelay(20); // the last step may be in loop() right now
      break;
    }
    vTaskDelay(1);
  }
  homingWatchAxis = NULL;
  if (!homingHit) stepToFinal(a, a->pos); // stop
  homingWaitMotion(a);
  vTaskDelay(50 / portTICK_PERIOD_MS); // let the carriage settle before the next move
  return homingHit && !homingAbort;
}

bool homeAxis(Axis* a, float search, float latch, float finalVel, float offset, float home, float maxTravelMm) {
  float spm = axisStepsPerMm(a);
  int dir = search > 0 ? 1 : -1;
  const float backOffMm = 20;
  bool ok = true;
  a->movingManually = true;
  stepperEnable(a, true);
  if (switchActive(a)) ok = homingMoveUntil(a, -dir, search, false, backOffMm);
  if (ok) ok = homingMoveUntil(a, dir, search, true, maxTravelMm);
  if (ok) {
    if (latch * search < 0) {
      ok = homingMoveUntil(a, -dir, latch, false, backOffMm);
    } else {
      ok = homingMoveUntil(a, -dir, search, false, backOffMm) && homingMoveUntil(a, dir, latch, true, backOffMm + 5);
    }
  }
  if (ok) {
    a->machineOffset = lround(offset * spm) - homingHitPos;
    a->homed = true;
    a->speedMax = finalVel > 0 ? lround(finalVel * spm) : a->speedManualMove;
    ok = stepToFinal(a, lround(home * spm) - a->machineOffset) && homingWaitMotion(a);
  }
  a->speedMax = LONG_MAX;
  a->movingManually = false;
  stepperEnable(a, false);
  return ok;
}

bool homingOne(Axis* a, const char* searching) {
  if (!switchUsed(a)) return true;
  cncStatusText = searching;
  bool isX = a == &x;
  float travel = (isX ? MAX_TRAVEL_MM_X : MAX_TRAVEL_MM_Z) * 1.2 + 20;
  bool ok = homeAxis(a, isX ? HOME_X_SEARCH_VEL : HOME_Z_SEARCH_VEL, isX ? HOME_X_LATCH_VEL : HOME_Z_LATCH_VEL,
      isX ? HOME_X_FINAL_VEL : HOME_Z_FINAL_VEL, isX ? HOME_X_OFFSET : HOME_Z_OFFSET, isX ? HOME_X_HOME : HOME_Z_HOME, travel);
  if (!ok && !homingAbort) {
    const char* text = isX ? "Homing X: switch not found" : "Homing Z: switch not found";
    cncLog(text);
    cncStatusText = text;
  }
  return ok;
}

void homingRun() {
  homingAbort = false;
  if (mode != MODE_GCODE) {
    setModeFromTask(MODE_GCODE);
    unsigned long t0 = millis();
    while (mode != MODE_GCODE && millis() - t0 < 500) vTaskDelay(5);
  }
  const char* refusal = NULL;
  if (mode != MODE_GCODE) refusal = "Homing: not in GCODE mode";
  else if (isOn || cncBusy() || cncStopRequest) refusal = "Homing: stop the program first";
  else if (ALARM_USE && inAlarm) refusal = "Homing: driver alarm";
  else if (emergencyStop != ESTOP_NONE) refusal = "Homing: emergency stop";
  if (refusal != NULL) {
    cncStatusText = refusal;
    cncLog(refusal);
    return;
  }
  homingActive = true;
  x.homed = z.homed = a1.homed = false;
  bool ok = HOME_Z_FIRST ? (homingOne(&z, "Homing Z") && homingOne(&x, "Homing X"))
                         : (homingOne(&x, "Homing X") && homingOne(&z, "Homing Z"));
  if (ok && a1.active) {
    a1.machineOffset = -a1.pos; // C: machine 0 where it stands
    a1.homed = true;
  }
  homingActive = false;
  if (ok) {
    cncStatusText = "Homed";
    cncLog("Home All done");
  } else if (homingAbort && !cncFaultStop && strcmp(cncStatusText, cncFaultText) != 0) {
    cncStatusText = "Homing stopped";
    cncLog("Homing stopped");
  }
}

void taskHoming(void* param) {
  while (emergencyStop == ESTOP_NONE) {
    if (homingRequest) {
      homingRequest = false;
      homingRun();
    }
    vTaskDelay(10);
  }
  vTaskDelete(NULL);
}

// Machine coordinate of an axis in mm (or degrees), for the page.
float machinePosMm(Axis* a) {
  return (a->pos + a->machineOffset) / axisStepsPerMm(a);
}
