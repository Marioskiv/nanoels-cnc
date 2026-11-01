// safety.ino - restarts, firmware updates and a lasting event log (NanoEls H4 CNC fork).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
//
// 1. Why the H4 restarted. A crash (panic), a watchdog or a voltage drop (brownout) restarts the ESP32 at once. If
//    that happened while something moved, or during a G-code program (also a power cut), the saved positions are
//    not where the axes are: the H4 says so, and the GCODE mode does not start until Home All (or, on a machine
//    without home switches, until the operator confirms that the zeros were checked again).
// 2. A new firmware has to prove itself. The bootloader can go back to the previous firmware, but the Arduino core
//    accepts every new one at once. Here a new firmware is accepted only after it has run for FIRMWARE_OK_MS;
//    if it restarts before that (it crashes), the bootloader starts the previous one again and the log says so.
// 3. A lasting log: restarts, faults, alarms, programs started / ended (every cncLog line) go to /events.log on
//    the flash, written only while nothing moves (a flash write stops the CPU for some ms).

#include <esp_ota_ops.h>
#include <esp_system.h>
#include <initializer_list>

#define SAFETY_NAMESPACE "h4safe"
#define FIRMWARE_OK_MS 20000UL   // a new firmware is accepted after this long without a restart
#define EVENTS_FILE "/events.log"
#define EVENTS_OLD_FILE "/events.old.log"
#define EVENTS_MAX_BYTES 24000   // then the file becomes events.old.log and a new one starts
#define EVENT_QUEUE 24

// RTC memory keeps its contents through a crash, a watchdog and a software restart (not through a power cut).
RTC_NOINIT_ATTR uint32_t rtcSafetyMagic;
RTC_NOINIT_ATTR uint32_t rtcMoving;      // 1 while anything moves or the H4 is ON
RTC_NOINIT_ATTR uint32_t rtcCrashCount;  // restarts by crash / watchdog / brownout in a row
#define RTC_SAFETY_MAGIC 0x48345346UL

volatile bool restartUnconfirmed = false; // the GCODE mode waits for Home All or a confirmation
String restartText = "";                   // what happened, for the page and the log
bool programFlagSet = false;               // "a program runs" is written in the flash
bool firmwareAccepted = false;
bool eventsFsReady = false;               // LittleFS is mounted (wifi.ino)
uint32_t bootNumber = 0;

char eventQueue[EVENT_QUEUE][128];
volatile uint32_t eventHead = 0, eventTail = 0;
portMUX_TYPE eventMux = portMUX_INITIALIZER_UNLOCKED;

// verifyRollbackLater() (the Arduino core asks it at start) is in rollback.cpp: true = this sketch accepts a new
// firmware itself, in safetyLoop().

const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "restart";
    case ESP_RST_PANIC: return "crash (panic)";
    case ESP_RST_INT_WDT: return "watchdog (interrupts)";
    case ESP_RST_TASK_WDT: return "watchdog (task)";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "voltage drop (brownout)";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    default: return "unknown";
  }
}

bool resetIsAbnormal(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
}

// One line for the lasting log (any task, not from an interrupt). Written to the flash later, when nothing moves.
void eventLog(const char* text) {
  char line[128];
  snprintf(line, sizeof(line), "#%lu %lus %s", (unsigned long)bootNumber, (unsigned long)(millis() / 1000), text);
  portENTER_CRITICAL(&eventMux);
  if (eventHead - eventTail >= EVENT_QUEUE) eventTail = eventTail + 1; // full: the oldest unwritten line goes
  memcpy(eventQueue[eventHead % EVENT_QUEUE], line, sizeof(eventQueue[0])); // line is 0-terminated (snprintf)
  eventHead = eventHead + 1;
  portEXIT_CRITICAL(&eventMux);
}

