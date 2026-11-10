// wifi.ino - WiFi web page of the NanoEls H4 CNC fork: G-code files kept on the controller, live status,
// Start / Pause / Resume / Stop, the H4 keys, WiFi settings and firmware update.
// https://github.com/Marioskiv/nanoels-cnc
//
// WiFi set-up (setup access point, credentials in Preferences), the firmware upload, the machine settings
// (config.ino) and the held keys with a fail-safe release follow the NanoEls H5 firmware (MIT,
// Copyright (c) Maxim Kachurovskiy). The file storage is different from H5:
// files are written to and read from flash in pieces, so a program can be much larger than the RAM,
// and it is loaded into RAM (PSRAM when the board has it) before it starts.
//
// RULE: no flash access while anything moves. A flash read or write turns the CPU cache off; the GCODE step
// interrupt is not cache-safe (Arduino's gptimer allocates it without ESP_INTR_FLAG_IRAM), so it waits, and so
// do the step tasks of the other modes (turn, thread, ...), for up to milliseconds: the motors would stutter or
// a thread lose its sync. Every handler that touches LittleFS, Preferences or the firmware partition is refused
// while motionActive(): a G-code program, homing, the H4 switched ON in any mode, or an axis stepping (jog).
//
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)

#include "webpage.h"
#include <WiFiUdp.h>

#define WIFI_CONFIG_NAMESPACE "h4wifi"
// The H4's own network ("direct" connection: the tablet joins it, no router). Short and easy to type; the password
// can be changed from the app (Settings, Connection), it is kept as "appw" in the WiFi settings.
const char* WIFI_SETUP_AP_SSID = "H4";
const char* WIFI_AP_DEFAULT_PASSWORD = "12345678";
String wifiApPassword = WIFI_AP_DEFAULT_PASSWORD;
// The app finds the H4 by itself: it sends "H4CNC?" by UDP broadcast to this port, the H4 answers to the sender
// "H4CNC {json}" with its address, firmware and connection (discoveryLoop). Works on the router and on its own network.
const uint16_t WIFI_DISCOVERY_PORT = 4210;
WiFiUDP discoveryUdp;
bool discoveryStarted = false;
const size_t PROGRAM_MAX_RAM_NO_PSRAM = 120000; // without PSRAM a program must fit in the normal RAM

WebServer server(80);
String wifiSsid = "";
String wifiPassword = "";
String wifiStatusText = "WiFi off";
String wifiIp = "";
bool wifiSetupAp = false;
bool wifiOwnNetwork = false; // stored in the WiFi settings ("own"): start the H4's own network, do not look for the router
bool wifiRestartPending = false;
unsigned long wifiRestartAt = 0;

char* webProgram = NULL; // the loaded file (malloc / ps_malloc)
String webProgramName = "";
int webProgramSize = 0;

File uploadFile;
bool uploadFailed = false;
String uploadMessage = "";
size_t uploadBytes = 0;
bool fwUploadActive = false;
bool fwUploadChecked = false;  // the first block was looked at: it must be an application image
bool fwUploadFailed = false;
String fwUploadMessage = "";

// Cached free space: LittleFS usedBytes() reads the flash, so it is only measured when nothing runs.
size_t fsTotalBytes = 0;
size_t fsUsedBytes = 0;

// A key held on the page (arrows for jogging): the page repeats the press every 150 ms while the finger
// is down. If the repeats stop (page closed, phone asleep, WiFi lost), the key is released here.
const unsigned long HELD_KEY_TIMEOUT_MS = 500;
int heldKey = 0;
unsigned long heldKeyMs = 0;

// One G-code line typed on the page (MDI), run as a one-line program.
char mdiProgram[200];

bool cncRunActive() {
  return homingActive || homingRequest || (mode == MODE_GCODE && (isOn || cncBusy() || cncStopRequest));
}

// Any motion, or the machine ON in any mode (it may move at any moment, e.g. following the spindle). Only reads
// flags: refusing a request changes nothing in the program or the planner.
// The name of the program the web page loaded (empty for a Grbl sender).
String currentProgramName() {
  return webProgramName;
}

bool motionActive() {
  return cncRunActive() || isOn || stepperIsRunning(&x) || stepperIsRunning(&z) || stepperIsRunning(&a1);
}

bool refuseWhileRunning() {
  if (!motionActive()) return false;
  server.send(409, "text/plain", "The machine is ON or moving: stop it (OFF) first.");
  return true;
}

void updateFsUsage() {
  fsTotalBytes = LittleFS.totalBytes();
  fsUsedBytes = LittleFS.usedBytes();
}

String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c < 32) out += ' ';
    else out += c;
  }
  return out;
}

bool validFileName(const String& name) {
  if (name.length() < 1 || name.length() > 40) return false;
  for (unsigned int i = 0; i < name.length(); i++) {
    char c = name.charAt(i);
    if (!(isalnum(c) || c == '-' || c == '_' || c == '.' || c == ' ')) return false;
  }
  return !name.startsWith(".");
}

String filePath(const String& name) {
  return "/gcode/" + name;
}

// ------------------------------------------------------------------ handlers

void handlePage() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", H4_WEB_PAGE);
}

// One line of the H4 display as JSON text; its special characters as Unicode.
String lcdLineJson(int r) {
  String out;
  for (int i = 0; i < 20; i++) {
    unsigned char c = lcd.shadow[r][i];
    switch (c) {
      case 8: out += "\\u339c"; break;   // mm (custom character 0)
      case 1: out += "\\u2191"; break;   // stops: up, down, left, right, up-down, left-right
      case 2: out += "\\u2193"; break;
      case 3: out += "\\u2190"; break;
      case 4: out += "\\u2192"; break;
      case 5: out += "\\u2195"; break;
      case 6: out += "\\u2194"; break;
      case 7: out += "\\u00d8"; break;   // Ø (diameter view, latheview.ino)
      case 223: out += "\\u00b0"; break; // degree
      case 126: out += "\\u2192"; break;
      case 127: out += "\\u2190"; break;
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      default: out += (c >= 32 && c < 127) ? (char)c : ' ';
    }
  }
  return out;
}

