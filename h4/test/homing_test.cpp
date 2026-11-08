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
bool HOME_Z_FIRST, HOME_REQUIRED, ALARM_USE, ALARM_WHEN_LOW, LIMIT_UNHOMES = true;
float HOME_CHECK_TOL_MM = 0.05;
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
volatile bool homingRequest, homingAbort, homingActive, homeCheckRequest;
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
#define IRAM_ATTR
// The step interrupt of a running program (h4.ino cncStepTick): it reads the inputs on every tick (cncInputsTick);
// once it stops, no more program steps until the scenario starts a new program.
bool program = false, isrStopped = false, loopAlive = true;
bool cncInputsTick(uint32_t nowUs, bool stepping);
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
    // as cncStepTick: the inputs count only while the program's steps run (not after a fault, not after OFF)
    bool programRuns = program && isOn && !isrStopped && !cncFaultStop;
    if (cncInputsTick((uint32_t)nowUs, programRuns)) isrStopped = true;
    bool programStopped = program && (isrStopped || !isOn);
    if (!cncFaultStop && !programStopped) { stepAxis(&x, stepAcc[0], 50e-6); stepAxis(&z, stepAcc[1], 50e-6); }
    if (loopAlive) cncInputsLoop();
    if (cncFaultStop && ++faultUs >= 20) { cncFaultStop = false; faultUs = 0; }  // taskGcode resets (h4.ino:2082)
    if (onTick) onTick();
  }
}

