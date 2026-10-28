// https://github.com/kachurovskiy/nanoels
// CNC fork: https://github.com/Marioskiv/nanoels-cnc - the GCODE mode is a real CNC (look-ahead, G93, X/Z/C
// together). All other modes are unchanged. The motion code (cnc_core.cpp) is ported from Grbl, so this
// fork is distributed under the GNU GPL v3 (see LICENSE); the original NanoEls code is MIT.

/* Change values in this section to suit your hardware. */
// CNC fork: these values are the defaults. Most of them can also be changed on the web page (Machine
// settings, stored in Preferences); the web values win over the ones written here.

// Define your hardware parameters here.
int ENCODER_PPR = 600; // 600 step spindle optical rotary encoder. Fractional values not supported.
int ENCODER_BACKLASH = 3; // Numer of impulses encoder can issue without movement of the spindle

// Spindle rotary encoder pins. Swap values if the rotation direction is wrong.
#define ENC_A 7
#define ENC_B 15

// Main lead screw (Z) parameters.
long SCREW_Z_DU = 20000; // 2mm lead screw in deci-microns (10^-7 of a meter)
long MOTOR_STEPS_Z = 800;
long SPEED_START_Z = 2 * MOTOR_STEPS_Z; // Initial speed of a motor, steps / second.
long ACCELERATION_Z = 30 * MOTOR_STEPS_Z; // Acceleration of a motor, steps / second ^ 2.
long SPEED_MANUAL_MOVE_Z = 6 * MOTOR_STEPS_Z; // Maximum speed of a motor during manual move, steps / second.
bool INVERT_Z = false; // change (true/false) if the carriage moves e.g. "left" when you press "right".
bool NEEDS_REST_Z = false; // Set to false for closed-loop drivers, true for open-loop.
long MAX_TRAVEL_MM_Z = 300; // Lathe bed doesn't allow to travel more than this in one go, 30cm / ~1 foot
long BACKLASH_DU_Z = 6500; // 0.65mm backlash in deci-microns (10^-7 of a meter)
const char NAME_Z = 'Z'; // Text shown on screen before axis position value, GCode axis name

// Cross-slide lead screw (X) parameters.
long SCREW_X_DU = 12500; // 1.25mm lead screw with 3x reduction in deci-microns (10^-7) of a meter
long MOTOR_STEPS_X = 2400; // 800 steps at 3x reduction
long SPEED_START_X = MOTOR_STEPS_X; // Initial speed of a motor, steps / second.
long ACCELERATION_X = 10 * MOTOR_STEPS_X; // Acceleration of a motor, steps / second ^ 2.
long SPEED_MANUAL_MOVE_X = 3 * MOTOR_STEPS_X; // Maximum speed of a motor during manual move, steps / second.
bool INVERT_X = true; // change (true/false) if the carriage moves e.g. "left" when you press "right".
bool NEEDS_REST_X = false; // Set to false for all kinds of drivers or X will be unlocked when not moving.
long MAX_TRAVEL_MM_X = 100; // Cross slide doesn't allow to travel more than this in one go, 10cm
long BACKLASH_DU_X = 1500; // 0.15mm backlash in deci-microns (10^-7 of a meter)
const char NAME_X = 'X'; // Text shown on screen before axis position value, GCode axis name

// Manual stepping with left/right/up/down buttons. Only used when step isn't default continuous (1mm or 0.1").
long STEP_TIME_MS = 500; // Time in milliseconds it should take to make 1 manual step.
long DELAY_BETWEEN_STEPS_MS = 80; // Time in milliseconds to wait between steps.

/* Changing anything below shouldn't be needed for basic use. */

// Configuration for axis connected to A1. This is uncommon. Dividing head (C) motor parameters.
// Throughout the configuration below we assume 1mm = 1degree of rotation, so 1du = 0.0001degree.
bool ACTIVE_A1 = false; // Whether the axis is connected
bool ROTARY_A1 = true; // Whether the axis is rotary or linear
long MOTOR_STEPS_A1 = 300; // Number of motor steps for 1 rotation of the the worm gear screw (full step with 20:30 reduction)
long SCREW_A1_DU = 20000; // Degrees multiplied by 10000 that the spindle travels per 1 turn of the worm gear. 2 degrees.
long SPEED_START_A1 = 1600; // Initial speed of a motor, steps / second.
long ACCELERATION_A1 = 16000; // Acceleration of a motor, steps / second ^ 2.
long SPEED_MANUAL_MOVE_A1 = 3200; // Maximum speed of a motor during manual move, steps / second.
bool INVERT_A1 = false; // change (true/false) if the carriage moves e.g. "left" when you press "right".
bool NEEDS_REST_A1 = false; // Set to false for closed-loop drivers. Open-loop: true if you need holding torque, false otherwise.
long MAX_TRAVEL_MM_A1 = 360; // Probably doesn't make sense to ask the dividin head to travel multiple turns.
long BACKLASH_DU_A1 = 0; // Assuming no backlash on the worm gear
const char NAME_A1 = 'C'; // Text shown on screen before axis position value, GCode axis name

// Manual handwheels on A1 and A2. Ignore if you don't have them installed.
bool PULSE_1_USE = false; // Whether there's a pulse generator connected on A11-A13 to be used for movement.
char PULSE_1_AXIS = NAME_Z; // Set to NAME_X to make A11-A13 pulse generator control X instead.
bool PULSE_1_INVERT = false; // Set to true to change the direction in which encoder moves the axis
bool PULSE_2_USE = false; // Whether there's a pulse generator connected on A21-A23 to be used for movement.
char PULSE_2_AXIS = NAME_X; // Set to NAME_Z to make A21-A23 pulse generator control Z instead.
bool PULSE_2_INVERT = true; // Set to false to change the direction in which encoder moves the axis
float PULSE_PER_REVOLUTION = 100; // PPR of handwheels used on A1 and/or A2.
const long PULSE_MIN_WIDTH_US = 1000; // Microseconds width of the pulse that is required for it to be registered. Prevents noise.
const long PULSE_HALF_BACKLASH = 2; // Prevents spurious reverses when moving using a handwheel. Raise to 3 or 4 if they still happen.

int ENCODER_STEPS_INT = ENCODER_PPR * 2; // Number of encoder impulses PCNT counts per revolution of the spindle
const int ENCODER_FILTER = 2; // Encoder pulses shorter than this will be ignored. Clock cycles, 1 - 1023.
const int PCNT_LIM = 31000; // Limit used in hardware pulse counter logic.
const int PCNT_CLEAR = 30000; // Limit where we reset hardware pulse counter value to avoid overflow. Less than PCNT_LIM.
const long DUPR_MAX = 254000; // No more than 1 inch pitch
const int32_t STARTS_MAX = 124; // No more than 124-start thread
const long PASSES_MAX = 999; // No more turn or face passes than this
const long SAFE_DISTANCE_DU = 5000; // Step back 0.5mm from the material when moving between cuts in automated modes
const long SAVE_DELAY_US = 5000000; // Wait 5s after last save and last change of saveable data before saving again
long DIRECTION_SETUP_DELAY_US = 5; // Stepper driver needs some time to adjust to direction change (LinuxCNC DIRSETUP)
const long STEPPED_ENABLE_DELAY_MS = 100; // Delay after stepper is enabled and before issuing steps

// Version of the pref storage format, should be changed when non-backward-compatible
// changes are made to the storage logic, resulting in Preferences wipe on first start.
#define PREFERENCES_VERSION 1
#define PREF_NAMESPACE "h4"
#define GCODE_NAMESPACE "gc"

// GCode-related constants.
const float LINEAR_INTERPOLATION_PRECISION = 0.1; // 0 < x <= 1, smaller values make for quicker G0 and G1 moves
const long GCODE_WAIT_EPSILON_STEPS = 10;
bool SPINDLE_PAUSES_GCODE = true; // pause GCode execution when spindle stops. Set to false for polar milling, where the chuck is turned by the C motor
int GCODE_MIN_RPM = 30; // pause GCode execution if RPM is below this

// GCODE mode as a CNC. Axis speeds and accelerations are the SPEED_MANUAL_MOVE_* and ACCELERATION_* values above.
float CNC_JUNCTION_DEVIATION = 0.01; // mm, how fast corners are taken (as Grbl $11). Smaller = slower corners
int CNC_STEP_PULSE_US = 4; // length of a step pulse in GCODE mode; most drivers need 2.5 us or more
bool CNC_RESPECT_STOPS = false; // true: refuse GCODE moves outside the stops set on the H4 (the old GCODE mode ignored them)
float CNC_ARC_TOLERANCE = 0.002; // mm, largest distance between an arc (G2/G3) and the straight pieces it is cut in
bool GRBL_ON_USB = false; // true: USB speaks the Grbl 1.1 protocol (UGS, gSender, CNCjs, bCNC); WiFi port 23 always does

// Home switches, soft limits and driver alarm, as in LinuxCNC (see homing.ino and CNC.md). All off by default,
// then the H4 behaves as the original. Inputs on terminal A2 (not with handwheel 2): A21 = home switch X,
// A22 = home switch Z, A23 = driver alarm. The values have the meaning of the LinuxCNC .ini keys.
bool HOME_X_USE = false; // home switch on A21
bool HOME_X_NC = false; // false: switch normally open, closes to GND when pressed; true: normally closed
float HOME_X_SEARCH_VEL = -3; // mm/s, the sign is the direction towards the switch (HOME_SEARCH_VEL)
float HOME_X_LATCH_VEL = 0.5; // mm/s, opposite sign: leave the switch slowly; same sign: back off, approach slowly
float HOME_X_FINAL_VEL = 5; // mm/s to HOME after the switch (HOME_FINAL_VEL)
float HOME_X_OFFSET = 0; // machine coordinate of the switch point, mm (HOME_OFFSET)
float HOME_X_HOME = 0; // machine coordinate to go to after homing, mm (HOME)
bool HOME_X_IS_LIMIT = true; // the switch also stops everything when it is hit outside homing (limit switch)
bool LIMIT_X_USE = false; // soft limits once homed (MIN_LIMIT, MAX_LIMIT, machine coordinates)
float LIMIT_X_MIN = 0;
float LIMIT_X_MAX = 100;
bool HOME_Z_USE = false; // home switch on A22
bool HOME_Z_NC = false;
float HOME_Z_SEARCH_VEL = 3;
float HOME_Z_LATCH_VEL = -0.5;
float HOME_Z_FINAL_VEL = 5;
float HOME_Z_OFFSET = 0;
float HOME_Z_HOME = 0;
bool HOME_Z_IS_LIMIT = true;
bool LIMIT_Z_USE = false;
float LIMIT_Z_MIN = -300;
float LIMIT_Z_MAX = 0;
bool HOME_Z_FIRST = true; // Home All: Z first, then X (HOME_SEQUENCE); A1 (C) takes its zero where it stands
bool HOME_REQUIRED = true; // a G-code program starts only after Home All (when a home switch is used)
bool ALARM_USE = false; // driver alarm outputs on A23 (all drivers together, see CNC.md)
bool ALARM_WHEN_LOW = true; // true: an alarm connects A23 to GND (output closes on alarm)
#define IN_HOME_X 12 // A21
#define IN_HOME_Z 13 // A22
#define IN_ALARM 14 // A23
const bool WIFI_ENABLED = true; // web page on the controller: G-code files, status, control (see wifi.ino)

// Your own machine: copy my_machine.h.example to my_machine.h in this folder and write your values there (any of
// the settings above). They are applied at start-up, before the settings saved on the web page, and the page's
// "Defaults from the firmware" goes back to them. my_machine.h is not part of the repository, so a newer firmware
// can be copied over this folder without losing it.
#if __has_include("my_machine.h")
#include "my_machine.h"
#define HAS_MY_MACHINE
#endif

// To be incremented whenever a measurable improvement is made.
#define SOFTWARE_VERSION 12
// Version of this CNC fork's firmware: the app compares it with the newest "firmware-v..." release on GitHub.
// Raise it with every firmware change that is released.
#define FW_VERSION "1.1"

// To be changed whenever a different PCB / encoder / stepper / ... design is used.
#define HARDWARE_VERSION 4

#define Z_ENA 16
#define Z_DIR 17
#define Z_STEP 18

#define X_ENA 8
#define X_DIR 19
#define X_STEP 20

#define BUZZ 4
#define SCL 5
#define SDA 6

#define A11 9
#define A12 10
#define A13 11

#define A21 12
#define A22 13
#define A23 14

#define B_LEFT 57
#define B_RIGHT 37
#define B_UP 47
#define B_DOWN 67
#define B_MINUS 5
#define B_PLUS 64
#define B_ON 17
#define B_OFF 27
#define B_STOPL 7
#define B_STOPR 15
#define B_STOPU 6
#define B_STOPD 16
#define B_DISPL 14
#define B_STEP 24
#define B_SETTINGS 34
#define B_MEASURE 54
#define B_REVERSE 44
#define B_0 51
#define B_1 41
#define B_2 61
#define B_3 31
#define B_4 2
#define B_5 21
#define B_6 12
#define B_7 11
#define B_8 22
#define B_9 1
#define B_BACKSPACE 32
#define B_MODE_GEARS 42
#define B_MODE_TURN 52
#define B_MODE_FACE 62
#define B_MODE_CONE 3
#define B_MODE_CUT 13
#define B_MODE_THREAD 23
#define B_MODE_OTHER 33
#define B_X 53
#define B_Z 43
#define B_A 4
#define B_B 63

#define PREF_VERSION "v"
#define PREF_DUPR "d"
#define PREF_POS_Z "zp"
#define PREF_LEFT_STOP_Z "zls"
#define PREF_RIGHT_STOP_Z "zrs"
#define PREF_ORIGIN_POS_Z "zpo"
#define PREF_POS_GLOBAL_Z "zpg"
#define PREF_MOTOR_POS_Z "zpm"
#define PREF_DISABLED_Z "zd"
#define PREF_POS_X "xp"
#define PREF_LEFT_STOP_X "xls"
#define PREF_RIGHT_STOP_X "xrs"
#define PREF_ORIGIN_POS_X "xpo"
#define PREF_POS_GLOBAL_X "xpg"
#define PREF_MOTOR_POS_X "xpm"
#define PREF_DISABLED_X "xd"
#define PREF_POS_A1 "a1p"
#define PREF_LEFT_STOP_A1 "a1ls"
#define PREF_RIGHT_STOP_A1 "a1rs"
#define PREF_ORIGIN_POS_A1 "a1po"
#define PREF_POS_GLOBAL_A1 "a1pg"
#define PREF_MOTOR_POS_A1 "a1pm"
#define PREF_DISABLED_A1 "a1d"
#define PREF_SPINDLE_POS "sp"
#define PREF_SPINDLE_POS_AVG "spa"
#define PREF_OUT_OF_SYNC "oos"
#define PREF_SPINDLE_POS_GLOBAL "spg"
#define PREF_SHOW_ANGLE "ang"
#define PREF_SHOW_TACHO "rpm"
#define PREF_STARTS "sta"
#define PREF_MODE "mod"
#define PREF_MEASURE "mea"
#define PREF_CONE_RATIO "cr"
#define PREF_TURN_PASSES "tp"
#define PREF_MOVE_STEP "ms"
#define PREF_AUX_FORWARD "af"

#define MOVE_STEP_1 10000 // 1mm
#define MOVE_STEP_2 1000 // 0.1mm
#define MOVE_STEP_3 100 // 0.01mm

#define MOVE_STEP_IMP_1 25400 // 1/10"
#define MOVE_STEP_IMP_2 2540 // 1/100"
#define MOVE_STEP_IMP_3 254 // 1/1000" also known as 1 thou

#define MODE_NORMAL 0
#define MODE_ASYNC 2
#define MODE_CONE 3
#define MODE_TURN 4
#define MODE_FACE 5
#define MODE_CUT 6
#define MODE_THREAD 7
#define MODE_ELLIPSE 8
#define MODE_GCODE 9
#define MODE_A1 10

#define MEASURE_METRIC 0
#define MEASURE_INCH 1
#define MEASURE_TPI 2

#define ESTOP_NONE 0
#define ESTOP_KEY 1
#define ESTOP_POS 2
#define ESTOP_MARK_ORIGIN 3
#define ESTOP_ON_OFF 4
#define ESTOP_OFF_MANUAL_MOVE 5

// For MEASURE_TPI, round TPI to the nearest integer if it's within this range of it.
// E.g. 80.02tpi would be shown as 80tpi but 80.04tpi would be shown as-is.
const float TPI_ROUND_EPSILON = 0.03;

float ENCODER_STEPS_FLOAT = ENCODER_STEPS_INT; // Convenience float version of ENCODER_STEPS_INT
long RPM_BULK = ENCODER_STEPS_INT; // Measure RPM averaged over this number of encoder pulses
const long RPM_UPDATE_INTERVAL_MICROS = 1000000; // Don't redraw RPM more often than once per second

const long GCODE_FEED_DEFAULT_DU_SEC = 20000; // Default feed in du/sec in GCode mode
const float GCODE_FEED_MIN_DU_SEC = 167; // Minimum feed in du/sec in GCode mode - F1

#define DREAD(x) digitalRead(x)
#define DHIGH(x) digitalWrite(x, HIGH)
#define DLOW(x) digitalWrite(x, LOW)
#define DWRITE(x, y) digitalWrite(x, y)

#define DELAY(x) vTaskDelay(x / portTICK_PERIOD_MS);

// ESP32 hardware pulse counter library used to count spindle encoder pulses.
#include "driver/pcnt.h"

#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal.h>
// The display keeps a copy of its 4 lines so that the web page can show the same screen.
class MirroredLcd : public LiquidCrystal {
public:
  using LiquidCrystal::LiquidCrystal;
  char shadow[4][21];
  uint8_t col = 0, row = 0;
  void clearShadow() {
    for (int r = 0; r < 4; r++) {
      memset(shadow[r], ' ', 20);
      shadow[r][20] = '\0';
    }
    col = 0;
    row = 0;
  }
  void clear() { LiquidCrystal::clear(); clearShadow(); }
  void home() { LiquidCrystal::home(); col = 0; row = 0; }
  void setCursor(uint8_t c, uint8_t r) { LiquidCrystal::setCursor(c, r); col = c; row = r; }
  size_t write(uint8_t b) override {
    if (row < 4 && col < 20) shadow[row][col] = b == 0 ? 8 : (char)b; // 0 would end the string: custom char 0 as 8
    col++;
    return LiquidCrystal::write(b);
  }
  using Print::write;
};
MirroredLcd lcd(21, 48, 47, 38, 39, 40, 41, 42, 2, 1);
#define LCD_HASH_INITIAL -3845709 // Random number that's unlikely to naturally occur as an actual hash
long lcdHashLine0 = LCD_HASH_INITIAL;
long lcdHashLine1 = LCD_HASH_INITIAL;
long lcdHashLine2 = LCD_HASH_INITIAL;
long lcdHashLine3 = LCD_HASH_INITIAL;
bool splashScreen = false;

#include <Preferences.h>

#include <Adafruit_TCA8418.h>
Adafruit_TCA8418 keypad;

#include "driver/gptimer.h"
#include "soc/gpio_reg.h"
#include "esp_timer.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "cnc_core.h"
#include "cnc_gcode.h"
#include "cnc_bench.h"
unsigned long keypadTimeUs = 0;

// Most buttons we only have "down" handling, holding them has no effect.
// Buttons with special "holding" logic have flags below.
bool buttonLeftPressed = false;
bool buttonRightPressed = false;
bool buttonUpPressed = false;
bool buttonDownPressed = false;
bool buttonOffPressed = false;
bool buttonGearsPressed = false;
bool buttonTurnPressed = false;

bool inNumpad = false;
int numpadDigits[20];
int numpadIndex = 0;

bool isOn = false;
bool nextIsOn; // isOn value that should be applied asap
bool nextIsOnFlag; // whether nextIsOn requires attention
unsigned long resetMillis = 0;
int emergencyStop = 0;

bool beepFlag = false; // allows time-critical code to ask for a beep on another core

long dupr = 0; // pitch, tenth of a micron per rotation
long savedDupr = 0; // dupr saved in Preferences
long nextDupr = dupr; // dupr value that should be applied asap
bool nextDuprFlag = false; // whether nextDupr requires attention

SemaphoreHandle_t motionMutex; // controls blocks of code where variables affecting the motion loop() are changed

int starts = 1; // number of starts in a multi-start thread
int savedStarts = 0; // starts saved in Preferences
int nextStarts = starts; // number of starts that should be used asap
bool nextStartsFlag = false; // whether nextStarts requires attention

struct Axis {
  SemaphoreHandle_t mutex;

  char name;
  bool active;
  bool rotational;
  float motorSteps; // motor steps per revolution of the axis
  float screwPitch; // lead screw pitch in deci-microns (10^-7 of a meter)

  long pos; // relative position of the tool in stepper motor steps
  long savedPos; // value saved in Preferences
  float fractionalPos; // fractional distance in steps that we meant to travel but couldn't
  long originPos; // relative position of the stepper motor to origin, in steps
  long savedOriginPos; // originPos saved in Preferences
  long posGlobal; // global position of the motor in steps
  long savedPosGlobal; // posGlobal saved in Preferences
  int pendingPos; // steps of the stepper motor that we should make as soon as possible
  long motorPos; // position of the motor in stepper motor steps, same as pos unless moving back, then differs by backlashSteps
  long savedMotorPos; // motorPos saved in Preferences
  bool continuous; // whether current movement is expected to continue until an unknown position

  long leftStop; // left stop value of pos
  long savedLeftStop; // value saved in Preferences
  long nextLeftStop; // left stop value that should be applied asap
  bool nextLeftStopFlag; // whether nextLeftStop required attention

  long rightStop; // right stop value of pos
  long savedRightStop; // value saved in Preferences
  long nextRightStop; // right stop value that should be applied asap
  bool nextRightStopFlag; // whether nextRightStop requires attention

  long speed; // motor speed in steps / second
  long speedStart; // Initial speed of a motor, steps / second.
  long speedMax; // To limit max speed e.g. for manual moves
  long speedManualMove; // Maximum speed of a motor during manual move, steps / second.
  long acceleration; // Acceleration of a motor, steps / second ^ 2.
  long decelerateSteps; // Number of steps before the end position the deceleration should start.

  bool direction; // To reset speed when direction changes.
  bool directionInitialized;
  unsigned long stepStartUs;
  int stepperEnableCounter;
  bool disabled;
  bool savedDisabled;

  bool invertStepper; // change (true/false) if the carriage moves e.g. "left" when you press "right".
  bool needsRest; // set to false for closed-loop drivers, true for open-loop.
  bool movingManually; // whether stepper is being moved by left/right buttons
  long estopSteps; // amount of steps to exceed machine limits
  long backlashSteps; // amount of steps in reverse direction to re-engage the carriage
  long gcodeRelativePos; // absolute position in steps that relative GCode refers to
  long machineOffset; // machine coordinate = pos + machineOffset, set by homing (kept by markOrigin like originPos)
  bool homed; // homed since the start (Home All)

  int ena; // Enable pin of this motor
  int dir; // Direction pin of this motor
  int step; // Step pin of this motor
};