void handleLog() {
  String json = "[";
  portENTER_CRITICAL(&cncLogMux);
  uint32_t count = cncLogCount;
  portEXIT_CRITICAL(&cncLogMux);
  int n = count < CNC_LOG_LINES ? count : CNC_LOG_LINES;
  for (int i = 0; i < n; i++) {
    char copy[sizeof(cncLogText[0])];
    portENTER_CRITICAL(&cncLogMux);
    memcpy(copy, cncLogText[(count - n + i) % CNC_LOG_LINES], sizeof(copy));
    portEXIT_CRITICAL(&cncLogMux);
    if (i > 0) json += ",";
    json += "\"" + jsonEscape(String(copy)) + "\"";
  }
  json += "]";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// MDI: one line, typed on the page, in the GCODE mode while no program runs. The modes start fresh
// (G90 G1 G94 mm), as for every program.
void handleMdi() {
  if (refuseWhileRunning()) return;
  if (emergencyStop != ESTOP_NONE) {
    server.send(409, "text/plain", "Emergency stop: restart the controller.");
    return;
  }
  String line = server.arg("line");
  line.trim();
  if (line.equalsIgnoreCase("$X")) { // unlock, as in Grbl: allow moves before Home All
    cncHomeOverride = true;
    server.send(200, "text/plain", "Unlocked: moves are allowed before Home All. Take care.");
    return;
  }
  if (line.equalsIgnoreCase("$H")) { // Home All, as in Grbl
    if (!homingConfigured()) {
      server.send(409, "text/plain", "No home switch is set up (Machine settings).");
      return;
    }
    homingRequest = true;
    server.send(200, "text/plain", "Home All");
    return;
  }
  if (line.length() == 0 || line.length() >= sizeof(mdiProgram) - 2 || line.indexOf('\n') >= 0 || line.indexOf('\r') >= 0) {
    server.send(400, "text/plain", "One line, up to 190 characters.");
    return;
  }
  if (mode != MODE_GCODE) setModeFromTask(MODE_GCODE);
  unsigned long t0 = millis();
  while (mode != MODE_GCODE && millis() - t0 < 500) vTaskDelay(5);
  if (mode != MODE_GCODE) {
    server.send(500, "text/plain", "Could not switch to GCODE mode.");
    return;
  }
  snprintf(mdiProgram, sizeof(mdiProgram), "%s\n", line.c_str());
  gcodeProgramCharIndex = 0;
  gcodeProgramData = mdiProgram;
  gcodeProgramSize = strlen(mdiProgram);
  cncUserHold = false;
  setIsOnFromTask(true);
  server.send(200, "text/plain", "Running: " + line);
}

void handleStatus() {
  cncAppSeenMs = millis(); // the app / web page is watching (h4.ino cncLinkWatch)
  float divisor = measure == MEASURE_METRIC ? 10000.0 : 254000.0;
  const char* state = "Idle";
  if (cncStopRequest) state = "Stopping";
  else if (cncPaused) state = "Paused";
  else if (cncUserHold || cncSpindleHold) state = "Hold";
  else if (isOn && mode == MODE_GCODE) state = "Run";
  else if (isOn) state = "On";
  float progress = gcodeProgramSize > 0 ? float(gcodeProgramCharIndex) / gcodeProgramSize : 0;
  String json = "{";
  json += "\"mode\":" + String(mode);
  json += ",\"gcodeMode\":" + String(mode == MODE_GCODE ? 1 : 0);
  json += ",\"on\":" + String(isOn ? 1 : 0);
  json += ",\"state\":\"" + String(state) + "\"";
  json += ",\"text\":\"" + jsonEscape(String((const char*)cncStatusText)) + "\"";
  json += ",\"x\":" + String(getAxisPosDu(&x) / divisor, 4);
  json += ",\"z\":" + String(getAxisPosDu(&z) / divisor, 4);
  json += ",\"c\":" + String(a1.active ? getAxisPosDu(&a1) / 10000.0 : 0.0, 4);
  json += ",\"cActive\":" + String(a1.active ? 1 : 0);
  json += ",\"restart\":\"" + jsonEscape(restartText) + "\""; // restarted while moving, not confirmed (safety.ino)
  json += ",\"lzv\":" + String(latheZView) + ",\"lxv\":" + String(latheXView); // how the lathe modes show Z / X
  json += ",\"inch\":" + String(measure == MEASURE_METRIC ? 0 : 1);
  json += ",\"feed\":" + String(cnc_st_realtime_rate(), 1);
  json += ",\"rpm\":" + String(getApproxRpm());
  // the chuck angle from the app's zero (spindle encoder, display only; chuckzero below)
  json += ",\"chuck\":" + String(spindleModulo(spindlePosGlobal - chuckAngleZero) * 360 / ENCODER_STEPS_FLOAT, 1);
  char execLine[64];
  cncExecLine(execLine, sizeof(execLine), true);  // the line that runs now, with its number (h4.ino)
  json += ",\"line\":\"" + jsonEscape(String(execLine)) + "\"";
  json += ",\"lineNo\":" + String(cnc_st_current_line()) + ",\"stoppedAt\":" + String(cncStoppedAtLine);
  json += ",\"progress\":" + String(progress, 4);
  json += ",\"program\":\"" + jsonEscape(webProgramName) + "\"";
  json += ",\"programLoaded\":" + String(webProgram != NULL && gcodeProgramData == webProgram ? 1 : 0);
  json += ",\"underruns\":" + String(cnc_st_underruns());
  json += ",\"diag\":{\"isrUs\":" + String(cncDiagIsrMaxCycles / (float)cncCpuCyclesPerUs, 1) +
      ",\"lateUs\":" + String(cncDiagLateMaxTicks / (float)(CNC_TIMER_HZ / 1000000), 1) +
      ",\"late\":" + String(cncDiagLateCount) + ",\"rate\":" + String(cncDiagRateMax) +
      ",\"limit\":" + String(cncStepRateLimit()) + ",\"steps\":" + String(cncDiagSteps) +
      ",\"movePulses\":" + String(moveDiagPulses) + ",\"timer\":\"" + jsonEscape(String(cncStepTimerText)) + "\"" +
      ",\"movePulseMinUs\":" + String(moveDiagPulses ? moveDiagPulseMinCycles / (float)cncCpuCyclesPerUs : 0, 2) +
      ",\"movePulseMaxUs\":" + String(moveDiagPulseMaxCycles / (float)cncCpuCyclesPerUs, 2) +
      ",\"loopGapMs\":" + String(loopGapMaxUs / 1000.0, 1) + ",\"loopLong\":" + String(loopGapLongCount) + "}";
  json += ",\"benchN\":" + String(cncBenchCount) + ",\"bench\":\"" + jsonEscape(String(cncBenchText)) + "\"";
  cnc_modes_t modes;
  cnc_gc_get_modes(&modes);
  const char* wcsNames[] = {"G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3"};
  json += ",\"wcs\":\"" + String(wcsNames[constrain(modes.coord, 0, 8)]) + "\"";
  json += ",\"rssi\":" + String(wifiSetupAp ? 0 : WiFi.RSSI());
  json += ",\"fw\":\"" FW_VERSION "\"";
  json += ",\"api\":1"; // the version of this interface; the app's screen checks it
  json += ",\"fo\":" + String(cncFeedOverrideWanted) + ",\"ro\":" + String(cncRapidOverrideWanted);
  if (!modes.overridesEnabled) json += ",\"ovrOff\":1";
  // what the program asked of the spindle, coolant and tool (the H4 switches neither spindle nor coolant)
  json += ",\"spin\":" + String(modes.spindle) + ",\"sp\":" + String(modes.spindleSpeed, 0) +
      ",\"cool\":\"" + String(modes.mist && modes.flood ? "M7 M8" : (modes.mist ? "M7" : (modes.flood ? "M8" : ""))) + "\"" +
      ",\"tool\":" + String(modes.tool);  // M49 / M50 P0 in the program: the overrides do not apply
  json += ",\"estop\":" + String(emergencyStop);
  // Lathe modes, as the H4's own screen shows them (read only): pitch, starts, passes, cone ratio, external /
  // internal, setup step, pass, spindle angle, and the stops in the displayed units (null: not set).
  auto stopJson = [&](Axis* a, long s, bool isSet) -> String {
    return isSet ? String(stepsToDu(a, s + a->originPos) / divisor, 4) : String("null");
  };
  json += ",\"lathe\":{\"pitch\":" + String(dupr / 10000.0, 4) + ",\"measure\":" + String(measure) +
      ",\"starts\":" + String(starts) + ",\"flank\":" + String(threadFlank ? 1 : 0) + ",\"passes\":" + String(turnPasses) + ",\"cone\":" + String(coneRatio, 5) +
      ",\"aux\":" + String(auxForward ? 1 : 0) + ",\"setup\":" + String(setupIndex) + ",\"op\":" + String(opIndex) +
      ",\"ang\":" + String(spindleModulo(spindlePos) * 360 / ENCODER_STEPS_FLOAT, 1) +
      ",\"zl\":" + stopJson(&z, z.leftStop, z.leftStop != LONG_MAX) + ",\"zr\":" + stopJson(&z, z.rightStop, z.rightStop != LONG_MIN) +
      ",\"xl\":" + stopJson(&x, x.leftStop, x.leftStop != LONG_MAX) + ",\"xr\":" + stopJson(&x, x.rightStop, x.rightStop != LONG_MIN) +
      // for the app's part: operations ended normally, and the tool in a frame that zeroing does not move
      ",\"done\":" + String((unsigned long)latheDoneCount) +
      ",\"zm\":" + String(stepsToDu(&z, z.pos + z.machineOffset) / divisor, 4) + ",\"xm\":" + String(stepsToDu(&x, x.pos + x.machineOffset) / divisor, 4) + "}";
  json += ",\"boot\":" + String((unsigned long)bootNumber); // changes at every start (safety.ino)
  json += ",\"running\":" + String(cncRunActive() ? 1 : 0);
  json += ",\"fsTotal\":" + String((unsigned long)fsTotalBytes);
  json += ",\"fsUsed\":" + String((unsigned long)fsUsedBytes);
  json += ",\"psram\":" + String((unsigned long)(psramFound() ? ESP.getFreePsram() : 0));
  json += ",\"heap\":" + String((unsigned long)ESP.getFreeHeap());
  json += ",\"wifi\":\"" + jsonEscape(wifiStatusText) + "\"";
  json += ",\"lcd\":[";
  for (int r = 0; r < 4; r++) {
    if (r > 0) json += ",";
    json += "\"" + lcdLineJson(r) + "\"";
  }
  json += "],\"log\":" + String((unsigned long)cncLogCount);
  json += ",\"held\":" + String(heldKey);
  json += ",\"mdi\":" + String(gcodeProgramData == mdiProgram && cncRunActive() ? 1 : 0);
  json += ",\"in\":" + String((int)inRaw);
  json += ",\"act\":[" + String(inHomeX ? 1 : 0) + "," + String(inHomeZ ? 1 : 0) + "," + String(inAlarm ? 1 : 0) + "]";
  json += ",\"use\":[" + String(HOME_X_USE ? 1 : 0) + "," + String(HOME_Z_USE ? 1 : 0) + "," + String(ALARM_USE ? 1 : 0) + "]";
  json += ",\"homed\":[" + String(x.homed ? 1 : 0) + "," + String(z.homed ? 1 : 0) + "," + String(a1.homed ? 1 : 0) + "]";
  json += ",\"homing\":" + String(homingActive ? 1 : 0);
  json += ",\"a2\":" + String(PULSE_2_USE ? 0 : 1);
  if (x.homed) json += ",\"mx\":" + String(machinePosMm(&x), 4);
  if (z.homed) json += ",\"mz\":" + String(machinePosMm(&z), 4);
  json += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

void handleFileList() {
  if (refuseWhileRunning()) return;
  wifiFsBusy = true;
  String json = "[";
  File dir = LittleFS.open("/gcode");
  if (dir && dir.isDirectory()) {
    bool first = true;
    File f = dir.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + jsonEscape(String(f.name())) + "\",\"size\":" + String((unsigned long)f.size()) + "}";
      }
      f.close();
      f = dir.openNextFile();
    }
    dir.close();
  }
  json += "]";
  updateFsUsage();
  wifiFsBusy = false;
  server.send(200, "application/json", json);
}

