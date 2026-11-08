/*
  follow_test.cpp - the lathe modes' axis following the spindle: the firmware's own moveAxis() (h4.ino, unchanged:
  move_funcs.inc, made by run_tests.sh) on a simulated CPU clock, with the target moving on at a constant speed as the
  spindle moves it (gearbox, threads: stepToContinuous every loop pass). Checked for Z (400 steps/mm, start speed
  1600 steps/s, acceleration 24000 steps/s^2: the H4's defaults) at 500 ... 6000 steps/s and a loop pass every
  2 ... 20 us: the axis keeps up within 2 steps (0.005 mm) once it has reached the speed.
  (On the PC simulation of the whole firmware, test/h4sim, the loop() thread is interrupted for milliseconds by the
  PC, so the following looks irregular there above the start speed; it is not the firmware - this test shows.)
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <initializer_list>

typedef void* SemaphoreHandle_t;
static int xSemaphoreTake(SemaphoreHandle_t, int) { return 1; }
static void xSemaphoreGive(SemaphoreHandle_t) {}
#define pdTRUE 1
static uint64_t cycles = 0;
static const uint64_t CPU_MHZ = 240;
static uint32_t esp_cpu_get_cycle_count() { cycles += 12; return (uint32_t)cycles; }
static unsigned long micros() { return (unsigned long)(cycles / CPU_MHZ); }
static void delayMicroseconds(uint32_t us) { cycles += us * CPU_MHZ; }
static void digitalWrite(int, int) { cycles += 48; }
#define HIGH 1
#define LOW 0
#define DHIGH(x) digitalWrite(x, HIGH)
#define DLOW(x) digitalWrite(x, LOW)
#define DWRITE(x, y) digitalWrite(x, y)
#define MODE_NORMAL 0
#define MODE_GCODE 9
static int mode = MODE_NORMAL;
static int CNC_STEP_PULSE_US = 5;
static long DIRECTION_SETUP_DELAY_US = 10;
static long DIRECTION_HOLD_US = 10;
static volatile uint32_t cncCpuCyclesPerUs = 240, moveDiagPulseMinCycles = UINT32_MAX, moveDiagPulseMaxCycles = 0, moveDiagPulses = 0;
#include "move_funcs.inc"

int main() {
  int checks = 0, failures = 0;
  double worstAll = 0;
  for (double passUs : {2.0, 5.0, 10.0, 20.0}) {
    for (double v : {500.0, 1000.0, 1600.0, 2667.0, 4000.0, 6000.0}) {
      Axis a = {};
      a.step = 18;
      a.dir = 17;
      a.speedStart = 1600;
      a.speed = 1600;
      a.speedMax = LONG_MAX;
      a.acceleration = 24000;
      a.continuous = true;
      cycles = 0;
      double t = 0, worst = 0;
      while (t < 3.0) {
        long target = (long)floor(v * t);
        a.pendingPos = target - a.motorPos;  // as stepToContinuous: the steps still to go to the spindle's place
        moveAxis(&a);
        cycles += (uint64_t)(passUs * CPU_MHZ);
        t = cycles / (CPU_MHZ * 1e6);
        if (t > 1.0) worst = fmax(worst, fabs(floor(v * t) - a.motorPos));  // after reaching the speed
      }
      checks++;
      if (worst > 2) {
        failures++;
        printf("FAIL: loop every %.0f us, Z at %.0f steps/s: %.0f steps behind\n", passUs, v, worst);
      }
      worstAll = fmax(worstAll, worst);
    }
  }
  printf("follow: %d checks, %d failures (the axis at most %.0f steps from the spindle's place)\n", checks, failures, worstAll);
  return failures ? 1 : 0;
}
