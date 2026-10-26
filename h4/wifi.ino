// wifi.ino - WiFi web page of the NanoEls H4 CNC fork: G-code files kept on the controller, live status,
// Start / Pause / Resume / Stop, the H4 keys, WiFi settings and firmware update.
// https://github.com/Marioskiv/nanoels
//
// WiFi set-up (setup access point, credentials in Preferences) and the firmware upload follow the
// NanoEls H5 firmware (MIT, Copyright (c) Maxim Kachurovskiy). The file storage is different from H5:
// files are written to and read from flash in pieces, so a program can be much larger than the RAM,
// and it is loaded into RAM (PSRAM when the board has it) before it starts.
//
// RULE: no flash access while a G-code program runs. A flash read or write stops the CPU cache and with
// it the step interrupt for up to milliseconds, which would make the motors stutter. Every handler that
// touches LittleFS, Preferences or the firmware partition is refused while cncRunActive().
//
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)

#include "webpage.h"

#define WIFI_CONFIG_NAMESPACE "h4wifi"
const char* WIFI_SETUP_AP_SSID = "NanoEls-H4-Setup";
const char* WIFI_SETUP_AP_PASSWORD = "nanoels-h4";
const size_t PROGRAM_MAX_RAM_NO_PSRAM = 120000; // without PSRAM a program must fit in the normal RAM

WebServer server(80);
String wifiSsid = "";
String wifiPassword = "";
String wifiStatusText = "WiFi off";
String wifiIp = "";
bool wifiSetupAp = false;
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
bool fwUploadFailed = false;
String fwUploadMessage = "";

// Cached free space: LittleFS usedBytes() reads the flash, so it is only measured when nothing runs.
size_t fsTotalBytes = 0;
size_t fsUsedBytes = 0;

bool cncRunActive() {
  return mode == MODE_GCODE && (isOn || cncBusy() || cncStopRequest);
}

