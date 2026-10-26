// config.ino - machine settings of the NanoEls H4 CNC fork that can be changed on the web page.
// https://github.com/Marioskiv/nanoels
//
// The idea follows the Machine Config of the NanoEls H5 firmware (MIT, Copyright (c) Maxim Kachurovskiy):
// the values written at the top of h4.ino are the defaults; values saved from the web page are kept in
// Preferences (namespace "h4cfg") and loaded at start-up, before anything uses them. A change takes
// effect after a restart. Pins and axis letters stay in h4.ino.
//
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)

#define MACHINE_CONFIG_NAMESPACE "h4cfg"
#define MACHINE_CONFIG_VERSION 1

// Types: 'l' long, 'i' int, 'b' bool, 'f' float, 'a' axis letter (Z or X).
// Units tell the page how to show a value: "du" is stored in deci-microns and shown in mm (degrees for
// a rotary A1), "v:z" / "a:z" are steps/s and steps/s^2 of an axis, shown in mm/s and mm/s^2.
struct MachineConfigItem {
  const char* key;
  char type;
  void* ptr;
  double min;
  double max;
  const char* group;
  const char* label;
  const char* unit;
};

const MachineConfigItem MACHINE_CONFIG[] = {
  {"encPpr", 'i', (void*)&ENCODER_PPR, 1, 10000, "Spindle encoder", "Encoder pulses per turn (PPR)", ""},
  {"encBl", 'i', (void*)&ENCODER_BACKLASH, 0, 100, "Spindle encoder", "Encoder backlash (counts)", ""},

  {"zSteps", 'l', (void*)&MOTOR_STEPS_Z, 1, 1000000, "Z axis", "Motor steps per screw turn", "steps"},
  {"zScrew", 'l', (void*)&SCREW_Z_DU, 1, 10000000, "Z axis", "Travel per screw turn", "du"},
  {"zStart", 'l', (void*)&SPEED_START_Z, 1, 1000000, "Z axis", "Start speed", "v:z"},
  {"zSpeed", 'l', (void*)&SPEED_MANUAL_MOVE_Z, 1, 1000000, "Z axis", "Maximum speed (manual moves, G0)", "v:z"},
  {"zAcc", 'l', (void*)&ACCELERATION_Z, 1, 100000000, "Z axis", "Acceleration", "a:z"},
  {"zInv", 'b', (void*)&INVERT_Z, 0, 1, "Z axis", "Invert direction", ""},
  {"zRest", 'b', (void*)&NEEDS_REST_Z, 0, 1, "Z axis", "Disable the driver when stopped (open loop)", ""},
  {"zTravel", 'l', (void*)&MAX_TRAVEL_MM_Z, 1, 100000, "Z axis", "Longest manual travel", "mm"},
  {"zBl", 'l', (void*)&BACKLASH_DU_Z, 0, 100000, "Z axis", "Backlash", "du"},

  {"xSteps", 'l', (void*)&MOTOR_STEPS_X, 1, 1000000, "X axis", "Motor steps per screw turn", "steps"},
  {"xScrew", 'l', (void*)&SCREW_X_DU, 1, 10000000, "X axis", "Travel per screw turn", "du"},
  {"xStart", 'l', (void*)&SPEED_START_X, 1, 1000000, "X axis", "Start speed", "v:x"},
  {"xSpeed", 'l', (void*)&SPEED_MANUAL_MOVE_X, 1, 1000000, "X axis", "Maximum speed (manual moves, G0)", "v:x"},
  {"xAcc", 'l', (void*)&ACCELERATION_X, 1, 100000000, "X axis", "Acceleration", "a:x"},
  {"xInv", 'b', (void*)&INVERT_X, 0, 1, "X axis", "Invert direction", ""},
  {"xRest", 'b', (void*)&NEEDS_REST_X, 0, 1, "X axis", "Disable the driver when stopped (open loop)", ""},
  {"xTravel", 'l', (void*)&MAX_TRAVEL_MM_X, 1, 100000, "X axis", "Longest manual travel", "mm"},
  {"xBl", 'l', (void*)&BACKLASH_DU_X, 0, 100000, "X axis", "Backlash", "du"},

  {"aAct", 'b', (void*)&ACTIVE_A1, 0, 1, "A1 axis (C)", "Connected (terminal A1)", ""},
  {"aRot", 'b', (void*)&ROTARY_A1, 0, 1, "A1 axis (C)", "Rotary (degrees)", ""},
  {"aSteps", 'l', (void*)&MOTOR_STEPS_A1, 1, 1000000, "A1 axis (C)", "Motor steps per screw turn", "steps"},
  {"aScrew", 'l', (void*)&SCREW_A1_DU, 1, 100000000, "A1 axis (C)", "Travel per screw turn", "du"},
  {"aStart", 'l', (void*)&SPEED_START_A1, 1, 1000000, "A1 axis (C)", "Start speed", "v:a"},
  {"aSpeed", 'l', (void*)&SPEED_MANUAL_MOVE_A1, 1, 1000000, "A1 axis (C)", "Maximum speed (manual moves, G0)", "v:a"},
  {"aAcc", 'l', (void*)&ACCELERATION_A1, 1, 100000000, "A1 axis (C)", "Acceleration", "a:a"},
  {"aInv", 'b', (void*)&INVERT_A1, 0, 1, "A1 axis (C)", "Invert direction", ""},
  {"aRest", 'b', (void*)&NEEDS_REST_A1, 0, 1, "A1 axis (C)", "Disable the driver when stopped (open loop)", ""},
  {"aTravel", 'l', (void*)&MAX_TRAVEL_MM_A1, 1, 1000000, "A1 axis (C)", "Longest manual travel (mm or degrees)", ""},
  {"aBl", 'l', (void*)&BACKLASH_DU_A1, 0, 100000, "A1 axis (C)", "Backlash", "du"},

  {"p1Use", 'b', (void*)&PULSE_1_USE, 0, 1, "Handwheels", "Handwheel on terminal A1", ""},
  {"p1Axis", 'a', (void*)&PULSE_1_AXIS, 0, 0, "Handwheels", "Handwheel A1 moves", ""},
  {"p1Inv", 'b', (void*)&PULSE_1_INVERT, 0, 1, "Handwheels", "Handwheel A1 inverted", ""},
  {"p2Use", 'b', (void*)&PULSE_2_USE, 0, 1, "Handwheels", "Handwheel on terminal A2", ""},
  {"p2Axis", 'a', (void*)&PULSE_2_AXIS, 0, 0, "Handwheels", "Handwheel A2 moves", ""},
  {"p2Inv", 'b', (void*)&PULSE_2_INVERT, 0, 1, "Handwheels", "Handwheel A2 inverted", ""},
  {"pPpr", 'f', (void*)&PULSE_PER_REVOLUTION, 1, 10000, "Handwheels", "Handwheel pulses per turn", ""},

  {"stepMs", 'l', (void*)&STEP_TIME_MS, 1, 10000, "Manual stepping", "Time for one manual step", "ms"},
  {"stepGap", 'l', (void*)&DELAY_BETWEEN_STEPS_MS, 0, 10000, "Manual stepping", "Pause between steps", "ms"},

  {"gcPause", 'b', (void*)&SPINDLE_PAUSES_GCODE, 0, 1, "GCODE mode", "Pause the program when the spindle stops (off for polar milling)", ""},
  {"gcRpm", 'i', (void*)&GCODE_MIN_RPM, 1, 10000, "GCODE mode", "Spindle stopped below", "rpm"},
  {"cncJd", 'f', (void*)&CNC_JUNCTION_DEVIATION, 0.001, 1, "GCODE mode", "Corner tolerance, junction deviation", "mm"},
  {"cncPulse", 'i', (void*)&CNC_STEP_PULSE_US, 1, 50, "GCODE mode", "Step pulse length", "µs"},
  {"cncStops", 'b', (void*)&CNC_RESPECT_STOPS, 0, 1, "GCODE mode", "Refuse moves outside the H4 stops", ""},
};
const int MACHINE_CONFIG_COUNT = sizeof(MACHINE_CONFIG) / sizeof(MACHINE_CONFIG[0]);
double machineConfigDefaults[MACHINE_CONFIG_COUNT];

