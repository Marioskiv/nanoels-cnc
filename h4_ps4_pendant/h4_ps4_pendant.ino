// h4_ps4_pendant.ino - a PS4 / PS5 gamepad as a jog pendant for the NanoEls H4 CNC fork.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// Runs on a plain ESP32 (NOT the S3: the PS4/PS5 gamepads use Bluetooth Classic, which the S3 does not have).
// The gamepad connects to this ESP32 with Bluepad32; this ESP32 joins the same WiFi as the H4 and sends it
// Grbl jog commands on TCP port 23 ($J=, jog cancel, $H, $X, Ctrl-X). Nothing changes in the H4 firmware.
// Arduino: board package "esp32_bluepad32" (see README.md next to this file), board "ESP32 Dev Module".
//
// Buttons (the same layout as the LinuxCNC gamepad.hal of the XZC machine):
//   L1 (hold)          jog enable (deadman): nothing moves without it; releasing it stops at once
//   R1 (hold)          fast jog speed
//   Left stick <->     Z  (right = +Z)
//   Right stick up/dn  X  (down = +X)
//   L2 / R2            C+ / C-  (press further = faster)
//   L1 + Share         Home All ($H)
//   L1 + D-pad down    touch off Z: the tool's current point becomes Z0 of the active work offset
//   Options            STOP: stops all motion of the H4, a running program too (Grbl Ctrl-X)
//   PS                 clear an alarm ($X)
//   D-pad up / down    feed override +10 / -10 % (without L1); D-pad left: 100 %
//   Cross, Circle, Triangle, Square: not used (the H4 has no machine power and no spindle control).
// Light bar: white = no WiFi, green = ready, blue = jogging, red = the H4 does not answer, an alarm, or a
// refused command (the H4's reason is printed on USB and shown on the H4 web page).
//
// Safety: every jog command is a short piece (BLOCK_SECONDS) and the H4 never holds more than LEAD_SECONDS of
// them. If the stick returns, L1 is released, the gamepad or the WiFi is lost, motion stops: at once with a jog
// cancel when this ESP32 can still send it, otherwise after at most LEAD_SECONDS of travel plus braking.
// This is NOT an emergency stop: keep the power-supply switch within reach.

#include <Bluepad32.h>
#include <WiFi.h>

// ------------------------------------------------------------------ settings
const char* WIFI_SSID = "";              // the network of the H4
const char* WIFI_PASSWORD = "";
const char* H4_ADDRESS = "192.168.1.50"; // shown on the H4 display; fix it in the router (DHCP reservation)
const uint16_t H4_PORT = 23;

const float JOG_SPEED = 400;             // mm/min, normal
const float JOG_SPEED_FAST = 1000;       // mm/min, with R1 held (the H4 still limits each axis to its own maximum)
const float C_SPEED_FACTOR = 6;          // C deg/min = the above x this (400 -> 40 deg/s, 1000 -> 100 deg/s)
const bool USE_C = true;                 // false when the H4 has no C axis
const float DEAD_ZONE_Z = 0.35;          // part of the stick travel ignored around the centre (0..1)
const float DEAD_ZONE_X = 0.45;
const float DEAD_ZONE_TRIGGER = 0.05;
const int DIR_Z = 1;                     // -1 reverses an axis
const int DIR_X = 1;
const int DIR_C = 1;

const float BLOCK_SECONDS = 0.05;        // length of one jog command, in time
const float LEAD_SECONDS = 0.2;          // most jog time the H4 may hold
const int MAX_BLOCKS_IN_H4 = 4;          // most jog commands in the H4's planner
const unsigned long GAMEPAD_TIMEOUT_MS = 250; // no report from the gamepad this long while jogging -> stop
const unsigned long IDLE_DISCONNECT_MS = 10000; // leave port 23 free for a G-code sender when not in use