// Early in setup(): why did the H4 start, and was something moving then?
void safetyBoot() {
  esp_reset_reason_t r = esp_reset_reason();
  bool rtcValid = rtcSafetyMagic == RTC_SAFETY_MAGIC && r != ESP_RST_POWERON;
  bool wasMoving = rtcValid && rtcMoving == 1;
  bool abnormal = resetIsAbnormal(r);
  rtcSafetyMagic = RTC_SAFETY_MAGIC;
  rtcCrashCount = abnormal ? (rtcValid ? rtcCrashCount + 1 : 1) : 0;
  rtcMoving = 0;

  Preferences p;
  p.begin(SAFETY_NAMESPACE);
  bootNumber = p.getULong("boots", 0) + 1;
  p.putULong("boots", bootNumber);
  bool programWasRunning = p.getBool("run", false);
  String program = p.getString("prog", "");
  if (programWasRunning) {
    p.putBool("run", false);
    p.putString("prog", "");
  }
  p.end();

  char line[128];
  snprintf(line, sizeof(line), "start, firmware %s, cause: %s", FW_VERSION, resetReasonText(r));
  eventLog(line);
  if (abnormal && rtcCrashCount > 1) {
    snprintf(line, sizeof(line), "%lu abnormal restarts in a row", (unsigned long)rtcCrashCount);
    eventLog(line);
  }

  // Was the previous firmware update refused (it restarted before it was accepted)?
  const esp_partition_t* other = esp_ota_get_next_update_partition(NULL);
  esp_ota_img_states_t otherState;
  if (other != NULL && esp_ota_get_state_partition(other, &otherState) == ESP_OK && otherState == ESP_OTA_IMG_ABORTED) {
    eventLog("the last firmware update restarted before it was accepted: the previous firmware runs again");
  }

  if ((abnormal && wasMoving) || programWasRunning) {
    restartUnconfirmed = true;
    if (programWasRunning) {
      restartText = "Restarted (" + String(resetReasonText(r)) + ") during the G-code program " + program +
          ": the positions are not reliable. Home All, or check the zeros and confirm.";
    } else {
      restartText = "Restarted (" + String(resetReasonText(r)) + ") while the machine moved: the positions are not "
          "reliable. Home All, or check the zeros and confirm.";
    }
    eventLog(restartText.c_str());
    beepFlag = true;
  }
}

// A G-code program starts (taskGcode, before its first move): remember it in the flash, so that a power cut
// during it is known at the next start.
void safetyProgramStart(const String& name) {
  Preferences p;
  p.begin(SAFETY_NAMESPACE);
  p.putBool("run", true);
  p.putString("prog", name.substring(0, 60));
  p.end();
  programFlagSet = true;
  eventLog(("program start: " + (name.length() ? name : String("(sender)"))).c_str());
}

// Home All done, or the operator confirmed the zeros after a restart.
void safetyConfirmRestart(const char* how) {
  if (!restartUnconfirmed) return;
  restartUnconfirmed = false;
  restartText = "";
  eventLog(how);
}

// The GCODE mode's start refusal (homing.ino) asks this.
const char* safetyStartRefusal() {
  return restartUnconfirmed ? "Restarted while moving: Home All or confirm the zeros" : NULL;
}

void eventsFlush() {
  if (eventHead == eventTail) return;
  File f = LittleFS.open(EVENTS_FILE, "a");
  if (!f) return;
  if (f.size() > EVENTS_MAX_BYTES) {
    f.close();
    LittleFS.remove(EVENTS_OLD_FILE);
    LittleFS.rename(EVENTS_FILE, EVENTS_OLD_FILE);
    f = LittleFS.open(EVENTS_FILE, "a");
    if (!f) return;
  }
  while (true) {
    char line[128];
    portENTER_CRITICAL(&eventMux);
    bool any = eventHead != eventTail;
    if (any) {
      memcpy(line, eventQueue[eventTail % EVENT_QUEUE], sizeof(line));
      eventTail = eventTail + 1;
    }
    portEXIT_CRITICAL(&eventMux);
    if (!any) break;
    f.println(line);
  }
  f.close();
}

// From taskDisplay, often.
void safetyLoop() {
  bool moving = motionActive();
  rtcMoving = moving ? 1 : 0;
  if (!firmwareAccepted && millis() > FIRMWARE_OK_MS) {
    firmwareAccepted = true;
    rtcCrashCount = 0;
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running != NULL && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
      eventLog("new firmware accepted");
    }
  }
  if (moving || wifiFsBusy) return; // flash writes stop the CPU: only while nothing moves
  if (programFlagSet && !cncRunActive()) {
    programFlagSet = false;
    Preferences p;
    p.begin(SAFETY_NAMESPACE);
    p.putBool("run", false);
    p.putString("prog", "");
    p.end();
  }
  if (eventsFsReady) eventsFlush();
}

// The lasting log as text: the older file, then the current one, then lines not written yet.
String eventsText() {
  String out;
  for (const char* name : {EVENTS_OLD_FILE, EVENTS_FILE}) {
    if (!eventsFsReady) break;
    File f = LittleFS.open(name, "r");
    if (f) {
      out += f.readString();
      f.close();
    }
  }
  portENTER_CRITICAL(&eventMux);
  uint32_t from = eventTail, to = eventHead;
  portEXIT_CRITICAL(&eventMux);
  for (uint32_t i = from; i < to; i++) {
    out += eventQueue[i % EVENT_QUEUE];
    out += "\n";
  }
  return out;
}