// Upload: the file arrives in pieces and is written as it comes (no size limit but the free space).
void handleUploadData() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    wifiFsBusy = true;
    uploadFailed = false;
    uploadBytes = 0;
    uploadMessage = "";
    String name = upload.filename;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (motionActive()) {
      uploadFailed = true;
      uploadMessage = "The machine is ON or moving: stop it (OFF) first.";
      return;
    }
    if (!validFileName(name)) {
      uploadFailed = true;
      uploadMessage = "File name: letters, digits, - _ . and spaces, up to 40 characters.";
      return;
    }
    LittleFS.mkdir("/gcode");
    uploadFile = LittleFS.open(filePath(name), "w");
    if (!uploadFile) {
      uploadFailed = true;
      uploadMessage = "Cannot create the file.";
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFailed || !uploadFile) return;
    if (motionActive()) {
      uploadFailed = true;
      uploadMessage = "The machine started during the upload.";
      return;
    }
    size_t written = uploadFile.write(upload.buf, upload.currentSize);
    uploadBytes += written;
    if (written != upload.currentSize) {
      uploadFailed = true;
      uploadMessage = "The controller's flash is full.";
    }
  } else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    String name = upload.filename;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (upload.status == UPLOAD_FILE_ABORTED && !uploadFailed) {
      uploadFailed = true;
      uploadMessage = "Upload interrupted.";
    }
    if (uploadFailed && validFileName(name) && !motionActive()) LittleFS.remove(filePath(name));
    if (!uploadFailed) uploadMessage = "Saved " + name + " (" + String((unsigned long)uploadBytes) + " bytes)";
    if (!motionActive()) updateFsUsage();
    wifiFsBusy = false;
  }
}

void handleUploadDone() {
  server.send(uploadFailed ? 400 : 200, "text/plain", uploadMessage);
}

void handleFileDelete() {
  if (refuseWhileRunning()) return;
  String name = server.arg("name");
  wifiFsBusy = true;
  bool ok = validFileName(name) && LittleFS.remove(filePath(name));
  if (ok) updateFsUsage();
  wifiFsBusy = false;
  if (!ok) {
    server.send(400, "text/plain", "Cannot delete " + name);
    return;
  }
  if (name == webProgramName && gcodeProgramData != webProgram) webProgramName = "";
  server.send(200, "text/plain", "Deleted " + name);
}

