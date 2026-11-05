// moveAxis() step pulses (jog, handwheels, Home All, G32 / G33 and the lathe modes): the firmware's own moveAxis(),
// setDir() and moveCyclesPerUs(), taken unchanged from h4.ino, run on a simulated CPU clock. Every pin write is
// recorded with its time. In the GCODE mode every step pulse must last at least CNC_STEP_PULSE_US, the direction pin
// must keep its level DIRECTION_HOLD_US after the last pulse ended and be set DIRECTION_SETUP_DELAY_US before the next
// pulse; the steps and positions must add up. In a lathe mode nothing must wait (the original pulse).
//   g++ -std=c++17 -I<dir with move_funcs.inc> move_pulse_test.cpp   (run_tests.sh makes move_funcs.inc)
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

typedef void* SemaphoreHandle_t;
static int xSemaphoreTake(SemaphoreHandle_t, int) { return 1; }
static void xSemaphoreGive(SemaphoreHandle_t) {}
#define pdTRUE 1

// ---- simulated CPU: 240 cycles per microsecond
static uint64_t cycles = 0;
static const uint64_t CPU_MHZ = 240;
static uint32_t esp_cpu_get_cycle_count() { cycles += 12; return (uint32_t)cycles; }  // a read takes 50 ns
static unsigned long micros() { return (unsigned long)(cycles / CPU_MHZ); }
static void delayMicroseconds(uint32_t us) { cycles += us * CPU_MHZ; }
struct PinEvent { uint64_t t; int pin; int level; };
static std::vector<PinEvent> events;
static void digitalWrite(int pin, int level) { cycles += 48; events.push_back({cycles, pin, level}); }  // 0.2 us
#define HIGH 1
#define LOW 0
#define DHIGH(x) digitalWrite(x, HIGH)
#define DLOW(x) digitalWrite(x, LOW)
#define DWRITE(x, y) digitalWrite(x, y)

// ---- the firmware's settings and state used by these functions
#define MODE_NORMAL 0
#define MODE_TURN 2
#define MODE_GCODE 4
static int mode = MODE_GCODE;
static int CNC_STEP_PULSE_US = 5;
static long DIRECTION_SETUP_DELAY_US = 10;
static long DIRECTION_HOLD_US = 10;
static volatile uint32_t cncCpuCyclesPerUs = 240;
static volatile uint32_t moveDiagPulseMinCycles = UINT32_MAX;
static volatile uint32_t moveDiagPulseMaxCycles = 0;
static volatile uint32_t moveDiagPulses = 0;

#include "move_funcs.inc"

static int failures = 0, checks = 0;
static void check(bool ok, const char* what, double value) {
  checks++;
  if (!ok) {
    failures++;
    if (failures < 20) printf("FAIL: %s (%.3f us)\n", what, value);
  }
}

static Axis makeAxis() {
  Axis a = {};
  a.step = 20;
  a.dir = 19;
  a.speedStart = 400;
  a.speed = 400;
  a.speedMax = LONG_MAX;
  a.acceleration = 100000;
  a.decelerateSteps = 50;
  return a;
}

// Runs moves (signed step counts) through moveAxis(), as loop() does (a pass every 'passUs' microseconds).
static void run(Axis* a, const std::vector<int>& moves, double passUs) {
  for (int m : moves) {
    a->pendingPos = m;
    for (long guard = 0; a->pendingPos != 0 && guard < 50000000; guard++) {
      moveAxis(a);
      cycles += (uint64_t)(passUs * CPU_MHZ);
    }
  }
}

