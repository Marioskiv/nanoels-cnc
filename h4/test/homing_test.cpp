// homing_test.cpp - Home All, home switches as limits, driver alarm and soft limits of the H4 CNC fork, on a PC.
// The firmware's own homing.ino is compiled unchanged; around it a small machine is simulated: time, two axes that
// move at their speed when given a target (as the H4's moveAxis), switches at a physical place with contact
// bounce, electrical noise, a broken wire and a driver alarm. loop() reads the inputs every 50 us as on the H4.
//   g++ -O2 -std=c++17 -o homing_test homing_test.cpp && ./homing_test
// https://github.com/Marioskiv/nanoels-cnc - Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <string>
using std::max;
using std::min;

// ------------------------------------------------------------------ what homing.ino uses from h4.ino
struct Axis {
  char name;
  bool active;
  float motorSteps, screwPitch;
  long pos, motorPos;
  int pendingPos;
  bool continuous;
  long speedMax, speedManualMove;
  bool movingManually;
  long machineOffset;
  bool homed;
  bool enabled;
};
Axis x, z, a1;
long MAX_TRAVEL_MM_X = 135, MAX_TRAVEL_MM_Z = 600;
bool PULSE_2_USE = false;
bool HOME_X_USE, HOME_X_NC, HOME_X_IS_LIMIT, LIMIT_X_USE, HOME_Z_USE, HOME_Z_NC, HOME_Z_IS_LIMIT, LIMIT_Z_USE;
float HOME_X_SEARCH_VEL, HOME_X_LATCH_VEL, HOME_X_FINAL_VEL, HOME_X_OFFSET, HOME_X_HOME, LIMIT_X_MIN, LIMIT_X_MAX;
float HOME_Z_SEARCH_VEL, HOME_Z_LATCH_VEL, HOME_Z_FINAL_VEL, HOME_Z_OFFSET, HOME_Z_HOME, LIMIT_Z_MIN, LIMIT_Z_MAX;
bool HOME_Z_FIRST, HOME_REQUIRED, ALARM_USE, ALARM_WHEN_LOW;
#define IN_HOME_X 12
#define IN_HOME_Z 13
#define IN_ALARM 14
#define MODE_GCODE 9
#define CNC_THREAD_NONE 0
#define ESTOP_NONE 0
#define portTICK_PERIOD_MS 1
#define GPIO_IN_REG 0
volatile uint8_t inRaw = 0;
volatile bool inHomeX, inHomeZ, inAlarm, cncHomeOverride, cncFaultStop, cncStopRequest;
volatile uint32_t cncFaultCount;
const char* volatile cncFaultText = "";
const char* volatile cncStatusText = "";
volatile bool homingRequest, homingAbort, homingActive;
Axis* volatile homingWatchAxis = NULL;
volatile bool homingWantActive, homingHit;
volatile long homingHitPos;
volatile int cncThreadState = CNC_THREAD_NONE;
volatile bool beepFlag;
bool isOn = false;
int mode = MODE_GCODE;
int emergencyStop = ESTOP_NONE;
bool busy = false;
bool cncBusy() { return busy; }
std::vector<std::string> logLines;
void cncLog(const char* t) { logLines.push_back(t); }
void setIsOnFromLoop(bool on) { isOn = on; }
void setModeFromTask(int m) { mode = m; }
void stepperEnable(Axis* a, bool v) { a->enabled = v; }