void initAxis(Axis* a, char name, bool active, bool rotational, float motorSteps, float screwPitch, long speedStart, long speedManualMove,
    long acceleration, bool invertStepper, bool needsRest, long maxTravelMm, long backlashDu, int ena, int dir, int step) {
  a->mutex = xSemaphoreCreateMutex();

  a->name = name;
  a->active = active;
  a->rotational = rotational;
  a->motorSteps = motorSteps;
  a->screwPitch = screwPitch;

  a->pos = 0;
  a->machineOffset = 0;
  a->homed = false;
  a->savedPos = 0;
  a->fractionalPos = 0.0;
  a->originPos = 0;
  a->savedOriginPos = 0;
  a->posGlobal = 0;
  a->savedPosGlobal = 0;
  a->pendingPos = 0;
  a->motorPos = 0;
  a->savedMotorPos = 0;
  a->continuous = false;

  a->leftStop = 0;
  a->savedLeftStop = 0;
  a->nextLeftStopFlag = false;

  a->rightStop = 0;
  a->savedRightStop = 0;
  a->nextRightStopFlag = false;

  a->speed = speedStart;
  a->speedStart = speedStart;
  a->speedMax = LONG_MAX;
  a->speedManualMove = speedManualMove;
  a->acceleration = acceleration;
  a->decelerateSteps = 0;
  long s = speedManualMove;
  while (s > speedStart) {
    a->decelerateSteps++;
    s -= a->acceleration / float(s);
  }

  a->direction = true;
  a->directionInitialized = false;
  a->stepStartUs = 0;
  a->stepperEnableCounter = 0;
  a->disabled = false;
  a->savedDisabled = false;

  a->invertStepper = invertStepper;
  a->needsRest = needsRest;
  a->movingManually = false;
  a->estopSteps = maxTravelMm * 10000 / a->screwPitch * a->motorSteps;
  a->backlashSteps = backlashDu * a->motorSteps / a->screwPitch;
  a->gcodeRelativePos = 0;

  a->ena = ena;
  a->dir = dir;
  a->step = step;
}

Axis z;
Axis x;
Axis a1;

unsigned long saveTime = 0; // micros() of the previous Prefs write
unsigned long spindleEncTime = 0; // micros() of the previous spindle update
unsigned long spindleEncTimeDiffBulk = 0; // micros() between RPM_BULK spindle updates
unsigned long spindleEncTimeAtIndex0 = 0; // micros() when spindleEncTimeIndex was 0
int spindleEncTimeIndex = 0; // counter going between 0 and RPM_BULK - 1
long spindlePos = 0; // Spindle position
long spindlePosAvg = 0; // Spindle position accounting for encoder backlash
long savedSpindlePosAvg = 0; // spindlePosAvg saved in Preferences
long savedSpindlePos = 0; // spindlePos value saved in Preferences
int spindleCount = 0; // Last processed spindle encoder pulse counter value.
int spindlePosSync = 0; // Non-zero if gearbox is on and a soft limit was removed while axis was on it
int savedSpindlePosSync = 0; // spindlePosSync saved in Preferences
long spindlePosGlobal = 0; // global spindle position that is unaffected by e.g. zeroing
long savedSpindlePosGlobal = 0; // spindlePosGlobal saved in Preferences

volatile int pulse1Delta = 0; // Outstanding pulses generated by pulse generator on terminal A1.
volatile int pulse2Delta = 0; // Outstanding pulses generated by pulse generator on terminal A2.

bool showAngle = false; // Whether to show 0-359 spindle angle on screen
bool showTacho = false; // Whether to show spindle RPM on screen
bool savedShowAngle = false; // showAngle value saved in Preferences
bool savedShowTacho = false; // showTacho value saved in Preferences
int shownRpm = 0;
unsigned long shownRpmTime = 0; // micros() when shownRpm was set

long moveStep = 0; // thousandth of a mm
long savedMoveStep = 0; // moveStep saved in Preferences

volatile int mode = -1; // mode of operation (ELS, multi-start ELS, asynchronous)
int nextMode = 0; // mode value that should be applied asap
bool nextModeFlag = false; // whether nextMode needs attention
int savedMode = -1; // mode saved in Preferences

int measure = MEASURE_METRIC; // Whether to show distances in inches
int savedMeasure = MEASURE_METRIC; // measure value saved in Preferences

float coneRatio = 1; // In cone mode, how much X moves for 1 step of Z
float savedConeRatio = 0; // value of coneRatio saved in Preferences
float nextConeRatio = 0; // coneRatio that should be applied asap
bool nextConeRatioFlag = false; // whether nextConeRatio requires attention

int turnPasses = 3; // In turn mode, how many turn passes to make
int savedTurnPasses = 0; // value of turnPasses saved in Preferences

long setupIndex = 0; // Index of automation setup step
bool auxForward = true; // True for external, false for external thread
bool savedAuxForward = false; // value of auxForward saved in Preferences

long opIndex = 0; // Index of an automation operation
bool opIndexAdvanceFlag = false; // Whether user requested to move to the next pass
long opSubIndex = 0; // Sub-index of an automation operation
int opDuprSign = 1; // 1 if dupr was positive when operation started, -1 if negative
long opDupr = 0; // dupr that the multi-pass operation started with

const int customCharMmCode = 0;
byte customCharMm[] = {
  B11010,
  B10101,
  B10101,
  B00000,
  B11010,
  B10101,
  B10101,
  B00000
};
const int customCharLimUpCode = 1;
byte customCharLimUp[] = {
  B11111,
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B00000,
  B00000
};
const int customCharLimDownCode = 2;
byte customCharLimDown[] = {
  B00000,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100,
  B11111,
  B00000
};
const int customCharLimLeftCode = 3;
byte customCharLimLeft[] = {
  B10000,
  B10010,
  B10100,
  B11111,
  B10100,
  B10010,
  B10000,
  B00000
};
const int customCharLimRightCode = 4;
byte customCharLimRight[] = {
  B00001,
  B01001,
  B00101,
  B11111,
  B00101,
  B01001,
  B00001,
  B00000
};
const int customCharLimUpDownCode = 5;
byte customCharLimUpDown[] = {
  B11111,
  B00100,
  B01110,
  B00000,
  B01110,
  B00100,
  B11111,
  B00000
};
const int customCharLimLeftRightCode = 6;
byte customCharLimLeftRight[] = {
  B00000,
  B10001,
  B10001,
  B11111,
  B10001,
  B10001,
  B00000,
  B00000
};

String gcodeCommand = "";
long gcodeFeedDuPerSec = GCODE_FEED_DEFAULT_DU_SEC;
bool gcodeInitialized = false;
bool gcodeAbsolutePositioning = true;
bool gcodeInBrace = false;
bool gcodeInSemicolon = false;
bool serialInKeycode = false;
int serialKeycode = 0;
String keycodeCommand = "";
bool gcodeInSave = false;
bool gcodeInSaveFirstLine = false;
String gcodeSaveName = "";
String gcodeSaveValue = "";
int gcodeProgramIndex = 0;
int gcodeProgramCount = 0;
String gcodeProgram = "";
int gcodeProgramCharIndex = 0;
// The program the GCODE mode runs: a program stored in Preferences (gcodeProgram) or a G-code file
// loaded from the controller's flash into RAM by the web page (wifi.ino). Read only through these.
const char* volatile gcodeProgramData = "";
volatile int gcodeProgramSize = 0;

hw_timer_t *async_timer = timerBegin(80);
bool timerAttached = false;

// GCODE mode as a CNC: state shared by taskGcode (core 0), loop() (core 1) and the step interrupt.
#define CNC_TIMER_HZ 20000000
#define CNC_IDLE 0
#define CNC_RUN 1
gptimer_handle_t cncTimer = NULL;
portMUX_TYPE cncMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool cncStepsEnabled = true; // false after an emergency stop
volatile int cncState = CNC_IDLE;
volatile bool cncPaused = false; // M0, M1, M6: waiting for ON or '~'
volatile bool cncSpindleHold = false; // stopped because the spindle stopped
volatile bool cncResumeRequest = false;
volatile bool cncStopRequest = false; // OFF / '!' / error: decelerate, then drop the rest of the program
// Overrides wanted by the Grbl real-time commands, the web page and the gamepad; taskGcode applies them.
volatile int cncFeedOverrideWanted = 100;  // %, 10 ... 200
volatile int cncRapidOverrideWanted = 100; // %, 100 / 50 / 25

// The Grbl 1.1 override commands 0x90 ... 0x97 (feed 100 %, +10, -10, +1, -1; rapids 100, 50, 25 %).
void cncOverrideCommand(int code) {
  int f = cncFeedOverrideWanted;
  switch (code) {
    case 0x90: f = 100; break;
    case 0x91: f += 10; break;
    case 0x92: f -= 10; break;
    case 0x93: f += 1; break;
    case 0x94: f -= 1; break;
    case 0x95: cncRapidOverrideWanted = 100; break;
    case 0x96: cncRapidOverrideWanted = 50; break;
    case 0x97: cncRapidOverrideWanted = 25; break;
  }
  cncFeedOverrideWanted = constrain(f, CNC_FEED_OVERRIDE_MIN, CNC_FEED_OVERRIDE_MAX);
}
volatile bool cncStartRequest = false;
volatile bool wifiFsBusy = false; // the web page is reading or writing the flash: no program may start now
volatile bool cncUserHold = false; // Pause from the web page: decelerate and wait for Resume
bool cncOwesDisable = false; // steppers to release once a stopped program has come to rest
// Inputs on A2 (homing.ino). Read and debounced in loop().
volatile uint8_t inRaw = 0; // bit 0 A21, bit 1 A22, bit 2 A23: level as read (1 = high)
volatile bool inHomeX = false, inHomeZ = false, inAlarm = false; // active, after the NO/NC setting
volatile bool cncFaultStop = false; // limit or alarm: no more steps until taskGcode has reset the planner
volatile uint32_t cncFaultCount = 0; // counts limit / alarm stops (the Grbl protocol reports each as ALARM)
volatile bool cncHomeOverride = false; // Grbl $X: programs may run without Home All
bool cncGrblLine = false; // the line comes from the Grbl protocol: errors are answered there, not printed here
bool cncStartKeepModes = false; // a Grbl session starts: keep G90 / G21 / F ... as they are
char cncLastError[100] = "";
// The WiFi address on the H4's own display (wifi.ino fills it): in the GCODE mode while nothing runs, and in
// every mode for the first seconds after WiFi has started. Several texts are shown in turn.
char wifiDisplayText[3][21];
volatile int wifiDisplayCount = 0;
volatile unsigned long wifiDisplayUntil = 0;

const char* wifiDisplayLine() {
  int n = wifiDisplayCount;
  if (n <= 0) return NULL;
  return wifiDisplayText[(millis() / 3000) % n];
}
const char* volatile cncFaultText = "";
volatile bool homingRequest = false, homingAbort = false, homingActive = false;
Axis* volatile homingWatchAxis = NULL; // loop() stops this axis when its switch reaches homingWantActive
volatile bool homingWantActive = false, homingHit = false;
volatile long homingHitPos = 0;
// G32 / G33 threading move, run by loop() like the other spindle-synchronised modes.
#define CNC_THREAD_NONE 0
#define CNC_THREAD_PHASE 1 // waiting until the spindle reaches the thread start
#define CNC_THREAD_RUN 2   // the axes follow the spindle encoder
volatile int cncThreadState = CNC_THREAD_NONE;
Axis* cncThreadLeadAxis = NULL;
long cncThreadPitchDu = 0;   // lead in deci-microns, signed as the lead axis moves
long cncThreadDiff[2] = {0, 0}; // X, Z steps from the thread start
long cncThreadSpindleTarget = 0; // encoder counts for the whole move
long cncThreadLastPhase = 0;
bool cncThreadPhaseKnown = false;
// Shown on the display. Never a String: the display task reads it while taskGcode (higher priority) runs.
const char* volatile cncStatusText = "";
char gcodeDisplayLine[21] = ""; // the G-code line being run, for the display
String gcodeSerialBacklog = ""; // characters read while waiting for the planner
bool gcodePollInBrace = false;
bool gcodePollInSemicolon = false;
unsigned long gcodeAwaitOnMs = 0;
uint32_t cncLineNo = 0;
uint32_t cncOffsetsSeen = 0;
uint32_t cncOffsetsSaved = 0;
volatile uint32_t cncCpuCyclesPerTick = 12; // set in setup() from the CPU clock
volatile uint32_t cncCpuCyclesPerUs = 240;

int getApproxRpm() {
  unsigned long t = micros();
  if (t > spindleEncTime + 50000) {
    // RPM less than 10.
    spindleEncTimeDiffBulk = 0;
    return 0;
  }
  if (t < shownRpmTime + RPM_UPDATE_INTERVAL_MICROS) {
    // Don't update RPM too often to avoid flickering.
    return shownRpm;
  }
  int rpm = 0;
  if (spindleEncTimeDiffBulk > 0) {
    rpm = 60000000 / spindleEncTimeDiffBulk;
    if (abs(rpm - shownRpm) > (rpm < 1000 ? 3 : 5)) {
      // Don't update RPM with insignificant differences.
      shownRpm = rpm;
      shownRpmTime = t;
    }
  }
  return rpm;
}

bool stepperIsRunning(Axis* a) {
  return micros() - a->stepStartUs < 50000;
}

// Returns number of letters printed.
int printDeciMicrons(long deciMicrons, int precisionPointsMax) {
  if (deciMicrons == 0) {
    return lcd.print("0");
  }
  bool imperial = measure != MEASURE_METRIC;
  long v = imperial ? round(deciMicrons / 25.4) : deciMicrons;
  int points = 0;
  if (v == 0 && precisionPointsMax >= 5) {
    points = 5;
  } else if ((v % 10) != 0 && precisionPointsMax >= 4) {
    points = 4;
  } else if ((v % 100) != 0 && precisionPointsMax >= 3) {
    points = 3;
  } else if ((v % 1000) != 0 && precisionPointsMax >= 2) {
    points = 2;
  } else if ((v % 10000) != 0 && precisionPointsMax >= 1) {
    points = 1;
  }
  int count = lcd.print(deciMicrons / (imperial ? 254000.0 : 10000.0), points);
  count += imperial ? lcd.print("\"") : lcd.write(customCharMmCode);
  return count;
}

int printDegrees(long degrees10000) {
  int points = 0;
  if ((degrees10000 % 100) != 0) {
    points = 3;
  } else if ((degrees10000 % 1000) != 0) {
    points = 2;
  } else if ((degrees10000 % 10000) != 0) {
    points = 1;
  }
  int count = lcd.print(degrees10000 / 10000.0, points);
  count += lcd.print(char(223)); // degree symbol
  return count;
}

int printDupr(long value) {
  int count = 0;
  if (measure != MEASURE_TPI) {
    count += printDeciMicrons(value, 5);
  } else {
    float tpi = 254000.0 / value;
    if (abs(tpi - round(tpi)) < TPI_ROUND_EPSILON) {
      count += lcd.print(int(round(tpi)));
    } else {
      int tpi100 = round(tpi * 100);
      int points = 0;
      if ((tpi100 % 10) != 0) {
        points = 2;
      } else if ((tpi100 % 100) != 0) {
        points = 1;
      }
      count += lcd.print(tpi, points);
    }
    count += lcd.print("tpi");
  }
  return count;
}

void printLcdSpaces(int charIndex) {
  // Our screen has width 20.
  for (; charIndex < 20; charIndex++) {
    lcd.print(" ");
  }
}

long stepsToDu(Axis* a, long steps) {
  return round(steps * a->screwPitch / a->motorSteps);
}

long getAxisPosDu(Axis* a) {
  return stepsToDu(a, a->pos + a->originPos);
}

long getAxisStopDiffDu(Axis* a) {
  if (a->leftStop == LONG_MAX || a->rightStop == LONG_MIN) return 0;
  return stepsToDu(a, a->leftStop - a->rightStop);
}

int printAxisPos(Axis* a) {
  if (a->rotational) {
    return printDegrees(getAxisPosDu(a));
  }
  return printDeciMicrons(getAxisPosDu(a), 3);
}

int printAxisStopDiff(Axis* a, bool addTrailingSpace) {
  int count = 0;
  if (a->rotational) {
    count = printDegrees(getAxisStopDiffDu(a));
  } else {
    count = printDeciMicrons(getAxisStopDiffDu(a), 3);
  }
  if (addTrailingSpace) {
    count += lcd.print(' ');
  }
  return count;
}

int printAxisPosWithName(Axis* a, bool addTrailingSpace) {
  if (!a->active || a->disabled) return 0;
  int count = lcd.print(a->name);
  count += printAxisPos(a);
  if (addTrailingSpace) {
    count += lcd.print(' ');
  }
  return count;
}

int printNoTrailing0(float value) {
  long v = round(value * 100000);
  int points = 0;
  if ((v % 10) != 0) {
    points = 5;
  } else if ((v % 100) != 0) {
    points = 4;
  } else if ((v % 1000) != 0) {
    points = 3;
  } else if ((v % 10000) != 0) {
    points = 2;
  } else if ((v % 100000) != 0) {
    points = 1;
  }
  return lcd.print(value, points);
}

bool needZStops() {
  return mode == MODE_TURN || mode == MODE_FACE || mode == MODE_THREAD || mode == MODE_ELLIPSE;
}

bool isPassMode() {
  return mode == MODE_TURN || mode == MODE_FACE || mode == MODE_CUT || mode == MODE_THREAD || mode == MODE_ELLIPSE;
}

bool manualMovesAllowedWhenOn() {
  return mode == MODE_NORMAL || mode == MODE_ASYNC || mode == MODE_CONE || mode == MODE_A1;
}

int getLastSetupIndex() {
  if (mode == MODE_CONE || mode == MODE_GCODE) return 2;
  if (mode == MODE_TURN || mode == MODE_FACE || mode == MODE_CUT || mode == MODE_THREAD || mode == MODE_ELLIPSE) return 3;
  return 0;
}

Axis* getPitchAxis() {
  return mode == MODE_FACE ? &x : &z;
}

long getPassModeZStart() {
  if (mode == MODE_TURN || mode == MODE_THREAD) return dupr > 0 ? z.rightStop : z.leftStop;
  if (mode == MODE_FACE) return auxForward ? z.rightStop : z.leftStop;
  if (mode == MODE_ELLIPSE) return dupr > 0 ? z.leftStop : z.rightStop;
  return z.pos;
}

long getPassModeXStart() {
  if (mode == MODE_TURN || mode == MODE_THREAD) return auxForward ? x.rightStop : x.leftStop;
  if (mode == MODE_FACE || mode == MODE_CUT) return dupr > 0 ? x.rightStop : x.leftStop;
  if (mode == MODE_ELLIPSE) return x.rightStop;
  return x.pos;
}

int printMode() {
  if (mode == MODE_ASYNC) {
    return lcd.print("ASY ");
  } else if (mode == MODE_CONE) {
    return lcd.print("CONE ");
  } else if (mode == MODE_TURN) {
    return lcd.print("TURN ");
  } else if (mode == MODE_FACE) {
    return lcd.print("FACE ");
  } else if (mode == MODE_CUT) {
    return lcd.print("CUT ");
  } else if (mode == MODE_THREAD) {
    return lcd.print("THRD ");
  } else if (mode == MODE_ELLIPSE) {
    return lcd.print("ELLI ");
  } else if (mode == MODE_GCODE) {
    return lcd.print("GCODE ");
  } else if (mode == MODE_A1) {
    return lcd.print("A1 ");
  }
  return 0;
}