// ------------------------------------------------------------------ state
ControllerPtr pad = nullptr;
WiFiClient h4;
bool h4Ready = false;            // the H4 has answered with its start message
char rx[160];
int rxLen = 0;
int outstanding = 0;             // lines sent and not yet answered with ok / error
int plannerFreeMax = 0;          // largest "Bf:" seen = the planner size
int plannerUsed = 0;             // jog commands in the H4's planner (status report, plus the ones sent since)
bool h4Alarm = false;
bool refused = false;            // an error while jogging: wait until the sticks / L1 are released
bool jogging = false;
bool waitIdle = false;           // after a stop: no new jog until the H4 reports that it stands still
bool connectFailed = false;
float queuedSeconds = 0;
unsigned long lastUseMs = 0, lastStatusMs = 0, lastLoopMs = 0, lastPadDataMs = 0, lastConnectTryMs = 0;
uint16_t lastButtons = 0, lastMisc = 0;
uint8_t lastDpad = 0;
uint32_t lastColor = 0xffffffff;
// a command pressed while the connection to the H4 is still starting: sent once it is ready (within 2 s)
const char* pendingLine = nullptr;
unsigned long pendingMs = 0, waitIdleMs = 0;

void stopJog();

// ------------------------------------------------------------------ gamepad

void onConnectedController(ControllerPtr ctl) {
  if (pad == nullptr && ctl->isGamepad()) {
    pad = ctl;
    lastColor = 0xffffffff;
    Serial.printf("Gamepad connected: %s\n", ctl->getModelName().c_str());
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (ctl == pad) {
    pad = nullptr;
    Serial.println("Gamepad disconnected");
    stopJog();
  }
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = (r << 16) | (g << 8) | b;
  if (pad == nullptr || c == lastColor) return;
  pad->setColorLED(r, g, b);
  lastColor = c;
}

void rumble() {
  if (pad != nullptr) pad->playDualRumble(0, 200, 0x80, 0x40);
}

// -1..1 from a stick (-511..512), with the dead zone taken out so that motion starts smoothly from 0.
float stick(int32_t raw, float deadZone) {
  float v = constrain(raw / 512.0f, -1.0f, 1.0f);
  float a = fabsf(v);
  if (a <= deadZone) return 0;
  return (v > 0 ? 1 : -1) * (a - deadZone) / (1 - deadZone);
}

float trigger(int32_t raw) {
  float v = constrain(raw / 1023.0f, 0.0f, 1.0f);
  return v <= DEAD_ZONE_TRIGGER ? 0 : (v - DEAD_ZONE_TRIGGER) / (1 - DEAD_ZONE_TRIGGER);
}

// ------------------------------------------------------------------ H4 connection

bool h4Connected() {
  return h4.connected();
}

void h4Disconnect() {
  if (h4.connected()) h4.stop();
  h4Ready = false;
  outstanding = 0;
  plannerUsed = 0;
  rxLen = 0;
}

bool h4Connect() {
  if (h4.connected()) return true;
  h4Ready = false;
  if (WiFi.status() != WL_CONNECTED || millis() - lastConnectTryMs < 1000) return false;
  lastConnectTryMs = millis();
  if (!h4.connect(H4_ADDRESS, H4_PORT, 500)) {
    Serial.printf("No answer from the H4 at %s:%d\n", H4_ADDRESS, H4_PORT);
    connectFailed = true;
    return false;
  }
  connectFailed = false;
  waitIdle = false;
  h4.setNoDelay(true);
  outstanding = 0;
  plannerUsed = 0;
  rxLen = 0;
  h4Alarm = false;
  return true;
}

void sendLine(const char* line) {
  if (!h4.connected()) return;
  h4.print(line);
  h4.print("\n");
  outstanding++;
  lastUseMs = millis();
}

void sendByte(uint8_t b) {
  if (h4.connected()) h4.write(b);
}

// One answer line from the H4.
void handleLine(const char* s) {
  if (strncmp(s, "Grbl ", 5) == 0) {
    h4Ready = true;  // start message (also after a Ctrl-X)
    outstanding = 0;
    plannerUsed = 0;
  } else if (strcmp(s, "ok") == 0) {
    if (outstanding > 0) outstanding--;
  } else if (strncmp(s, "error:", 6) == 0) {
    if (outstanding > 0) outstanding--;
    Serial.printf("H4 refused: %s\n", s);
    if (jogging) {
      stopJog();
      refused = true;
    }
    rumble();
  } else if (strncmp(s, "ALARM", 5) == 0) {
    h4Alarm = true;
    stopJog();
    refused = true;
    rumble();
  } else if (s[0] == '<') {
    // status report <State|WPos:...|Bf:planner,rx|...>
    h4Alarm = strncmp(s + 1, "Alarm", 5) == 0;
    if (strncmp(s + 1, "Jog", 3) != 0 && strncmp(s + 1, "Run", 3) != 0 && strncmp(s + 1, "Home", 4) != 0) waitIdle = false;
    const char* bf = strstr(s, "|Bf:");
    if (bf) {
      int freeBlocks = atoi(bf + 4);
      if (freeBlocks > plannerFreeMax) plannerFreeMax = freeBlocks;
      plannerUsed = plannerFreeMax - freeBlocks;
    }
  } else if (strncmp(s, "[MSG:", 5) == 0) {
    Serial.println(s);
  }
}

void readH4() {
  while (h4.connected() && h4.available() > 0) {
    char c = h4.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rx[rxLen] = '\0';
      if (rxLen > 0) handleLine(rx);
      rxLen = 0;
    } else if (rxLen < (int)sizeof(rx) - 1) {
      rx[rxLen++] = c;
    }
  }
}

