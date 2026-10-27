// grbl.ino - the Grbl 1.1 protocol for the GCODE mode of the NanoEls H4 CNC fork, so that the usual G-code
// senders (Universal G-code Sender, gSender, CNCjs, bCNC, ...) can drive the H4: over WiFi on TCP port 23
// (as the ESP32 Grbl versions do), and over USB when GRBL_ON_USB is set.
// https://github.com/Marioskiv/nanoels-cnc
//
// What a sender sees: the start message "Grbl 1.1h ['$' for help]", "ok" / "error:N" for every line,
// the real-time commands ? ! ~ Ctrl-X and jog cancel, the status report <State|WPos:...|FS:...>, $$ $# $G $I
// $N $X $H $J= and ALARM messages. The G-code itself is read by the same interpreter as files (cnc_gcode.cpp,
// RS274/NGC as LinuxCNC; Grbl's G-code is a subset of it). The protocol follows the public Grbl 1.1
// interface documents; no Grbl source code is used here.
//
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later (this fork)

#define GRBL_TCP_PORT 23
#define GRBL_LINE_MAX 256
#define GRBL_QUEUE_LINES 8
#define GRBL_CHANNELS 2 // 0 USB, 1 TCP

struct GrblLine {
  char text[GRBL_LINE_MAX];
  uint8_t channel;
};

WiFiServer grblServer(GRBL_TCP_PORT);
WiFiClient grblClient;
bool grblServerStarted = false;
SemaphoreHandle_t grblOutMutex = NULL;
QueueHandle_t grblQueue = NULL;
char grblLineBuf[GRBL_CHANNELS][GRBL_LINE_MAX];
int grblLineLen[GRBL_CHANNELS] = {0, 0};
bool grblLineOverflow[GRBL_CHANNELS] = {false, false};
bool grblInComment[GRBL_CHANNELS] = {false, false};
volatile int grblChannel = -1;          // channel of the current session (answers go there)
volatile bool grblSession = false;      // a sender's lines are running (isOn is ours)
volatile unsigned long grblLastMs = 0;
volatile bool grblResetRequest = false; // Ctrl-X
volatile bool grblJogging = false;
volatile int grblResetChannel = -1;
uint32_t grblFaultSeen = 0;
bool grblReportInches = false;          // $13

bool grblInit() {
  if (grblQueue == NULL) grblQueue = xQueueCreate(GRBL_QUEUE_LINES, sizeof(GrblLine));
  if (grblOutMutex == NULL) grblOutMutex = xSemaphoreCreateMutex();
  return grblQueue != NULL && grblOutMutex != NULL;
}

// ------------------------------------------------------------------ output

void grblWrite(int ch, const char* text) {
  if (ch < 0 || !grblInit()) return;
  if (xSemaphoreTake(grblOutMutex, 50) != pdTRUE) return;
  if (ch == 0) {
    Serial.print(text);
  } else if (grblClient && grblClient.connected()) {
    grblClient.print(text);
  }
  xSemaphoreGive(grblOutMutex);
}

void grblWriteLine(int ch, const String& text) {
  String t = text + "\r\n";
  grblWrite(ch, t.c_str());
}

void grblMessage(const char* msg) {
  if (grblChannel < 0) return;
  grblWriteLine(grblChannel, String("[MSG:") + msg + "]");
}

void grblBanner(int ch) {
  grblWrite(ch, "\r\nGrbl 1.1h ['$' for help]\r\n");
  grblWriteLine(ch, "[MSG:NanoEls H4 CNC - G-code as LinuxCNC; axes X, Z and C]");
}

// Grbl error numbers for the interpreter's messages (Grbl 1.1 interface, error codes).
int grblErrorCode(const char* m) {
  if (strstr(m, "same modal group")) return 21;
  if (strstr(m, "feed")) return 22;
  if (strstr(m, "arc") || strstr(m, "radius")) return 33;
  if (strstr(m, "outside the limits")) return 15;
  if (strstr(m, "two ") && strstr(m, "words")) return 25;
  if (strstr(m, "without a number") || strstr(m, "bad number")) return 2;
  if (strstr(m, "unexpected character")) return 1;
  if (strstr(m, "no ") && strstr(m, "axis")) return 20;
  return 20; // unsupported or invalid command
}