void updateDisplay() {
  int rpm = showTacho ? getApproxRpm() : 0;
  int charIndex = 0;

  if (splashScreen) {
    splashScreen = false;
    lcd.clear();
    lcd.setCursor(6, 1);
    lcd.print("NanoEls");
    lcd.setCursor(6, 2);
    lcd.print("H" + String(HARDWARE_VERSION) + " V" + String(SOFTWARE_VERSION));
    lcdHashLine0 = LCD_HASH_INITIAL;
    lcdHashLine1 = LCD_HASH_INITIAL;
    lcdHashLine2 = LCD_HASH_INITIAL;
    lcdHashLine3 = LCD_HASH_INITIAL;
    delay(2000);
  }
  if (lcdHashLine0 == LCD_HASH_INITIAL) {
    // First run after reset.
    lcd.clear();
    lcdHashLine1 = LCD_HASH_INITIAL;
    lcdHashLine2 = LCD_HASH_INITIAL;
    lcdHashLine3 = LCD_HASH_INITIAL;
  }

  long newHashLine0 = isOn + (z.leftStop - z.rightStop) + (x.leftStop - x.rightStop) + spindlePosSync + moveStep + mode + measure + setupIndex * 10;
  if (lcdHashLine0 != newHashLine0) {
    lcdHashLine0 = newHashLine0;
    charIndex = 0;
    lcd.setCursor(0, 0);
    if (setupIndex == 0 || !isPassMode()) {
      charIndex += printMode();
      charIndex += lcd.print(isOn ? "ON " : "off ");
      int beforeStops = charIndex;
      if (z.leftStop != LONG_MAX) {
        charIndex += lcd.write(customCharLimLeftCode);
      }
      if (x.leftStop != LONG_MAX && x.rightStop != LONG_MIN) {
        charIndex += lcd.write(customCharLimUpDownCode);
      } else if (x.leftStop != LONG_MAX) {
        charIndex += lcd.write(customCharLimUpCode);
      } else if (x.rightStop != LONG_MIN) {
        charIndex += lcd.write(customCharLimDownCode);
      }
      if (z.rightStop != LONG_MIN) {
        charIndex += lcd.write(customCharLimRightCode);
      }
      if (beforeStops != charIndex) {
        charIndex += lcd.print(" ");
      }

      if (spindlePosSync && !isPassMode()) {
        charIndex += lcd.print("SYN ");
      }
      if (mode == MODE_NORMAL && !spindlePosSync) {
        charIndex += lcd.print("step ");
      }
      charIndex += printDeciMicrons(moveStep, 5);
    } else {
      if (needZStops()) {
        charIndex += lcd.write(customCharLimLeftRightCode);
        charIndex += printAxisStopDiff(&z, true);
        while (charIndex < 10) charIndex += lcd.print(" ");
      } else {
        charIndex += printMode();
      }
      charIndex += lcd.write(customCharLimUpDownCode);
      charIndex += printAxisStopDiff(&x, false);
    }
    printLcdSpaces(charIndex);
  }

  long newHashLine1 = dupr + starts + mode + measure + setupIndex;
  if (lcdHashLine1 != newHashLine1) {
    lcdHashLine1 = newHashLine1;
    charIndex = 0;
    lcd.setCursor(0, 1);
    charIndex += lcd.print("Pitch ");
    charIndex += printDupr(dupr);
    if (starts != 1) {
      charIndex += lcd.print(" x");
      charIndex += lcd.print(starts);
    }
    printLcdSpaces(charIndex);
  }

  long zDisplayPos = z.pos + z.originPos;
  long xDisplayPos = x.pos + x.originPos;
  long a1DisplayPos = a1.pos + a1.originPos;
  long newHashLine2 = zDisplayPos + xDisplayPos + a1DisplayPos + measure + z.disabled + x.disabled + mode;
  if (lcdHashLine2 != newHashLine2) {
    lcdHashLine2 = newHashLine2;
    charIndex = 0;
    lcd.setCursor(0, 2);
    charIndex += printAxisPosWithName(&z, true);
    while (charIndex < 10) charIndex += lcd.print(" ");
    charIndex += printAxisPosWithName(&x, true);
    printLcdSpaces(charIndex);
  }

  long numpadResult = getNumpadResult();
  long gcodeCommandHash = 0;
  // From the copy, not from gcodeCommand: taskGcode changes that String while the display runs.
  for (int i = 0; gcodeDisplayLine[i]; i++) {
    gcodeCommandHash += gcodeDisplayLine[i] * (i + 1);
  }
  bool spindleStopped = micros() > spindleEncTime + 100000;
  long newHashLine3 = z.pos + (showAngle ? spindlePos : -1) + (showTacho ? rpm : -2) + measure + (numpadResult > 0 ? numpadResult : -1) + mode * 5 + dupr +
      (mode == MODE_CONE ? round(coneRatio * 10000) : 0) + turnPasses + opIndex + setupIndex + gcodeProgramIndex + gcodeProgramCount + spindleStopped * 3 + (isOn ? 139 : -117) + (inNumpad ? 10 : 0) + (auxForward ? 17 : -31) +
      (z.leftStop == LONG_MAX ? 123 : z.leftStop) + (z.rightStop == LONG_MIN ? 1234 : z.rightStop) +
      (x.leftStop == LONG_MAX ? 1235 : x.leftStop) + (x.rightStop == LONG_MIN ? 123456 : x.rightStop) + gcodeCommandHash + (long)(intptr_t)cncStatusText +
      (mode == MODE_A1 ? a1.pos + a1.originPos + (a1.leftStop == LONG_MAX ? 123 : a1.leftStop) + (a1.rightStop == LONG_MIN ? 1234 : a1.rightStop) + a1.disabled : 0) + x.pos + z.pos;
  const char* wifiLine = wifiDisplayLine();
  if (wifiLine != NULL) {
    for (int i = 0; wifiLine[i]; i++) newHashLine3 += wifiLine[i] * (i + 7);
    newHashLine3 += (long)(wifiDisplayUntil - millis()) > 0 ? 4099 : 0;
  }
  if (lcdHashLine3 != newHashLine3) {
    lcdHashLine3 = newHashLine3;
    charIndex = 0;
    lcd.setCursor(0, 3);
    if (mode == MODE_A1 && !inNumpad) {
      if (a1.leftStop != LONG_MAX && a1.rightStop != LONG_MIN) {
        charIndex += lcd.write(customCharLimUpDownCode);
        charIndex += lcd.print(" ");
      } else if (a1.leftStop != LONG_MAX) {
        charIndex += lcd.write(customCharLimDownCode);
        charIndex += lcd.print(" ");
      } else if (a1.rightStop != LONG_MIN) {
        charIndex += lcd.write(customCharLimUpCode);
        charIndex += lcd.print(" ");
      }
      charIndex += printAxisPosWithName(&a1, false);
    } else if (!isOn && !inNumpad && setupIndex == 0 && wifiDisplayLine() != NULL && (long)(wifiDisplayUntil - millis()) > 0) {
      charIndex += lcd.print(wifiDisplayLine()); // just after start: where the web page is
    } else if (mode == MODE_GCODE) {
      if (setupIndex == 1 && gcodeProgramCount == 0) {
        charIndex += lcd.print("No stored programs");
      } else if (setupIndex == 1) {
        Preferences pref;
        pref.begin(GCODE_NAMESPACE);
        if (gcodeProgramIndex >= gcodeProgramCount) {
          charIndex += lcd.print("Program deleted");
        } else {
          String programName = pref.getString(String(gcodeProgramIndex).c_str());
          if (programName.length() == 0) charIndex += lcd.print("(empty name)");
          else charIndex += lcd.print(programName.substring(0, 20));
        }
        pref.end();
      } else if (setupIndex == 2) {
        if (spindleStopped) charIndex += lcd.print("Turn on the spindle!");
        else charIndex += lcd.print("Spindle on. Go?");
      } else if (isOn && cncStatusText[0]) {
        charIndex += lcd.print((const char*)cncStatusText);
      } else if (isOn) {
        charIndex += lcd.print(gcodeDisplayLine);
      } else if (wifiDisplayLine() != NULL) {
        charIndex += lcd.print(wifiDisplayLine()); // the address of the web page
      }
    } else if (isPassMode()) {
      bool missingZStops = needZStops() && (z.leftStop == LONG_MAX || z.rightStop == LONG_MIN);
      bool missingStops = missingZStops || x.leftStop == LONG_MAX || x.rightStop == LONG_MIN;
      if (!inNumpad && missingStops) {
        charIndex += lcd.print(needZStops() ? "Set all stops" : "Set X stops");
      } else if (numpadResult != 0 && setupIndex == 1) {
        long passes = min(PASSES_MAX, numpadResult);
        charIndex += lcd.print(passes);
        if (passes == 1) charIndex += lcd.print(" pass?");
        else charIndex += lcd.print(" passes?");
      } else if (!isOn && setupIndex == 1) {
        charIndex += lcd.print(turnPasses);
        if (turnPasses == 1) charIndex += lcd.print(" pass?");
        else charIndex += lcd.print(" passes?");
      } else if (!isOn && setupIndex == 2) {
        if (mode == MODE_FACE) {
          charIndex += lcd.print(auxForward ? "Right to left?" : "Left to right?");
        } else if (mode == MODE_CUT) {
          charIndex += lcd.print(dupr >= 0 ? "Pitch > 0, external" : "Pitch < 0, internal");
        } else {
          charIndex += lcd.print(auxForward ? "External?" : "Internal?");
        }
      } else if (!isOn && setupIndex == 3) {
        long zOffset = getPassModeZStart() - z.pos;
        long xOffset = getPassModeXStart() - x.pos;
        charIndex += lcd.print("Go");
        if (zOffset != 0) {
          charIndex += lcd.print(" ");
          charIndex += lcd.print(z.name);
          charIndex += printDeciMicrons(stepsToDu(&z, zOffset), 2);
        }
        if (xOffset != 0) {
          charIndex += lcd.print(" ");
          charIndex += lcd.print(x.name);
          charIndex += printDeciMicrons(stepsToDu(&x, xOffset), 2);
        }
        charIndex += lcd.print("?");
      } else if (isOn && numpadResult == 0) {
        charIndex += lcd.print("Pass ");
        charIndex += lcd.print(opIndex);
        charIndex += lcd.print(" of ");
        charIndex += lcd.print(max(opIndex, long(turnPasses * starts)));
      }
    } else if (mode == MODE_CONE) {
      if (numpadResult != 0 && setupIndex == 1) {
        charIndex += lcd.print("Use ratio ");
        charIndex += lcd.print(numpadToConeRatio(), 5);
        charIndex += lcd.print("?");
      } else if (!isOn && setupIndex == 1) {
        charIndex += lcd.print("Use ratio ");
        charIndex += printNoTrailing0(coneRatio);
        charIndex += lcd.print("?");
      } else if (!isOn && setupIndex == 2) {
        charIndex += lcd.print(auxForward ? "External?" : "Internal?");
      } else if (!isOn && setupIndex == 3) {
        charIndex += lcd.print("Go?");
      } else if (isOn && numpadResult == 0) {
        charIndex += lcd.print("Cone ratio ");
        charIndex += printNoTrailing0(coneRatio);
      }
    }

    if (charIndex == 0 && inNumpad) { // Also show for 0 input to allow setting limits to 0.
      charIndex += lcd.print("Use ");
      charIndex += printDupr(numpadToDeciMicrons());
      charIndex += lcd.print("?");
    }

    if (charIndex > 0) {
      // No space for shared RPM/angle text.
    } else if (showAngle) {
      charIndex += lcd.print("Angle ");
      charIndex += lcd.print(spindleModulo(spindlePos) * 360 / ENCODER_STEPS_FLOAT, 2);
      charIndex += lcd.print(char(223));
    } else if (showTacho) {
      charIndex += lcd.print("Tacho ");
      charIndex += lcd.print(rpm);
      charIndex += lcd.print("rpm");
    }
    printLcdSpaces(charIndex);
  }
}

unsigned long pulse1HighMicros = 0;
unsigned long pulse2HighMicros = 0;

// Called on a FALLING interrupt for the first axis rotary encoder pin.
void IRAM_ATTR pulse1Enc() {
  unsigned long now = micros();
  if (DREAD(A12)) {
    pulse1HighMicros = now;
  } else if (now > pulse1HighMicros + PULSE_MIN_WIDTH_US) {
    pulse1Delta += (DREAD(A13) ? -1 : 1) * (PULSE_1_INVERT ? -1 : 1);
  }
}

// Called on a FALLING interrupt for the second axis rotary encoder pin.
void IRAM_ATTR pulse2Enc() {
  unsigned long now = micros();
  if (DREAD(A22)) {
    pulse2HighMicros = now;
  } else if (now > pulse2HighMicros + PULSE_MIN_WIDTH_US) {
    pulse2Delta += (DREAD(A23) ? -1 : 1) * (PULSE_2_INVERT ? -1 : 1);
  }
}

void setAsyncTimerEnable(bool value) {
  if (value) {
    timerStart(async_timer);
  } else {
    timerStop(async_timer);
  }
}

void taskDisplay(void *param) {
  while (emergencyStop == ESTOP_NONE) {
    updateDisplay();
    // Calling Preferences.commit() blocks all interrupts for 30ms, don't call saveIfChanged() if
    // encoder is likely to move soon.
    unsigned long now = micros();
    // Never while the GCODE mode moves: a Preferences write stops the CPU for ~30 ms.
    if (!cncBusy() && !stepperIsRunning(&z) && !stepperIsRunning(&x) && (now > spindleEncTime + SAVE_DELAY_US) && (now < saveTime || now > saveTime + SAVE_DELAY_US) && (now < keypadTimeUs || now > keypadTimeUs + SAVE_DELAY_US)) {
      if (saveIfChanged()) {
        saveTime = now;
      }
    }
    if (beepFlag) {
      beepFlag = false;
      beep();
    }
    if (abs(z.pendingPos) > z.estopSteps || abs(x.pendingPos) > x.estopSteps) {
      setEmergencyStop(ESTOP_POS);
    }
    taskYIELD();
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EMERGENCY STOP");
  lcd.setCursor(0, 1);
  if (emergencyStop == ESTOP_KEY) {
    lcd.print("Key down at power-up");
    lcd.setCursor(0, 2);
    lcd.print("Hardware failure?");
  } else if (emergencyStop == ESTOP_POS) {
    lcd.print("Requested position");
    lcd.setCursor(0, 2);
    lcd.print("outside machine");
  } else if (emergencyStop == ESTOP_MARK_ORIGIN) {
    lcd.print("Unable to");
    lcd.setCursor(0, 2);
    lcd.print("mark origin");
  } else if (emergencyStop == ESTOP_ON_OFF) {
    lcd.print("Unable to");
    lcd.setCursor(0, 2);
    lcd.print("turn on/off");
  } else if (emergencyStop == ESTOP_OFF_MANUAL_MOVE) {
    lcd.print("Off during");
    lcd.setCursor(0, 2);
    lcd.print("manual move");
  }
  vTaskDelete(NULL);
}

void taskKeypad(void *param) {
  while (emergencyStop == ESTOP_NONE) {
    processKeypadEvent();
    taskYIELD();
  }
  vTaskDelete(NULL);
}

void waitForPendingPosNear0(Axis* a) {
  while (abs(a->pendingPos) > a->motorSteps / 3) {
    taskYIELD();
  }
}

void waitForPendingPos0(Axis* a) {
  while (a->pendingPos != 0) {
    taskYIELD();
  }
}

bool isContinuousStep() {
  return moveStep == (measure == MEASURE_METRIC ? MOVE_STEP_1 : MOVE_STEP_IMP_1);
}

// For rotational axis the moveStep of 0.1" means 0.1°.
long getMoveStepForAxis(Axis* a) {
  return (a->rotational && measure != MEASURE_METRIC) ? (moveStep / 25.4) : moveStep;
}

long getStepMaxSpeed(Axis* a) {
  return isContinuousStep() ? a->speedManualMove : min(long(a->speedManualMove), abs(getMoveStepForAxis(a)) * 1000 / STEP_TIME_MS);
}

void waitForStep(Axis* a) {
  if (isContinuousStep()) {
    // Move continuously for default step.
    waitForPendingPosNear0(a);
  } else {
    // Move with tiny pauses allowing to stop precisely.
    a->continuous = false;
    waitForPendingPos0(a);
    DELAY(DELAY_BETWEEN_STEPS_MS);
  }
}

int getAndResetPulses(Axis* a) {
  int delta = 0;
  if (PULSE_1_AXIS == a->name) {
    if (pulse1Delta < -PULSE_HALF_BACKLASH) {
      noInterrupts();
      delta = pulse1Delta + PULSE_HALF_BACKLASH;
      pulse1Delta = -PULSE_HALF_BACKLASH;
      interrupts();
    } else if (pulse1Delta > PULSE_HALF_BACKLASH) {
      noInterrupts();
      delta = pulse1Delta - PULSE_HALF_BACKLASH;
      pulse1Delta = PULSE_HALF_BACKLASH;
      interrupts();
    }
  } else if (PULSE_2_AXIS == a->name) {
    if (pulse2Delta < -PULSE_HALF_BACKLASH) {
      noInterrupts();
      delta = pulse2Delta + PULSE_HALF_BACKLASH;
      pulse2Delta = -PULSE_HALF_BACKLASH;
      interrupts();
    } else if (pulse2Delta > PULSE_HALF_BACKLASH) {
      noInterrupts();
      delta = pulse2Delta - PULSE_HALF_BACKLASH;
      pulse2Delta = PULSE_HALF_BACKLASH;
      interrupts();
    }
  }
  return delta;
}

void taskMoveZ(void *param) {
  while (emergencyStop == ESTOP_NONE) {
    int pulseDelta = getAndResetPulses(&z);
    bool left = buttonLeftPressed;
    bool right = buttonRightPressed;
    if (!left && !right && pulseDelta == 0) {
      taskYIELD();
      continue;
    }
    if (spindlePosSync != 0) {
      // Edge case.
      taskYIELD();
      continue;
    }
    if (isOn && !manualMovesAllowedWhenOn()) {
      setIsOnFromTask(false);
    }
    // GCODE mode: a handwheel turned during a program stops it; move only once it has stopped.
    while (mode == MODE_GCODE && (cncBusy() || cncStopRequest) && emergencyStop == ESTOP_NONE) {
      taskYIELD();
    }
    int sign = pulseDelta == 0 ? (left ? 1 : -1) : (pulseDelta > 0 ? 1 : -1);
    bool stepperOn = true;
    stepperEnable(&z, true);
    z.movingManually = true;
    if (isOn && dupr != 0 && mode == MODE_NORMAL) {
      // Move by moveStep in the desired direction but stay in the thread by possibly traveling a little more.
      int diff = ceil(moveStep * 1.0 / abs(dupr * starts)) * ENCODER_STEPS_FLOAT * sign * (dupr > 0 ? 1 : -1);
      long prevSpindlePos = spindlePos;
      bool resting = false;
      do {
        z.speedMax = z.speedManualMove;
        if (xSemaphoreTake(motionMutex, 100) == pdTRUE) {
          if (!resting) {
            spindlePos += diff;
            spindlePosAvg += diff;
          }
          // If spindle is moving, it will be changing spindlePos at the same time. Account for it.
          while (diff > 0 ? (spindlePos < prevSpindlePos) : (spindlePos > prevSpindlePos)) {
            spindlePos += diff;
            spindlePosAvg += diff;
          };
          prevSpindlePos = spindlePos;
          xSemaphoreGive(motionMutex);
        }

        long newPos = posFromSpindle(&z, prevSpindlePos, true);
        if (newPos != z.pos) {
          stepToContinuous(&z, newPos);
          waitForPendingPosNear0(&z);
        } else if (z.pos == (left ? z.leftStop : z.rightStop)) {
          // We're standing on a stop with the L/R move button pressed.
          resting = true;
          if (stepperOn) {
            stepperEnable(&z, false);
            stepperOn = false;
          }
          DELAY(200);
        }
      } while (left ? buttonLeftPressed : buttonRightPressed);
    } else {
      z.speedMax = getStepMaxSpeed(&z);
      int delta = 0;
      do {
        float fractionalDelta = (pulseDelta == 0 ? moveStep * sign / z.screwPitch : pulseDelta / PULSE_PER_REVOLUTION) * z.motorSteps + z.fractionalPos;
        delta = round(fractionalDelta);
        // Don't lose fractional steps when moving by 0.01" or 0.001".
        z.fractionalPos = fractionalDelta - delta;
        if (delta == 0) {
          // When moveStep is e.g. 1 micron and MOTOR_STEPS_Z is 200, make delta non-zero.
          delta = sign;
        }

        long posCopy = z.pos + z.pendingPos;
        // Don't left-right move out of stops.
        if (posCopy + delta > z.leftStop) {
          delta = z.leftStop - posCopy;
        } else if (posCopy + delta < z.rightStop) {
          delta = z.rightStop - posCopy;
        }
        delta = cncClampJog(&z, posCopy, delta); // soft limits, not further into a pressed home switch
        z.speedMax = getStepMaxSpeed(&z);
        stepToContinuous(&z, posCopy + delta);
        waitForStep(&z);
      } while (delta != 0 && (left ? buttonLeftPressed : buttonRightPressed));
      z.continuous = false;
      waitForPendingPos0(&z);
      if (isOn && mode == MODE_CONE) {
        if (xSemaphoreTake(motionMutex, 100) != pdTRUE) {
          setEmergencyStop(ESTOP_MARK_ORIGIN);
        } else {
          markOrigin();
          xSemaphoreGive(motionMutex);
        }
      } else if (isOn && mode == MODE_ASYNC) {
        // Restore async direction.
        updateAsyncTimerSettings();
      }
    }
    z.movingManually = false;
    if (stepperOn) {
      stepperEnable(&z, false);
    }
    z.speedMax = LONG_MAX;
    taskYIELD();
  }
  vTaskDelete(NULL);
}

void taskMoveX(void *param) {
  while (emergencyStop == ESTOP_NONE) {
    int pulseDelta = getAndResetPulses(&x);
    bool up = buttonUpPressed || pulseDelta > 0;
    bool down = buttonDownPressed || pulseDelta < 0;
    if (!up && !down) {
      taskYIELD();
      continue;
    }
    if (isOn && !manualMovesAllowedWhenOn()) {
      setIsOnFromTask(false);
    }
    // GCODE mode: a handwheel turned during a program stops it; move only once it has stopped.
    while (mode == MODE_GCODE && (cncBusy() || cncStopRequest) && emergencyStop == ESTOP_NONE) {
      taskYIELD();
    }
    x.movingManually = true;
    x.speedMax = getStepMaxSpeed(&x);
    stepperEnable(&x, true);

    int delta = 0;
    int sign = up ? 1 : -1;
    do {
      float fractionalDelta = (pulseDelta == 0 ? moveStep * sign / x.screwPitch : pulseDelta / PULSE_PER_REVOLUTION) * x.motorSteps + x.fractionalPos;
      delta = round(fractionalDelta);
      // Don't lose fractional steps when moving by 0.01" or 0.001".
      x.fractionalPos = fractionalDelta - delta;
      if (delta == 0) {
        // When moveStep is e.g. 1 micron and MOTOR_STEPS_Z is 200, make delta non-zero.
        delta = sign;
      }

      long posCopy = x.pos + x.pendingPos;
      if (posCopy + delta > x.leftStop) {
        delta = x.leftStop - posCopy;
      } else if (posCopy + delta < x.rightStop) {
        delta = x.rightStop - posCopy;
      }
      delta = cncClampJog(&x, posCopy, delta); // soft limits, not further into a pressed home switch
      stepToContinuous(&x, posCopy + delta);
      waitForStep(&x);
      pulseDelta = getAndResetPulses(&x);
    } while (delta != 0 && (pulseDelta != 0 || (up ? buttonUpPressed : buttonDownPressed)));
    x.continuous = false;
    waitForPendingPos0(&x);
    if (isOn && mode == MODE_CONE) {
      if (xSemaphoreTake(motionMutex, 100) != pdTRUE) {
        setEmergencyStop(ESTOP_MARK_ORIGIN);
      } else {
        markOrigin();
        xSemaphoreGive(motionMutex);
      }
    }
    x.movingManually = false;
    x.speedMax = LONG_MAX;
    stepperEnable(&x, false);

    taskYIELD();
  }
  vTaskDelete(NULL);
}

void taskMoveA1(void *param) {
  while (emergencyStop == ESTOP_NONE) {
    bool plus = buttonTurnPressed;
    bool minus = buttonGearsPressed;
    if (mode != MODE_A1 || (!plus && !minus)) {
      taskYIELD();
      continue;
    }
    a1.movingManually = true;
    a1.speedMax = getStepMaxSpeed(&a1);
    stepperEnable(&a1, true);

    int delta = 0;
    int sign = plus ? 1 : -1;
    do {
      float fractionalDelta = getMoveStepForAxis(&a1) * sign / a1.screwPitch * a1.motorSteps + a1.fractionalPos;
      delta = round(fractionalDelta);
      a1.fractionalPos = fractionalDelta - delta;
      if (delta == 0) delta = sign;

      long posCopy = a1.pos + a1.pendingPos;
      if (posCopy + delta > a1.leftStop) {
        delta = a1.leftStop - posCopy;
      } else if (posCopy + delta < a1.rightStop) {
        delta = a1.rightStop - posCopy;
      }
      stepToContinuous(&a1, posCopy + delta);
      waitForStep(&a1);
    } while (plus ? buttonTurnPressed : buttonGearsPressed);
    a1.continuous = false;
    waitForPendingPos0(&a1);
    // Restore async direction.
    if (isOn && mode == MODE_A1) updateAsyncTimerSettings();
    a1.movingManually = false;
    a1.speedMax = LONG_MAX;
    stepperEnable(&a1, false);
    taskYIELD();
  }
  vTaskDelete(NULL);
}

// ============================================================ GCODE mode as a CNC
// The planner and the step segments are prepared in taskGcode (core 0). The step interrupt (core 1)
// only takes ready segments, as in Grbl, and keeps the H4 axis positions (pos, motorPos, posGlobal)
// exactly as moveAxis() does, so the display and every other mode see the true position.

// Last messages of the GCODE mode (errors, notes), shown on the web page.
#define CNC_LOG_LINES 12
char cncLogText[CNC_LOG_LINES][112];
volatile uint32_t cncLogCount = 0;
portMUX_TYPE cncLogMux = portMUX_INITIALIZER_UNLOCKED;

void cncLog(const char* text) {
  portENTER_CRITICAL(&cncLogMux);
  char* dest = cncLogText[cncLogCount % CNC_LOG_LINES];
  strncpy(dest, text, sizeof(cncLogText[0]) - 1);
  dest[sizeof(cncLogText[0]) - 1] = '\0';
  cncLogCount = cncLogCount + 1;
  portEXIT_CRITICAL(&cncLogMux);
}

IRAM_ATTR Axis* cncAxis(int i) {
  return i == CNC_X ? &x : (i == CNC_Z ? &z : &a1);
}

// The step interrupt works as FluidNC's "timed" stepping engine (and Grbl): the step computed by the previous
// interrupt is put out at the very start of this one, so its timing does not depend on how long the computation
// takes; the next step is computed while the pulse is high and only the rest of the pulse length is waited; the
// direction pins of the next step are set right after the pulse, so the driver sees them a whole step period
// (and at least DIRECTION_SETUP_DELAY_US) before its step. Step pulse, H4 style: the step pin idles HIGH and a step
// is a LOW pulse. The positions count a step when it is put out, so "not busy" always means "all steps given".
static cnc_tick_t cncPending;                   // computed, put out by the next interrupt
static bool cncPendingValid = false;
static bool cncDirWait = false;                 // the direction pins changed for the pending step
static uint32_t cncDirSetCycles = 0;
// Self-measurement, shown on the web page (Diagnostics): no instrument is needed to see the limits.
volatile uint32_t cncDiagIsrMaxCycles = 0;      // longest interrupt
volatile uint32_t cncDiagLateMaxTicks = 0;      // latest start after its alarm (timer ticks)
volatile uint32_t cncDiagLateCount = 0;         // interrupts that started more than 10 us late
volatile uint32_t cncDiagRateMax = 0;           // highest step rate given, steps per second
volatile uint32_t cncDiagSteps = 0;             // step pulses given (all axes together)

void cncDiagReset() {
  cncDiagIsrMaxCycles = 0;
  cncDiagLateMaxTicks = 0;
  cncDiagLateCount = 0;
  cncDiagRateMax = 0;
  cncDiagSteps = 0;
}

static bool IRAM_ATTR cncOnTimer(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* ctx) {
  uint32_t cycles0 = esp_cpu_get_cycle_count();
  uint32_t late = (uint32_t)(edata->count_value - edata->alarm_value);
  if (late > cncDiagLateMaxTicks) cncDiagLateMaxTicks = late;
  if (late > CNC_TIMER_HZ / 100000) cncDiagLateCount = cncDiagLateCount + 1;  // over 10 us
  bool run = cncStepsEnabled && !cncFaultStop;

  // 1. Put out the pending step.
  uint32_t stepMask = 0;
  uint32_t pulseStart = 0;
  if (cncPendingValid && run) {
    if (cncDirWait) {
      uint32_t dirCycles = DIRECTION_SETUP_DELAY_US * cncCpuCyclesPerUs;
      while (esp_cpu_get_cycle_count() - cncDirSetCycles < dirCycles) {}
    }
    for (int i = 0; i < CNC_N_AXIS; i++) {
      if (cncPending.step & (1 << i)) stepMask |= 1UL << cncAxis(i)->step;
    }
    REG_WRITE(GPIO_OUT_W1TC_REG, stepMask);
    pulseStart = esp_cpu_get_cycle_count();
    unsigned long nowUs = (unsigned long)esp_timer_get_time();
    for (int i = 0; i < CNC_N_AXIS; i++) {
      if (!(cncPending.step & (1 << i))) continue;
      Axis* a = cncAxis(i);
      int d = (cncPending.dirNeg & (1 << i)) ? -1 : 1;
      a->motorPos += d;
      a->posGlobal += d;
      if (!cncPending.motorOnly) a->pos += d;
      a->stepStartUs = nowUs;
    }
    cncDiagSteps = cncDiagSteps + 1;
  }
  cncPendingValid = false;
  cncDirWait = false;

  // 2. Compute the next step while the pulse is high.
  cnc_tick_t t;
  bool active = false;
  portENTER_CRITICAL_ISR(&cncMux);
  if (run) active = cnc_isr(&t);
  portEXIT_CRITICAL_ISR(&cncMux);
  if (!run) t.period = CNC_TIMER_HZ / 1000;
  if (active && t.step) {
    cncPending = t;
    cncPendingValid = true;
    uint32_t rate = CNC_TIMER_HZ / (t.period > 0 ? t.period : 1);
    if (rate > cncDiagRateMax) cncDiagRateMax = rate;
  }

  // 3. End the pulse after CNC_STEP_PULSE_US.
  if (stepMask) {
    uint32_t pulseCycles = CNC_STEP_PULSE_US * cncCpuCyclesPerUs;
    while (esp_cpu_get_cycle_count() - pulseStart < pulseCycles) {}
    REG_WRITE(GPIO_OUT_W1TS_REG, stepMask);
  }

  // 4. Direction pins of the next step.
  if (cncPendingValid) {
    for (int i = 0; i < CNC_N_AXIS; i++) {
      if (!(cncPending.step & (1 << i))) continue;
      Axis* a = cncAxis(i);
      bool positive = !(cncPending.dirNeg & (1 << i));
      // a->direction is the cache that setDir() uses: keep it true so manual moves set the pin correctly later.
      if (!a->directionInitialized || a->direction != positive) {
        a->direction = positive;
        a->directionInitialized = true;
        if (positive ^ a->invertStepper) REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << a->dir);
        else REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << a->dir);
        cncDirWait = true;
      }
    }
    if (cncDirWait) cncDirSetCycles = esp_cpu_get_cycle_count();
  }

  uint32_t used = esp_cpu_get_cycle_count() - cycles0;
  if (used > cncDiagIsrMaxCycles) cncDiagIsrMaxCycles = used;
  // Next tick, never in the past even if this one took long.
  uint64_t elapsed = used / cncCpuCyclesPerTick;
  uint64_t next = edata->alarm_value + t.period;
  uint64_t earliest = edata->count_value + elapsed + 100;
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = next > earliest ? next : earliest;
  gptimer_set_alarm_action(timer, &alarm);
  return false;
}