// Functions take the index into MACHINE_CONFIG (the Arduino IDE puts prototypes above the struct).
double configGet(int index) {
  const MachineConfigItem& it = MACHINE_CONFIG[index];
  switch (it.type) {
    case 'l': return *(long*)it.ptr;
    case 'i': return *(int*)it.ptr;
    case 'b': return *(bool*)it.ptr ? 1 : 0;
    case 'f': return *(float*)it.ptr;
    case 'a': return *(char*)it.ptr;
  }
  return 0;
}

void configSet(int index, double v) {
  const MachineConfigItem& it = MACHINE_CONFIG[index];
  switch (it.type) {
    case 'l': *(long*)it.ptr = lround(v); break;
    case 'i': *(int*)it.ptr = (int)lround(v); break;
    case 'b': *(bool*)it.ptr = v != 0; break;
    case 'f': *(float*)it.ptr = (float)v; break;
    case 'a': *(char*)it.ptr = (char)lround(v); break;
  }
}

bool configValid(int index, double v) {
  const MachineConfigItem& it = MACHINE_CONFIG[index];
  if (isnan(v) || isinf(v)) return false;
  if (it.type == 'a') return v == NAME_Z || v == NAME_X;
  if (it.type == 'b') return v == 0 || v == 1;
  if ((it.type == 'l' || it.type == 'i') && v != floor(v)) return false;
  return v >= it.min && v <= it.max;
}