// ------------------------------------------------------------------ status report

float grblUnit(Axis* a) {
  return a->rotational || !grblReportInches ? 1.0 : 1 / 25.4;
}

void grblStatus(int ch) {
  const char* state = "Idle";
  bool held = cncPaused || cncUserHold || cncSpindleHold;
  if (homingActive || homingRequest) state = "Home";
  else if ((ALARM_USE && inAlarm) || cncFaultStop) state = "Alarm";
  else if (held) state = cnc_hold_complete() || cncPaused ? "Hold:0" : "Hold:1";
  else if (grblJogging && cncBusy()) state = "Jog";
  else if (cncBusy() || stepperIsRunning(&x) || stepperIsRunning(&z)) state = "Run";
  else if (mode == MODE_GCODE && cncStartRefusal() != NULL && strcmp(cncStartRefusal(), "Home All first") == 0) state = "Alarm";
  char buf[200];
  float wx = (x.pos + x.originPos) / cncStepsPerUnit(CNC_X) * grblUnit(&x);
  float wz = (z.pos + z.originPos) / cncStepsPerUnit(CNC_Z) * grblUnit(&z);
  int n = snprintf(buf, sizeof(buf), "<%s|WPos:%.3f,0.000,%.3f", state, wx, wz);
  if (a1.active) n += snprintf(buf + n, sizeof(buf) - n, ",%.3f", (a1.pos + a1.originPos) / cncStepsPerUnit(CNC_C));
  n += snprintf(buf + n, sizeof(buf) - n, "|Bf:%d,%d|FS:%.0f,%d", cnc_plan_blocks_free(),
      (int)(grblQueue ? uxQueueSpacesAvailable(grblQueue) : 0) * 16, cnc_st_realtime_rate() * grblUnit(&x), getApproxRpm());
  n += snprintf(buf + n, sizeof(buf) - n, "|Ov:%d,%d,100", cnc_feed_override(), cnc_rapid_override());
  bool pins = (HOME_X_USE && inHomeX) || (HOME_Z_USE && inHomeZ);
  if (pins) n += snprintf(buf + n, sizeof(buf) - n, "|Pn:%s%s", HOME_X_USE && inHomeX ? "X" : "", HOME_Z_USE && inHomeZ ? "Z" : "");
  snprintf(buf + n, sizeof(buf) - n, ">\r\n");
  grblWrite(ch, buf);
}

// ------------------------------------------------------------------ input (USB in taskGcode, TCP in taskWifi)

bool grblRoomFor(int ch) {
  (void)ch;
  return grblInit() && uxQueueSpacesAvailable(grblQueue) > 0;
}