// Highest step rate of one axis in the GCODE mode: a step pulse and an equal pause (FluidNC's limit for its
// "timed" engine). The step interrupt must also fit in that time: the web page shows how long it really takes.
// Self-test of a stored program (web page: Test): read and planned as when it runs, nothing moves (cnc_bench.h).
volatile bool cncBenchRequest = false;
const char* volatile cncBenchData = NULL;
volatile size_t cncBenchSize = 0;
char cncBenchName[64] = "";
char cncBenchText[300] = "";
volatile uint32_t cncBenchCount = 0;  // changes when a new result is there

uint64_t cncBenchNowUs() {
  return (uint64_t)esp_timer_get_time();
}

void cncBenchYield() {
  vTaskDelay(1);
}

void cncRunBench() {
  cnc_offsets_t offs;
  cnc_gc_get_offsets(&offs);
  cnc_modes_t modes;
  cnc_gc_get_modes(&modes);
  uint32_t changes = cnc_gc_offsets_changes();
  bool saved = cncOffsetsSaved == changes, seen = cncOffsetsSeen == changes;
  cncSyncFromAxes(false);
  cnc_bench_t r;
  cnc_bench_run(cncBenchData, cncBenchSize, &r, cncBenchNowUs, cncBenchYield);
  // back to where it was: position from the axes, the offsets and modes as before (nothing is stored)
  cncSyncFromAxes(false);
  cnc_gc_set_offsets(&offs);
  cnc_gc_set_modes(&modes);
  if (saved) cncOffsetsSaved = cnc_gc_offsets_changes();
  if (seen) cncOffsetsSeen = cnc_gc_offsets_changes();
  double planned = r.busyUs > 0 ? r.blocks / (r.busyUs / 1e6) : 0;
  int n = snprintf(cncBenchText, sizeof(cncBenchText), "Test %s: %lu lines, %lu moves, %.1f min at the programmed speeds. "
      "The H4 plans %.0f moves/s, the program needs at most %.0f/s (line %lu): ", cncBenchName, (unsigned long)r.lines,
      (unsigned long)r.blocks, r.programMinutes, planned, r.neededPerSecond, (unsigned long)r.neededAtLine);
  if (r.stopLine && strncmp(r.stopReason, "threading", 9) != 0) {
    snprintf(cncBenchText + n, sizeof(cncBenchText) - n, "STOPPED at line %lu: %s", (unsigned long)r.stopLine, r.stopReason);
  } else if (planned >= 2 * r.neededPerSecond) {
    snprintf(cncBenchText + n, sizeof(cncBenchText) - n, "OK, %.0f times faster than needed%s", r.neededPerSecond > 0 ? planned / r.neededPerSecond : 999.0,
        r.stopLine ? " (stopped at a threading line, not part of the test)" : "");
  } else {
    snprintf(cncBenchText + n, sizeof(cncBenchText) - n, "TOO CLOSE: the machine may slow down around line %lu "
        "(fewer, longer moves in the CAM / converter help)", (unsigned long)r.neededAtLine);
  }
  cncLog(cncBenchText);
  cncBenchCount = cncBenchCount + 1;
}

uint32_t cncStepRateLimit() {
  return 1000000UL / (2UL * max(1, CNC_STEP_PULSE_US));
}

void cncConfigure() {
  cnc_settings_t s;
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  for (int i = 0; i < CNC_N_AXIS; i++) {
    Axis* a = cncAxis(i);
    float stepsPerUnit = a->motorSteps / (a->screwPitch / 10000.0); // steps per mm, or per degree for a rotary axis
    s.stepsPerUnit[i] = stepsPerUnit;
    s.maxRate[i] = a->speedManualMove / stepsPerUnit * 60;
    if (a->active && (uint32_t)a->speedManualMove > cncStepRateLimit()) {
      // never ask the step interrupt for more than it can give: the axis is slowed, and the page says so
      s.maxRate[i] = cncStepRateLimit() / stepsPerUnit * 60;
      char text[100];
      snprintf(text, sizeof(text), "%c: maximum speed lowered to %lu steps/s (step pulse %d us)", a->name,
          (unsigned long)cncStepRateLimit(), CNC_STEP_PULSE_US);
      cncLog(text);
    }
    s.acceleration[i] = a->acceleration / stepsPerUnit * 3600;
    m.name[i] = a->name;
    m.active[i] = a->active;
    m.rotary[i] = a->rotational;
    m.backlashSteps[i] = a->backlashSteps;
  }
  s.junctionDeviation = CNC_JUNCTION_DEVIATION;
  s.timerHz = CNC_TIMER_HZ;
  m.checkLimits = CNC_RESPECT_STOPS;
  // The step interrupt writes the GPIO registers of pins 0-31 directly (all H4 motor pins are there).
  for (int i = 0; i < CNC_N_AXIS; i++) {
    Axis* a = cncAxis(i);
    if (a->active && (a->step >= 32 || a->dir >= 32)) {
      cncStepsEnabled = false;
      Serial.println("error: GCODE mode needs step/dir pins below 32");
    }
  }
  m.arcTolerance = CNC_ARC_TOLERANCE;
  m.spindleRpm = cncSpindleRpm;
  cnc_core_settings(&s);
  cnc_gc_init(&m);
  cncLoadOffsets();
  cnc_plan_reset();
  cnc_st_reset();
}

void cncStartTimer() {
  cncCpuCyclesPerTick = max(1UL, (unsigned long)(getCpuFrequencyMhz() * 1000000UL / CNC_TIMER_HZ));
  cncCpuCyclesPerUs = getCpuFrequencyMhz();
  gptimer_config_t config;
  memset(&config, 0, sizeof(config));
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = CNC_TIMER_HZ;
  // Highest interrupt level a C handler may have (as FluidNC's step timer): WiFi and the other interrupts (level 1)
  // cannot delay the steps.
  config.intr_priority = 3;
  if (gptimer_new_timer(&config, &cncTimer) != ESP_OK) {
    Serial.println("error: CNC step timer not available");
    return;
  }
  gptimer_event_callbacks_t callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.on_alarm = cncOnTimer;
  gptimer_register_event_callbacks(cncTimer, &callbacks, NULL);
  gptimer_enable(cncTimer);
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = CNC_TIMER_HZ / 1000;
  gptimer_set_alarm_action(cncTimer, &alarm);
  gptimer_start(cncTimer);
}

bool cncBusy() {
  return cncState != CNC_IDLE || cnc_st_busy() || cncThreadState != CNC_THREAD_NONE || cncFaultStop;
}

// Takes the tool position, the work zero, the backlash state and the stops from the H4 axes.
// Only when nothing is queued.
// G95: the spindle speed measured by the encoder.
float cncSpindleRpm() {
  return getApproxRpm();
}

float cncStepsPerUnit(int i) {
  Axis* a = cncAxis(i);
  return a->motorSteps / (a->screwPitch / 10000.0);
}

// G54 is the H4's own zero (X0 / Z0 keys, originPos): work coordinate = pos + originPos.
// In machine coordinates its origin is at pos = -originPos, i.e. machine (-originPos + machineOffset).
void cncG54FromAxes() {
  cnc_offsets_t o;
  cnc_gc_get_offsets(&o);
  for (int i = 0; i < CNC_N_AXIS; i++) {
    Axis* a = cncAxis(i);
    o.coord[0][i] = (-a->originPos + a->machineOffset) / cncStepsPerUnit(i);
  }
  cnc_gc_set_offsets(&o);
}

void cncG54ToAxes() {
  cnc_offsets_t o;
  cnc_gc_get_offsets(&o);
  for (int i = 0; i < CNC_N_AXIS; i++) {
    Axis* a = cncAxis(i);
    a->originPos = -(lround(o.coord[0][i] * cncStepsPerUnit(i)) - a->machineOffset);
  }
}

// G55 ... G59.3, G28 and G30 positions are kept in Preferences (machine coordinates; they mean something
// after Home All). Written only when nothing moves (a flash write stops the step interrupt).
#define WCS_NAMESPACE "h4wcs"

void cncLoadOffsets() {
  cnc_offsets_t o;
  memset(&o, 0, sizeof(o));
  Preferences p;
  if (p.begin(WCS_NAMESPACE, true)) {
    if (p.getBytesLength("o") == sizeof(o)) p.getBytes("o", &o, sizeof(o));
    p.end();
  }
  o.active = 0;
  o.g92Applied = false;
  memset(o.g92, 0, sizeof(o.g92));
  cnc_gc_set_offsets(&o);
  cncOffsetsSaved = cnc_gc_offsets_changes();
}

void cncSaveOffsetsIfChanged() {
  if (cnc_gc_offsets_changes() == cncOffsetsSaved || cncBusy() || isOn || wifiFsBusy) return;
  cnc_offsets_t o;
  cnc_gc_get_offsets(&o);
  Preferences p;
  if (p.begin(WCS_NAMESPACE, false)) {
    p.putBytes("o", &o, sizeof(o));
    p.end();
  }
  cncOffsetsSaved = cnc_gc_offsets_changes();
}

void cncSyncFromAxes(bool keepModes) {
  int32_t pos[CNC_N_AXIS], mOffset[CNC_N_AXIS];
  bool neg[CNC_N_AXIS];
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  for (int i = 0; i < CNC_N_AXIS; i++) {
    Axis* a = cncAxis(i);
    pos[i] = a->pos;
    mOffset[i] = a->machineOffset;
    // After a move down, moveAxis() leaves the motor backlashSteps below the tool.
    neg[i] = a->backlashSteps > 0 && a->motorPos != a->pos;
    m.name[i] = a->name;
    m.active[i] = a->active;
    m.rotary[i] = a->rotational;
    m.backlashSteps[i] = a->backlashSteps;
    m.minSteps[i] = (!CNC_RESPECT_STOPS || a->rightStop == LONG_MIN) ? INT32_MIN : a->rightStop;
    m.maxSteps[i] = (!CNC_RESPECT_STOPS || a->leftStop == LONG_MAX) ? INT32_MAX : a->leftStop;
    long lo, hi;
    if (softLimitSteps(a, &lo, &hi)) { // homing.ino
      if (lo > m.minSteps[i]) m.minSteps[i] = lo;
      if (hi < m.maxSteps[i]) m.maxSteps[i] = hi;
    }
  }
  m.checkLimits = CNC_RESPECT_STOPS || softLimitsActive();
  m.arcTolerance = CNC_ARC_TOLERANCE;
  m.spindleRpm = cncSpindleRpm;
  portENTER_CRITICAL(&cncMux);
  cnc_st_reset();
  cnc_plan_reset();
  portEXIT_CRITICAL(&cncMux);
  if (keepModes) cnc_gc_set_machine(&m);
  else cnc_gc_init(&m);
  cnc_gc_sync(pos, mOffset, neg);
  cncG54FromAxes();
}

void cncSyncFromAxes() {
  cncSyncFromAxes(false);
}

// ---- G32 / G33: the method of the G32 of the NanoEls H5 (MIT, Maxim Kachurovskiy), run from loop():
// wait until the spindle reaches the same thread phase as every earlier pass, mark the origin there, then
// move X and Z in proportion to the spindle turns counted by the encoder.
long cncSpindleFromPosWithPitch(Axis* a, long pos, long pitchDu) {
  return lround((double)pos * a->screwPitch * ENCODER_STEPS_INT / a->motorSteps / pitchDu);
}

long cncThreadPhase() {
  return spindleModulo(spindlePosGlobal - cncSpindleFromPosWithPitch(cncThreadLeadAxis, cncThreadLeadAxis->posGlobal, cncThreadPitchDu));
}

// Called from loop() holding motionMutex.
void cncThreadLoop() {
  if (!isOn || cncStopRequest || mode != MODE_GCODE) {
    cncThreadState = CNC_THREAD_NONE; // the axes finish the step they were given and stop
    return;
  }
  if (cncThreadState == CNC_THREAD_PHASE) {
    if (getApproxRpm() < GCODE_MIN_RPM) {
      cncThreadPhaseKnown = false;
      return;
    }
    long phase = cncThreadPhase();
    bool start = phase == 0 || (cncThreadPhaseKnown && phase - cncThreadLastPhase < -ENCODER_STEPS_INT / 2);
    cncThreadLastPhase = phase;
    cncThreadPhaseKnown = true;
    if (!start) return;
    markOrigin();
    spindlePos = phase;
    spindlePosAvg = phase;
    x.speedMax = LONG_MAX;
    z.speedMax = LONG_MAX;
    cncThreadState = CNC_THREAD_RUN;
  }
  long spindle = spindlePosAvg;
  bool finished = spindle >= cncThreadSpindleTarget;
  float progress = finished ? 1.0 : spindle / float(cncThreadSpindleTarget);
  if (progress < 0) progress = 0;
  if (finished) {
    stepToFinal(&x, cncThreadDiff[0]);
    stepToFinal(&z, cncThreadDiff[1]);
    cncThreadState = CNC_THREAD_NONE;
  } else {
    stepToContinuous(&x, lround(cncThreadDiff[0] * progress));
    stepToContinuous(&z, lround(cncThreadDiff[1] * progress));
  }
}

// Runs in taskGcode after the planner has emptied. False if it was refused or stopped.
bool cncThread() {
  int32_t target[CNC_N_AXIS];
  float leadMm;
  cnc_gc_thread(target, &leadMm);
  long xDiff = target[CNC_X] - x.pos;
  long zDiff = target[CNC_Z] - z.pos;
  Axis* lead = zDiff != 0 ? &z : (xDiff != 0 ? &x : NULL);
  if (lead == NULL) return true;
  long leadDiff = lead == &z ? zDiff : xDiff;
  long pitchDuAbs = lround(leadMm * 10000.0);
  long pitchDu = leadDiff > 0 ? pitchDuAbs : -pitchDuAbs;
  long spindleTarget = cncSpindleFromPosWithPitch(lead, leadDiff, pitchDu);
  if (pitchDuAbs <= 0 || spindleTarget <= 0) {
    Serial.println("error: G32/G33 move too short for its lead");
    cncLog("error: G32/G33 move too short for its lead");
    cncSyncFromAxes(true);
    return false;
  }
  if (xSemaphoreTake(motionMutex, 100) != pdTRUE) {
    Serial.println("error: failed to start G32/G33");
    cncLog("error: failed to start G32/G33");
    cncSyncFromAxes(true);
    return false;
  }
  cncThreadLeadAxis = lead;
  cncThreadPitchDu = pitchDu;
  cncThreadDiff[0] = xDiff;
  cncThreadDiff[1] = zDiff;
  cncThreadSpindleTarget = spindleTarget;
  cncThreadPhaseKnown = false;
  cncThreadState = CNC_THREAD_PHASE;
  xSemaphoreGive(motionMutex);
  cncStatusText = "Thread: waiting";
  while (cncThreadState != CNC_THREAD_NONE) {
    if (cncThreadState == CNC_THREAD_RUN) cncStatusText = "Thread";
    if (emergencyStop != ESTOP_NONE) return false;
    cncPollSerial();
    vTaskDelay(1);
  }
  // Wait for the last steps, then the planner continues from where the axes are.
  while (x.pendingPos != 0 || z.pendingPos != 0) {
    if (emergencyStop != ESTOP_NONE) return false;
    cncPollSerial();
    vTaskDelay(1);
  }
  cncStatusText = "";
  bool completed = isOn && !cncStopRequest && x.pos == cncThreadDiff[0] && z.pos == cncThreadDiff[1];
  cncSyncFromAxes(true);
  return completed;
}

// Called often from taskGcode: stop requests, the spindle check and the step segments.
void cncService() {
  if (cncFeedOverrideWanted != cnc_feed_override() || cncRapidOverrideWanted != cnc_rapid_override()) {
    cnc_set_overrides(cncFeedOverrideWanted, cncRapidOverrideWanted);  // re-plans what is queued at once
  }
  if (cncFaultStop) {
    // A limit switch or a driver alarm: the step interrupt has stopped at once. Drop everything planned;
    // the positions are exact (every step given is counted).
    cncThreadState = CNC_THREAD_NONE;
    cncState = CNC_IDLE;
    cncPaused = false;
    cncSpindleHold = false;
    cncUserHold = false;
    cncStopRequest = false;
    cncSyncFromAxes();
    cncFaultStop = false;
    cncStatusText = cncFaultText;
    if (cncOwesDisable) {
      cncOwesDisable = false;
      stepperEnable(&z, false);
      stepperEnable(&x, false);
      stepperEnable(&a1, false);
    }
    return;
  }
  if (cncStopRequest) {
    cnc_hold_start();
    cnc_st_prep_buffer();
    if (cnc_hold_complete() || !cnc_st_busy()) {
      cncState = CNC_IDLE;
      cncPaused = false;
      cncSpindleHold = false;
      cncUserHold = false;
      cncStatusText = "";
      cncSyncFromAxes(); // drops what is left of the program; the position is exact
      if (cncOwesDisable) {
        cncOwesDisable = false;
        stepperEnable(&z, false);
        stepperEnable(&x, false);
        stepperEnable(&a1, false);
      }
      cncStopRequest = false;
    }
    return;
  }
  if (cncUserHold && !cnc_hold_active()) {
    cnc_hold_start();
    cncStatusText = "Paused from the web";
  } else if (!cncUserHold && cnc_hold_active() && !cncSpindleHold && cnc_hold_complete()) {
    cnc_hold_resume();
    cncStatusText = "";
  }
  if (SPINDLE_PAUSES_GCODE && isOn && mode == MODE_GCODE && cnc_st_busy() && !cncUserHold) {
    bool spindleOn = getApproxRpm() >= GCODE_MIN_RPM;
    if (!spindleOn && !cncSpindleHold) {
      cnc_hold_start();
      cncSpindleHold = true;
      cncStatusText = "Spindle stopped";
    } else if (spindleOn && cncSpindleHold && cnc_hold_complete()) {
      cnc_hold_resume();
      cncSpindleHold = false;
      cncStatusText = "";
    }
  }
  cnc_st_prep_buffer();
  if (cncState == CNC_RUN && !cnc_st_busy() && !cncSpindleHold) cncState = CNC_IDLE;
}