bool refuseWhileRunning() {
  if (!cncRunActive()) return false;
  server.send(409, "text/plain", "The machine is running a program. Stop it first.");
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

void handleStatus() {
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
  json += ",\"inch\":" + String(measure == MEASURE_METRIC ? 0 : 1);
  json += ",\"feed\":" + String(cnc_st_realtime_rate(), 1);
  json += ",\"rpm\":" + String(getApproxRpm());
  json += ",\"line\":\"" + jsonEscape(String(gcodeDisplayLine)) + "\"";
  json += ",\"progress\":" + String(progress, 4);
  json += ",\"program\":\"" + jsonEscape(webProgramName) + "\"";
  json += ",\"programLoaded\":" + String(webProgram != NULL && gcodeProgramData == webProgram ? 1 : 0);
  json += ",\"underruns\":" + String(cnc_st_underruns());
  json += ",\"estop\":" + String(emergencyStop);
  json += ",\"running\":" + String(cncRunActive() ? 1 : 0);
  json += ",\"fsTotal\":" + String((unsigned long)fsTotalBytes);
  json += ",\"fsUsed\":" + String((unsigned long)fsUsedBytes);
  json += ",\"psram\":" + String((unsigned long)(psramFound() ? ESP.getFreePsram() : 0));
  json += ",\"heap\":" + String((unsigned long)ESP.getFreeHeap());
  json += ",\"wifi\":\"" + jsonEscape(wifiStatusText) + "\"";
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
    if (cncRunActive()) {
      uploadFailed = true;
      uploadMessage = "The machine is running a program. Stop it first.";
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
    if (cncRunActive()) {
      uploadFailed = true;
      uploadMessage = "A program started during the upload.";
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
    if (uploadFailed && validFileName(name) && !cncRunActive()) LittleFS.remove(filePath(name));
    if (!uploadFailed) uploadMessage = "Saved " + name + " (" + String((unsigned long)uploadBytes) + " bytes)";
    if (!cncRunActive()) updateFsUsage();
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
    setIsOnFromTask(false); // decelerates along the path, then drops the rest (GCODE mode)
    cncUserHold = false;
    server.send(200, "text/plain", "Stopping");
  } else if (what == "pause") {
    if (mode != MODE_GCODE || !isOn) {
      server.send(409, "text/plain", "Nothing runs.");
      return;
    }
    cncUserHold = true;
    server.send(200, "text/plain", "Pausing");
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
    setIsOnFromTask(true);
    server.send(200, "text/plain", "Started " + name);
  } else {
    server.send(400, "text/plain", "Unknown command");
  }
}

// One H4 key: press, then release (the same path as the keycodes sent over USB).
void handleKey() {
  int code = server.arg("code").toInt();
  if (code <= 0 || code > 127) {
    server.send(400, "text/plain", "Bad key");
    return;
  }
  for (int pass = 0; pass < 2; pass++) {
    unsigned long t0 = millis();
    while (serialKeycode != 0 && millis() - t0 < 300) vTaskDelay(2);
    if (serialKeycode != 0) {
      server.send(503, "text/plain", "Busy");
      return;
    }
    serialKeycode = pass == 0 ? (code | 128) : code;
  }
  server.send(200, "text/plain", "ok");
}

void handleWifiGet() {
  String json = "{\"ssid\":\"" + jsonEscape(wifiSsid) + "\",\"passwordSet\":" + String(wifiPassword.length() ? 1 : 0) +
      ",\"setupAp\":" + String(wifiSetupAp ? 1 : 0) + ",\"ip\":\"" + wifiIp + "\",\"apName\":\"" + WIFI_SETUP_AP_SSID + "\"}";
  server.send(200, "application/json", json);
}

void handleWifiSave() {
  if (refuseWhileRunning()) return;
  String ssid = server.arg("ssid");
  ssid.trim();
  String password = server.arg("password");
  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63 || (password.length() > 0 && password.length() < 8)) {
    server.send(400, "text/plain", "Network name 1-32 characters, password empty or 8-63 characters.");
    return;
  }
  wifiFsBusy = true;
  Preferences cfg;
  cfg.begin(WIFI_CONFIG_NAMESPACE);
  cfg.putString("ssid", ssid);
  if (password.length() > 0 || server.arg("clear") == "1") cfg.putString("password", password);
  cfg.end();
  wifiFsBusy = false;
  wifiRestartPending = true;
  wifiRestartAt = millis() + 1500;
  server.send(200, "text/plain", "Saved. The controller restarts and joins " + ssid + ".");
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
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fwUploadFailed || !fwUploadActive) return;
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

// ------------------------------------------------------------------ WiFi

bool wifiJoin() {
  if (wifiSsid.length() == 0) return false;
  WiFi.persistent(false); // no flash writes by the WiFi driver
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // steady answers to the page
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiStatusText = "Joining " + wifiSsid;
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) vTaskDelay(500 / portTICK_PERIOD_MS);
  if (WiFi.status() != WL_CONNECTED) return false;
  wifiIp = WiFi.localIP().toString();
  wifiStatusText = "http://" + wifiIp;
  Serial.println("Web page: http://" + wifiIp);
  return true;
}

bool wifiSetupAccessPoint() {
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  wifiSetupAp = WiFi.softAP(WIFI_SETUP_AP_SSID, WIFI_SETUP_AP_PASSWORD);
  if (!wifiSetupAp) {
    wifiStatusText = "WiFi failed";
    return false;
  }
  wifiIp = WiFi.softAPIP().toString();
  wifiStatusText = String("Setup: ") + WIFI_SETUP_AP_SSID + " http://" + wifiIp;
  Serial.println(wifiStatusText);
  return true;
}

void taskWifi(void* param) {
  Preferences cfg;
  cfg.begin(WIFI_CONFIG_NAMESPACE, true);
  wifiSsid = cfg.getString("ssid", "");
  wifiPassword = cfg.getString("password", "");
  cfg.end();
  if (!LittleFS.begin(true)) {
    Serial.println("error: LittleFS not available - check the partition scheme (partitions.csv, Flash Size 16MB)");
  } else {
    LittleFS.mkdir("/gcode");
    updateFsUsage();
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
  server.on("/wifi", HTTP_GET, handleWifiGet);
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.on("/firmware", HTTP_POST, handleFirmwareDone, handleFirmwareData);
  server.begin();
  while (emergencyStop == ESTOP_NONE) {
    server.handleClient();
    if (wifiRestartPending && (long)(millis() - wifiRestartAt) >= 0 && !cncRunActive()) ESP.restart();
    if (!wifiSetupAp && WiFi.status() != WL_CONNECTED) {
      wifiStatusText = "WiFi lost";
    } else if (!wifiSetupAp) {
      wifiStatusText = "http://" + wifiIp;
    }
    vTaskDelay(2);
  }
  vTaskDelete(NULL);
}