void grblInput(int ch, char c) {
  if (!grblInit()) return;
  uint8_t u = (uint8_t)c;
  // real-time commands: act at once, never queued
  if (c == '?') {
    grblStatus(ch);
    return;
  }
  if (c == '!') {
    if (isOn && mode == MODE_GCODE) cncUserHold = true;
    return;
  }
  if (c == '~') {
    if (cncPaused) cncResumeRequest = true;
    cncUserHold = false;
    return;
  }
  if (u == 0x18) {  // soft reset: stop (decelerating), drop everything queued
    grblResetRequest = true;
    grblResetChannel = ch;
    homingAbort = true;
    homingRequest = false;
    cncUserHold = false;
    setIsOnFromTask(false);
    xQueueReset(grblQueue);
    grblLineLen[ch] = 0;
    return;
  }
  if (u == 0x85) {  // jog cancel
    if (grblJogging) setIsOnFromTask(false);
    return;
  }
  if (u >= 0x90 && u <= 0x97) {  // feed and rapid overrides
    cncOverrideCommand(u);
    return;
  }
  if (u >= 0x80) return;  // spindle / coolant overrides and other real-time bytes: ignored as Grbl ignores unknown ones
  if (c == '\r' || c == '\n') {
    grblInComment[ch] = false;
    if (grblLineOverflow[ch]) {
      grblLineOverflow[ch] = false;
      grblLineLen[ch] = 0;
      grblWriteLine(ch, "error:14");
      return;
    }
    if (grblLineLen[ch] == 0) {
      // an empty line still gets its "ok" (senders count them)
      if (c == '\n') grblWriteLine(ch, "ok");
      return;
    }
    GrblLine line;
    memcpy(line.text, grblLineBuf[ch], grblLineLen[ch]);
    line.text[grblLineLen[ch]] = '\0';
    line.channel = ch;
    grblLineLen[ch] = 0;
    if (xQueueSend(grblQueue, &line, 0) != pdTRUE) grblWriteLine(ch, "error:14");
    return;
  }
  // comments are dropped here, as Grbl does
  if (grblInComment[ch]) {
    if (c == ')') grblInComment[ch] = false;
    return;
  }
  if (c == '(') {
    grblInComment[ch] = true;
    return;
  }
  if (c == ';') {
    grblInComment[ch] = true;  // until the end of the line
    return;
  }
  if (c < ' ') return;
  if (grblLineLen[ch] >= GRBL_LINE_MAX - 1) {
    grblLineOverflow[ch] = true;
    return;
  }
  grblLineBuf[ch][grblLineLen[ch]++] = c;
}

// ------------------------------------------------------------------ TCP port 23 (called from taskWifi)

void grblTcpLoop() {
  if (!grblInit()) return;
  if (!grblServerStarted) {
    grblServer.begin();
    grblServer.setNoDelay(true);
    grblServerStarted = true;
  }
  if (grblServer.hasClient()) {
    WiFiClient incoming = grblServer.accept();
    if (xSemaphoreTake(grblOutMutex, 50) == pdTRUE) {
      if (grblClient) grblClient.stop();  // one sender at a time: the newest one
      grblClient = incoming;
      grblClient.setNoDelay(true);
      xSemaphoreGive(grblOutMutex);
    }
    grblLineLen[1] = 0;
    grblBanner(1);
  }
  while (grblClient && grblClient.connected() && grblClient.available() > 0 && grblRoomFor(1)) {
    grblInput(1, (char)grblClient.read());
  }
}

// ------------------------------------------------------------------ lines (called from taskGcode)

// Starts the H4 for the sender's lines (keeps the modes from line to line, as Grbl). False: refused.
bool grblEnsureOn(int ch) {
  if (isOn && grblSession) return true;
  if (isOn || cncBusy() || homingActive) {
    grblWriteLine(ch, "[MSG:The H4 is busy with another program]");
    return false;
  }
  if (mode != MODE_GCODE) {
    setModeFromTask(MODE_GCODE);
    unsigned long t0 = millis();
    while (mode != MODE_GCODE && millis() - t0 < 500) vTaskDelay(5);
    if (mode != MODE_GCODE) return false;
  }
  const char* refusal = cncStartRefusal();
  if (refusal != NULL) {
    grblMessage(refusal);
    return false;
  }
  gcodeProgramData = "";  // nothing of an earlier file program may run now
  gcodeProgramSize = 0;
  gcodeProgramCharIndex = 0;
  cncStartKeepModes = true;
  grblSession = true;
  setIsOnFromTask(true);
  unsigned long t0 = millis();
  while (!isOn && millis() - t0 < 500) vTaskDelay(2);
  if (!isOn) {
    grblSession = false;
    cncStartKeepModes = false;
    return false;
  }
  return true;
}

String grblNum(double v) {
  return String(v, 3);
}