// The browser draws the path from the file itself; only while nothing runs (flash read).
void handleFileGet() {
  if (refuseWhileRunning()) return;
  String name = server.arg("name");
  if (!validFileName(name)) {
    server.send(400, "text/plain", "Bad name");
    return;
  }
  wifiFsBusy = true;
  File f = LittleFS.open(filePath(name), "r");
  if (!f) {
    wifiFsBusy = false;
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(f, "text/plain");
  f.close();
  wifiFsBusy = false;
}

// Loads a file into RAM and makes it the GCODE program. It starts with Start (web) or ON (keys).
bool loadProgram(const String& name, String* error) {
  File f = LittleFS.open(filePath(name), "r");
  if (!f) {
    *error = "File not found: " + name;
    return false;
  }
  size_t size = f.size();
  bool psram = psramFound();
  if (!psram && size > PROGRAM_MAX_RAM_NO_PSRAM) {
    f.close();
    *error = "The program (" + String((unsigned long)size) + " bytes) needs PSRAM: in the Arduino IDE choose PSRAM: QSPI PSRAM and upload the firmware again.";
    return false;
  }
  // Free the previous file only if it is not the program in use.
  if (webProgram != NULL && gcodeProgramData != webProgram) {
    free(webProgram);
    webProgram = NULL;
  }
  if (webProgram != NULL) {
    gcodeProgramData = "";
    gcodeProgramSize = 0;
    free(webProgram);
    webProgram = NULL;
  }
  char* buffer = (char*)(psram ? ps_malloc(size + 2) : malloc(size + 2));
  if (buffer == NULL) {
    f.close();
    *error = "Not enough memory for " + String((unsigned long)size) + " bytes.";
    return false;
  }
  size_t got = 0;
  while (got < size) {
    int n = f.read((uint8_t*)buffer + got, min((size_t)4096, size - got));
    if (n <= 0) break;
    got += n;
  }
  f.close();
  if (got != size) {
    free(buffer);
    *error = "Read error.";
    return false;
  }
  buffer[size] = '\n'; // the last line is run even without a line end
  buffer[size + 1] = '\0';
  webProgram = buffer;
  webProgramName = name;
  webProgramSize = size + 1;
  return true;
}

void handleCommand() {
  String what = server.arg("do");
  if (what == "stop") {
    cncPreflightCancel = true; // a check before a start does not start the program
    homingAbort = true;
    homingRequest = false;
    if (!isOn && !cncBusy()) haltManualMoves(); // a move of the H4 keys or of the page's safe approach
    setIsOnFromTask(false); // decelerates along the path, then drops the rest (GCODE mode)
    cncUserHold = false;
    server.send(200, "text/plain", "Stopping");
  } else if (what == "bench") {
    if (refuseWhileRunning()) return;
    if (isOn || cncBusy() || homingActive) {
      server.send(409, "text/plain", "Turn the machine off first.");
      return;
    }
    String name = server.arg("name");
    String error = "";
    wifiFsBusy = true;
    bool loaded = validFileName(name) && loadProgram(name, &error);
    wifiFsBusy = false;
    if (!loaded) {
      server.send(400, "text/plain", error.length() ? error : "Bad name");
      return;
    }
    snprintf(cncBenchName, sizeof(cncBenchName), "%s", name.c_str());
    cncBenchData = webProgram;
    cncBenchSize = webProgramSize > 0 ? webProgramSize - 1 : 0;  // without the closing 0
    cncBenchRequest = true;
    server.send(200, "text/plain", "Testing " + name + " (nothing moves) ...");
  } else if (what == "confirmRestart") {
    // after a restart while moving: the operator checked the zeros again (machines without Home All)
    safetyConfirmRestart("the operator confirmed the zeros after the restart");
    server.send(200, "text/plain", "Confirmed");
  } else if (what == "diagReset") {
    cncDiagReset();
    server.send(200, "text/plain", "Diagnostics cleared");
  } else if (what == "ovr") {
    // Grbl override code: 144 feed 100 %, 145 +10, 146 -10, 147 +1, 148 -1, 149 rapids 100 %, 150 50 %, 151 25 %
    int code = server.arg("code").toInt();
    if (code < 0x90 || code > 0x97) {
      server.send(400, "text/plain", "Bad override");
      return;
    }
    cncOverrideCommand(code);
    server.send(200, "text/plain", "Feed " + String(cncFeedOverrideWanted) + " %, rapids " + String(cncRapidOverrideWanted) + " %");
  } else if (what == "home") {
    if (emergencyStop != ESTOP_NONE) { // nothing moves after an emergency stop (the web server keeps answering since 1.38)
      server.send(409, "text/plain", "Emergency stop: restart the controller.");
      return;
    }
    if (!homingConfigured()) {
      server.send(409, "text/plain", "No home switch is set up (Machine settings).");
      return;
    }
    if (refuseWhileRunning()) return;
    homingRequest = true;
    server.send(200, "text/plain", "Home All");
  } else if (what == "homecheck") {
    if (emergencyStop != ESTOP_NONE) {
      server.send(409, "text/plain", "Emergency stop: restart the controller.");
      return;
    }
    if (!homingConfigured()) {
      server.send(409, "text/plain", "No home switch is set up (Machine settings).");
      return;
    }
    if (refuseWhileRunning()) return;
    if (homingActive || homingRequest) {
      server.send(409, "text/plain", "Home All or a home check is running.");
      return;
    }
    homeCheckRequest = true;
    server.send(200, "text/plain", "Home check");
  } else if (what == "pause") {
    if (mode != MODE_GCODE || !isOn) {
      server.send(409, "text/plain", "Nothing runs.");
      return;
    }
    cncUserHold = true;
    server.send(200, "text/plain", "Pausing");
  } else if (what == "chuckzero") {
    // the app's chuck angle 0 here: display only, any time (the lathe modes do not use it); saved as the H4's positions are
    chuckAngleZero = spindlePosGlobal;
    keypadTimeUs = micros();  // saved a few seconds later, as after a key press
    server.send(200, "text/plain", "Chuck angle 0 here");
  } else if (what == "resume") {
    if (cncPaused) cncResumeRequest = true; // M0 / M1 / M6
    cncUserHold = false;
    server.send(200, "text/plain", "Resuming");
  } else if (what == "start") {
    if (refuseWhileRunning()) return;
    if (emergencyStop != ESTOP_NONE) {
      server.send(409, "text/plain", "Emergency stop: restart the controller.");
      return;
    }
    String name = server.arg("name");
    String error = "";
    wifiFsBusy = true;
    bool loaded = validFileName(name) && loadProgram(name, &error);
    wifiFsBusy = false;
    if (!loaded) {
      server.send(400, "text/plain", error.length() ? error : "Bad name");
      return;
    }
    if (mode != MODE_GCODE) setModeFromTask(MODE_GCODE);
    // wait for loop() to switch the mode (it turns isOn off when it does)
    unsigned long t0 = millis();
    while (mode != MODE_GCODE && millis() - t0 < 500) vTaskDelay(5);
    if (mode != MODE_GCODE) {
      server.send(500, "text/plain", "Could not switch to GCODE mode.");
      return;
    }
    gcodeProgramCharIndex = 0;
    gcodeProgramData = webProgram;
    gcodeProgramSize = webProgramSize;
    cncUserHold = false;
    cncRunFromRequest = server.hasArg("from") ? (uint32_t)max(0L, server.arg("from").toInt()) : 0; // start from line N
    // the whole program is checked first (nothing moves), then the machine turns on (h4.ino cncRunPreflight)
    cncRequestPreflight(webProgram, webProgramSize, name.c_str());
    server.send(200, "text/plain", "Checking " + name + " before it starts (nothing moves) ...");
  } else {
    server.send(400, "text/plain", "Unknown command");
  }
}

// Hands one key event to the keypad task (the same path as the keycodes sent over USB).
bool sendKeyEvent(int event) {
  unsigned long t0 = millis();
  while (serialKeycode != 0 && millis() - t0 < 300) vTaskDelay(2);
  if (serialKeycode != 0) return false;
  serialKeycode = event;
  return true;
}

// ------------------------------------------------------------------ Lathe screen

// A number from the page (1.42): the whole text must be a finite number between lo and hi. Arduino's toFloat() reads
// "abc" as 0 (a stop at 0, a move to 0) and lets "nan" / "inf" / "1e40" through, which lround() turned into the
// smallest long (labs() of it is negative, so a pitch range check passed). Found by fuzzing the API (test/h4sim).
bool webNumber(const String& text, double lo, double hi, double* out) {
  String t = text;
  t.trim();
  if (t.length() == 0) return false;
  char* end;
  double v = strtod(t.c_str(), &end);
  if (*end != '\0' || !isfinite(v) || v < lo || v > hi) return false;
  *out = v;
  return true;
}
#define WEB_POS_MAX 10000.0  // mm (or inch): far beyond any lathe travel, still well inside a long in steps

// A value as the H4 displays it (mm, or inch in the inch modes) -> the axis position in steps.
long latheDisplayToPos(Axis* a, float v) {
  double du = v * (measure == MEASURE_METRIC ? 10000.0 : 254000.0);
  return lround(du / a->screwPitch * a->motorSteps) - a->originPos;
}

// The Lathe screen sets the H4's own lathe values with the functions its keys use (setModeFromTask, setDupr,
// setStarts, setTurnPasses, setConeRatio, the stops, external / internal), and never while anything moves or
// the H4 is ON. "do=start" presses ON exactly as many times as the H4's setup steps need, through the same key
// queue as a real key, so the mode starts as it does from the keypad (and refuses as it does: missing stops).
// Ends the remaining manual move of X and Z where they are (only while the H4 is OFF).
void haltManualMoves() {
  stepToFinal(&x, x.pos);
  stepToFinal(&z, z.pos);
}

void handleLathe() {
  String what = server.arg("do");
  // How the lathe modes show Z and X, on the H4's display and on this page (latheview.ino). Saved like the
  // H4's other values (when nothing moves). Only the display changes, never the motion.
  if (what == "view") {
    int zv = server.arg("z").toInt(), xv = server.arg("x").toInt();
    if (server.arg("z") == "" || server.arg("x") == "" || !latheViewValid(zv, xv)) {
      server.send(400, "text/plain", "Bad view");
      return;
    }
    latheZView = zv;
    latheXView = xv;
    server.send(200, "text/plain", "View set");
    return;
  }
  // Zero on the part, as the H4's keys do: Z0 / X0 here (keys Z / X), or X0 on the centre line from a measured
  // diameter (numpad + key A: the outside then reads -Ø/2). Only while the H4 is OFF and nothing moves.
  if (what == "z0" || what == "x0" || what == "dia") {
    if (isOn || cncBusy() || homingActive || mode == MODE_GCODE) {
      server.send(409, "text/plain", "The H4 is ON, moving or in GCODE mode.");
      return;
    }
    if (what == "z0") markAxis0(&z);
    else if (what == "x0") markAxis0(&x);
    else {
      double d = 0;
      if (!webNumber(server.arg("d"), 1e-6, 9999.999, &d)) {
        server.send(400, "text/plain", "Bad diameter");
        return;
      }
      long du = lround(d * (measure == MEASURE_METRIC ? 10000.0 : 254000.0));
      long pos = x.pos + du / x.screwPitch * x.motorSteps;
      x.originPos = -(x.pos + pos) / 2;
    }
    server.send(200, "text/plain", "Zero set");
    return;
  }
  // The page's safe approach and retract (a clearance away from the part, as a CAM's retract height): one axis
  // moves to a place in displayed units at the manual-move speed, as a numpad move of the H4 keys does, but not
  // held back by the lathe stops (they belong to the operation) - only by the machine's soft limits once homed.
  // The page waits for it to arrive before the next one; STOP (or "halt") stops it at once.
  if (what == "goto") {
    if (isOn || cncBusy() || homingActive || mode == MODE_GCODE || emergencyStop != ESTOP_NONE) {
      server.send(409, "text/plain", "The H4 is ON, moving or in GCODE mode.");
      return;
    }
    Axis* a = server.arg("axis") == "x" ? &x : (server.arg("axis") == "z" ? &z : NULL);
    if (a == NULL || !server.hasArg("v")) {
      server.send(400, "text/plain", "axis x or z and v");
      return;
    }
    double v = 0;
    if (!webNumber(server.arg("v"), -WEB_POS_MAX, WEB_POS_MAX, &v)) {
      server.send(400, "text/plain", "Bad position");
      return;
    }
    long target = latheDisplayToPos(a, v);
    long lo, hi;
    if (softLimitSteps(a, &lo, &hi) && (target < lo || target > hi)) { // homing.ino
      server.send(409, "text/plain", "Outside the machine's limits.");
      return;
    }
    if (labs(target - a->pos) > a->estopSteps) {
      server.send(409, "text/plain", "Too far for one move.");
      return;
    }
    a->speedMax = a->speedManualMove;
    if (!stepToFinal(a, target)) {
      server.send(503, "text/plain", "Busy");
      return;
    }
    server.send(200, "text/plain", "Moving");
    return;
  }
  if (what == "halt") {
    if (!isOn) haltManualMoves();
    server.send(200, "text/plain", "Stopped");
    return;
  }
  if (what == "start") {
    if (isOn || cncBusy() || homingActive || mode == MODE_GCODE || emergencyStop != ESTOP_NONE) {
      server.send(409, "text/plain", "The H4 is ON, moving or in GCODE mode.");
      return;
    }
    int presses = getLastSetupIndex() - setupIndex + 1;
    for (int i = 0; i < presses; i++) {
      if (!sendKeyEvent(B_ON | 128) || !sendKeyEvent(B_ON)) {
        server.send(503, "text/plain", "Busy");
        return;
      }
    }
    server.send(200, "text/plain", "Starting");
    return;
  }
  if (refuseWhileRunning()) return;
  // every number checked before anything is applied: a bad one changes nothing (400)
  double pitchIn = 0, coneIn = 0, stopIn[4] = {0, 0, 0, 0};
  const char* stopKeys[4] = {"zl", "zr", "xl", "xr"};
  if (server.hasArg("pitch") && !webNumber(server.arg("pitch"), -1000, 1000, &pitchIn)) {  // then lround fits a long; DUPR_MAX below
    server.send(400, "text/plain", "Pitch out of range");
    return;
  }
  if (server.hasArg("cone") && !webNumber(server.arg("cone"), -1e6, 1e6, &coneIn)) {
    server.send(400, "text/plain", "Bad cone ratio");
    return;
  }
  for (int i = 0; i < 4; i++) {
    if (server.hasArg(stopKeys[i]) && server.arg(stopKeys[i]).length() > 0 &&
        !webNumber(server.arg(stopKeys[i]), -WEB_POS_MAX, WEB_POS_MAX, &stopIn[i])) {
      server.send(400, "text/plain", String("Bad stop ") + stopKeys[i]);
      return;
    }
  }
  if (server.hasArg("mode")) {
    int m = server.arg("mode").toInt();
    if (m != MODE_NORMAL && (m < MODE_ASYNC || m > MODE_ELLIPSE)) {
      server.send(400, "text/plain", "Bad mode");
      return;
    }
    setModeFromTask(m);
  }
  if (server.hasArg("pitch")) {
    long du = lround(pitchIn * (measure == MEASURE_METRIC ? 10000.0 : 254000.0));
    if (labs(du) > DUPR_MAX) {
      server.send(400, "text/plain", "Pitch out of range");
      return;
    }
    setDupr(du);
  }
  if (server.hasArg("flank")) threadFlank = server.arg("flank").toInt() == 1;
  if (server.hasArg("starts")) setStarts(constrain(server.arg("starts").toInt(), 1, (int)STARTS_MAX));
  if (server.hasArg("passes")) setTurnPasses(constrain(server.arg("passes").toInt(), 1, (int)PASSES_MAX));
  if (server.hasArg("cone")) {
    float c = coneIn;
    if (!(c > 0 && c < 100)) {
      server.send(400, "text/plain", "Bad cone ratio");
      return;
    }
    setConeRatio(c);
  }
  if (server.hasArg("aux")) auxForward = server.arg("aux") == "1";
  // A new pitch, starts, cone ratio or mode is applied by loop(), and applying it moves the axes' position origin
  // (markOrigin): the stops are worked out only after that, in the frame they will be used in.
  unsigned long t0 = millis();
  while ((nextDuprFlag || nextStartsFlag || nextConeRatioFlag || nextModeFlag) && millis() - t0 < 1000) vTaskDelay(2);
  if (nextDuprFlag || nextStartsFlag || nextConeRatioFlag || nextModeFlag) {
    server.send(503, "text/plain", "The H4 did not take the values in time: try again.");
    return;
  }
  // stops: a value in displayed units, or empty to clear the stop
  struct { const char* k; Axis* a; bool left; } stops[] = {{"zl", &z, true}, {"zr", &z, false}, {"xl", &x, true}, {"xr", &x, false}};
  for (int i = 0; i < 4; i++) {
    auto& st = stops[i];
    if (!server.hasArg(st.k)) continue;
    bool clear = server.arg(st.k).length() == 0;
    if (st.left) setLeftStop(st.a, clear ? LONG_MAX : latheDisplayToPos(st.a, stopIn[i]));
    else setRightStop(st.a, clear ? LONG_MIN : latheDisplayToPos(st.a, stopIn[i]));
  }
  server.send(200, "text/plain", "ok");
}

void releaseHeldKey() {
  if (heldKey == 0) return;
  int code = heldKey;
  heldKey = 0;
  // The release must arrive: retry while the keypad task is busy (each try waits up to 300 ms).
  for (int i = 0; i < 10 && !sendKeyEvent(code); i++) vTaskDelay(5);
}

// Key on the page. hold=1: pressed and kept down (repeat every 150 ms); hold=0: released;
// no hold: one press and release.
void handleKey() {
  int code = server.arg("code").toInt();
  if (code <= 0 || code > 127) {
    server.send(400, "text/plain", "Bad key");
    return;
  }
  String hold = server.arg("hold");
  if (hold == "1") {
    if (heldKey == code) {
      heldKeyMs = millis(); // still held
    } else {
      releaseHeldKey();
      if (!sendKeyEvent(code | 128)) {
        server.send(503, "text/plain", "Busy");
        return;
      }
      heldKey = code;
      heldKeyMs = millis();
    }
  } else if (hold == "0") {
    if (heldKey == code) releaseHeldKey();
  } else {
    releaseHeldKey();
    if (!sendKeyEvent(code | 128) || !sendKeyEvent(code)) {
      server.send(503, "text/plain", "Busy");
      return;
    }
  }
  server.send(200, "text/plain", "ok");
}

// Gamepad of the page: wanted speeds x, z (mm/min) and c (deg/min), repeated about every 100 ms while
// moving; 0 0 0 (or no request for 0.3 s) stops. 409 with the reason when the H4 refused a jog.
void handleJog() {
  if (emergencyStop != ESTOP_NONE) {
    server.send(409, "text/plain", "Emergency stop: restart the controller.");
    return;
  }
  float v[3];
  const char* names[3] = {"x", "z", "c"};
  for (int i = 0; i < 3; i++) {
    v[i] = server.arg(names[i]).toFloat();
    if (!isfinite(v[i])) v[i] = 0;
    v[i] = constrain(v[i], -100000.0f, 100000.0f);
  }
  if (!webJogRequest(v[0], v[1], v[2])) {
    server.send(409, "text/plain", webJogError);
    return;
  }
  server.send(200, "text/plain", "ok");
}

// The connection as stored and as it is now. "mode" is the stored choice (direct = the H4's own network, router);
// "setupAp" says the own network is on now (direct chosen, or the router was not found at the start). The own
// network's password is given to the app (it is on the local network only and shown on the H4 display anyway), so
// the app can join that network by itself.
String wifiInfoJson() {
  bool sta = !wifiSetupAp && WiFi.status() == WL_CONNECTED;
  return String("{\"mode\":\"") + (wifiOwnNetwork || wifiSsid.length() == 0 ? "direct" : "router") + "\",\"ssid\":\"" + jsonEscape(wifiSsid) +
      "\",\"passwordSet\":" + String(wifiPassword.length() ? 1 : 0) + ",\"setupAp\":" + String(wifiSetupAp ? 1 : 0) +
      ",\"ip\":\"" + wifiIp + "\",\"apName\":\"" + WIFI_SETUP_AP_SSID + "\",\"apPassword\":\"" + jsonEscape(wifiApPassword) +
      "\",\"rssi\":" + String(sta ? (int)WiFi.RSSI() : 0) + ",\"fw\":\"" + FW_VERSION + "\"}";
}

void handleWifiGet() {
  server.send(200, "application/json", wifiInfoJson());
}

// The networks the H4 sees (for choosing the router in the app), strongest first, each name once. Takes 2-4 s.
void handleWifiScan() {
  if (refuseWhileRunning()) return;
  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;
  String json = "[";
  int shown = 0;
  bool used[64] = {false};
  if (n > 64) n = 64;
  for (int k = 0; k < n && shown < 30; k++) {
    int best = -1;
    for (int i = 0; i < n; i++) if (!used[i] && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) best = i;
    if (best < 0) break;
    used[best] = true;
    String name = WiFi.SSID(best);
    if (name.length() == 0 || name == WIFI_SETUP_AP_SSID) continue;
    bool seen = false;
    for (int i = 0; i < n; i++) if (used[i] && i != best && WiFi.SSID(i) == name) seen = true; // a stronger one was listed
    if (seen) continue;
    if (shown++) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(name) + "\",\"rssi\":" + String((int)WiFi.RSSI(best)) + ",\"lock\":" + String(WiFi.encryptionType(best) != 0 ? 1 : 0) + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleWifiSave() {
  if (refuseWhileRunning()) return;
  // mode=direct: the H4's own network at every start (the stored router is kept, for the SETTINGS key and the app);
  // mode=router: join ssid / password. Without mode (older pages): an empty network name = direct.
  String mode = server.arg("mode");
  String ssid = server.arg("ssid");
  ssid.trim();
  String password = server.arg("password");
  String apPassword = server.arg("appw"); // a new password for the own network (empty = keep)
  bool ownNetwork = mode.length() ? mode == "direct" : ssid.length() == 0;
  if (mode.length() && mode != "direct" && mode != "router") {
    server.send(400, "text/plain", "mode: direct or router.");
    return;
  }
  if (!ownNetwork && ssid.length() == 0) {
    server.send(400, "text/plain", "Choose the router's network.");
    return;
  }
  if (ssid.length() > 32 || password.length() > 63 || (!ownNetwork && password.length() > 0 && password.length() < 8)) {
    server.send(400, "text/plain", "Network name 1-32 characters, password empty or 8-63 characters.");
    return;
  }
  if (apPassword.length() > 0 && (apPassword.length() < 8 || apPassword.length() > 63)) {
    server.send(400, "text/plain", "The H4 network's password: 8-63 characters.");
    return;
  }
  wifiFsBusy = true;
  Preferences cfg;
  cfg.begin(WIFI_CONFIG_NAMESPACE);
  cfg.putBool("own", ownNetwork);
  if (!ownNetwork) {
    cfg.putString("ssid", ssid);
    if (password.length() > 0 || server.arg("clear") == "1") cfg.putString("password", password);
  }
  if (apPassword.length() > 0) cfg.putString("appw", apPassword);
  cfg.end();
  wifiFsBusy = false;
  wifiRestartPending = true;
  wifiRestartAt = millis() + 1500;
  if (apPassword.length() > 0) wifiApPassword = apPassword;
  if (ownNetwork) server.send(200, "text/plain", String("Saved. The H4 restarts with its own network ") + WIFI_SETUP_AP_SSID + " (password " + wifiApPassword + "), address 192.168.4.1.");
  else server.send(200, "text/plain", "Saved. The H4 restarts and joins " + ssid + ".");
}

void handleFirmwareData() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    fwUploadFailed = false;
    fwUploadMessage = "";
    if (cncRunActive() || isOn) {
      fwUploadFailed = true;
      fwUploadMessage = "Turn the machine off first.";
      return;
    }
    String name = upload.filename;
    name.toLowerCase();
    if (!name.endsWith(".bin")) {
      fwUploadFailed = true;
      fwUploadMessage = "Only .bin firmware files.";
      return;
    }
    wifiFsBusy = true;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      wifiFsBusy = false;
      fwUploadFailed = true;
      fwUploadMessage = String("Cannot start the update: ") + Update.errorString();
      return;
    }
    fwUploadActive = true;
    fwUploadChecked = false;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fwUploadFailed || !fwUploadActive) return;
    if (!fwUploadChecked) {
      // an application starts with the image header (magic 0xE9) and, at byte 32, its description (magic
      // 0xABCD5432); a full flash image for USB (it starts with the bootloader) or any other file is refused
      fwUploadChecked = true;
      const uint8_t* b = upload.buf;
      if (upload.currentSize < 36 || b[0] != 0xE9 || b[32] != 0x32 || b[33] != 0x54 || b[34] != 0xCD || b[35] != 0xAB) {
        fwUploadFailed = true;
        fwUploadMessage = "Not a firmware update file (the ...-usb file is only for the first installation over USB).";
        Update.abort();
        fwUploadActive = false;
        wifiFsBusy = false;
        return;
      }
    }
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      fwUploadFailed = true;
      fwUploadMessage = String("Write failed: ") + Update.errorString();
      Update.abort();
      fwUploadActive = false;
      wifiFsBusy = false;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fwUploadActive && !fwUploadFailed) {
      if (Update.end(true)) {
        fwUploadMessage = "Firmware updated. Restarting...";
        wifiRestartPending = true;
        wifiRestartAt = millis() + 1500;
      } else {
        fwUploadFailed = true;
        fwUploadMessage = String("Update failed: ") + Update.errorString();
      }
    }
    fwUploadActive = false;
    wifiFsBusy = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (fwUploadActive) Update.abort();
    fwUploadActive = false;
    wifiFsBusy = false;
    fwUploadFailed = true;
    fwUploadMessage = "Upload interrupted.";
  }
}