void cncPrintStatus() {
  Serial.print("<");
  Serial.print(cncSpindleHold || cncPaused || cncUserHold ? "Hold" : (isOn ? "Run" : "Idle"));
  Serial.print("|WPos:");
  float divisor = measure == MEASURE_METRIC ? 10000.0 : 254000.0;
  Serial.print(getAxisPosDu(&x) / divisor, 3);
  Serial.print(",0.000,");
  Serial.print(getAxisPosDu(&z) / divisor, 3);
  if (a1.active) {
    Serial.print(",");
    Serial.print(getAxisPosDu(&a1) / 10000.0, 3);
  }
  Serial.print("|FS:");
  Serial.print(mode == MODE_GCODE ? round(cnc_st_realtime_rate()) : round(gcodeFeedDuPerSec * 60 / 10000.0));
  Serial.print(",");
  Serial.print(getApproxRpm());
  Serial.print("|Un:");
  Serial.print(cnc_st_underruns()); // the step buffer ran empty while moving: should always be 0
  Serial.print("|Id:");
  Serial.print("H" + String(HARDWARE_VERSION) + "V" + String(SOFTWARE_VERSION) + "CNC");
  Serial.print(">"); // no new line to allow client to easily cut out the status response
}

// While taskGcode waits for the planner: answer '!', '~' and '?' at once, keep everything else for later.
void cncPollSerial() {
  if (GRBL_ON_USB) {
    while (Serial.available() > 0 && grblRoomFor(0)) grblInput(0, Serial.read());
    return;
  }
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (gcodePollInBrace) {
      if (c == ')') gcodePollInBrace = false;
    } else if (gcodePollInSemicolon) {
      if (c < 32) gcodePollInSemicolon = false;
    } else if (c == '(') {
      gcodePollInBrace = true;
    } else if (c == ';') {
      gcodePollInSemicolon = true;
    } else if (c == '!') {
      setIsOnFromTask(false);
      continue;
    } else if (c == '~') {
      if (cncPaused) {
        cncResumeRequest = true;
      } else {
        setIsOnFromTask(true);
        if (!isOn) gcodeAwaitOnMs = millis(); // the lines after '~' wait until loop() has turned on
      }
      continue;
    } else if (c == '?') {
      cncPrintStatus();
      continue;
    }
    gcodeSerialBacklog += c;
  }
}

// Waits in taskGcode until all motion is done. False if the program was stopped meanwhile.
bool cncWaitIdle() {
  while (cncBusy()) {
    if (!isOn || cncStopRequest || emergencyStop != ESTOP_NONE) return false;
    cncService();
    cncPollSerial();
    vTaskDelay(1);
  }
  return isOn && !cncStopRequest;
}

// G4 / cycle dwell, seconds. False if the program was stopped meanwhile.
bool cncDwell(float seconds) {
  unsigned long until = millis() + (unsigned long)(seconds * 1000);
  while ((long)(until - millis()) > 0) {
    if (!isOn || cncStopRequest) return false;
    cncPollSerial();
    vTaskDelay(1);
  }
  return true;
}

// Runs one G-code line in the GCODE mode. False on an error (printed) or if the program was stopped.
bool cncProcessLine(const String& line, bool isSerial) {
  if (cncStartRequest && !cncBusy()) {
    cncStartRequest = false;
    cncSyncFromAxes(cncStartKeepModes);
    cncStartKeepModes = false;
  }
  cncLineNo++;
  strncpy(gcodeDisplayLine, line.c_str(), sizeof(gcodeDisplayLine) - 1);
  gcodeDisplayLine[sizeof(gcodeDisplayLine) - 1] = '\0';
  char msg[96];
  float dwell = 0;
  int r;
  while (true) {
    if (!isOn || cncStopRequest || emergencyStop != ESTOP_NONE) return false;
    r = cnc_gc_line(line.c_str(), cncLineNo, msg, sizeof(msg), &dwell);
    if (r == CNC_DWELL_MORE || r == CNC_THREAD_MORE) {
      // a canned cycle (dwell at the bottom) or G76 (a threading pass): run it, then the same line goes on
      if (cnc_st_busy()) cncState = CNC_RUN;
      if (!cncWaitIdle()) return false;
      if (r == CNC_DWELL_MORE) {
        if (!cncDwell(dwell)) return false;
      } else if (!cncThread()) {
        return false;
      }
      continue;
    }
    if (r != CNC_WAIT) break;
    cncService();
    cncPollSerial();
    vTaskDelay(1);
  }
  if (r == CNC_ERROR) {
    strncpy(cncLastError, msg, sizeof(cncLastError) - 1);
    cncLastError[sizeof(cncLastError) - 1] = '\0';
    if (!cncGrblLine) {
      Serial.print("error: ");
      Serial.print(msg);
      Serial.print(" in: ");
      Serial.println(line);
    }
    String text = "error line " + String(cncLineNo) + ": " + msg + " in: " + line;
    cncLog(text.c_str());
    return false;
  }
  if (msg[0]) {
    if (cncGrblLine) {
      grblMessage(msg);
    } else {
      Serial.print("[MSG:");
      Serial.print(msg);
      Serial.println("]");
    }
    String text = "line " + String(cncLineNo) + ": " + msg;
    cncLog(text.c_str());
  }
  // G20 / G21 also switch the H4 display units, as in the old GCODE mode.
  String upper = line;
  upper.toUpperCase();
  if (upper.indexOf("G20") >= 0) setMeasure(MEASURE_INCH);
  else if (upper.indexOf("G21") >= 0) setMeasure(MEASURE_METRIC);
  if (cnc_st_busy()) cncState = CNC_RUN;
  if (cnc_gc_offsets_changes() != cncOffsetsSeen) {
    // G10 / G92 / G28.1 ...: a changed G54 is the H4's own zero, so the display follows it at once
    cncOffsetsSeen = cnc_gc_offsets_changes();
    cncG54ToAxes();
  }
  cncService();

  // A G94 program streamed over USB gets "ok" only after its move (the old protocol: senders such as
  // lathecode send '!' after the last "ok"). G93 programs and stored programs run with look-ahead.
  bool waitForMotion = r != CNC_OK || (isSerial && !cnc_gc_inverse_time());
  if (waitForMotion && !cncWaitIdle()) return false;
  if (r == CNC_DWELL) {
    if (!cncDwell(dwell)) return false;
  } else if (r == CNC_PAUSE) {
    cncPaused = true;
    cncResumeRequest = false;
    cncStatusText = "Paused: ON to go on";
    beepFlag = true;
    while (!cncResumeRequest) {
      if (!isOn || cncStopRequest || emergencyStop != ESTOP_NONE) {
        cncPaused = false;
        cncStatusText = "";
        return false;
      }
      cncPollSerial();
      vTaskDelay(10);
    }
    cncPaused = false;
    cncResumeRequest = false;
    cncStatusText = "";
  } else if (r == CNC_END) {
    gcodeProgramCharIndex = gcodeProgramSize; // a stored program stops reading here
    setIsOnFromTask(false);
  } else if (r == CNC_THREAD) {
    if (!cncThread()) return false;
  }
  return true;
}