String grblAxes3(const double* v) {
  // X Y Z as Grbl reports them (Y is always 0 on this machine), then C
  String s = grblNum(v[CNC_X]) + ",0.000," + grblNum(v[CNC_Z]);
  if (a1.active) s += "," + grblNum(v[CNC_C]);
  return s;
}

void grblSettings(int ch) {
  float spuX = cncStepsPerUnit(CNC_X), spuZ = cncStepsPerUnit(CNC_Z), spuC = cncStepsPerUnit(CNC_C);
  String s;
  s += "$0=" + String(CNC_STEP_PULSE_US) + "\r\n";
  s += "$1=255\r\n$2=0\r\n$3=0\r\n$4=0\r\n$5=0\r\n$6=0\r\n$10=0\r\n";
  s += "$11=" + String(CNC_JUNCTION_DEVIATION, 3) + "\r\n";
  s += "$12=" + String(CNC_ARC_TOLERANCE, 3) + "\r\n";
  s += "$13=" + String(grblReportInches ? 1 : 0) + "\r\n";
  s += "$20=" + String(LIMIT_X_USE || LIMIT_Z_USE ? 1 : 0) + "\r\n";
  s += "$21=" + String((HOME_X_USE && HOME_X_IS_LIMIT) || (HOME_Z_USE && HOME_Z_IS_LIMIT) ? 1 : 0) + "\r\n";
  s += "$22=" + String(HOME_X_USE || HOME_Z_USE ? 1 : 0) + "\r\n";
  s += "$23=" + String((HOME_X_SEARCH_VEL < 0 ? 1 : 0) | (HOME_Z_SEARCH_VEL < 0 ? 4 : 0)) + "\r\n";
  s += "$24=" + String(fabs(HOME_X_LATCH_VEL) * 60, 1) + "\r\n";
  s += "$25=" + String(fabs(HOME_X_SEARCH_VEL) * 60, 1) + "\r\n";
  s += "$26=1\r\n$27=0.000\r\n$30=1000\r\n$31=0\r\n$32=0\r\n";
  s += "$100=" + String(spuX, 3) + "\r\n$101=1.000\r\n$102=" + String(spuZ, 3) + "\r\n";
  if (a1.active) s += "$103=" + String(spuC, 3) + "\r\n";
  s += "$110=" + String(x.speedManualMove / spuX * 60, 1) + "\r\n$111=1.0\r\n$112=" + String(z.speedManualMove / spuZ * 60, 1) + "\r\n";
  if (a1.active) s += "$113=" + String(a1.speedManualMove / spuC * 60, 1) + "\r\n";
  s += "$120=" + String(x.acceleration / spuX, 1) + "\r\n$121=1.0\r\n$122=" + String(z.acceleration / spuZ, 1) + "\r\n";
  if (a1.active) s += "$123=" + String(a1.acceleration / spuC, 1) + "\r\n";
  s += "$130=" + String((float)MAX_TRAVEL_MM_X, 1) + "\r\n$131=0.0\r\n$132=" + String((float)MAX_TRAVEL_MM_Z, 1) + "\r\n";
  grblWrite(ch, s.c_str());
}

void grblOffsets(int ch) {
  cnc_offsets_t o;
  cnc_gc_get_offsets(&o);
  const char* names[] = {"G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3"};
  String s;
  for (int i = 0; i < CNC_COORD_SYSTEMS; i++) s += String("[") + names[i] + ":" + grblAxes3(o.coord[i]) + "]\r\n";
  s += "[G28:" + grblAxes3(o.g28) + "]\r\n[G30:" + grblAxes3(o.g30) + "]\r\n";
  double g92[CNC_N_AXIS];
  for (int i = 0; i < CNC_N_AXIS; i++) g92[i] = o.g92Applied ? o.g92[i] : 0;
  s += "[G92:" + grblAxes3(g92) + "]\r\n";
  cnc_modes_t m;
  cnc_gc_get_modes(&m);
  s += "[TLO:" + grblNum(m.tloZ) + "]\r\n[PRB:0.000,0.000,0.000:0]\r\n";
  grblWrite(ch, s.c_str());
}