// ------------------------------------------------------------------ jogging

void stopJog() {
  if (jogging) {
    sendByte(0x85);  // jog cancel: the H4 brakes and drops the rest
    waitIdle = true; // (it refuses a new jog while it is still braking)
    waitIdleMs = millis();
  }
  jogging = false;
  queuedSeconds = 0;
}

// Sends one jog piece of BLOCK_SECONDS at the given speeds (mm/min for X and Z, deg/min for C).
void sendJog(float vx, float vz, float vc) {
  float dx = vx / 60 * BLOCK_SECONDS, dz = vz / 60 * BLOCK_SECONDS, dc = vc / 60 * BLOCK_SECONDS;
  // the feed as LinuxCNC and Grbl read it: along X/Z when they move (C follows in the same time), else C alone
  float linear = sqrtf(dx * dx + dz * dz);
  float feed = (linear > 0 ? linear : fabsf(dc)) / BLOCK_SECONDS * 60;
  char line[120];
  int n = snprintf(line, sizeof(line), "$J=G91 G94 G21");
  if (dx != 0) n += snprintf(line + n, sizeof(line) - n, " X%.4f", dx);
  if (dz != 0) n += snprintf(line + n, sizeof(line) - n, " Z%.4f", dz);
  if (dc != 0) n += snprintf(line + n, sizeof(line) - n, " C%.4f", dc);
  snprintf(line + n, sizeof(line) - n, " F%.1f", feed);
  sendLine(line);
  queuedSeconds += BLOCK_SECONDS;
  plannerUsed++;
  jogging = true;
}

// ------------------------------------------------------------------ main