void handleFirmwareDone() {
  server.send(fwUploadFailed ? 400 : 200, "text/plain", fwUploadMessage);
}

// Machine settings (config.ino).
void handleConfigGet() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", machineConfigJson());
}

void handleConfigSave() {
  if (refuseWhileRunning()) return;
  if (isOn) {
    server.send(409, "text/plain", "Turn the machine off first.");
    return;
  }
  bool reset = server.arg("reset") == "1";
  double values[MACHINE_CONFIG_COUNT];
  for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) {
    const MachineConfigItem& it = MACHINE_CONFIG[i];
    values[i] = reset ? machineConfigDefaults[i] : configGet(i);
    if (reset || !server.hasArg(it.key)) continue;
    String text = server.arg(it.key);
    text.trim();
    double v;
    if (it.type == 'a') {
      v = text.length() == 1 ? (double)toupper(text.charAt(0)) : NAN;
    } else {
      char* end;
      v = strtod(text.c_str(), &end);
      if (text.length() == 0 || *end != '\0') v = NAN;
    }
    if (!configValid(i, v)) {
      server.send(400, "text/plain", String(it.group) + ", " + it.label + ": value not allowed (" + text + ")");
      return;
    }
    values[i] = v;
  }
  String problem = configCrossCheck(values);
  if (problem.length() > 0) {
    server.send(400, "text/plain", problem);
    return;
  }
  bool scaleChanged = configScaleChanged(values);
  wifiFsBusy = true;
  Preferences cfg;
  bool ok = cfg.begin(MACHINE_CONFIG_NAMESPACE, false);
  if (ok) {
    cfg.clear();
    if (!reset) {
      cfg.putInt("v", MACHINE_CONFIG_VERSION);
      for (int i = 0; i < MACHINE_CONFIG_COUNT; i++) {
        // every value, not only the changed ones: a firmware update with other defaults keeps this machine's values
        ok = ok && cfg.putDouble(MACHINE_CONFIG[i].key, values[i]) > 0;
      }
    }
    cfg.end();
  }
  if (ok && scaleChanged) {
    Preferences pref;
    if (pref.begin(PREF_NAMESPACE, false)) {
      pref.clear();
      pref.end();
    }
  }
  wifiFsBusy = false;
  if (!ok) {
    server.send(500, "text/plain", "Could not save the settings.");
    return;
  }
  wifiRestartPending = true;
  wifiRestartAt = millis() + 1500;
  server.send(200, "text/plain", String(reset ? "Defaults restored." : "Saved.") +
      (scaleChanged ? " The axis scale changed: positions, stops and the other saved H4 values were cleared." : "") +
      " The controller restarts now.");
}