void grblParserState(int ch) {
  cnc_modes_t m;
  cnc_gc_get_modes(&m);
  const char* cs[] = {"G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3"};
  String s = "[GC:";
  if (m.motion == 800) s += "G80";
  else s += "G" + String(m.motion / 10);
  s += String(" ") + cs[m.coord] + " G" + String(m.plane / 10) + (m.inches ? " G20" : " G21") + (m.incremental ? " G91" : " G90");
  s += m.feedMode == 930 ? " G93" : (m.feedMode == 950 ? " G95" : " G94");
  s += m.diameter ? " G7" : " G8";
  s += " M5 M9 T0 F" + String(m.feed, 1) + " S0]";
  grblWriteLine(ch, s);
}

// A G-code line of the sender. True when it was planned (or done).
bool grblRunGcode(int ch, const char* text) {
  if (!grblEnsureOn(ch)) return false;
  cncGrblLine = true;
  cncLastError[0] = '\0';
  grblChannel = ch;
  bool ok = cncProcessLine(String(text), false);
  cncGrblLine = false;
  return ok;
}

void grblAnswer(int ch, bool ok) {
  if (ok) {
    grblWriteLine(ch, "ok");
  } else {
    if (cncLastError[0]) grblWriteLine(ch, String("[MSG:") + cncLastError + "]");
    grblWriteLine(ch, "error:" + String(cncLastError[0] ? grblErrorCode(cncLastError) : 9));
  }
}

// A jog ($J= without "$J="): one move, the modal state stays as it was (Grbl).
bool grblJog(int ch, const String& words) {
  cnc_modes_t saved;
  cnc_gc_get_modes(&saved);
  grblJogging = true;
  String line = "G1 " + words;
  bool ok = grblRunGcode(ch, line.c_str());
  cnc_modes_t after;
  cnc_gc_get_modes(&after);
  saved.coord = after.coord;  // (a jog may not change it anyway)
  cnc_gc_set_modes(&saved);
  return ok;
}

void grblSystemCommand(int ch, const char* text) {
  String cmd = String(text);
  cmd.trim();
  String up = cmd;
  up.toUpperCase();
  if (up == "$") {
    grblWriteLine(ch, "[HLP:$$ $# $G $I $N $x=val $Nx=line $J=line $SLP $C $X $H ~ ! ? ctrl-x]");
    grblWriteLine(ch, "ok");
  } else if (up == "$$") {
    grblSettings(ch);
    grblWriteLine(ch, "ok");
  } else if (up == "$#") {
    grblOffsets(ch);
    grblWriteLine(ch, "ok");
  } else if (up == "$G") {
    grblParserState(ch);
    grblWriteLine(ch, "ok");
  } else if (up == "$I") {
    grblWriteLine(ch, "[VER:1.1h.NanoEls-H4-CNC:]");
    grblWriteLine(ch, "[OPT:V," + String(CNC_BLOCK_BUFFER_SIZE) + ",256]");
    grblWriteLine(ch, "ok");
  } else if (up == "$N") {
    grblWriteLine(ch, "$N0=");
    grblWriteLine(ch, "$N1=");
    grblWriteLine(ch, "ok");
  } else if (up == "$X") {
    cncHomeOverride = true;
    grblWriteLine(ch, "[MSG:Caution: Unlocked]");
    grblWriteLine(ch, "ok");
  } else if (up == "$H") {
    if (!homingConfigured()) {
      grblWriteLine(ch, "[MSG:No home switch is set up (Machine settings)]");
      grblWriteLine(ch, "error:5");
      return;
    }
    if (isOn || cncBusy()) {
      grblWriteLine(ch, "error:8");
      return;
    }
    homingRequest = true;
    unsigned long t0 = millis();
    while ((homingRequest || homingActive) && millis() - t0 < 600000) {
      if (GRBL_ON_USB) {
        while (Serial.available() > 0 && grblRoomFor(0)) grblInput(0, Serial.read());  // ? ! ctrl-x meanwhile
      }
      vTaskDelay(10);
    }
    bool ok = (!HOME_X_USE || x.homed) && (!HOME_Z_USE || z.homed);
    if (ok) {
      cncHomeOverride = false;
      grblWriteLine(ch, "ok");
    } else {
      grblWriteLine(ch, "ALARM:9");
      grblWriteLine(ch, "error:9");
    }
  } else if (up.startsWith("$J=")) {
    if (isOn && !grblSession) {
      grblWriteLine(ch, "error:8");
      return;
    }
    grblAnswer(ch, grblJog(ch, cmd.substring(3)));
  } else if (up == "$SLP") {
    grblWriteLine(ch, "ok");
  } else if (up == "$C") {
    grblWriteLine(ch, "[MSG:Check mode is not supported]");
    grblWriteLine(ch, "error:3");
  } else if (up.indexOf('=') > 0) {
    // settings are changed on the H4 web page (Machine settings), where they are checked and stored
    if (up.startsWith("$13=")) {
      grblReportInches = up.substring(4).toInt() != 0;
      grblWriteLine(ch, "ok");
    } else if (up.startsWith("$10=") || up.startsWith("$N")) {
      grblWriteLine(ch, "ok");
    } else {
      grblWriteLine(ch, "[MSG:Change machine settings on the H4 web page]");
      grblWriteLine(ch, "error:3");
    }
  } else {
    grblWriteLine(ch, "error:3");
  }
}