int configIndex(const char* key) {
  for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) if (strcmp(MACHINE_CONFIG[i].key, key) == 0) return i;
  return -1;
}

// Checks between values. values[] in MACHINE_CONFIG order. Empty string = fine.
String configCrossCheck(const double* values) {
  const char* axes[3] = {"z", "x", "a"};
  for (int a = 0; a < 3; a++) {
    double start = values[configIndex((String(axes[a]) + "Start").c_str())];
    double speed = values[configIndex((String(axes[a]) + "Speed").c_str())];
    if (start > speed) return String("Start speed above the maximum speed (") + axes[a] + ")";
  }
  if (values[configIndex("aAct")] != 0 && values[configIndex("p1Use")] != 0) {
    return "The A1 axis and handwheel 1 use the same terminal (A1): choose one";
  }
  return "";
}

void configDerived() {
  ENCODER_STEPS_INT = ENCODER_PPR * 2;
  ENCODER_STEPS_FLOAT = ENCODER_STEPS_INT;
  RPM_BULK = ENCODER_STEPS_INT;
}

// Called first in setup().
void loadMachineConfig() {
  for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) machineConfigDefaults[i] = configGet(i);
  Preferences cfg;
  if (cfg.begin(MACHINE_CONFIG_NAMESPACE, true)) {
    if (cfg.getInt("v", 0) == MACHINE_CONFIG_VERSION) {
      double values[MACHINE_CONFIG_COUNT];
      for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) {
        const MachineConfigItem& it = MACHINE_CONFIG[i];
        values[i] = machineConfigDefaults[i];
        if (cfg.isKey(it.key)) {
          double v = cfg.getDouble(it.key, values[i]);
          if (configValid(i, v)) values[i] = v;
        }
      }
      // A stored set that does not pass the checks is ignored as a whole: the defaults are safe.
      if (configCrossCheck(values).length() == 0) {
        for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) configSet(i, values[i]);
      }
    }
    cfg.end();
  }
  configDerived();
}

// The scale of an axis (steps per mm) or of the encoder changed: positions and stops stored in steps
// would be wrong, so the saved H4 state is cleared (as the H4 does when its storage format changes).
bool configScaleChanged(const double* values) {
  const char* keys[] = {"encPpr", "zSteps", "zScrew", "xSteps", "xScrew", "aSteps", "aScrew"};
  for (const char* k : keys) {
    int i = configIndex(k);
    if (values[i] != configGet(i)) return true;
  }
  return false;
}

// ------------------------------------------------------------------ for the web page (wifi.ino)

String machineConfigJson() {
  String json = "{\"items\":[";
  for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) {
    const MachineConfigItem& it = MACHINE_CONFIG[i];
    if (i > 0) json += ",";
    json += "{\"k\":\"" + String(it.key) + "\",\"t\":\"" + String(it.type) + "\",\"g\":\"" + it.group + "\",\"l\":\"" + it.label +
        "\",\"u\":\"" + it.unit + "\",\"min\":" + String(it.min, 4) + ",\"max\":" + String(it.max, 1);
    if (it.type == 'a') {
      json += ",\"v\":\"" + String((char)configGet(i)) + "\",\"d\":\"" + String((char)machineConfigDefaults[i]) + "\"";
    } else {
      json += ",\"v\":" + String(configGet(i), 6) + ",\"d\":" + String(machineConfigDefaults[i], 6);
    }
    json += "}";
  }
  json += "],\"a1Name\":\"" + String(NAME_A1) + "\"}";
  return json;
}