void setup() {
  Serial.begin(115200);
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableNewBluetoothConnections(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("NanoEls H4 PS4 pendant: hold Share + PS on the gamepad to pair it");
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastLoopMs) / 1000.0f;
  lastLoopMs = now;
  if (BP32.update()) lastPadDataMs = now;
  readH4();
  if (!h4.connected() && (h4Ready || jogging)) {
    Serial.println("H4 connection lost");
    h4Disconnect();
    jogging = false;
    queuedSeconds = 0;
  }

  bool padOk = pad != nullptr && pad->isConnected() && now - lastPadDataMs < GAMEPAD_TIMEOUT_MS;
  uint16_t buttons = padOk ? pad->buttons() : 0;
  uint16_t misc = padOk ? pad->miscButtons() : 0;
  uint8_t dpad = padOk ? pad->dpad() : 0;
  bool l1 = buttons & BUTTON_SHOULDER_L;
  bool r1 = buttons & BUTTON_SHOULDER_R;
  bool pressedShare = (misc & MISC_BUTTON_SELECT) && !(lastMisc & MISC_BUTTON_SELECT);
  bool pressedOptions = (misc & MISC_BUTTON_START) && !(lastMisc & MISC_BUTTON_START);
  bool pressedPs = (misc & MISC_BUTTON_SYSTEM) && !(lastMisc & MISC_BUTTON_SYSTEM);
  bool pressedDown = (dpad & DPAD_DOWN) && !(lastDpad & DPAD_DOWN);
  bool pressedUp = (dpad & DPAD_UP) && !(lastDpad & DPAD_UP);
  bool pressedLeft = (dpad & DPAD_LEFT) && !(lastDpad & DPAD_LEFT);
  lastButtons = buttons;
  lastMisc = misc;
  lastDpad = dpad;

  // STOP works without L1, and connects first when needed
  if (pressedOptions) {
    if (h4Connect() || h4.connected()) {
      sendByte(0x18);
      Serial.println("STOP sent");
    }
    jogging = false;
    queuedSeconds = 0;
    refused = true;
    lastUseMs = now;
  }
  if (l1 || pressedPs) {
    lastUseMs = now;
    h4Connect();
  }

  float speed = r1 ? JOG_SPEED_FAST : JOG_SPEED;
  float vz = padOk ? DIR_Z * stick(pad->axisX(), DEAD_ZONE_Z) * speed : 0;
  float vx = padOk ? DIR_X * stick(pad->axisRY(), DEAD_ZONE_X) * speed : 0;
  float vc = padOk && USE_C ? DIR_C * (trigger(pad->brake()) - trigger(pad->throttle())) * speed * C_SPEED_FACTOR : 0;
  bool wanted = l1 && (vx != 0 || vz != 0 || vc != 0);
  if (!wanted) refused = false;  // released: the next jog may try again

  if (pressedPs) pendingLine = "$X";
  if (l1 && pressedShare && !jogging) pendingLine = "$H";
  if (l1 && pressedDown && !jogging) pendingLine = "G10 L20 P0 Z0";
  if (pressedPs || pressedShare || pressedDown) pendingMs = now;
  if (pendingLine != nullptr && now - pendingMs > 2000) pendingLine = nullptr;
  if (waitIdle && now - waitIdleMs > 3000) waitIdle = false;

  if (h4.connected() && h4Ready) {
    if (!l1 && pressedUp) sendByte(0x91);    // feed override +10 %
    if (!l1 && pressedDown) sendByte(0x92);  // -10 %
    if (!l1 && pressedLeft) sendByte(0x90);  // 100 %
    if (pendingLine != nullptr) {
      sendLine(pendingLine);
      pendingLine = nullptr;
    }
    if (!wanted || refused || h4Alarm) {
      stopJog();
    } else if (waitIdle && !jogging) {
      // the last stop is still braking
    } else {
      queuedSeconds = max(0.0f, queuedSeconds - dt);
      if (queuedSeconds < LEAD_SECONDS && outstanding < 2 && plannerUsed < MAX_BLOCKS_IN_H4) sendJog(vx, vz, vc);
    }
    if (now - lastStatusMs >= 100) {
      lastStatusMs = now;
      sendByte('?');
    }
    if (!l1 && !jogging && now - lastUseMs > IDLE_DISCONNECT_MS) h4Disconnect();
  }

  if (WiFi.status() != WL_CONNECTED) setColor(40, 40, 40);
  else if (connectFailed || h4Alarm || refused) setColor(255, 0, 0);
  else if (jogging) setColor(0, 0, 255);
  else setColor(0, 255, 0);
  delay(5);
}