// One queued line, in taskGcode. True when a line was handled.
bool grblServiceLine() {
  if (!grblInit()) return false;
  // a limit or a driver alarm since the last look: tell the sender
  if (cncFaultCount != grblFaultSeen) {
    grblFaultSeen = cncFaultCount;
    if (grblChannel >= 0) {
      grblWriteLine(grblChannel, "ALARM:1");
      grblMessage(cncFaultText);
    }
  }
  if (grblResetRequest && !cncBusy() && !isOn) {
    grblResetRequest = false;
    grblSession = false;
    grblJogging = false;
    cncHomeOverride = false;
    grblBanner(grblResetChannel);
  }
  if (grblSession && (!isOn || mode != MODE_GCODE)) {
    grblSession = false;  // M2 / M30, OFF or a stop ended it
    grblJogging = false;
  }
  GrblLine line;
  if (xQueueReceive(grblQueue, &line, 0) != pdTRUE) {
    // nothing more from the sender: once all has moved, the H4 is off again (the modes stay)
    if (grblSession && !cncBusy() && !cncPaused && !cncUserHold && millis() - grblLastMs > 300) {
      grblSession = false;
      grblJogging = false;
      setIsOnFromTask(false);
    }
    if (grblJogging && !cncBusy()) grblJogging = false;
    return false;
  }
  grblLastMs = millis();
  grblChannel = line.channel;
  if (line.text[0] == '$') {
    grblSystemCommand(line.channel, line.text);
  } else if (isOn && !grblSession) {
    grblWriteLine(line.channel, "error:8");  // a file program of the web page runs
  } else {
    grblJogging = false;
    grblAnswer(line.channel, grblRunGcode(line.channel, line.text));
  }
  grblLastMs = millis();
  return true;
}

// ------------------------------------------------------------------ gamepad of the web page

// The web page reads a gamepad (on the phone / computer) and sends the wanted speeds to /jog about every 100 ms
// (wifi.ino). Here they become short jogs, as a Grbl jog pendant sends them: pieces of WEB_JOG_PIECE_S seconds,
// never more than WEB_JOG_LEAD_S seconds or WEB_JOG_MAX_BLOCKS of them queued. Speeds 0, or no request for
// WEB_JOG_TIMEOUT_MS (page closed, WiFi lost), stop at once with a jog cancel.
#define WEB_JOG_PIECE_S 0.05f
#define WEB_JOG_LEAD_S 0.2f
#define WEB_JOG_MAX_BLOCKS 4
#define WEB_JOG_TIMEOUT_MS 300
volatile float webJogSpeed[3] = {0, 0, 0};  // X mm/min, Z mm/min, C deg/min
volatile unsigned long webJogMs = 0;
volatile bool webJogRefused = false;        // set on a refusal; cleared when the page sends speeds 0
char webJogError[100] = "";
bool webJogRunning = false;
float webJogQueuedS = 0;
unsigned long webJogLastMs = 0;
unsigned long webJogStopMs = 0;