// Pin timing of the recorded writes: pulse widths, direction hold and set-up.
static void checkTiming(const Axis& a, double minPulse, double minHold, double minSetup, const char* name, bool expectShort) {
  uint64_t lastHigh = 0, lowAt = 0, dirAt = 0;
  bool haveHigh = false, pendingDir = false;
  int stepLevel = 1, dirLevel = -1;
  double shortest = 1e9;
  char text[160];
  for (const PinEvent& e : events) {
    double tUs = 0;
    if (e.pin == a.step) {
      if (e.level == LOW && stepLevel == 1) {
        lowAt = e.t;
        if (pendingDir) {
          tUs = (e.t - dirAt) / (double)CPU_MHZ;
          snprintf(text, sizeof(text), "%s: direction set-up before the next pulse", name);
          check(tUs >= minSetup, text, tUs);
          pendingDir = false;
        }
      } else if (e.level == HIGH && stepLevel == 0) {
        tUs = (e.t - lowAt) / (double)CPU_MHZ;
        if (tUs < shortest) shortest = tUs;
        snprintf(text, sizeof(text), "%s: step pulse width", name);
        check(tUs >= minPulse, text, tUs);
        lastHigh = e.t;
        haveHigh = true;
      }
      stepLevel = e.level;
    } else if (e.pin == a.dir) {
      if (dirLevel != -1 && e.level != dirLevel && haveHigh) {
        tUs = (e.t - lastHigh) / (double)CPU_MHZ;
        snprintf(text, sizeof(text), "%s: direction hold after the last pulse", name);
        check(tUs >= minHold, text, tUs);
      }
      if (e.level != dirLevel) { dirAt = e.t; pendingDir = true; }
      dirLevel = e.level;
    }
  }
  if (expectShort) {
    snprintf(text, sizeof(text), "%s: the original pulse, nothing added (shortest)", name);
    check(shortest < 1.0, text, shortest);
  }
  printf("%s: shortest pulse %.2f us\n", name, shortest);
}

int main() {
  std::vector<int> moves = {120, -120, 3, -3, 1, -1, 1, -1, 400, -37, 37, -400};
  long net = 0;
  for (int m : moves) net += m;

  // GCODE mode, slow (Home All latch) and fast (jog at the highest speed, reversals as fast as possible)
  for (int fast = 0; fast < 2; fast++) {
    mode = MODE_GCODE;
    events.clear();
    moveDiagPulseMinCycles = UINT32_MAX; moveDiagPulseMaxCycles = 0; moveDiagPulses = 0;
    Axis a = makeAxis();
    if (fast) { a.speedStart = 1000000; a.speed = 1000000; }  // a step every microsecond if nothing waited
    run(&a, moves, fast ? 0.2 : 5);
    const char* name = fast ? "GCODE fast" : "GCODE slow";
    checkTiming(a, CNC_STEP_PULSE_US, DIRECTION_HOLD_US, DIRECTION_SETUP_DELAY_US, name, false);
    check(a.posGlobal == net && a.motorPos == net && a.pos == net, "GCODE: steps add up", a.posGlobal);
    long steps = 0;
    for (int m : moves) steps += labs(m);
    check(moveDiagPulses == (uint32_t)steps, "GCODE: every pulse measured", moveDiagPulses);
    double minUs = moveDiagPulseMinCycles / 240.0;
    check(minUs >= CNC_STEP_PULSE_US, "GCODE: measured shortest pulse >= CNC_STEP_PULSE_US", minUs);
    // settings of other drivers: 2.5 us pulse, 20 us hold and set-up
    CNC_STEP_PULSE_US = 3; DIRECTION_HOLD_US = 20; DIRECTION_SETUP_DELAY_US = 20;
    events.clear();
    Axis b = makeAxis();
    if (fast) { b.speedStart = 1000000; b.speed = 1000000; }
    run(&b, moves, fast ? 0.2 : 5);
    checkTiming(b, 3, 20, 20, fast ? "GCODE fast 3/20/20" : "GCODE slow 3/20/20", false);
    CNC_STEP_PULSE_US = 5; DIRECTION_HOLD_US = 10; DIRECTION_SETUP_DELAY_US = 10;
  }

  // lathe modes: unchanged (no wait added to the pulse)
  for (int m : {MODE_TURN, MODE_NORMAL}) {
    mode = m;
    events.clear();
    Axis a = makeAxis();
    run(&a, moves, 5);
    checkTiming(a, 0, 0, DIRECTION_SETUP_DELAY_US, m == MODE_TURN ? "lathe (turn)" : "lathe (gearbox)", true);
    check(a.posGlobal == net, "lathe: steps add up", a.posGlobal);
  }

  printf("move pulses: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