// ------------------------------------------------------------------ WiFi

// Four characters for the LCD (h4.ino, line 2, right side): "W 62" joined to a router with signal quality 62 %,
// "AP1" its own network with 1 device connected, "W.." joining, "W--" no connection. Refreshed every 2 s, the
// quality in steps of 5 so the number does not flicker.
const char* wifiLcdTag() {
  static char tag[5] = "";
  static unsigned long lastMs = 0;
  if (!WIFI_ENABLED) return "";
  if (tag[0] != 0 && millis() - lastMs < 2000) return tag;
  lastMs = millis();
  wifi_mode_t m = WiFi.getMode();
  if (m == WIFI_AP || m == WIFI_AP_STA) {
    snprintf(tag, sizeof(tag), "AP%d", constrain((int)WiFi.softAPgetStationNum(), 0, 9));
  } else if (WiFi.status() == WL_CONNECTED) {
    int q = constrain(2 * (WiFi.RSSI() + 100), 0, 100); // -50 dBm and better = 100 %, -100 dBm = 0 %
    q = (q + 2) / 5 * 5;
    snprintf(tag, sizeof(tag), "W%3d", q);
  } else if (m == WIFI_STA) {
    snprintf(tag, sizeof(tag), "W..");
  } else {
    snprintf(tag, sizeof(tag), "W--");
  }
  return tag;
}