// From the web server (wifi.ino). False: the last jog was refused (reason in webJogError).
bool webJogRequest(float vx, float vz, float vc) {
  webJogSpeed[0] = vx;
  webJogSpeed[1] = vz;
  webJogSpeed[2] = vc;
  webJogMs = millis();
  if (vx == 0 && vz == 0 && vc == 0) webJogRefused = false;
  return !webJogRefused;
}

void webJogStop() {
  if (!webJogRunning) return;
  webJogRunning = false;
  webJogQueuedS = 0;
  webJogStopMs = millis();
  if (grblJogging) setIsOnFromTask(false);  // as the Grbl jog cancel: brake, drop the rest
}

// In taskGcode. True when a jog piece was planned.
bool webJogService() {
  unsigned long now = millis();
  float vx = webJogSpeed[0], vz = webJogSpeed[1], vc = webJogSpeed[2];
  bool wanted = now - webJogMs < WEB_JOG_TIMEOUT_MS && (vx != 0 || vz != 0 || vc != 0) && !webJogRefused;
  if (!wanted) {
    webJogStop();
    return false;
  }
  webJogQueuedS = max(0.0f, webJogQueuedS - (now - webJogLastMs) / 1000.0f);
  webJogLastMs = now;
  if (!webJogRunning) {
    if (now - webJogStopMs < 500 || (cncBusy() && !isOn)) return false;  // the last stop is still braking
    if (isOn || cncBusy()) {
      snprintf(webJogError, sizeof(webJogError), "A program is running");
      webJogRefused = true;
      return false;
    }
  }
  if (webJogQueuedS >= WEB_JOG_LEAD_S || cnc_plan_blocks_used() >= WEB_JOG_MAX_BLOCKS) return false;
  const char* refusal = isOn ? NULL : (homingActive || homingRequest ? "Home All is running" : cncStartRefusal());
  if (refusal != NULL) {
    snprintf(webJogError, sizeof(webJogError), "%s", refusal);
    webJogRefused = true;
    return false;
  }
  float dx = vx / 60 * WEB_JOG_PIECE_S, dz = vz / 60 * WEB_JOG_PIECE_S, dc = vc / 60 * WEB_JOG_PIECE_S;
  // the feed as LinuxCNC and Grbl read it: along X/Z when they move (C follows in the same time), else C alone
  float linear = sqrtf(dx * dx + dz * dz);
  float feed = (linear > 0 ? linear : fabsf(dc)) / WEB_JOG_PIECE_S * 60;
  char words[120];
  int n = snprintf(words, sizeof(words), "G91 G94 G21");
  if (dx != 0) n += snprintf(words + n, sizeof(words) - n, " X%.4f", dx);
  if (dz != 0) n += snprintf(words + n, sizeof(words) - n, " Z%.4f", dz);
  if (dc != 0) n += snprintf(words + n, sizeof(words) - n, " C%.4f", dc);
  snprintf(words + n, sizeof(words) - n, " F%.1f", feed);
  int channel = grblChannel;  // a connected sender keeps its messages
  bool ok = grblJog(-1, String(words));
  grblChannel = channel;
  grblLastMs = millis();
  if (!ok) {
    snprintf(webJogError, sizeof(webJogError), "%s", cncLastError[0] ? cncLastError : "Jog refused");
    webJogRefused = true;
    webJogStop();
    return false;
  }
  webJogRunning = true;
  webJogQueuedS += WEB_JOG_PIECE_S;
  return true;
}