// safety.ino (tested in safety_test.cpp): no restart to confirm here
const char* safetyStartRefusal() { return NULL; }
void safetyConfirmRestart(const char*) {}

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
  program = isrStopped = false; loopAlive = true;
  cncInputsTickReset();  // the step interrupt takes the inputs as they are on its next tick
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
  // A jog with the keys still moving or braking (firmware 1.25): a program may not start until every axis stands still
  // (the keypad task would switch the driver off at the end of its move, under the running program).
  check(cncAxisMovingRefusal() == NULL, "all axes stand still: no refusal");
  {
    Axis* axes[3] = {&x, &z, &a1};
    const char* names[3] = {"X", "Z", "C"};
    for (int i = 0; i < 3; i++) {
      char what[96];
      axes[i]->movingManually = true;
      snprintf(what, sizeof(what), "%s jogged with the keys: refused (%s)", names[i], cncAxisMovingRefusal() ? cncAxisMovingRefusal() : "none");
      check(cncAxisMovingRefusal() != NULL && cncAxisMovingRefusal()[0] == names[i][0], what);
      axes[i]->movingManually = false;
      axes[i]->pendingPos = i == 1 ? -3 : 7;  // braking: steps still to give, the key already let go
      snprintf(what, sizeof(what), "%s still braking (steps left): refused", names[i]);
      check(cncAxisMovingRefusal() != NULL && cncAxisMovingRefusal()[0] == names[i][0], what);
      axes[i]->pendingPos = 0;
    }
  }
  check(cncAxisMovingRefusal() == NULL, "after the jog has stopped a program may start");

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
  // with soft limits it searches their span + 10 mm (597 + 10 for Z), not 1.2 x the longest manual move + 20 (740)
  check(fabsf(mm(z, z.pos)) <= (LIMIT_Z_MAX - LIMIT_Z_MIN) + 10 + 0.01f && fabsf(mm(z, z.pos)) >= (LIMIT_Z_MAX - LIMIT_Z_MIN) + 10 - 0.01f,
        "disconnected switch: stops after the soft-limit span + 10 mm");
  // X starting at the far end of its travel (soft-limit span from its switch): still found within span + 10 mm
  machine();
  x.pos = x.motorPos = swX.at + (long)((LIMIT_X_MAX - LIMIT_X_MIN) * 1200);
  vTaskDelay(5);
  homingRun();
  expectHomed(x, swX, HOME_X_OFFSET, HOME_X_HOME, "X from the far end of its travel");
  // without soft limits: as before (1.2 x the longest manual move + 20 mm)
  machine();
  swZ.disconnected = true;
  LIMIT_Z_USE = false;
  homingRun();
  check(!z.homed && fabsf(mm(z, z.pos)) <= MAX_TRAVEL_MM_Z * 1.2f + 20 + 0.01f && fabsf(mm(z, z.pos)) > (LIMIT_Z_MAX - LIMIT_Z_MIN) + 10,
        "disconnected switch, no soft limits: searches 1.2 x the longest manual move + 20 mm");
  LIMIT_Z_USE = true;

  // 6. Noise on a limit input while a program runs: a pulse shorter than the 0.5 ms debounce is ignored, a longer
  //    one stops everything ("Limit X"). Setting off (as LinuxCNC): positions kept, still homed.
  machine();
  LIMIT_UNHOMES = false;
  homingRun();
  busy = true;
  swX.noiseFrom = nowUs + 1000; swX.noiseTo = swX.noiseFrom + 300;
  vTaskDelay(5);
  check(cncFaultCount == 0, "noise of 0.3 ms: no limit fault");
  swX.noiseFrom = nowUs + 1000; swX.noiseTo = swX.noiseFrom + 900;
  vTaskDelay(5);
  check(cncFaultCount == 1 && !strcmp(cncFaultText, "Limit X"), "0.9 ms on the input: Limit X");
  check(x.homed, "setting off: a limit keeps the axes homed (every step given is counted)");
  busy = false;
  // 6b. The same with the setting on (default, firmware 1.28, as Grbl): not homed, the next program needs Home All.
  machine();
  LIMIT_UNHOMES = true;
  homingRun();
  check(cncStartRefusal() == NULL, "homed: a program may start");
  busy = true;
  swX.noiseFrom = nowUs + 1000; swX.noiseTo = swX.noiseFrom + 900;
  vTaskDelay(5);
  busy = false;
  check(cncFaultCount == 1 && !strcmp(cncFaultText, "Limit X"), "setting on: Limit X");
  check(!x.homed && !z.homed, "setting on: after the limit nothing is homed");
  check(cncStartRefusal() != NULL && !strcmp(cncStartRefusal(), "Home All first"), "setting on: the next program needs Home All first");
  homingRun();
  check(x.homed && z.homed && cncStartRefusal() == NULL, "Home All again: a program may start");

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
  //     never further into a pressed switch; before homing there are none. (The axis is put on its switch by hand
  //     below: with the setting 'Home All again after a limit' that unhomes it, as in 6b; here it is off, to test the
  //     clamping.)
  machine();
  LIMIT_UNHOMES = false;
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
  LIMIT_UNHOMES = true;

  // 11. X first (HOME_SEQUENCE the other way) and latch in the search direction (back off, approach slowly).
  machine();
  HOME_Z_FIRST = false;
  HOME_X_LATCH_VEL = -0.5;
  homingRun();
  expectHomed(x, swX, HOME_X_OFFSET, HOME_X_HOME, "X first, latch towards the switch, X");
  expectHomed(z, swZ, HOME_Z_OFFSET, HOME_Z_HOME, "X first, Z");

  // 12. The step interrupt reads the limits and the alarm too (C16): a program moving X onto its switch is stopped by
  //     the interrupt, loop() gives the fault once; with loop() stuck the axis still stops and stays stopped.
  for (int stuck = 0; stuck < 2; stuck++) {
    machine();
    homingRun();
    long before = cncFaultCount;
    logLines.clear();
    x.speedMax = 12 * 1200;               // the program moves X at 12 mm/s towards its switch
    program = true; isOn = true; loopAlive = !stuck;
    stepToFinal(&x, x.pos - 200 * 1200);
    uint64_t t0 = nowUs;
    while (!isrStopped && isOn && nowUs - t0 < 30000000) vTaskDelay(1);
    long stopPos = x.pos;
    vTaskDelay(1000);                       // one second more
    char buf[160];
    // with loop() stuck only the step interrupt can stop it; with loop() running, whichever sees it first
    snprintf(buf, sizeof(buf), "program onto the X switch%s: stopped%s", stuck ? ", loop() stuck" : "", stuck ? " by the step interrupt" : "");
    check(stuck ? isrStopped : (isrStopped || !isOn), buf);
    float past = (swX.at - stopPos) / 1200.0f;  // how far past the switch point, mm
    snprintf(buf, sizeof(buf), "program onto the X switch%s: stops %.3f mm past the switch (bounce + 0.5 ms at 12 mm/s)", stuck ? ", loop() stuck" : "", past);
    check(past >= 0 && past <= 12.0f * (swX.bounceUs + 600) * 1e-6f + 0.01f, buf);
    snprintf(buf, sizeof(buf), "program onto the X switch%s: no step after the stop", stuck ? ", loop() stuck" : "");
    check(x.pos == stopPos, buf);
    if (stuck) {
      check(cncFaultCount == before && cncIsrFault == 1, "loop() stuck: still stopped after 1 s, the fault waits for loop()");
      loopAlive = true;
      vTaskDelay(5);
    }
    int limitLines = 0;
    for (auto& l : logLines) if (l == "Limit X") limitLines++;
    snprintf(buf, sizeof(buf), "program onto the X switch%s: one Limit X fault (%d), OFF", stuck ? ", loop() back" : "", limitLines);
    check(cncFaultCount == before + 1 && limitLines == 1 && !strcmp(cncFaultText, "Limit X") && !isOn && cncIsrFault == 0, buf);
  }
  // noise of 0.3 ms on the X input while a program moves: no stop
  machine();
  homingRun();
  x.speedMax = 1200;
  program = true; isOn = true;
  stepToFinal(&x, x.pos + 20 * 1200);       // away from the switch
  swX.noiseFrom = nowUs + 100000; swX.noiseTo = swX.noiseFrom + 300;
  vTaskDelay(500);
  check(!isrStopped && cncFaultCount == 0, "noise of 0.3 ms while a program moves: the step interrupt does not stop");
  // a program that starts with X resting on its switch and moves away: no stop (an edge, not a level)
  machine();
  homingRun();
  x.pos = x.motorPos = swX.at - 100;        // on the switch
  vTaskDelay(10);
  cncInputsTickReset(); vTaskDelay(1);      // as at a start: the inputs as they are
  long faults = cncFaultCount;
  x.speedMax = 1200;
  program = true; isOn = true;
  stepToFinal(&x, x.pos + 5 * 1200);
  vTaskDelay(500);
  check(!isrStopped && cncFaultCount == faults && x.pos > swX.at, "a program moving off its switch: no stop");
  // the driver alarm while a program moves: stopped by the step interrupt, one fault, axes not homed
  machine();
  homingRun();
  z.speedMax = 25 * 400;
  program = true; isOn = true;
  stepToFinal(&z, z.pos - 100 * 400);
  logLines.clear();
  onTick = [&]() { if (nowUs > 0 && !alarmActive && z.pos < -20 * 400) alarmActive = true; };
  while (!isrStopped && isOn && z.pos > -60 * 400) vTaskDelay(1);
  long zStop = z.pos;
  vTaskDelay(100);
  int alarmLines = 0;
  for (auto& l : logLines) if (l == "Driver alarm or E-stop") alarmLines++;
  check((isrStopped || !isOn) && z.pos == zStop && alarmLines == 1 && !z.homed && !x.homed, "alarm while a program moves: stopped, one fault, not homed");
  // the same alarm with loop() stuck: only the step interrupt stops it
  machine();
  homingRun();
  z.speedMax = 25 * 400;
  program = true; isOn = true; loopAlive = false;
  stepToFinal(&z, z.pos - 100 * 400);
  alarmActive = false;
  onTick = [&]() { if (!alarmActive && z.pos < -20 * 400) alarmActive = true; };
  while (!isrStopped && z.pos > -60 * 400) vTaskDelay(1);
  zStop = z.pos;
  vTaskDelay(500);
  check(isrStopped && z.pos == zStop && cncIsrFault == 3, "alarm with loop() stuck: stopped by the step interrupt, stays stopped");
  loopAlive = true;
  logLines.clear();
  vTaskDelay(5);
  alarmLines = 0;
  for (auto& l : logLines) if (l == "Driver alarm or E-stop") alarmLines++;
  check(alarmLines == 1 && !isOn && !z.homed && cncIsrFault == 0, "alarm, loop() back: one fault, OFF, not homed");
  onTick = nullptr; alarmActive = false;
  // Home All itself is not stopped by the step interrupt (it steps from loop(), its switches are expected)
  machine();
  program = false;
  homingRun();
  check(x.homed && z.homed && !isrStopped && cncIsrFault == 0, "Home All: the step interrupt does not interfere");


  // 12. Home check (firmware 1.29): each axis to its switch again the way Home All found it, compared, back.
  {
    char buf[200];
    machine();
    homeCheckRun();
    check(!strcmp(cncStatusText, "Home check: Home All first"), "home check before Home All: refused");
    homingRun();
    stepToFinal(&x, x.pos + 20 * 1200); stepToFinal(&z, z.pos - 30 * 400); homingWaitMotion(&x); homingWaitMotion(&z);
    long xp = x.pos, zp = z.pos;
    homeCheckRun();
    snprintf(buf, sizeof(buf), "nothing moved: OK for both, back where they were (%s)", cncStatusText);
    check(x.homed && z.homed && x.pos == xp && z.pos == zp && strstr(cncStatusText, "X +0.00") && strstr(cncStatusText, "Z ")
          && !strstr(cncStatusText, "MOVED") && cncFaultCount == 0, buf);
    // X lost 0.5 mm (its switch is now 0.5 mm further away, as seen by the step count): MOVED, X not homed
    swX.at -= 600;
    homeCheckRun();
    snprintf(buf, sizeof(buf), "X slipped 0.5 mm: found, MOVED, X not homed, Z still OK (%s)", cncStatusText);
    check(!x.homed && z.homed && x.pos == xp && strstr(cncStatusText, "X -0.50") && strstr(cncStatusText, "MOVED")
          && strstr(cncStatusText, "Home All again"), buf);
    check(cncStartRefusal() != NULL && !strcmp(cncStartRefusal(), "Home All first"), "after a failed home check: Home All first");
    // Z slipped 3 mm the other way: its switch is met before the 2 mm mark
    homingRun();
    stepToFinal(&z, z.pos - 30 * 400); homingWaitMotion(&z);
    swZ.at -= 3 * 400;
    homeCheckRun();
    snprintf(buf, sizeof(buf), "Z slipped 3 mm: switch found early, Z not homed, no limit fault (%s)", cncStatusText);
    check(!z.homed && strstr(cncStatusText, "more than 2 mm early") && cncFaultCount == 0, buf);
    // a switch that no longer answers: not found, not homed, the axis back where it was
    homingRun();
    stepToFinal(&x, x.pos + 20 * 1200); homingWaitMotion(&x);
    xp = x.pos;
    swX.disconnected = true;
    homeCheckRun();
    snprintf(buf, sizeof(buf), "X switch not answering: not found, X not homed, back (%s)", cncStatusText);
    check(!x.homed && x.pos == xp && strstr(cncStatusText, "not found"), buf);
    swX.disconnected = false;
    // within the tolerance: 0.02 mm is OK
    homingRun();
    stepToFinal(&x, x.pos + 20 * 1200); homingWaitMotion(&x);
    swX.at -= 24;
    homeCheckRun();
    snprintf(buf, sizeof(buf), "X 0.02 mm off: within 0.05 mm, OK, still homed (%s)", cncStatusText);
    check(x.homed && !strstr(cncStatusText, "MOVED"), buf);
    // STOP during the check: stops, nothing judged
    onTick = [&]() { if (homingActive && nowUs % 100000 == 0) homingAbort = true; };
    homeCheckRun();
    onTick = nullptr;
    check(!strcmp(cncStatusText, "Home check stopped") && x.homed, "STOP during the check: stopped, X still homed");
    // on its switch (put there by hand; with 'Home All again after a limit' that alone unhomes it): refused
    LIMIT_UNHOMES = false;
    homingRun();
    x.pos = x.motorPos = swX.at - 5;
    vTaskDelay(5);
    homeCheckRun();
    LIMIT_UNHOMES = true;
    snprintf(buf, sizeof(buf), "X on its switch: jog off first (%s)", cncStatusText);
    check(strstr(cncStatusText, "jog it off first") != NULL, buf);
  }

  // 13. (work plan B2) Repeatability: 300 Home All from random places, random contact bounce (0.5 .. 3 ms) and noise
  //     pulses shorter than the 0.5 ms debounce on both inputs: the switch point found must stay within the
  //     bounce time at the latch speed (X 0.5 mm/s, Z 2.5 mm/s) and never be refused.
  {
    char buf[200];
    uint32_t r = 99;
    auto rnd = [&]() { r = r * 1103515245u + 12345u; return (r >> 8) & 0xFFFFFF; };
    double minX = 1e9, maxX = -1e9, minZ = 1e9, maxZ = -1e9;
    int bad = 0, faults = 0;
    machine();
    for (int k = 0; k < 300; k++) {
      // a random place inside the travel (moved without the switches: as by hand / jog before homing)
      x.pos = x.motorPos = swX.at + (long)(1200 * (5 + (rnd() % 100000) / 1000.0));
      z.pos = z.motorPos = swZ.at - (long)(400 * (5 + (rnd() % 500000) / 1000.0));
      swX.bounceUs = 500 + rnd() % 2500; swZ.bounceUs = 500 + rnd() % 2500;
      swX.rnd = rnd(); swZ.rnd = rnd();
      onTick = [&]() {  // a noise pulse of 50 .. 400 us now and then on either input
        if (nowUs >= swX.noiseTo && rnd() % 4000 == 0) { swX.noiseFrom = nowUs; swX.noiseTo = nowUs + 50 + rnd() % 350; }
        if (nowUs >= swZ.noiseTo && rnd() % 4000 == 0) { swZ.noiseFrom = nowUs; swZ.noiseTo = nowUs + 50 + rnd() % 350; }
      };
      vTaskDelay(5);
      uint32_t f0 = cncFaultCount;
      homingRun();
      onTick = nullptr;
      if (!x.homed || !z.homed) { bad++; continue; }
      if (cncFaultCount != f0) faults++;
      double sx = mm(x, swX.at + x.machineOffset), sz = mm(z, swZ.at + z.machineOffset);
      minX = std::min(minX, sx); maxX = std::max(maxX, sx); minZ = std::min(minZ, sz); maxZ = std::max(maxZ, sz);
    }
    snprintf(buf, sizeof(buf), "300 Home All with bounce and short noise: all homed (%d failed), no fault (%d)", bad, faults);
    check(bad == 0 && faults == 0, buf);
    snprintf(buf, sizeof(buf), "repeatability X: switch found within %.4f mm (at 0.5 mm/s, 3 ms bounce + 0.5 ms debounce = 0.0018 mm)", maxX - minX);
    check(maxX - minX <= 0.0018 + 2.0 / 1200, buf);
    snprintf(buf, sizeof(buf), "repeatability Z: switch found within %.4f mm (at 2.5 mm/s: 0.0088 mm)", maxZ - minZ);
    check(maxZ - minZ <= 0.0088 + 2.0 / 400, buf);
  }

  // 14. (B2) A normally open switch stuck closed (welded contact): homing moves off it, cannot, and says so; nothing homed
  {
    machine();
    swX.disconnected = false;
    x.pos = x.motorPos = swX.at + 1200 * 30;
    swX.at = x.pos + 1200 * 200;  // pressed everywhere the axis can be
    vTaskDelay(5);
    homingRun();
    char buf[200];
    snprintf(buf, sizeof(buf), "NO switch stuck closed: not homed, message (%s)", cncStatusText);
    check(!x.homed && strstr(cncStatusText, "stays pressed") != NULL, buf);
  }

  // 15. (B2) Noise longer than the debounce (1 ms) during the search, 25 mm before the switch: before firmware 1.34
  //     X was homed 25 mm off; now refused, as LinuxCNC (the switch must still be pressed when the search has stopped)
  {
    machine();
    x.pos = x.motorPos = swX.at + 1200 * 40;  // 40 mm from the switch
    long trueSwitch = swX.at;
    bool fired = false;
    onTick = [&]() {
      // when X is 25 mm from its switch during the search: a 1 ms pulse on its input
      if (!fired && homingActive && homingWatchAxis == &x && x.pos < trueSwitch + 1200 * 25) {
        fired = true; swX.noiseFrom = nowUs; swX.noiseTo = nowUs + 1000;
      }
    };
    homingRun();
    onTick = nullptr;
    double err = x.homed ? mm(x, swX.at + x.machineOffset) - HOME_X_OFFSET : 0;
    char buf[240];
    snprintf(buf, sizeof(buf), "1 ms noise 25 mm before the X switch during the search: %s (%s)",
             !x.homed ? "homing refused" : fabs(err) < 0.01 ? "ignored, homed right" : "WRONG HOME", cncStatusText);
    check(!x.homed && strstr(cncStatusText, "not pressed any more") != NULL, buf);
    homingRun();  // the noise is gone: Home All again works
    snprintf(buf, sizeof(buf), "then Home All again: homed at the right place (off by %.4f mm)", mm(x, swX.at + x.machineOffset) - HOME_X_OFFSET);
    check(x.homed && fabs(mm(x, swX.at + x.machineOffset) - HOME_X_OFFSET) < 0.01, buf);
  }
  printf("homing: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