bool wifiJoin() {
  if (wifiSsid.length() == 0 || wifiOwnNetwork) return false; // no router stored, or the H4's own network is chosen (SETTINGS key)
  WiFi.persistent(false); // no flash writes by the WiFi driver
  WiFi.setHostname("h4"); // its name in the router's list of devices
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // steady answers to the page
  // DIAG: what the board sees and why a join fails (no password is printed)
  Serial.printf("[wifi-diag] saved SSID=\"%s\" (len %d), password len %d\n", wifiSsid.c_str(), (int)wifiSsid.length(), (int)wifiPassword.length());
  WiFi.onEvent([](arduino_event_id_t ev, arduino_event_info_t info) {
    if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) Serial.printf("[wifi-diag] disconnected, reason=%d (%s)\n", info.wifi_sta_disconnected.reason, WiFi.disconnectReasonName((wifi_err_reason_t)info.wifi_sta_disconnected.reason));
    else if (ev == ARDUINO_EVENT_WIFI_STA_CONNECTED) Serial.println("[wifi-diag] associated with the access point");
    else if (ev == ARDUINO_EVENT_WIFI_STA_GOT_IP) Serial.println("[wifi-diag] got IP");
  });
  int found = WiFi.scanNetworks();
  Serial.printf("[wifi-diag] scan: %d networks\n", found);
  for (int i = 0; i < found; i++) {
    Serial.printf("[wifi-diag]  %2d \"%s\" rssi=%d ch=%d auth=%d%s\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), (int)WiFi.encryptionType(i), WiFi.SSID(i) == wifiSsid ? "  <== saved" : "");
  }
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiStatusText = "Joining " + wifiSsid;
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) vTaskDelay(500 / portTICK_PERIOD_MS);
  Serial.printf("[wifi-diag] join end, WiFi.status()=%d\n", (int)WiFi.status());
  if (WiFi.status() != WL_CONNECTED) return false;
  wifiIp = WiFi.localIP().toString();
  wifiStatusText = "http://" + wifiIp;
  wifiShow(("IP " + wifiIp).c_str(), NULL, NULL, 30000);
  Serial.println("Web page: http://" + wifiIp);
  return true;
}