// ------------------------------------------------------------------ the simulated machine
uint64_t nowUs = 0;
unsigned long micros() { return (unsigned long)nowUs; }
unsigned long millis() { return (unsigned long)(nowUs / 1000); }
struct Switch {           // a switch pressed when the axis is beyond `at` (in pos steps) in direction `dir`
  long at; int dir;
  bool nc;                // normally closed (pressed = open = high)
  bool broken;            // wire broken: always high
  bool disconnected;      // never closes
  uint64_t bounceUs;      // contact bounce after every change
  bool lastPressed; uint64_t changedUs; uint32_t rnd;
  uint64_t noiseFrom, noiseTo;  // a noise pulse that pulls the input low (an NO switch looks pressed)
};
Switch swX, swZ;
bool alarmActive = false;
bool motionBlocked();
bool stepToFinal(Axis* a, long newPos) {
  if (motionBlocked()) return false;
  a->continuous = false;
  a->pendingPos = (int)(newPos - a->pos);
  return true;
}
static bool level(Switch& s, Axis& a) {   // input level (true = high)
  if (s.broken) return true;
  bool pressed = !s.disconnected && (s.dir > 0 ? a.pos >= s.at : a.pos <= s.at);
  if (pressed != s.lastPressed) { s.lastPressed = pressed; s.changedUs = nowUs; }
  if (nowUs - s.changedUs < s.bounceUs) {  // bouncing: random level
    s.rnd = s.rnd * 1103515245 + 12345;
    pressed = (s.rnd >> 16) & 1;
  }
  bool high = s.nc ? pressed : !pressed;
  if (nowUs >= s.noiseFrom && nowUs < s.noiseTo) high = false;
  return high;
}
uint32_t gpioIn() {
  uint32_t v = 0;
  if (level(swX, x)) v |= 1u << IN_HOME_X;
  if (level(swZ, z)) v |= 1u << IN_HOME_Z;
  bool alarmLow = ALARM_WHEN_LOW ? alarmActive : !alarmActive;
  if (!alarmLow) v |= 1u << IN_ALARM;
  return v;
}
#define REG_READ(r) gpioIn()
void vTaskDelete(void*) {}
double stepAcc[2];
int faultUs = 0;
std::function<void()> onTick;  // a scenario's own event (an alarm at some time ...)
void cncInputsLoop();
void stepAxis(Axis* a, double& acc, double dtS) {
  if (a->pendingPos == 0) { acc = 0; return; }
  acc += a->speedMax * dtS;
  while (acc >= 1 && a->pendingPos != 0) {
    int d = a->pendingPos > 0 ? 1 : -1;
    a->pos += d; a->motorPos += d; a->pendingPos -= d; acc -= 1;
  }
}
void vTaskDelay(int ms) {        // the homing task waits; meanwhile loop() runs every 50 us and the axes move
  for (int i = 0; i < ms * 20; i++) {
    nowUs += 50;
    if (!cncFaultStop) { stepAxis(&x, stepAcc[0], 50e-6); stepAxis(&z, stepAcc[1], 50e-6); }
    cncInputsLoop();
    if (cncFaultStop && ++faultUs >= 20) { cncFaultStop = false; faultUs = 0; }  // taskGcode resets (h4.ino:2082)
    if (onTick) onTick();
  }
}

#include "../homing.ino"

// ------------------------------------------------------------------ scenarios
static int failures = 0, checks = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) { printf("FAIL %s\n", what); failures++; }
}
static float mm(Axis& a, long steps) { return steps / axisStepsPerMm(&a); }

// The author's machine (LinuxCNC .ini): X 1200 steps/mm, switch at the X- end, HOME_OFFSET -24.039, limits -22.039
// .. 108.961; Z 400 steps/mm, switch at the Z+ end, HOME_OFFSET 10, limits -590 .. 7.
static void machine(bool xnc = false, bool znc = false) {
  memset(&x, 0, sizeof(x)); memset(&z, 0, sizeof(z)); memset(&a1, 0, sizeof(a1));
  x.name = 'X'; x.active = true; x.motorSteps = 1200; x.screwPitch = 10000;  // 1200 steps per mm
  z.name = 'Z'; z.active = true; z.motorSteps = 400; z.screwPitch = 10000;
  a1.name = 'C'; a1.active = true; a1.motorSteps = 91.733333f * 360; a1.screwPitch = 3600000;
  x.speedMax = z.speedMax = a1.speedMax = LONG_MAX;
  x.speedManualMove = 12 * 1200; z.speedManualMove = 25 * 400;
  HOME_X_USE = HOME_Z_USE = true; HOME_X_NC = xnc; HOME_Z_NC = znc; HOME_X_IS_LIMIT = HOME_Z_IS_LIMIT = true;
  HOME_X_SEARCH_VEL = -3; HOME_X_LATCH_VEL = 0.5; HOME_X_FINAL_VEL = 5; HOME_X_OFFSET = -24.039; HOME_X_HOME = 0;
  HOME_Z_SEARCH_VEL = 3; HOME_Z_LATCH_VEL = -2.5; HOME_Z_FINAL_VEL = 5; HOME_Z_OFFSET = 10; HOME_Z_HOME = 0;
  LIMIT_X_USE = LIMIT_Z_USE = true; LIMIT_X_MIN = -22.039; LIMIT_X_MAX = 108.961; LIMIT_Z_MIN = -590; LIMIT_Z_MAX = 7;
  HOME_Z_FIRST = true; HOME_REQUIRED = true; ALARM_USE = true; ALARM_WHEN_LOW = true;
  inHomeX = inHomeZ = inAlarm = cncHomeOverride = cncFaultStop = cncStopRequest = false;
  homingRequest = homingAbort = homingActive = false; homingWatchAxis = NULL; homingHit = false;
  cncFaultText = ""; cncStatusText = ""; cncFaultCount = 0; isOn = false; busy = false; alarmActive = false;
  logLines.clear(); onTick = nullptr;
  // the switches: X at 30 mm towards X- from where the axis stands, Z at 40 mm towards Z+
  memset(&swX, 0, sizeof(swX)); memset(&swZ, 0, sizeof(swZ));
  swX.at = -30 * 1200; swX.dir = -1; swX.nc = xnc; swX.bounceUs = 2000; swX.rnd = 1; swX.lastPressed = false;
  swZ.at = 40 * 400; swZ.dir = 1; swZ.nc = znc; swZ.bounceUs = 2000; swZ.rnd = 7;
  swX.noiseFrom = swX.noiseTo = swZ.noiseFrom = swZ.noiseTo = 0;
  // the inputs as they are now (as after the first 0.1 s of a real start)
  uint32_t v = gpioIn();
  const int pins[3] = {IN_HOME_X, IN_HOME_Z, IN_ALARM};
  for (int i = 0; i < 3; i++) inStable[i] = inLast[i] = (v >> pins[i]) & 1;
  prevHomeX = prevHomeZ = prevAlarm = false;
  faultUs = 0;
  nowUs += 200000; vTaskDelay(150);
  cncFaultCount = 0;
}