void taskGcode(void *param) {
  while (emergencyStop == ESTOP_NONE) {
    if (cncStartRequest && !cncBusy()) {
      // A program starts (ON pressed or '~' received): positions and modes from the H4.
      cncStartRequest = false;
      cncDiagReset(); // the diagnostics of the web page describe this program
      cncSyncFromAxes(cncStartKeepModes);
      cncStartKeepModes = false;
    }
    if (GRBL_ON_USB) {
      // USB speaks Grbl (grbl.ino): every byte goes there, in every mode
      while (Serial.available() > 0 && grblRoomFor(0)) grblInput(0, Serial.read());
    }
    if (grblServiceLine()) continue; // one line of a Grbl sender, if there is one (grbl.ino)
    if (cncBenchRequest) {
      cncBenchRequest = false;
      if (!isOn && !cncBusy() && !homingActive) cncRunBench();
      continue;
    }
    if (webJogService()) continue;   // a jog piece from the web page's gamepad (grbl.ino)
    bool gotChar = false;
    if (mode != MODE_GCODE) {
      gcodeInitialized = false;
      gcodeSerialBacklog = "";
      gcodePollInBrace = false;
      gcodePollInSemicolon = false;
    } else if (!gcodeInitialized) {
      gcodeInitialized = true;
      gcodeCommand = "";
      gcodeAbsolutePositioning = true;
      gcodeFeedDuPerSec = GCODE_FEED_DEFAULT_DU_SEC;
      gcodeInBrace = false;
      gcodeInSemicolon = false;
    }
    // Implementing a relevant subset of RS274 (Gcode) and GRBL (state management) covering basic use cases.
    char receivedChar = '\0';
    bool isSerial = false;
    // GCODE mode: all USB input goes through cncPollSerial(), which acts on '!', '~' and '?' at once
    // (also while a line waits for the planner) and keeps the rest, in order, in gcodeSerialBacklog.
    if (mode == MODE_GCODE && !GRBL_ON_USB) cncPollSerial();
    if (mode == MODE_GCODE && isOn && gcodeProgramCharIndex < gcodeProgramSize) {
      receivedChar = gcodeProgramData[gcodeProgramCharIndex];
      gcodeProgramCharIndex++;
    } else if (mode == MODE_GCODE) {
      bool awaitingOn = gcodeAwaitOnMs != 0 && !isOn && millis() - gcodeAwaitOnMs < 200;
      if (isOn || !awaitingOn) gcodeAwaitOnMs = 0;
      if (gcodeSerialBacklog.length() > 0 && !awaitingOn) {
        isSerial = true;
        receivedChar = gcodeSerialBacklog.charAt(0);
        gcodeSerialBacklog.remove(0, 1);
      }
    } else if (!GRBL_ON_USB && Serial.available() > 0) {
      isSerial = true;
      receivedChar = Serial.read();
    }
    int charCode = int(receivedChar);
    if (charCode > 0) {
      gotChar = true;
      if (gcodeInBrace) {
        if (receivedChar == ')') gcodeInBrace = false;
      } else if (serialInKeycode) {
        if (charCode < 32) {
          if (serialKeycode == 0) {
            serialKeycode = keycodeCommand.toInt();
            Serial.println(serialKeycode);
          } else {
            Serial.println("slower");
          }
          serialInKeycode = false;
          keycodeCommand = "";
        } else {
          keycodeCommand += receivedChar;
        }
      } else if (receivedChar == '(') {
        gcodeInBrace = true;
      } else if (receivedChar == ';' /* start of comment till end of line */) {
        gcodeInSemicolon = true;
      } else if (gcodeInSemicolon && charCode >= 32) {
        // Ignoring comment.
      } else if (receivedChar == '!' /* stop */) {
        setIsOnFromTask(false);
      } else if (receivedChar == '~' /* resume */) {
        setIsOnFromTask(true);
      } else if (receivedChar == '%' /* start/end marker */) {
        // Not using % markers in this implementation.
      } else if (receivedChar == '?' /* status */) {
        Serial.print("<");
        Serial.print(isOn ? "Run" : "Idle");
        Serial.print("|WPos:");
        float divisor = measure == MEASURE_METRIC ? 10000.0 : 254000.0;
        Serial.print(getAxisPosDu(&x) / divisor, 3);
        Serial.print(",0.000,");
        Serial.print(getAxisPosDu(&z) / divisor, 3);
        Serial.print("|FS:");
        Serial.print(round(gcodeFeedDuPerSec * 60 / 10000.0));
        Serial.print(",");
        Serial.print(getApproxRpm());
        Serial.print("|Id:");
        Serial.print("H" + String(HARDWARE_VERSION) + "V" + String(SOFTWARE_VERSION));
        Serial.print(">"); // no new line to allow client to easily cut out the status response
      } else if (gcodeInSave && receivedChar == '"' /* end of saved program */) {
        gcodeInSave = false;
        if (gcodeSaveName.length() == 0) {
          if (removeAllGcode()) Serial.println("ok");
        } else if (gcodeSaveValue.length() > 1) {
          if (saveGcode()) Serial.println("ok");
        } else if (gcodeSaveName.length() == 1) {
          Serial.println("error: name must be at least 2 chars");
        } else {
          Preferences pref;
          pref.begin(GCODE_NAMESPACE);
          bool found = false;
          for (int i = 0; i < 256; i++) {
            if (!pref.isKey(String(i).c_str())) break;
            if (gcodeSaveName.equals(pref.getString(String(i).c_str()))) {
              found = true;
              if (removeGcode(i)) Serial.println("ok");
              break;
            }
          }
          if (!found) Serial.println("error: name not found");
          pref.end();
        }
        gcodeSaveName = "";
        gcodeSaveValue = "";
      } else if (!gcodeInSave && receivedChar == '"' /* start of save program */) {
        gcodeInSave = true;
        gcodeInSaveFirstLine = true;
      } else if (gcodeInSaveFirstLine && receivedChar >= 32) {
        gcodeSaveName += receivedChar;
      } else if (gcodeInSaveFirstLine && receivedChar < 32) {
        gcodeInSaveFirstLine = false;
        Serial.println("ok");
      } else if (gcodeInSave) {
        gcodeSaveValue += receivedChar;
        if (receivedChar < 32) {
          gcodeInBrace = false;
          gcodeInSemicolon = false;
          Serial.println("ok");
        }
      } else if (isOn && mode == MODE_GCODE) {
        // CNC: one whole line at a time, all words of it together.
        if (charCode < 32) {
          String line = gcodeCommand;
          line.trim();
          gcodeCommand = "";
          gcodeInSemicolon = false;
          if (line.length() == 0) {
            if (isSerial) Serial.println("ok");
          } else if (cncProcessLine(line, isSerial)) {
            if (isSerial) Serial.println("ok");
          } else {
            setIsOnFromTask(false); // error or stop: the program does not go on
          }
        } else {
          gcodeCommand += receivedChar;
        }
      } else if (isOn) {
        if (gcodeInBrace && charCode < 32) {
          Serial.println("error: comment not closed");
          setIsOnFromTask(false);
        } else if (charCode < 32 && gcodeCommand.length() > 1) {
          if (handleGcodeCommand(gcodeCommand) && isSerial) Serial.println("ok");
          gcodeCommand = "";
          gcodeInSemicolon = false;
        } else if (charCode < 32) {
          if (isSerial) Serial.println("ok");
          gcodeCommand = "";
        } else if (charCode >= 32 && (charCode == 'G' || charCode == 'M')) {
          // Split consequent G and M commands on one line.
          // No "ok" for commands in the middle of the line.
          handleGcodeCommand(gcodeCommand);
          gcodeCommand = receivedChar;
        } else if (charCode >= 32) {
          gcodeCommand += receivedChar;
        }
      } else if (receivedChar == '=' /* start of keycode command */) {
        serialInKeycode = true;
        keycodeCommand = "";
      } else {
        // ignoring non-realtime command input when off
        // to flush any commands coming after an error
      }
    }
    if (mode == MODE_GCODE && isOn && gcodeProgramCharIndex > 0 && gcodeProgramCharIndex == gcodeProgramSize && !cncBusy()) {
      setIsOnFromTask(false); // a stored program ends when its motion has ended
    }
    cncService();
    // (writing to flash stops the step interrupt for milliseconds: only when nothing moves or is queued)
    if (!homingActive && !cncBusy() && !stepperIsRunning(&x) && !stepperIsRunning(&z) && !stepperIsRunning(&a1)) cncSaveOffsetsIfChanged();
    // Wait a tick when there was nothing to read, so the lower-priority tasks on this core run.
    if (gotChar) taskYIELD();
    else vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

bool saveGcode() {
  Preferences pref;
  pref.begin(GCODE_NAMESPACE);
  bool success = false;
  if (gcodeSaveName.length() < 2) {
    Serial.println("error: name must be at least 2 chars");
  } else if (gcodeSaveValue.length() < 2) {
    Serial.println("error: program too short");
  } else if (pref.freeEntries() < 2 || gcodeProgramCount >= 256) {
    Serial.println("error: memory full");
  } else if (pref.isKey(gcodeSaveName.c_str())) {
    if (pref.putString(gcodeSaveName.c_str(), gcodeSaveValue) != gcodeSaveValue.length()) {
      Serial.println("error: failed to overwrite");
    } else {
      success = true;
    }
  } else {
    if (pref.putString(String(gcodeProgramCount).c_str(), gcodeSaveName) == 0) {
      Serial.println("error: not enough memory for program name");
    } else if (pref.putString(gcodeSaveName.c_str(), gcodeSaveValue) != gcodeSaveValue.length()) {
      pref.remove(String(gcodeProgramCount).c_str());
      Serial.println("error: not enough memory for program text");
    } else {
      gcodeProgramCount++;
      success = true;
    }
  }
  pref.end();
  return success;
}

bool removeGcode(int indexToRemove) {
  Preferences pref;
  pref.begin(GCODE_NAMESPACE);
  bool success = false;
  if (indexToRemove >= 0 && indexToRemove < 256 && pref.isKey(String(indexToRemove).c_str())) {
    success = true;
    String programName = pref.getString(String(indexToRemove).c_str());
    pref.remove(String(indexToRemove).c_str());
    if (programName.length() > 0) {
      pref.remove(programName.c_str());
    }
    // Move all the following program names down to avoid holes.
    for (int i = indexToRemove + 1; pref.isKey(String(i).c_str()); i++) {
      pref.putString(String(i - 1).c_str(), pref.getString(String(i).c_str()));
      pref.remove(String(i).c_str());
    }
    if (gcodeProgramCount > 0) gcodeProgramCount--;
    if (gcodeProgramCount > 0 && gcodeProgramIndex >= gcodeProgramCount) {
      gcodeProgramIndex = gcodeProgramCount - 1;
    }
  } else {
    Serial.print("error: program to delete not found at index ");
    Serial.println(indexToRemove);
  }
  pref.end();
  return success;
}

bool removeAllGcode() {
  Preferences pref;
  pref.begin(GCODE_NAMESPACE);
  bool success = pref.clear();
  if (!success) Serial.println("error: failed clearing GCODE_NAMESPACE");
  pref.end();
  return true;
}

void startPulseCounter(pcnt_unit_t unit, int gpioA, int gpioB) {
  pcnt_config_t pcntConfig;
  pcntConfig.pulse_gpio_num = gpioA;
  pcntConfig.ctrl_gpio_num = gpioB;
  pcntConfig.channel = PCNT_CHANNEL_0;
  pcntConfig.unit = unit;
  pcntConfig.pos_mode = PCNT_COUNT_INC;
  pcntConfig.neg_mode = PCNT_COUNT_DEC;
  pcntConfig.lctrl_mode = PCNT_MODE_REVERSE;
  pcntConfig.hctrl_mode = PCNT_MODE_KEEP;
  pcntConfig.counter_h_lim = PCNT_LIM;
  pcntConfig.counter_l_lim = -PCNT_LIM;
  pcnt_unit_config(&pcntConfig);
  pcnt_set_filter_value(unit, ENCODER_FILTER);
	pcnt_filter_enable(unit);
  pcnt_counter_pause(unit);
  pcnt_counter_clear(unit);
  pcnt_counter_resume(unit);
}

// Attaching interrupt on core 0 to have more time on core 1 where axes are moved.
void taskAttachInterrupts(void *param) {
  startPulseCounter(PCNT_UNIT_0, ENC_A, ENC_B);
  if (PULSE_1_USE) attachInterrupt(digitalPinToInterrupt(A12), pulse1Enc, CHANGE);
  if (PULSE_2_USE) attachInterrupt(digitalPinToInterrupt(A22), pulse2Enc, CHANGE);
  vTaskDelete(NULL);
}

void setEmergencyStop(int kind) {
  emergencyStop = kind;
  cncStepsEnabled = false; // GCODE mode: no more steps at all
  z.homed = x.homed = a1.homed = false;
  setAsyncTimerEnable(false);
  xSemaphoreTake(z.mutex, 10);
  xSemaphoreTake(x.mutex, 10);
  xSemaphoreTake(a1.mutex, 10);
}

void setup() {
#ifdef HAS_MY_MACHINE
  myMachine(); // your machine's values (my_machine.h)
#endif
  loadMachineConfig(); // machine settings saved from the web page (config.ino), before anything uses them
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  pinMode(Z_DIR, OUTPUT);
  pinMode(Z_STEP, OUTPUT);
  pinMode(Z_ENA, OUTPUT);
  DHIGH(Z_STEP);

  pinMode(X_DIR, OUTPUT);
  pinMode(X_STEP, OUTPUT);
  pinMode(X_ENA, OUTPUT);
  DHIGH(X_STEP);

  if (ACTIVE_A1) {
    pinMode(A12, OUTPUT);
    pinMode(A13, OUTPUT);
    pinMode(A11, OUTPUT);
    DHIGH(A13);
  }

  pinMode(BUZZ, OUTPUT);

  if (PULSE_1_USE) {
    pinMode(A11, OUTPUT);
    pinMode(A12, INPUT);
    pinMode(A13, INPUT);
    DLOW(A11);
  }

  if (PULSE_2_USE) {
    pinMode(A21, OUTPUT);
    pinMode(A22, INPUT);
    pinMode(A23, INPUT);
    DLOW(A21);
  }
  else {
    // Terminal A2 as inputs: home switches and driver alarm (homing.ino). Read even when not used, so the
    // web page can show them while wiring.
    pinMode(IN_HOME_X, INPUT_PULLUP);
    pinMode(IN_HOME_Z, INPUT_PULLUP);
    pinMode(IN_ALARM, INPUT_PULLUP);
  }

  Preferences pref;
  pref.begin(PREF_NAMESPACE);
  if (pref.getInt(PREF_VERSION) != PREFERENCES_VERSION) {
    pref.clear();
    pref.putInt(PREF_VERSION, PREFERENCES_VERSION);
  }

  initAxis(&z, NAME_Z, true, false, MOTOR_STEPS_Z, SCREW_Z_DU, SPEED_START_Z, SPEED_MANUAL_MOVE_Z, ACCELERATION_Z, INVERT_Z, NEEDS_REST_Z, MAX_TRAVEL_MM_Z, BACKLASH_DU_Z, Z_ENA, Z_DIR, Z_STEP);
  initAxis(&x, NAME_X, true, false, MOTOR_STEPS_X, SCREW_X_DU, SPEED_START_X, SPEED_MANUAL_MOVE_X, ACCELERATION_X, INVERT_X, NEEDS_REST_X, MAX_TRAVEL_MM_X, BACKLASH_DU_X, X_ENA, X_DIR, X_STEP);
  initAxis(&a1, NAME_A1, ACTIVE_A1, ROTARY_A1, MOTOR_STEPS_A1, SCREW_A1_DU, SPEED_START_A1, SPEED_MANUAL_MOVE_A1, ACCELERATION_A1, INVERT_A1, NEEDS_REST_A1, MAX_TRAVEL_MM_A1, BACKLASH_DU_A1, A11, A12, A13);
  cncConfigure();

  isOn = false;
  savedDupr = dupr = pref.getLong(PREF_DUPR);
  motionMutex = xSemaphoreCreateMutex();
  savedStarts = starts = min(STARTS_MAX, max(static_cast<int32_t>(1), pref.getInt(PREF_STARTS)));
  z.savedPos = z.pos = pref.getLong(PREF_POS_Z);
  z.savedPosGlobal = z.posGlobal = pref.getLong(PREF_POS_GLOBAL_Z);
  z.savedOriginPos = z.originPos = pref.getLong(PREF_ORIGIN_POS_Z);
  z.savedMotorPos = z.motorPos = pref.getLong(PREF_MOTOR_POS_Z);
  z.savedLeftStop = z.leftStop = pref.getLong(PREF_LEFT_STOP_Z, LONG_MAX);
  z.savedRightStop = z.rightStop = pref.getLong(PREF_RIGHT_STOP_Z, LONG_MIN);
  z.savedDisabled = z.disabled = pref.getBool(PREF_DISABLED_Z, false);
  x.savedPos = x.pos = pref.getLong(PREF_POS_X);
  x.savedPosGlobal = x.posGlobal = pref.getLong(PREF_POS_GLOBAL_X);
  x.savedOriginPos = x.originPos = pref.getLong(PREF_ORIGIN_POS_X);
  x.savedMotorPos = x.motorPos = pref.getLong(PREF_MOTOR_POS_X);
  x.savedLeftStop = x.leftStop = pref.getLong(PREF_LEFT_STOP_X, LONG_MAX);
  x.savedRightStop = x.rightStop = pref.getLong(PREF_RIGHT_STOP_X, LONG_MIN);
  x.savedDisabled = x.disabled = pref.getBool(PREF_DISABLED_X, false);
  a1.savedPos = a1.pos = pref.getLong(PREF_POS_A1);
  a1.savedPosGlobal = a1.posGlobal = pref.getLong(PREF_POS_GLOBAL_A1);
  a1.savedOriginPos = a1.originPos = pref.getLong(PREF_ORIGIN_POS_A1);
  a1.savedMotorPos = a1.motorPos = pref.getLong(PREF_MOTOR_POS_A1);
  a1.savedLeftStop = a1.leftStop = pref.getLong(PREF_LEFT_STOP_A1, LONG_MAX);
  a1.savedRightStop = a1.rightStop = pref.getLong(PREF_RIGHT_STOP_A1, LONG_MIN);
  a1.savedDisabled = a1.disabled = pref.getBool(PREF_DISABLED_A1, false);
  savedSpindlePos = spindlePos = pref.getLong(PREF_SPINDLE_POS);
  savedSpindlePosAvg = spindlePosAvg = pref.getLong(PREF_SPINDLE_POS_AVG);
  savedSpindlePosSync = spindlePosSync = pref.getInt(PREF_OUT_OF_SYNC);
  savedSpindlePosGlobal = spindlePosGlobal = pref.getLong(PREF_SPINDLE_POS_GLOBAL);
  savedShowAngle = showAngle = pref.getBool(PREF_SHOW_ANGLE);
  savedShowTacho = showTacho = pref.getBool(PREF_SHOW_TACHO);
  savedMoveStep = moveStep = pref.getLong(PREF_MOVE_STEP, MOVE_STEP_1);
  setModeFromLoop(savedMode = pref.getInt(PREF_MODE));
  savedMeasure = measure = pref.getInt(PREF_MEASURE);
  savedConeRatio = coneRatio = pref.getFloat(PREF_CONE_RATIO, coneRatio);
  savedTurnPasses = turnPasses = pref.getInt(PREF_TURN_PASSES, turnPasses);
  savedAuxForward = auxForward = pref.getBool(PREF_AUX_FORWARD, true);
  pref.end();

  if (!z.needsRest && !z.disabled) {
    DHIGH(z.ena);
  }
  if (!x.needsRest && !x.disabled) {
    DHIGH(x.ena);
  }
  if (a1.active && !a1.needsRest && !a1.disabled) {
    DHIGH(a1.ena);
  }

  pref.begin(GCODE_NAMESPACE);
  gcodeProgramCount = 0;
  for (int i = 0; i < 256; i++) {
    if (pref.isKey(String(i).c_str())) {
      gcodeProgramCount++;
    } else {
      break;
    }
  }
  pref.end();

  cncStartTimer(); // after the positions are restored; the timer idles until a program runs

  lcd.begin(20, 4);
  lcd.clearShadow();
  lcd.createChar(customCharMmCode, customCharMm);
  lcd.createChar(customCharLimLeftCode, customCharLimLeft);
  lcd.createChar(customCharLimRightCode, customCharLimRight);
  lcd.createChar(customCharLimUpCode, customCharLimUp);
  lcd.createChar(customCharLimDownCode, customCharLimDown);
  lcd.createChar(customCharLimUpDownCode, customCharLimUpDown);
  lcd.createChar(customCharLimLeftRightCode, customCharLimLeftRight);

  Serial.begin(115200);

  if (!Wire.begin(SDA, SCL)) {
    Serial.println("I2C initialization failed");
  } else if (!keypad.begin(TCA8418_DEFAULT_ADDR, &Wire)) {
    Serial.println("TCA8418 key controller not found");
  } else {
    keypad.matrix(7, 7);
    keypad.flush();
  }

  // Non-time-sensitive tasks on core 0.
  xTaskCreatePinnedToCore(taskDisplay, "taskDisplay", 10000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);

  delay(100);
  if (keypad.available()) {
    setEmergencyStop(ESTOP_KEY);
    return;
  } else {
    xTaskCreatePinnedToCore(taskKeypad, "taskKeypad", 10000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);
  }

  xTaskCreatePinnedToCore(taskMoveZ, "taskMoveZ", 10000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);
  xTaskCreatePinnedToCore(taskMoveX, "taskMoveX", 10000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);
  if (a1.active) xTaskCreatePinnedToCore(taskMoveA1, "taskMoveA1", 10000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);
  xTaskCreatePinnedToCore(taskAttachInterrupts, "taskAttachInterrupts", 10000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);
  // Priority 1: it prepares the step segments of the GCODE mode and must not wait behind the display.
  xTaskCreatePinnedToCore(taskGcode, "taskGcode", 10000 /* stack size */, NULL, 1 /* priority */, NULL, 0 /* core */);
  if (homingConfigured()) xTaskCreatePinnedToCore(taskHoming, "taskHoming", 6000 /* stack size */, NULL, 1 /* priority */, NULL, 0 /* core */);
  if (GRBL_ON_USB) grblBanner(0); // a Grbl sender on USB waits for this line
  if (WIFI_ENABLED) xTaskCreatePinnedToCore(taskWifi, "taskWifi", 12000 /* stack size */, NULL, 0 /* priority */, NULL, 0 /* core */);
}

bool saveIfChanged() {
  // Should avoid calling Preferences whenever possible to reduce memory wear and avoid ~20ms write delay that blocks interrupts.
  if (dupr == savedDupr && starts == savedStarts && z.pos == z.savedPos && z.originPos == z.savedOriginPos && z.posGlobal == z.savedPosGlobal && z.motorPos == z.savedMotorPos && z.leftStop == z.savedLeftStop && z.rightStop == z.savedRightStop && z.disabled == z.savedDisabled &&
      spindlePos == savedSpindlePos && spindlePosAvg == savedSpindlePosAvg && spindlePosSync == savedSpindlePosSync && savedSpindlePosGlobal == spindlePosGlobal && showAngle == savedShowAngle && showTacho == savedShowTacho && moveStep == savedMoveStep &&
      mode == savedMode && measure == savedMeasure && x.pos == x.savedPos && x.originPos == x.savedOriginPos && x.posGlobal == x.savedPosGlobal && x.motorPos == x.savedMotorPos && x.leftStop == x.savedLeftStop && x.rightStop == x.savedRightStop && x.disabled == x.savedDisabled &&
      a1.pos == a1.savedPos && a1.originPos == a1.savedOriginPos && a1.posGlobal == a1.savedPosGlobal && a1.motorPos == a1.savedMotorPos && a1.leftStop == a1.savedLeftStop && a1.rightStop == a1.savedRightStop && a1.disabled == a1.savedDisabled &&
      coneRatio == savedConeRatio && turnPasses == savedTurnPasses && savedAuxForward == auxForward) return false;

  Preferences pref;
  pref.begin(PREF_NAMESPACE);
  if (dupr != savedDupr) pref.putLong(PREF_DUPR, savedDupr = dupr);
  if (starts != savedStarts) pref.putInt(PREF_STARTS, savedStarts = starts);
  if (z.pos != z.savedPos) pref.putLong(PREF_POS_Z, z.savedPos = z.pos);
  if (z.posGlobal != z.savedPosGlobal) pref.putLong(PREF_POS_GLOBAL_Z, z.savedPosGlobal = z.posGlobal);
  if (z.originPos != z.savedOriginPos) pref.putLong(PREF_ORIGIN_POS_Z, z.savedOriginPos = z.originPos);
  if (z.motorPos != z.savedMotorPos) pref.putLong(PREF_MOTOR_POS_Z, z.savedMotorPos = z.motorPos);
  if (z.leftStop != z.savedLeftStop) pref.putLong(PREF_LEFT_STOP_Z, z.savedLeftStop = z.leftStop);
  if (z.rightStop != z.savedRightStop) pref.putLong(PREF_RIGHT_STOP_Z, z.savedRightStop = z.rightStop);
  if (z.disabled != z.savedDisabled) pref.putBool(PREF_DISABLED_Z, z.savedDisabled = z.disabled);
  if (spindlePos != savedSpindlePos) pref.putLong(PREF_SPINDLE_POS, savedSpindlePos = spindlePos);
  if (spindlePosAvg != savedSpindlePosAvg) pref.putLong(PREF_SPINDLE_POS_AVG, savedSpindlePosAvg = spindlePosAvg);
  if (spindlePosSync != savedSpindlePosSync) pref.putInt(PREF_OUT_OF_SYNC, savedSpindlePosSync = spindlePosSync);
  if (spindlePosGlobal != savedSpindlePosGlobal) pref.putLong(PREF_SPINDLE_POS_GLOBAL, savedSpindlePosGlobal = spindlePosGlobal);
  if (showAngle != savedShowAngle) pref.putBool(PREF_SHOW_ANGLE, savedShowAngle = showAngle);
  if (showTacho != savedShowTacho) pref.putBool(PREF_SHOW_TACHO, savedShowTacho = showTacho);
  if (moveStep != savedMoveStep) pref.putLong(PREF_MOVE_STEP, savedMoveStep = moveStep);
  if (mode != savedMode) pref.putInt(PREF_MODE, savedMode = mode);
  if (measure != savedMeasure) pref.putInt(PREF_MEASURE, savedMeasure = measure);
  if (x.pos != x.savedPos) pref.putLong(PREF_POS_X, x.savedPos = x.pos);
  if (x.posGlobal != x.savedPosGlobal) pref.putLong(PREF_POS_GLOBAL_X, x.savedPosGlobal = x.posGlobal);
  if (x.originPos != x.savedOriginPos) pref.putLong(PREF_ORIGIN_POS_X, x.savedOriginPos = x.originPos);
  if (x.motorPos != x.savedMotorPos) pref.putLong(PREF_MOTOR_POS_X, x.savedMotorPos = x.motorPos);
  if (x.leftStop != x.savedLeftStop) pref.putLong(PREF_LEFT_STOP_X, x.savedLeftStop = x.leftStop);
  if (x.rightStop != x.savedRightStop) pref.putLong(PREF_RIGHT_STOP_X, x.savedRightStop = x.rightStop);
  if (x.disabled != x.savedDisabled) pref.putBool(PREF_DISABLED_X, x.savedDisabled = x.disabled);
  if (a1.pos != a1.savedPos) pref.putLong(PREF_POS_A1, a1.savedPos = a1.pos);
  if (a1.posGlobal != a1.savedPosGlobal) pref.putLong(PREF_POS_GLOBAL_A1, a1.savedPosGlobal = a1.posGlobal);
  if (a1.originPos != a1.savedOriginPos) pref.putLong(PREF_ORIGIN_POS_A1, a1.savedOriginPos = a1.originPos);
  if (a1.motorPos != a1.savedMotorPos) pref.putLong(PREF_MOTOR_POS_A1, a1.savedMotorPos = a1.motorPos);
  if (a1.leftStop != a1.savedLeftStop) pref.putLong(PREF_LEFT_STOP_A1, a1.savedLeftStop = a1.leftStop);
  if (a1.rightStop != a1.savedRightStop) pref.putLong(PREF_RIGHT_STOP_A1, a1.savedRightStop = a1.rightStop);
  if (a1.disabled != a1.savedDisabled) pref.putBool(PREF_DISABLED_A1, a1.savedDisabled = a1.disabled);
  if (coneRatio != savedConeRatio) pref.putFloat(PREF_CONE_RATIO, savedConeRatio = coneRatio);
  if (turnPasses != savedTurnPasses) pref.putInt(PREF_TURN_PASSES, savedTurnPasses = turnPasses);
  if (auxForward != savedAuxForward) pref.putBool(PREF_AUX_FORWARD, savedAuxForward = auxForward);
  pref.end();
  return true;
}

void markAxisOrigin(Axis* a) {
  bool hasSemaphore = xSemaphoreTake(a->mutex, 10) == pdTRUE;
  if (!hasSemaphore) {
    beepFlag = true;
  }
  if (a->leftStop != LONG_MAX) {
    a->leftStop -= a->pos;
  }
  if (a->rightStop != LONG_MIN) {
    a->rightStop -= a->pos;
  }
  a->motorPos -= a->pos;
  a->originPos += a->pos;
  a->machineOffset += a->pos;
  a->pos = 0;
  a->fractionalPos = 0;
  a->pendingPos = 0;
  if (hasSemaphore) {
    xSemaphoreGive(a->mutex);
  }
}

void zeroSpindlePos() {
  spindlePos = 0;
  spindlePosAvg = 0;
  spindlePosSync = 0;
}

// Loose the thread and mark current physical positions of
// encoder and stepper as a new 0. To be called when dupr changes
// or ELS is turned on/off. Without this, changing dupr will
// result in stepper rushing across the lathe to the new position.
// Must be called while holding motionMutex.
void markOrigin() {
  markAxisOrigin(&z);
  markAxisOrigin(&x);
  markAxisOrigin(&a1);
  zeroSpindlePos();
}

void markAxis0(Axis* a) {
  a->originPos = -a->pos;
}

Axis* getAsyncAxis() {
  return mode == MODE_A1 ? &a1 : &z;
}

void updateAsyncTimerSettings() {
  // dupr and therefore direction can change while we're in async mode.
  setDir(getAsyncAxis(), dupr > 0);

  // dupr can change while we're in async mode, keep updating timer frequency.
  timerAlarm(async_timer, getTimerLimit(), true, 0);
  // without this timer stops working if already above new limit
  timerWrite(async_timer, 0);
}

void setDupr(long value) {
  // Can't apply changes right away since we might be in the middle of motion logic.
  nextDupr = value;
  nextDuprFlag = true;
}

// Must be called while holding motionMutex.
void applyDupr() {
  if (nextDupr == dupr) {
    return;
  }
  dupr = nextDupr;
  markOrigin();
  if (mode == MODE_ASYNC || mode == MODE_A1) {
    updateAsyncTimerSettings();
  }
}

void setStarts(int value) {
  // Can't apply changes right away since we might be in the middle of motion logic.
  nextStarts = value;
  nextStartsFlag = true;
}

// Must be called while holding motionMutex.
void applyStarts() {
  if (starts == nextStarts) {
    return;
  }
  starts = nextStarts;
  markOrigin();
}

void setMeasure(int value) {
  if (measure == value) {
    return;
  }
  measure = value;
  moveStep = measure == MEASURE_METRIC ? MOVE_STEP_1 : MOVE_STEP_IMP_1;
}

unsigned int getTimerLimit() {
  if (dupr == 0) {
    return 65535;
  }
  return min(long(65535), long(1000000 / (z.motorSteps * abs(dupr) / z.screwPitch)) - 1); // 1000000/Hz - 1
}

// Only used for async movement in ASYNC and A1 modes.
// Keep code in this method to absolute minimum to achieve high stepper speeds.
void IRAM_ATTR onAsyncTimer() {
  Axis* a = getAsyncAxis();
  if (!isOn || a->movingManually || (mode != MODE_ASYNC && mode != MODE_A1)) {
    return;
  } else if (dupr > 0 && a->pos < a->leftStop) {
    if (a->pos <= a->motorPos) {
      a->pos++;
    }
    a->motorPos++;
    a->posGlobal++;
  } else if (dupr < 0 && a->pos > a->rightStop) {
    if (a->pos >= a->motorPos + a->backlashSteps) {
      a->pos--;
    }
    a->motorPos--;
    a->posGlobal--;
  } else {
    return;
  }

  DLOW(a->step);
  a->stepStartUs = micros();
  delayMicroseconds(10);
  DHIGH(a->step);
}

void setModeFromTask(int value) {
  nextMode = value;
  nextModeFlag = true;
}

void setModeFromLoop(int value) {
  if (mode == value) {
    return;
  }
  if (isOn) {
    setIsOnFromLoop(false);
  }
  if (mode == MODE_THREAD) {
    setStarts(1);
  } else if (mode == MODE_ASYNC || mode == MODE_A1) {
    setAsyncTimerEnable(false);
  }
  mode = value;
  setupIndex = 0;
  if (mode == MODE_ASYNC || mode == MODE_A1) {
    if (!timerAttached) {
      timerAttached = true;
      timerAttachInterrupt(async_timer, &onAsyncTimer);
    }
    updateAsyncTimerSettings();
    setAsyncTimerEnable(true);
  }
}

void setTurnPasses(int value) {
  if (isOn) {
    beep();
  } else {
    turnPasses = value;
  }
}

void setConeRatio(float value) {
  // Can't apply changes right away since we might be in the middle of motion logic.
  nextConeRatio = value;
  nextConeRatioFlag = true;
}

void applyConeRatio() {
  if (nextConeRatio == coneRatio) {
    return;
  }
  coneRatio = nextConeRatio;
  markOrigin();
}

void reset() {
  z.homed = x.homed = a1.homed = false; // positions start again from 0
  z.leftStop = LONG_MAX;
  z.nextLeftStopFlag = false;
  z.rightStop = LONG_MIN;
  z.nextRightStopFlag = false;
  z.originPos = 0;
  z.posGlobal = 0;
  z.motorPos = 0;
  z.pendingPos = 0;
  z.disabled = false;
  x.leftStop = LONG_MAX;
  x.nextLeftStopFlag = false;
  x.rightStop = LONG_MIN;
  x.nextRightStopFlag = false;
  x.originPos = 0;
  x.posGlobal = 0;
  x.motorPos = 0;
  x.pendingPos = 0;
  x.disabled = false;
  a1.leftStop = LONG_MAX;
  a1.nextLeftStopFlag = false;
  a1.rightStop = LONG_MIN;
  a1.nextRightStopFlag = false;
  a1.originPos = 0;
  a1.posGlobal = 0;
  a1.motorPos = 0;
  a1.pendingPos = 0;
  a1.disabled = false;
  setDupr(0);
  setStarts(1);
  moveStep = MOVE_STEP_1;
  setModeFromTask(MODE_NORMAL);
  measure = MEASURE_METRIC;
  showTacho = false;
  showAngle = false;
  setConeRatio(1);
  auxForward = true;
}

long normalizePitch(long pitch) {
  int scale = 1;
  if (measure == MEASURE_METRIC) {
    // Drop the 3rd and 4th precision point if any.
    scale = 100;
  } else if (measure == MEASURE_INCH) {
    // Always drop the 4th precision point in inch representation if any.
    scale = 254;
  }
  return round(pitch / scale) * scale;
}

void buttonPlusMinusPress(bool plus) {
  // Mutex is aquired in setDupr() and setStarts().
  bool minus = !plus;
  if (mode == MODE_THREAD && setupIndex == 2) {
    if (minus && starts > 2) {
      setStarts(starts - 1);
    } else if (plus && starts < STARTS_MAX) {
      setStarts(starts + 1);
    }
  } else if (isPassMode() && setupIndex == 1 && getNumpadResult() == 0) {
    if (minus && turnPasses > 1) {
      setTurnPasses(turnPasses - 1);
    } else if (plus && turnPasses < PASSES_MAX) {
      setTurnPasses(turnPasses + 1);
    }
  } else if (measure != MEASURE_TPI) {
    int delta = measure == MEASURE_METRIC ? MOVE_STEP_3 : MOVE_STEP_IMP_3;
    // Switching between mm/inch/tpi often results in getting non-0 3rd and 4th
    // precision points that can't be easily controlled. Remove them.
    long normalizedDupr = normalizePitch(dupr);
    if (minus && dupr > -DUPR_MAX) {
      setDupr(max(-DUPR_MAX, normalizedDupr - delta));
    } else if (plus && dupr < DUPR_MAX) {
      setDupr(min(DUPR_MAX, normalizedDupr + delta));
    }
  } else { // TPI
    if (dupr == 0) {
      setDupr(plus ? 1 : -1);
    } else {
      long currentTpi = round(254000.0 / dupr);
      long tpi = currentTpi + (plus ? 1 : -1);
      long newDupr = tpi == 0 ? (plus ? DUPR_MAX : -DUPR_MAX) : round(254000.0 / tpi);
      // Happens for small pitches like 0.01mm.
      if (newDupr == dupr) {
        newDupr += plus ? -1 : 1;
      }
      if (newDupr != dupr && newDupr < DUPR_MAX && newDupr > -DUPR_MAX) {
        setDupr(newDupr);
      }
    }
  }
}

void beep() {
  tone(BUZZ, 1000, 500);
}

void buttonOnOffPress(bool on) {
  resetMillis = millis();
  bool missingZStops = needZStops() && (z.leftStop == LONG_MAX || z.rightStop == LONG_MIN);
  if (on && isPassMode() && (missingZStops || x.leftStop == LONG_MAX || x.rightStop == LONG_MIN)) {
    beep();
  } else if (!isOn && on && mode == MODE_GCODE && gcodeProgramIndex >= gcodeProgramCount && setupIndex == 1) {
    beep();
  } else if (!isOn && on && setupIndex < getLastSetupIndex()) {
    // Move to the next setup step.
    setupIndex++;
  } else if (isOn && on && (mode == MODE_TURN || mode == MODE_FACE || mode == MODE_THREAD)) {
    // Move to the next pass.
    opIndexAdvanceFlag = true;
  } else if (!on && (z.movingManually || x.movingManually || x.movingManually)) {
    setEmergencyStop(ESTOP_OFF_MANUAL_MOVE);
  } else if (!isOn && on && mode == MODE_GCODE && gcodeProgramIndex >= gcodeProgramCount) {
    beep();
  } else if (!isOn && on && mode == MODE_GCODE) {
    Preferences pref;
    pref.begin(GCODE_NAMESPACE);
    if (!pref.isKey(String(gcodeProgramIndex).c_str())) {
      beep();
    } else {
      String programName = pref.getString(String(gcodeProgramIndex).c_str());
      if (programName.length() == 0) {
        beep();
      } else {
        gcodeProgramCharIndex = 0;
        gcodeProgram = pref.getString(programName.c_str());
        gcodeProgram += '\n'; // ensures the last line is executed
        gcodeProgramData = gcodeProgram.c_str();
        gcodeProgramSize = gcodeProgram.length();
        setIsOnFromTask(on);
      }
    }
    pref.end();
  } else {
    setIsOnFromTask(on);
  }
}

void setIsOnFromTask(bool on) {
  nextIsOn = on;
  nextIsOnFlag = true;
}

void setIsOnFromLoop(bool on) {
  if (isOn && on) {
    return;
  }
  if (!on) {
    isOn = false;
    setupIndex = 0;
    if (homingActive) homingAbort = true; // OFF stops Home All too
    if (mode == MODE_GCODE && cncBusy()) {
      // GCODE mode still moving: decelerate along the path, then drop the rest. The steppers stay
      // enabled until it has stopped (taskGcode releases them), so no step and no position is lost.
      cncStopRequest = true;
      cncOwesDisable = true;
      return;
    }
  }
  if (on && mode == MODE_GCODE && wifiFsBusy) {
    // A file is being written or read on the flash: starting now would make the motors stutter.
    beepFlag = true;
    return;
  }
  if (on && mode == MODE_GCODE) {
    const char* refusal = cncStartRefusal(); // homing.ino: not homed, limit switch pressed, driver alarm
    if (refusal != NULL) {
      cncStatusText = refusal;
      beepFlag = true;
      return;
    }
  }
  stepperEnable(&z, on);
  stepperEnable(&x, on);
  stepperEnable(&a1, on);
  markOrigin();
  if (on) {
    // GCODE mode: taskGcode takes the new positions before it reads the first line (flag set before isOn).
    if (mode == MODE_GCODE) cncStartRequest = true;
    isOn = true;
    opDuprSign = dupr >= 0 ? 1 : -1;
    opDupr = dupr;
    opIndex = 0;
    opIndexAdvanceFlag = false;
    opSubIndex = 0;
    setupIndex = 0;
  }
}

void buttonOffRelease() {
  if (millis() - resetMillis > 3000) {
    reset();
    splashScreen = true;
  }
}

void setLeftStop(Axis* a, long value) {
  // Can't apply changes right away since we might be in the middle of motion logic.
  a->nextLeftStop = value;
  a->nextLeftStopFlag = true;
}

void leaveStop(Axis* a, long oldStop) {
  if (mode == MODE_CONE) {
    // To avoid rushing to a far away position if standing on limit.
    markOrigin();
  } else if (mode == MODE_NORMAL && a == getPitchAxis() && a->pos == oldStop) {
    // Spindle is most likely out of sync with the stepper because
    // it was spinning while the lead screw was on the stop.
    spindlePosSync = spindleModulo(spindlePos - spindleFromPos(a, a->pos));
  }
}

void applyLeftStop(Axis* a) {
  // Accept left stop even if it's lower than pos.
  // Stop button press processing takes time during which motor could have moved.
  long oldStop = a->leftStop;
  a->leftStop = a->nextLeftStop;
  leaveStop(a, oldStop);
}

void setRightStop(Axis* a, long value) {
  // Can't apply changes right away since we might be in the middle of motion logic.
  a->nextRightStop = value;
  a->nextRightStopFlag = true;
}

void applyRightStop(Axis* a) {
  // Accept right stop even if it's higher than pos.
  // Stop button press processing takes time during which motor could have moved.
  long oldStop = a->rightStop;
  a->rightStop = a->nextRightStop;
  leaveStop(a, oldStop);
}

void buttonLeftStopPress(Axis* a) {
  setLeftStop(a, a->leftStop == LONG_MAX ? a->pos : LONG_MAX);
}

void buttonRightStopPress(Axis* a) {
  setRightStop(a, a->rightStop == LONG_MIN ? a->pos : LONG_MIN);
}

void buttonDisplayPress() {
  if (!showAngle && !showTacho) {
    showAngle = true;
  } else if (showAngle) {
    showAngle = false;
    showTacho = true;
  } else {
    showTacho = false;
  }
}

void buttonMoveStepPress() {
  if (measure == MEASURE_METRIC) {
    if (moveStep == MOVE_STEP_1) {
      moveStep = MOVE_STEP_2;
    } else if (moveStep == MOVE_STEP_2) {
      moveStep = MOVE_STEP_3;
    } else {
      moveStep = MOVE_STEP_1;
    }
  } else {
    if (moveStep == MOVE_STEP_IMP_1) {
      moveStep = MOVE_STEP_IMP_2;
    } else if (moveStep == MOVE_STEP_IMP_2) {
      moveStep = MOVE_STEP_IMP_3;
    } else {
      moveStep = MOVE_STEP_IMP_1;
    }
  }
}

void setDir(Axis* a, bool dir) {
  // Start slow if direction changed.
  if (a->direction != dir || !a->directionInitialized) {
    a->speed = a->speedStart;
    a->direction = dir;
    a->directionInitialized = true;
    DWRITE(a->dir, dir ^ a->invertStepper);
    delayMicroseconds(DIRECTION_SETUP_DELAY_US);
  }
}

void buttonModePress() {
  if (mode == MODE_NORMAL) {
    setModeFromTask(ACTIVE_A1 ? MODE_A1 : MODE_ELLIPSE);
  } else if (mode == MODE_A1) {
    setModeFromTask(MODE_ELLIPSE);
  } else if (mode == MODE_ELLIPSE) {
    setModeFromTask(MODE_GCODE);
  } else if (mode == MODE_GCODE) {
    setModeFromTask(MODE_ASYNC);
  } else {
    setModeFromTask(MODE_NORMAL);
  }
}

void buttonMeasurePress() {
  if (measure == MEASURE_METRIC) {
    setMeasure(MEASURE_INCH);
  } else if (measure == MEASURE_INCH) {
    setMeasure(MEASURE_TPI);
  } else {
    setMeasure(MEASURE_METRIC);
  }
}

void buttonReversePress() {
  setDupr(-dupr);
}

void numpadPress(int digit) {
  if (!inNumpad) {
    numpadIndex = 0;
  }
  numpadDigits[numpadIndex] = digit;
  if (numpadIndex < 7) {
    numpadIndex++;
  } else {
    numpadIndex = 0;
  }
}

void numpadBackspace() {
  if (inNumpad && numpadIndex > 0) {
    numpadIndex--;
  }
}

void resetNumpad() {
  numpadIndex = 0;
}

long getNumpadResult() {
  long result = 0;
  for (int i = 0; i < numpadIndex; i++) {
    result += numpadDigits[i] * pow(10, numpadIndex - 1 - i);
  }
  return result;
}

void numpadPlusMinus(bool plus) {
  if (numpadDigits[numpadIndex - 1] < 9 && plus) {
    numpadDigits[numpadIndex - 1]++;
  } else if (numpadDigits[numpadIndex - 1] > 1 && !plus) {
    numpadDigits[numpadIndex - 1]--;
  }
  // TODO: implement going over 9 and below 1.
}

long numpadToDeciMicrons() {
  long result = getNumpadResult();
  if (result == 0) {
    return 0;
  }
  if (measure == MEASURE_INCH) {
    result = result * 254;
  } else if (measure == MEASURE_TPI) {
    result = round(254000.0 / result);
  } else { // Metric
    result = result * 10;
  }
  return result;
}

float numpadToConeRatio() {
  return getNumpadResult() / 100000.0;
}

bool processNumpad(int keyCode) {
  if (keyCode == B_0) {
    numpadPress(0);
    inNumpad = true;
  } else if (keyCode == B_1) {
    numpadPress(1);
    inNumpad = true;
  } else if (keyCode == B_2) {
    numpadPress(2);
    inNumpad = true;
  } else if (keyCode == B_3) {
    numpadPress(3);
    inNumpad = true;
  } else if (keyCode == B_4) {
    numpadPress(4);
    inNumpad = true;
  } else if (keyCode == B_5) {
    numpadPress(5);
    inNumpad = true;
  } else if (keyCode == B_6) {
    numpadPress(6);
    inNumpad = true;
  } else if (keyCode == B_7) {
    numpadPress(7);
    inNumpad = true;
  } else if (keyCode == B_8) {
    numpadPress(8);
    inNumpad = true;
  } else if (keyCode == B_9) {
    numpadPress(9);
    inNumpad = true;
  } else if (keyCode == B_BACKSPACE) {
    numpadBackspace();
    inNumpad = true;
  } else if (inNumpad && (keyCode == B_PLUS || keyCode == B_MINUS)) {
    numpadPlusMinus(keyCode == B_PLUS);
    return true;
  } else if (inNumpad) {
    inNumpad = false;
    return processNumpadResult(keyCode);
  }
  return inNumpad;
}

bool processNumpadResult(int keyCode) {
  long newDu = numpadToDeciMicrons();
  float newConeRatio = numpadToConeRatio();
  long numpadResult = getNumpadResult();
  resetNumpad();
  // Ignore numpad input unless confirmed with ON.
  if (keyCode == B_ON) {
    if (isPassMode() && setupIndex == 1) {
      setTurnPasses(int(min(PASSES_MAX, numpadResult)));
      setupIndex++;
    } else if (mode == MODE_CONE && setupIndex == 1) {
      setConeRatio(newConeRatio);
      setupIndex++;
    } else {
      if (abs(newDu) <= DUPR_MAX) {
        setDupr(newDu);
      }
    }
    // Don't use this ON press for starting the motion.
    return true;
  }

  // Shared piece for stops and moves.
  Axis* a = (keyCode == B_STOPL || keyCode == B_STOPR || keyCode == B_LEFT || keyCode == B_RIGHT || keyCode == B_Z) ? &z : &x;
  int sign = ((keyCode == B_STOPL || keyCode == B_STOPU || keyCode == B_LEFT || keyCode == B_UP || keyCode == B_Z || keyCode == B_X || keyCode == B_A) ? 1 : -1);
  if (mode == MODE_A1 && (keyCode == B_MODE_GEARS || keyCode == B_MODE_TURN || keyCode == B_MODE_FACE || keyCode == B_MODE_CONE || keyCode == B_MODE_THREAD)) {
    a = &a1;
    sign = (keyCode == B_MODE_GEARS || keyCode == B_MODE_FACE) ? -1 : 1;
  }
  long pos = a->pos + (a->rotational ? numpadResult * 10 : newDu) / a->screwPitch * a->motorSteps * sign;

  // Potentially assign a new value to a limit. Treat newDu as a relative distance from current position.
  if (keyCode == B_STOPL) {
    setLeftStop(&z, pos);
    return true;
  } else if (keyCode == B_STOPR) {
    setRightStop(&z, pos);
    return true;
  } else if (keyCode == B_STOPU) {
    setLeftStop(&x, pos);
    return true;
  } else if (keyCode == B_STOPD) {
    setRightStop(&x, pos);
    return true;
  } else if (mode == MODE_A1) {
    if (keyCode == B_MODE_CONE) {
      setLeftStop(&a1, pos);
      return true;
    } else if (keyCode == B_MODE_FACE) {
      setRightStop(&a1, pos);
      return true;
    }
  }

  // Potentially move by newDu in the given direction.
  // We don't support precision manual moves when ON yet. Can't stay in the thread for most modes.
  if (!isOn && (keyCode == B_LEFT || keyCode == B_RIGHT || keyCode == B_UP || keyCode == B_DOWN || (mode == MODE_A1 && (keyCode == B_MODE_GEARS || keyCode == B_MODE_TURN)))) {
    if (pos < a->rightStop) {
      pos = a->rightStop;
      beep();
    } else if (pos > a->leftStop) {
      pos = a->leftStop;
      beep();
    } else if (abs(pos - a->pos) > a->estopSteps) {
      beep();
      return true;
    }
    a->speedMax = a->speedManualMove;
    stepToFinal(a, pos);
    return true;
  }

  // Set axis 0 newDu ahead.
  if (keyCode == B_Z || keyCode == B_X || (mode == MODE_A1 && keyCode == B_MODE_THREAD)) {
    a->originPos = -pos;
    return true;
  }

  // Set X axis 0 from diameter.
  if (keyCode == B_A) {
    a->originPos = -(a->pos + pos) / 2;
    return true;
  }

  if (keyCode == B_STEP) {
    if (newDu > 0) {
      moveStep = newDu;
    } else {
      beep();
    }
    return true;
  }

  return false;
}

void processKeypadEvent() {
  int event = 0;
  if (serialKeycode != 0) {
    event = serialKeycode;
    serialKeycode = 0;
  } else if (keypad.available() > 0) {
    event = keypad.getEvent();
  }
  if (event == 0) return;
  int keyCode = event;
  bitWrite(keyCode, 7, 0);
  bool isPress = bitRead(event, 7) == 1; // 1 - press, 0 - release
  keypadTimeUs = micros();

  // Off button always gets handled.
  if (keyCode == B_OFF) {
    buttonOffPressed = isPress;
    isPress ? buttonOnOffPress(false) : buttonOffRelease();
  }

  if (mode == MODE_GCODE && isOn) {
    // Not allowed to interfere other than turn off, or ON to go on after M0 / M1 / M6.
    if (isPress && keyCode == B_ON && cncPaused) cncResumeRequest = true;
    else if (isPress && keyCode != B_OFF) beep();
    return;
  }

  // Releases don't matter in numpad but it has to run before LRUD since it might handle those keys.
  if (isPress && processNumpad(keyCode)) {
    return;
  }

  // Setup wizard navigation.
  if (isPress && setupIndex == 2 && (keyCode == B_LEFT || keyCode == B_RIGHT)) {
    auxForward = !auxForward;
  } else if (isPress && mode == MODE_GCODE && setupIndex == 1 && (keyCode == B_UP || keyCode == B_DOWN)) {
    if (gcodeProgramIndex > 0 && keyCode == B_UP) gcodeProgramIndex--;
    else if (gcodeProgramIndex == 0 && gcodeProgramCount > 0 && keyCode == B_UP) gcodeProgramIndex = gcodeProgramCount - 1;
    else if ((gcodeProgramIndex < gcodeProgramCount - 1) && keyCode == B_DOWN) gcodeProgramIndex++;
    else if (keyCode == B_DOWN) gcodeProgramIndex = 0;
  } else if (isPress && mode == MODE_GCODE && setupIndex == 1 && keyCode == B_MINUS) {
    removeGcode(gcodeProgramIndex);
    return;
  } else if (keyCode == B_LEFT) { // Make sure isPress=false propagates to motion flags.
    buttonLeftPressed = isPress;
  } else if (keyCode == B_RIGHT) {
    buttonRightPressed = isPress;
  } else if (keyCode == B_UP) {
    buttonUpPressed = isPress;
  } else if (keyCode == B_DOWN) {
    buttonDownPressed = isPress;
  } else if (keyCode == B_MODE_GEARS) {
    buttonGearsPressed = isPress;
  } else if (keyCode == B_MODE_TURN) {
    buttonTurnPressed = isPress;
  }

  // For all other keys we have no "release" logic.
  if (!isPress) {
    return;
  }

  // Rest of the buttons.
  if (keyCode == B_PLUS) {
    buttonPlusMinusPress(true);
  } else if (keyCode == B_MINUS) {
    buttonPlusMinusPress(false);
  } else if (keyCode == B_ON) {
    buttonOnOffPress(true);
  } else if (keyCode == B_STOPL) {
    buttonLeftStopPress(&z);
  } else if (keyCode == B_STOPR) {
    buttonRightStopPress(&z);
  } else if (keyCode == B_STOPU) {
    buttonLeftStopPress(&x);
  } else if (keyCode == B_STOPD) {
    buttonRightStopPress(&x);
  } else if (keyCode == B_MODE_OTHER) {
    buttonModePress();
  } else if (keyCode == B_DISPL) {
    buttonDisplayPress();
  } else if (keyCode == B_X) {
    markAxis0(&x);
  } else if (keyCode == B_Z) {
    markAxis0(&z);
  } else if (keyCode == B_A) {
    x.disabled = !x.disabled;
    updateEnable(&x);
  } else if (keyCode == B_B) {
    z.disabled = !z.disabled;
    updateEnable(&z);
  } else if (keyCode == B_STEP) {
    buttonMoveStepPress();
  } else if (keyCode == B_SETTINGS) {
    // TODO.
  } else if (keyCode == B_REVERSE) {
    buttonReversePress();
  } else if (keyCode == B_MEASURE) {
    buttonMeasurePress();
  } else if (keyCode == B_MODE_GEARS && mode != MODE_A1) {
    setModeFromTask(MODE_NORMAL);
  } else if (keyCode == B_MODE_TURN && mode != MODE_A1) {
    setModeFromTask(MODE_TURN);
  } else if (keyCode == B_MODE_FACE) {
    mode == MODE_A1 ? buttonRightStopPress(&a1) : setModeFromTask(MODE_FACE);
  } else if (keyCode == B_MODE_CONE) {
    mode == MODE_A1 ? buttonLeftStopPress(&a1) : setModeFromTask(MODE_CONE);
  } else if (keyCode == B_MODE_CUT) {
    if (mode == MODE_A1) {
      a1.disabled = !a1.disabled;
      updateEnable(&a1);
    } else {
      setModeFromTask(MODE_CUT);
    }
  } else if (keyCode == B_MODE_THREAD) {
    mode == MODE_A1 || (mode == MODE_GCODE && ACTIVE_A1) ? markAxis0(&a1) : setModeFromTask(MODE_THREAD);
  }
}

// Moves the stepper so that the tool is located at the newPos.
bool stepToContinuous(Axis* a, long newPos) {
  return stepTo(a, newPos, true);
}

bool stepToFinal(Axis* a, long newPos) {
  return stepTo(a, newPos, false);
}

bool stepTo(Axis* a, long newPos, bool continuous) {
  if (motionBlocked()) return false; // homing.ino: a limit or alarm has just stopped the machine
  if (xSemaphoreTake(a->mutex, 10) == pdTRUE) {
    a->continuous = continuous;
    if (newPos == a->pos) {
      a->pendingPos = 0;
    } else {
      a->pendingPos = newPos - a->motorPos - (newPos > a->pos ? 0 : a->backlashSteps);
    }
    xSemaphoreGive(a->mutex);
    return true;
  }
  return false;
}

// Calculates stepper position from spindle position.
long posFromSpindle(Axis* a, long s, bool respectStops) {
  long newPos = s * a->motorSteps / a->screwPitch / ENCODER_STEPS_FLOAT * dupr * starts;

  // Respect left/right stops.
  if (respectStops) {
    if (newPos < a->rightStop) {
      newPos = a->rightStop;
    } else if (newPos > a->leftStop) {
      newPos = a->leftStop;
    }
  }

  return newPos;
}

// Calculates spindle position from stepper position.
long spindleFromPos(Axis* a, long p) {
  return p * a->screwPitch * ENCODER_STEPS_FLOAT / a->motorSteps / (dupr * starts);
}

void stepperEnable(Axis* a, bool value) {
  if (!a->needsRest || !a->active) {
    return;
  }
  if (value) {
    a->stepperEnableCounter++;
    if (value == 1) {
      updateEnable(a);
    }
  } else if (a->stepperEnableCounter > 0) {
    a->stepperEnableCounter--;
    if (a->stepperEnableCounter == 0) {
      updateEnable(a);
    }
  }
}

void updateEnable(Axis* a) {
  if (!a->disabled && (!a->needsRest || a->stepperEnableCounter > 0)) {
    DHIGH(a->ena);
    // Stepper driver needs some time before it will react to pulses.
    DELAY(STEPPED_ENABLE_DELAY_MS);
  } else {
    DLOW(a->ena);
  }
}

void moveAxis(Axis* a) {
  // Most of the time a step isn't needed.
  if (a->pendingPos == 0) {
    if (a->speed > a->speedStart) {
      a->speed--;
    }
    return;
  }

  unsigned long nowUs = micros();
  float delayUs = 1000000.0 / a->speed;
  if (nowUs - a->stepStartUs < delayUs - 5) {
    // Not enough time has passed to issue this step.
    return;
  }

  if (xSemaphoreTake(a->mutex, 1) == pdTRUE) {
    // Check pendingPos again now that we have the mutex.
    if (a->pendingPos != 0) {
      bool dir = a->pendingPos > 0;
      setDir(a, dir);

      DLOW(a->step);
      int delta = dir ? 1 : -1;
      a->pendingPos -= delta;
      if (dir && a->motorPos >= a->pos) {
        a->pos++;
      } else if (!dir && a->motorPos <= (a->pos - a->backlashSteps)) {
        a->pos--;
      }
      a->motorPos += delta;
      a->posGlobal += delta;

      bool accelerate = a->continuous || a->pendingPos >= a->decelerateSteps || a->pendingPos <= -a->decelerateSteps;
      a->speed += (accelerate ? 1 : -1) * a->acceleration * delayUs / 1000000.0;
      if (a->speed > a->speedMax) {
        a->speed = a->speedMax;
      } else if (a->speed < a->speedStart) {
        a->speed = a->speedStart;
      }
      a->stepStartUs = nowUs;

      DHIGH(a->step);
    }
    xSemaphoreGive(a->mutex);
  }
}

void modeGearbox() {
  if (z.movingManually) {
    return;
  }
  z.speedMax = LONG_MAX;
  stepToContinuous(&z, posFromSpindle(&z, spindlePosAvg, true));
}

long spindleModulo(long value) {
  value = value % ENCODER_STEPS_INT;
  if (value < 0) {
    value += ENCODER_STEPS_INT;
  }
  return value;
}

long auxSafeDistance, startOffset;
void modeTurn(Axis* main, Axis* aux) {
  if (main->movingManually || aux->movingManually || turnPasses <= 0 ||
      main->leftStop == LONG_MAX || main->rightStop == LONG_MIN ||
      aux->leftStop == LONG_MAX || aux->rightStop == LONG_MIN ||
      dupr == 0 || (dupr * opDuprSign < 0) || starts < 1) {
    setIsOnFromLoop(false);
    return;
  }

  // Variables below have to be re-calculated every time because origin can change
  // while TURN is running e.g. due to dupr change.
  long mainStartStop = opDuprSign > 0 ? main->rightStop : main->leftStop;
  long mainEndStop = opDuprSign > 0 ? main->leftStop : main->rightStop;
  long auxStartStop = auxForward ? aux->rightStop : aux->leftStop;
  long auxEndStop = auxForward ? aux->leftStop : aux->rightStop;

  // opIndex 0 is only executed once, do setup calculations here.
  if (opIndex == 0) {
    auxSafeDistance = (auxForward ? -1 : 1) * SAFE_DISTANCE_DU * aux->motorSteps / aux->screwPitch;
    startOffset = starts == 1 ? 0 : round(ENCODER_STEPS_FLOAT / starts);

    // Move to right-bottom limit.
    main->speedMax = main->speedManualMove;
    aux->speedMax = aux->speedManualMove;
    long auxPos = auxStartStop;
    // Overstep by 1 so that "main" backlash is taken out before "opSubIndex == 1".
    long mainPos = mainStartStop + (opDuprSign > 0 ? -1 : 1);
    stepToFinal(main, mainPos);
    stepToFinal(aux, auxPos);
    if (main->pos == mainPos && aux->pos == auxPos) {
      stepToFinal(main, mainStartStop);
      opIndex = 1;
      opSubIndex = 0;
    }
  } else if (opIndex <= turnPasses * starts) {
    if (opIndexAdvanceFlag && (opIndex + starts) < turnPasses * starts) {
      opIndexAdvanceFlag = false;
      opIndex += starts;
    }
    long auxPos = auxEndStop - (auxEndStop - auxStartStop) / turnPasses * (turnPasses - ceil(opIndex / float(starts)));
    // Bringing X to starting position.
    if (opSubIndex == 0) {
      stepToFinal(aux, auxPos);
      if (aux->pos == auxPos) {
        opSubIndex = 1;
        spindlePosSync = spindleModulo(spindlePosGlobal - spindleFromPos(main, main->posGlobal) + startOffset * (opIndex - 1));
        return; // Instead of jumping to the next step, let spindlePosSync get to 0 first.
      }
    }
    // spindlePosSync counted down to 0, start thread from here.
    if (opSubIndex == 1) {
      markOrigin();
      main->speedMax = LONG_MAX;
      opSubIndex = 2;
      // markOrigin() changed Start/EndStop values, re-calculate them.
      return;
    }
    // Doing the pass cut.
    if (opSubIndex == 2) {
      // In case we were pushed to the next opIndex before finishing the current one.
      stepToFinal(aux, auxPos);
      stepToContinuous(main, posFromSpindle(main, spindlePosAvg, true));
      if (main->pos == mainEndStop) {
        opSubIndex = 3;
      }
    }
    // Retracting the tool
    if (opSubIndex == 3) {
      long auxPos = auxStartStop + auxSafeDistance;
      stepToFinal(aux, auxPos);
      if (aux->pos == auxPos) {
        opSubIndex = 4;
      }
    }
    // Returning to start of main.
    if (opSubIndex == 4) {
      main->speedMax = main->speedManualMove;
      // Overstep by 1 so that "main" backlash is taken out before "opSubIndex == 2".
      long mainPos = mainStartStop + (opDuprSign > 0 ? -1 : 1);
      stepToFinal(main, mainPos);
      if (main->pos == mainPos) {
        stepToFinal(main, mainStartStop);
        opSubIndex = 0;
        opIndex++;
      }
    }
  } else {
    // Move to right-bottom limit.
    main->speedMax = main->speedManualMove;
    long auxPos = auxStartStop;
    long mainPos = mainStartStop;
    stepToFinal(main, mainPos);
    stepToFinal(aux, auxPos);
    if (main->pos == mainPos && aux->pos == auxPos) {
      setIsOnFromLoop(false);
      beep();
    }
  }
}

void modeCone() {
  if (z.movingManually || x.movingManually || coneRatio == 0) {
    return;
  }

  float zToXRatio = -coneRatio / 2 / z.motorSteps * x.motorSteps / x.screwPitch * z.screwPitch * (auxForward ? 1 : -1);
  if (zToXRatio == 0) {
    return;
  }

  // TODO: calculate maximum speeds and accelerations to avoid potential desync.
  x.speedMax = LONG_MAX;
  z.speedMax = LONG_MAX;

  // Respect limits of both axis by translating them into limits on spindlePos value.
  long spindle = spindlePosAvg;
  long spindleMin = LONG_MIN;
  long spindleMax = LONG_MAX;
  if (z.leftStop != LONG_MAX) {
    (dupr > 0 ? spindleMax : spindleMin) = spindleFromPos(&z, z.leftStop);
  }
  if (z.rightStop != LONG_MIN) {
    (dupr > 0 ? spindleMin: spindleMax) = spindleFromPos(&z, z.rightStop);
  }
  if (x.leftStop != LONG_MAX) {
    long lim = spindleFromPos(&z, round(x.leftStop / zToXRatio));
    if (zToXRatio < 0) {
      (dupr > 0 ? spindleMin: spindleMax) = lim;
    } else {
      (dupr > 0 ? spindleMax : spindleMin) = lim;
    }
  }
  if (x.rightStop != LONG_MIN) {
    long lim = spindleFromPos(&z, round(x.rightStop / zToXRatio));
    if (zToXRatio < 0) {
      (dupr > 0 ? spindleMax : spindleMin) = lim;
    } else {
      (dupr > 0 ? spindleMin: spindleMax) = lim;
    }
  }
  if (spindle > spindleMax) {
    spindle = spindleMax;
  } else if (spindle < spindleMin) {
    spindle = spindleMin;
  }

  stepToContinuous(&z, posFromSpindle(&z, spindle, true));
  stepToContinuous(&x, round(z.pos * zToXRatio));
}

void modeCut() {
  if (x.movingManually || turnPasses <= 0 || x.leftStop == LONG_MAX || x.rightStop == LONG_MIN || dupr == 0 || dupr * opDuprSign < 0) {
    setIsOnFromLoop(false);
    return;
  }

  long startStop = opDuprSign > 0 ? x.rightStop : x.leftStop;
  long endStop = opDuprSign > 0 ? x.leftStop : x.rightStop;

  if (opIndex == 0) {
    // Move to back limit.
    x.speedMax = x.speedManualMove;
    long xPos = startStop;
    stepToFinal(&x, xPos);
    if (x.pos == xPos) {
      opIndex = 1;
      opSubIndex = 0;
    }
  } else if (opIndex <= turnPasses) {
    // Set spindlePos and x.pos in sync.
    if (opSubIndex == 0) {
      spindlePosAvg = spindlePos = spindleFromPos(&x, x.pos);
      opSubIndex = 1;
    }
    // Doing the pass cut.
    if (opSubIndex == 1) {
      x.speedMax = LONG_MAX;
      long endPos = endStop - (endStop - startStop) / turnPasses * (turnPasses - opIndex);
      long xPos = posFromSpindle(&x, spindlePosAvg, true);
      if (dupr > 0 && xPos > endPos) xPos = endPos;
      else if (dupr < 0 && xPos < endPos) xPos = endPos;
      stepToContinuous(&x, xPos);
      if (x.pos == endPos) {
        opSubIndex = 2;
      }
    }
    // Returning to start.
    if (opSubIndex == 2) {
      x.speedMax = x.speedManualMove;
      stepToFinal(&x, startStop);
      if (x.pos == startStop) {
        opSubIndex = 0;
        opIndex++;
      }
    }
  } else {
    setIsOnFromLoop(false);
    beep();
  }
}

void modeEllipse(Axis* main, Axis* aux) {
  if (main->movingManually || aux->movingManually || turnPasses <= 0 ||
      main->leftStop == LONG_MAX || main->rightStop == LONG_MIN ||
      aux->leftStop == LONG_MAX || aux->rightStop == LONG_MIN ||
      main->leftStop == main->rightStop ||
      aux->leftStop == aux->rightStop ||
      dupr == 0 || dupr != opDupr) {
    setIsOnFromLoop(false);
    return;
  }

  // Start from left or right depending on the pitch.
  long mainStartStop = opDuprSign > 0 ? main->rightStop : main->leftStop;
  long mainEndStop = opDuprSign > 0 ? main->leftStop : main->rightStop;
  long auxStartStop = aux->rightStop;
  long auxEndStop = aux->leftStop;

  main->speedMax = main->speedManualMove;
  aux->speedMax = aux->speedManualMove;

  if (opIndex == 0) {
    opIndex = 1;
    opSubIndex = 0;
    spindlePos = 0;
    spindlePosAvg = 0;
  } else if (opIndex <= turnPasses) {
    float pass0to1 = opIndex / float(turnPasses);
    long mainDelta = round(pass0to1 * (mainEndStop - mainStartStop));
    long auxDelta = round(pass0to1 * (auxEndStop - auxStartStop));
    long spindleDelta = spindleFromPos(main, mainDelta);

    // Move to starting position.
    if (opSubIndex == 0) {
      long auxPos = auxStartStop;
      stepToFinal(aux, auxPos);
      if (aux->pos == auxPos) {
        opSubIndex = 1;
      }
    } else if (opSubIndex == 1) {
      long mainPos = mainEndStop - mainDelta;
      stepToFinal(main, mainPos);
      if (main->pos == mainPos) {
        opSubIndex = 2;
        spindlePos = 0;
        spindlePosAvg = 0;
      }
    } else if (opSubIndex == 2) {
      float progress0to1 = 0;
      if ((spindleDelta > 0 && spindlePosAvg >= spindleDelta) || (spindleDelta < 0 && spindlePosAvg <= spindleDelta)) {
        progress0to1 = 1;
      } else {
        progress0to1 = spindlePosAvg / float(spindleDelta);
      }
      float mainCoeff = auxForward ? cos(HALF_PI * (3 + progress0to1)) : (1 + sin(HALF_PI * (progress0to1 - 1)));
      long mainPos = mainEndStop - mainDelta + round(mainDelta * mainCoeff);
      float auxCoeff = auxForward ? (1 + sin(HALF_PI * (3 + progress0to1))) : sin(HALF_PI * progress0to1);
      long auxPos = auxStartStop + round(auxDelta * auxCoeff);
      stepToContinuous(main, mainPos);
      stepToContinuous(aux, auxPos);
      if (progress0to1 == 1 && main->pos == mainPos && aux->pos == auxPos) {
        opIndex++;
        opSubIndex = 0;
      }
    }
  } else if (opIndex == turnPasses + 1) {
    stepToFinal(aux, auxStartStop);
    if (aux->pos == auxStartStop) {
      setIsOnFromLoop(false);
      beep();
    }
  }
}

long mmOrInchToAbsolutePos(Axis* a, float mmOrInch) {
  long scaleToDu = measure == MEASURE_METRIC ? 10000 : 254000;
  long part1 = a->gcodeRelativePos;
  long part2 = round(mmOrInch * scaleToDu / a->screwPitch * a->motorSteps);
  return part1 + part2;
}

String getValueString(const String& command, char letter) {
  int index = command.indexOf(letter);
  if (index == -1) {
    return "";
  }
  String valueString;
  for (int i = index + 1; i < command.length(); i++) {
    char c = command.charAt(i);
    if (isDigit(c) || c == '.' || c == '-') {
      valueString += c;
    } else {
      break;
    }
  }
  return valueString;
}

float getFloat(const String& command, char letter) {
  return getValueString(command, letter).toFloat();
}

int getInt(const String& command, char letter) {
  return getValueString(command, letter).toInt();
}

void updateAxisSpeeds(long diffX, long diffZ, long diffA1) {
  if (diffX == 0 && diffZ == 0 && diffA1 == 0) return;
  long absX = abs(diffX);
  long absZ = abs(diffZ);
  long absC = abs(diffA1);
  float stepsPerSecX = gcodeFeedDuPerSec * x.motorSteps / x.screwPitch;
  float minStepsPerSecX = GCODE_FEED_MIN_DU_SEC * x.motorSteps / x.screwPitch;
  if (stepsPerSecX > x.speedManualMove) stepsPerSecX = x.speedManualMove;
  else if (stepsPerSecX < minStepsPerSecX) stepsPerSecX = minStepsPerSecX;
  float stepsPerSecZ = gcodeFeedDuPerSec * z.motorSteps / z.screwPitch;
  float minStepsPerSecZ = GCODE_FEED_MIN_DU_SEC * z.motorSteps / z.screwPitch;
  if (stepsPerSecZ > z.speedManualMove) stepsPerSecZ = z.speedManualMove;
  else if (stepsPerSecZ < minStepsPerSecZ) stepsPerSecZ = minStepsPerSecZ;
  float stepsPerSecA1 = gcodeFeedDuPerSec * a1.motorSteps / a1.screwPitch;
  float minStepsPerSecA1 = GCODE_FEED_MIN_DU_SEC * a1.motorSteps / a1.screwPitch;
  if (stepsPerSecA1 > a1.speedManualMove) stepsPerSecA1 = a1.speedManualMove;
  else if (stepsPerSecA1 < minStepsPerSecA1) stepsPerSecA1 = minStepsPerSecA1;
  float secX = absX / stepsPerSecX;
  float secZ = absZ / stepsPerSecZ;
  float secA1 = absC / stepsPerSecA1;
  float sec = ACTIVE_A1 ? max(max(secX, secZ), secA1) : max(secX, secZ);
  x.speedMax = sec > 0 ? absX / sec : x.speedManualMove;
  z.speedMax = sec > 0 ? absZ / sec : z.speedManualMove;
  a1.speedMax = sec > 0 ? absC / sec : a1.speedManualMove;
}

void setFeedRate(const String& command) {
  float feed = getFloat(command, 'F');
  if (feed <= 0) return;
  gcodeFeedDuPerSec = round(feed * (measure == MEASURE_METRIC ? 10000 : 254000) / 60.0);
}

void gcodeWaitEpsilon(int epsilon) {
  while (abs(x.pendingPos) > epsilon || abs(z.pendingPos) > epsilon || abs(a1.pendingPos) > epsilon || (SPINDLE_PAUSES_GCODE && getApproxRpm() < GCODE_MIN_RPM)) {
    taskYIELD();
  }
}

void gcodeWaitNear() {
  gcodeWaitEpsilon(GCODE_WAIT_EPSILON_STEPS);
}

void gcodeWaitStop() {
  gcodeWaitEpsilon(0);
}

// Rapid positioning / linear interpolation.
void G00_01(const String& command) {
  long xStart = x.pos;
  long zStart = z.pos;
  long a1Start = a1.pos;
  long xEnd = command.indexOf(x.name) >= 0 ? mmOrInchToAbsolutePos(&x, getFloat(command, x.name)) : xStart;
  long zEnd = command.indexOf(z.name) >= 0 ? mmOrInchToAbsolutePos(&z, getFloat(command, z.name)) : zStart;
  long a1End = command.indexOf(a1.name) >= 0 ? mmOrInchToAbsolutePos(&a1, getFloat(command, a1.name)) : a1Start;
  long xDiff = xEnd - xStart;
  long zDiff = zEnd - zStart;
  long a1Diff = a1End - a1Start;
  updateAxisSpeeds(xDiff, zDiff, a1Diff);
  long chunks = round(max(max(abs(xDiff), abs(zDiff)), abs(a1Diff)) * LINEAR_INTERPOLATION_PRECISION);
  for (long i = 0; i < chunks; i++) {
    if (!isOn) return;
    float scale = i / float(chunks);
    stepToContinuous(&x, xStart + xDiff * scale);
    stepToContinuous(&z, zStart + zDiff * scale);
    if (ACTIVE_A1) stepToContinuous(&a1, a1Start + a1Diff * scale);
    gcodeWaitNear();
  }
  // To avoid any rounding error, move to precise position.
  stepToFinal(&x, xEnd);
  stepToFinal(&z, zEnd);
  if (ACTIVE_A1) stepToFinal(&a1, a1End);
  gcodeWaitStop();
}

bool handleGcode(const String& command) {
  int op = getInt(command, 'G');
  if (op == 0 || op == 1) { // 0 also covers X and Z commands without G.
    G00_01(command);
  } else if (op == 20 || op == 21) {
    setMeasure(op == 20 ? MEASURE_INCH : MEASURE_METRIC);
  } else if (op == 90 || op == 91) {
    gcodeAbsolutePositioning = op == 90;
  } else if (op == 94) {
    /* no-op feed per minute */
  } else if (op == 18) {
    /* no-op ZX plane selection */
  } else {
    Serial.print("error: unsupported command ");
    Serial.println(command);
    return false;
  }
  return true;
}

bool handleMcode(const String& command) {
  int op = getInt(command, 'M');
  if (op == 0 || op == 1 || op == 2 || op == 30) {
    setIsOnFromTask(false);
  } else {
    setIsOnFromTask(false);
    Serial.print("error: unsupported command ");
    Serial.println(command);
    return false;
  }
  return true;
}

// Process one command, return ok flag.
bool handleGcodeCommand(String command) {
  command.trim();
  if (command.length() == 0) return false;

  // Trim N.. prefix.
  char code = command.charAt(0);
  int spaceIndex = command.indexOf(' ');
  if (code == 'N' && spaceIndex > 0) {
    command = command.substring(spaceIndex + 1);
    code = command.charAt(0);
  }

  // Update position for relative calculations right before performing them.
  z.gcodeRelativePos = gcodeAbsolutePositioning ? -z.originPos : z.pos;
  x.gcodeRelativePos = gcodeAbsolutePositioning ? -x.originPos : x.pos;
  a1.gcodeRelativePos = gcodeAbsolutePositioning ? -a1.originPos : a1.pos;

  setFeedRate(command);
  switch (code) {
    case 'G':
    case NAME_Z:
    case NAME_X:
    case NAME_A1: return handleGcode(command);
    case 'F': return true; /* feed already handled above */
    case 'M': return handleMcode(command);
    case 'T': return true; /* ignoring tool changes */
    default: Serial.print("error: unsupported command "); Serial.println(code); return false;
  }
  return false;
}

void discountFullSpindleTurns() {
  // When standing at the stop, ignore full spindle turns.
  // This allows to avoid waiting when spindle direction reverses
  // and reduces the chance of the skipped stepper steps since
  // after a reverse the spindle starts slow.
  if (dupr != 0 && !stepperIsRunning(&z) && (mode == MODE_NORMAL || mode == MODE_CONE)) {
    int spindlePosDiff = 0;
    if (z.pos == z.rightStop) {
      long stopSpindlePos = spindleFromPos(&z, z.rightStop);
      if (dupr > 0) {
        if (spindlePos < stopSpindlePos - ENCODER_STEPS_INT) {
          spindlePosDiff = ENCODER_STEPS_INT;
        }
      } else {
        if (spindlePos > stopSpindlePos + ENCODER_STEPS_INT) {
          spindlePosDiff = -ENCODER_STEPS_INT;
        }
      }
    } else if (z.pos == z.leftStop) {
      long stopSpindlePos = spindleFromPos(&z, z.leftStop);
      if (dupr > 0) {
        if (spindlePos > stopSpindlePos + ENCODER_STEPS_INT) {
          spindlePosDiff = -ENCODER_STEPS_INT;
        }
      } else {
        if (spindlePos < stopSpindlePos - ENCODER_STEPS_INT) {
          spindlePosDiff = ENCODER_STEPS_INT;
        }
      }
    }
    if (spindlePosDiff != 0) {
      spindlePos += spindlePosDiff;
      spindlePosAvg += spindlePosDiff;
    }
  }
}

void processSpindleCounter() {
  int16_t count;
  pcnt_get_counter_value(PCNT_UNIT_0, &count);
  int delta = count - spindleCount;
  if (delta == 0) {
    return;
  }
  if (count >= PCNT_CLEAR || count <= -PCNT_CLEAR) {
    pcnt_counter_clear(PCNT_UNIT_0);
    spindleCount = 0;
  } else {
    spindleCount = count;
  }

  unsigned long microsNow = micros();
  if (showTacho || mode == MODE_GCODE) {
    if (spindleEncTimeIndex >= RPM_BULK) {
      spindleEncTimeDiffBulk = microsNow - spindleEncTimeAtIndex0;
      spindleEncTimeAtIndex0 = microsNow;
      spindleEncTimeIndex = 0;
    }
    spindleEncTimeIndex += abs(delta);
  } else {
    spindleEncTimeDiffBulk = 0;
  }

  spindlePos += delta;
  spindlePosGlobal += delta;
  if (spindlePosGlobal > ENCODER_STEPS_INT) {
    spindlePosGlobal -= ENCODER_STEPS_INT;
  } else if (spindlePosGlobal < 0) {
    spindlePosGlobal += ENCODER_STEPS_INT;
  }
  if (spindlePos > spindlePosAvg) {
    spindlePosAvg = spindlePos;
  } else if (spindlePos < spindlePosAvg - ENCODER_BACKLASH) {
    spindlePosAvg = spindlePos + ENCODER_BACKLASH;
  }
  spindleEncTime = microsNow;

  if (spindlePosSync != 0) {
    spindlePosSync += delta;
    if (spindlePosSync % ENCODER_STEPS_INT == 0) {
      spindlePosSync = 0;
      Axis* a = getPitchAxis();
      spindlePosAvg = spindlePos = spindleFromPos(a, a->pos);
    }
  }
}

// Apply changes requested by the keyboard thread.
void applySettings() {
  if (nextDuprFlag) {
    applyDupr();
    nextDuprFlag = false;
  }
  if (nextStartsFlag) {
    applyStarts();
    nextStartsFlag = false;
  }
  if (z.nextLeftStopFlag) {
    applyLeftStop(&z);
    z.nextLeftStopFlag = false;
  }
  if (z.nextRightStopFlag) {
    applyRightStop(&z);
    z.nextRightStopFlag = false;
  }
  if (x.nextLeftStopFlag) {
    applyLeftStop(&x);
    x.nextLeftStopFlag = false;
  }
  if (x.nextRightStopFlag) {
    applyRightStop(&x);
    x.nextRightStopFlag = false;
  }
  if (a1.nextLeftStopFlag) {
    applyLeftStop(&a1);
    a1.nextLeftStopFlag = false;
  }
  if (a1.nextRightStopFlag) {
    applyRightStop(&a1);
    a1.nextRightStopFlag = false;
  }
  if (nextConeRatioFlag) {
    applyConeRatio();
    nextConeRatioFlag = false;
  }
  if (nextIsOnFlag) {
    setIsOnFromLoop(nextIsOn);
    nextIsOnFlag = false;
  }
  if (nextModeFlag) {
    setModeFromLoop(nextMode);
    nextModeFlag = false;
  }
}

void loop() {
  if (emergencyStop != ESTOP_NONE) {
    return;
  }
  if (xSemaphoreTake(motionMutex, 1) != pdTRUE) {
    return;
  }
  applySettings();
  processSpindleCounter();
  discountFullSpindleTurns();
  cncInputsLoop(); // homing.ino: home switches, limits and driver alarm (nothing when not used)
  if (cncThreadState != CNC_THREAD_NONE) {
    cncThreadLoop(); // G32 / G33 in the GCODE mode
  } else if (!isOn || dupr == 0 || spindlePosSync != 0) {
    // None of the modes work.
  } else if (mode == MODE_NORMAL) {
    modeGearbox();
  } else if (mode == MODE_TURN) {
    modeTurn(&z, &x);
  } else if (mode == MODE_FACE) {
    modeTurn(&x, &z);
  } else if (mode == MODE_CUT) {
    modeCut();
  } else if (mode == MODE_CONE) {
    modeCone();
  } else if (mode == MODE_THREAD) {
    modeTurn(&z, &x);
  } else if (mode == MODE_ELLIPSE) {
    modeEllipse(&z, &x);
  }
  moveAxis(&z);
  moveAxis(&x);
  if (ACTIVE_A1) moveAxis(&a1);
  xSemaphoreGive(motionMutex);
}