bool wifiSetupAccessPoint() {
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  wifiSetupAp = WiFi.softAP(WIFI_SETUP_AP_SSID, wifiApPassword.c_str());
  if (!wifiSetupAp) {
    wifiStatusText = "WiFi failed";
    return false;
  }
  wifiIp = WiFi.softAPIP().toString();
  wifiStatusText = String("Setup: ") + WIFI_SETUP_AP_SSID + " http://" + wifiIp;
  wifiShow((String("WiFi ") + WIFI_SETUP_AP_SSID).c_str(), ("Password " + wifiApPassword).c_str(), ("Address " + wifiIp).c_str(), 60000);
  Serial.println(wifiStatusText);
  return true;
}

// An "H4CNC?" from the app (UDP broadcast) is answered to its sender with "H4CNC " and the connection (wifiInfoJson).
void discoveryLoop() {
  if (!discoveryStarted) return;
  int n = discoveryUdp.parsePacket();
  if (n <= 0) return;
  char b[16];
  int len = discoveryUdp.read((uint8_t*)b, sizeof(b) - 1);
  if (len < 0) len = 0;
  b[len] = 0;
  if (strncmp(b, "H4CNC?", 6) != 0) return;
  String answer = "H4CNC " + wifiInfoJson();
  discoveryUdp.beginPacket(discoveryUdp.remoteIP(), discoveryUdp.remotePort());
  discoveryUdp.write((const uint8_t*)answer.c_str(), answer.length());
  discoveryUdp.endPacket();
}

// Texts for the H4 display (h4.ino shows them in turn, 3 s each).
void wifiShow(const char* a, const char* b, const char* c, unsigned long forMs) {
  wifiDisplayCount = 0;
  snprintf(wifiDisplayText[0], 21, "%s", a);
  if (b) snprintf(wifiDisplayText[1], 21, "%s", b);
  if (c) snprintf(wifiDisplayText[2], 21, "%s", c);
  wifiDisplayUntil = millis() + forMs;
  wifiDisplayCount = c ? 3 : (b ? 2 : 1);
}

void taskWifi(void* param) {
  Preferences cfg;
  cfg.begin(WIFI_CONFIG_NAMESPACE, true);
  wifiSsid = cfg.getString("ssid", "");
  wifiPassword = cfg.getString("password", "");
  wifiOwnNetwork = cfg.getBool("own", false);
  wifiApPassword = cfg.getString("appw", WIFI_AP_DEFAULT_PASSWORD);
  if (wifiApPassword.length() < 8 || wifiApPassword.length() > 63) wifiApPassword = WIFI_AP_DEFAULT_PASSWORD;
  cfg.end();
  if (!LittleFS.begin(true)) {
    Serial.println("error: LittleFS not available - check the partition scheme (partitions.csv, Flash Size 16MB)");
  } else {
    LittleFS.mkdir("/gcode");
    updateFsUsage();
    eventsFsReady = true; // the lasting log may be written now (safety.ino)
  }
  if (!wifiJoin() && !wifiSetupAccessPoint()) {
    vTaskDelete(NULL);
    return;
  }
  server.on("/", HTTP_GET, handlePage);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/files", HTTP_GET, handleFileList);
  server.on("/file", HTTP_GET, handleFileGet);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
  server.on("/cmd", HTTP_POST, handleCommand);
  server.on("/key", HTTP_POST, handleKey);
  server.on("/jog", HTTP_POST, handleJog);
  server.on("/wifi", HTTP_GET, handleWifiGet);
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/firmware", HTTP_POST, handleFirmwareDone, handleFirmwareData);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/config", HTTP_POST, handleConfigSave);
  server.on("/mdi", HTTP_POST, handleMdi);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/events", HTTP_GET, []() { server.send(200, "text/plain; charset=utf-8", eventsText()); }); // lasting log (safety.ino)
  server.on("/lathe", HTTP_POST, handleLathe);
  server.begin();
  discoveryStarted = discoveryUdp.begin(WIFI_DISCOVERY_PORT);
  // Also after an emergency stop: the page must still show it and why (/status "estop"), the lasting log must be
  // readable and the settings / firmware pages usable. Every motion request refuses itself then ("Emergency stop:
  // restart the controller"), setEmergencyStop() holds the axes and stops the GCODE steps. Until 1.38 this loop ended
  // there and the H4 answered nothing: the app showed "NO CONNECTION" (found with test/h4sim).
  for (;;) {
    server.handleClient();
    grblTcpLoop(); // Grbl senders on TCP port 23 (grbl.ino)
    discoveryLoop(); // the app looking for the H4
    if (heldKey != 0 && millis() - heldKeyMs > HELD_KEY_TIMEOUT_MS) releaseHeldKey(); // fail-safe
    if (wifiToggleRequest) { // the SETTINGS key on the H4: own network <-> router
      wifiToggleRequest = false;
      if (!motionActive() && !wifiRestartPending) {
        bool own = !wifiOwnNetwork;
        if (!own && wifiSsid.length() == 0) { // no router stored yet: nothing to switch to
          wifiShow("No router stored", "Choose it in the app", "Settings, Connection", 4000);
        } else {
          wifiFsBusy = true;
          Preferences cfg;
          cfg.begin(WIFI_CONFIG_NAMESPACE);
          cfg.putBool("own", own);
          cfg.end();
          wifiFsBusy = false;
          wifiOwnNetwork = own;
          wifiShow(own ? "WiFi: own network" : "WiFi: router", "Restarting...", NULL, 3000);
          wifiRestartPending = true;
          wifiRestartAt = millis() + 1500;
        }
      }
    }
    if (wifiRestartPending && (long)(millis() - wifiRestartAt) >= 0 && !motionActive()) ESP.restart();
    bool lost = !wifiSetupAp && WiFi.status() != WL_CONNECTED;
    if (lost && wifiStatusText != "WiFi lost") {
      wifiStatusText = "WiFi lost";
      wifiShow("WiFi lost", "SETTINGS key:", (String("own network ") + WIFI_SETUP_AP_SSID).c_str(), 0);
    } else if (!lost && !wifiSetupAp && wifiStatusText != "http://" + wifiIp) {
      wifiStatusText = "http://" + wifiIp;
      wifiShow(("IP " + wifiIp).c_str(), NULL, NULL, 0);
    }
    vTaskDelay(2);
  }
}