// After Home All the machine coordinate of the switch point must be HOME_OFFSET (within the debounce time at the
// latch speed and one step) and the axis must stand at HOME.
static void expectHomed(Axis& a, Switch& s, float offset, float home, const char* what) {
  char buf[200];
  float switchMachine = mm(a, s.at + a.machineOffset);
  // the switch is seen when its contact has stopped bouncing and stayed 0.5 ms (debounce): at the latch speed
  float latch = fabsf(&a == &x ? HOME_X_LATCH_VEL : HOME_Z_LATCH_VEL);
  float tol = latch * (s.bounceUs + 600) * 1e-6f + 2.0f / axisStepsPerMm(&a);
  snprintf(buf, sizeof(buf), "%s: switch at machine %.4f mm, want %.4f +- %.4f", what, switchMachine, offset, tol);
  check(a.homed && fabsf(switchMachine - offset) <= tol, buf);
  snprintf(buf, sizeof(buf), "%s: stands at machine %.4f mm, want %.4f", what, mm(a, a.pos + a.machineOffset), home);
  check(fabsf(mm(a, a.pos + a.machineOffset) - home) <= 1.0f / axisStepsPerMm(&a) + 1e-4f, buf);
}

int main() {
  // 1. Home All, normally open switches with 2 ms contact bounce: X and Z homed, switch point = HOME_OFFSET,
  //    the axes at HOME; no limit fault from the bounce; programs may start afterwards.
  machine();
  check(cncStartRefusal() != NULL && !strcmp(cncStartRefusal(), "Home All first"), "before homing: Home All first");
  homingRun();
  expectHomed(x, swX, HOME_X_OFFSET, HOME_X_HOME, "NO switches, X");
  expectHomed(z, swZ, HOME_Z_OFFSET, HOME_Z_HOME, "NO switches, Z");
  check(a1.homed && a1.machineOffset == -a1.pos, "C takes machine 0 where it stands");
  check(cncFaultCount == 0, "no limit fault from the bounce during homing");
  check(cncStartRefusal() == NULL, "after Home All a program may start");
  check(!strcmp(cncStatusText, "Homed"), "status Homed");

  // 2. Normally closed switches: the same result.
  machine(true, true);
  homingRun();
  expectHomed(x, swX, HOME_X_OFFSET, HOME_X_HOME, "NC switches, X");
  expectHomed(z, swZ, HOME_Z_OFFSET, HOME_Z_HOME, "NC switches, Z");

  // 3. X already on its switch at the start: it backs off first, then homes normally; a program may not start
  //    while an axis rests on its switch.
  machine();
  x.pos = x.motorPos = swX.at - 100;  // pushed onto it by hand: that is a limit hit (as LinuxCNC)
  vTaskDelay(5);
  check(cncFaultCount == 1 && !strcmp(cncFaultText, "Limit X"), "pushed onto its switch: Limit X");
  check(cncStartRefusal() != NULL && !strcmp(cncStartRefusal(), "X on its switch"), "X on its switch: refused");
  homingRun();
  expectHomed(x, swX, HOME_X_OFFSET, HOME_X_HOME, "starting on the switch, X");

  // 4. NC switch with a broken wire (input always high = pressed): homing fails, nothing homed, programs refused.
  machine(true, false);
  swX.broken = true;
  vTaskDelay(5);
  homingRun();
  check(!x.homed, "broken NC wire: X not homed");
  check(strstr(cncStatusText, "Homing X: the switch stays pressed") != NULL, "broken NC wire: says the switch stays pressed");
  check(cncStartRefusal() != NULL, "broken NC wire: programs refused");

  // 5. NO switch disconnected (never closes): the axis travels its whole range, stops, reports, not homed.
  machine();
  swZ.disconnected = true;
  homingRun();
  check(!z.homed && !x.homed, "disconnected switch: not homed");
  check(!strcmp(cncStatusText, "Homing Z: switch not found"), "disconnected switch: message");
  check(fabsf(mm(z, z.pos)) <= MAX_TRAVEL_MM_Z * 1.2f + 20 + 0.01f, "disconnected switch: stops after its range");

  // 6. Noise on a limit input while a program runs: a pulse shorter than the 0.5 ms debounce is ignored, a longer
  //    one stops everything ("Limit X", positions kept, still homed).
  machine();
  homingRun();
  busy = true;
  swX.noiseFrom = nowUs + 1000; swX.noiseTo = swX.noiseFrom + 300;
  vTaskDelay(5);
  check(cncFaultCount == 0, "noise of 0.3 ms: no limit fault");
  swX.noiseFrom = nowUs + 1000; swX.noiseTo = swX.noiseFrom + 900;
  vTaskDelay(5);
  check(cncFaultCount == 1 && !strcmp(cncFaultText, "Limit X"), "0.9 ms on the input: Limit X");
  check(x.homed, "a limit keeps the axes homed (every step given is counted)");
  busy = false;

  // 7. Driver alarm in the middle of homing: everything stops at once, nothing homed, "Driver alarm".
  machine();
  uint64_t t0 = nowUs;
  onTick = [t0]() { if (nowUs > t0 + 1500000) alarmActive = true; };
  homingRun();
  check(!x.homed && !z.homed, "alarm during homing: not homed");
  check(cncFaultCount == 1 && !strcmp(cncFaultText, "Driver alarm or E-stop"), "alarm during homing: Driver alarm or E-stop");
  // homing refuses to start while the alarm is there
  onTick = nullptr;
  cncFaultStop = false;
  homingRun();
  check(!strcmp(cncStatusText, "Homing: driver alarm or E-stop"), "homing refused while the alarm is on");
  check(cncStartRefusal() != NULL && !strcmp(cncStartRefusal(), "Driver alarm or E-stop"), "programs refused while the alarm is on");

  // 8. STOP during homing: the axis stops, nothing homed, "Homing stopped".
  machine();
  t0 = nowUs;
  onTick = [t0]() { if (nowUs > t0 + 800000) homingAbort = true; };
  homingRun();
  check(!x.homed && !z.homed, "STOP during homing: not homed");
  check(!strcmp(cncStatusText, "Homing stopped"), "STOP during homing: message");

  // 9. Homing refused while a program runs.
  machine();
  isOn = true;
  homingRun();
  check(!strcmp(cncStatusText, "Homing: stop the program first"), "homing refused while a program runs");
  isOn = false;

  // 10. Soft limits once homed: in machine coordinates, the limits of the .ini; jogging is clamped to them and
  //     never further into a pressed switch; before homing there are none.
  machine();
  long lo, hi;
  check(!softLimitSteps(&x, &lo, &hi), "no soft limits before homing");
  homingRun();
  check(softLimitSteps(&x, &lo, &hi), "soft limits after homing");
  check(fabsf(mm(x, lo + x.machineOffset) - LIMIT_X_MIN) < 1e-3f && fabsf(mm(x, hi + x.machineOffset) - LIMIT_X_MAX) < 1e-3f,
        "X soft limits = MIN_LIMIT / MAX_LIMIT in machine coordinates");
  check(cncClampJog(&x, hi - 10, 100) == 10, "jog clamped at the upper soft limit");
  check(cncClampJog(&x, lo + 10, -100) == -10, "jog clamped at the lower soft limit");
  x.pos = x.motorPos = swX.at - 5;  // on the switch (past the limit, e.g. pushed by hand)
  vTaskDelay(5);
  check(cncClampJog(&x, x.pos, -1) == 0, "no jog further into a pressed switch");
  check(cncClampJog(&x, x.pos, 1) == 1, "jog away from a pressed switch, back inside the soft limits");
  x.pos = x.motorPos = hi + 500;  // beyond the upper soft limit
  vTaskDelay(5);
  check(cncClampJog(&x, x.pos, -1) == -1 && cncClampJog(&x, x.pos, 1) == 0, "outside the upper limit: back in only");

  // 11. X first (HOME_SEQUENCE the other way) and latch in the search direction (back off, approach slowly).
  machine();
  HOME_Z_FIRST = false;
  HOME_X_LATCH_VEL = -0.5;
  homingRun();
  expectHomed(x, swX, HOME_X_OFFSET, HOME_X_HOME, "X first, latch towards the switch, X");
  expectHomed(z, swZ, HOME_Z_OFFSET, HOME_Z_HOME, "X first, Z");

  printf("homing: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
