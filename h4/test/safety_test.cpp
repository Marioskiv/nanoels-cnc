// Tests of safety.ino (NanoEls H4 CNC fork): restarts while moving, power cuts during a program, the lasting log,
// accepting a new firmware. The file is compiled unchanged; the ESP32 parts it uses are small stand-ins here.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <map>

// --- stand-ins for the Arduino / ESP-IDF parts used by safety.ino
struct String : std::string {
  String() {}
  String(const char* s) : std::string(s) {}
  String(const std::string& s) : std::string(s) {}
  String substring(size_t a, size_t b) const { return String(std::string::substr(a, b > a ? b - a : 0)); }
  String operator+(const String& o) const { return String(std::string(*this) + std::string(o)); }
  String operator+(const char* o) const { return String(std::string(*this) + o); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a) + std::string(b)); }
  String& operator+=(const char* o) { append(o); return *this; }
  String& operator+=(const String& o) { append(o); return *this; }
  size_t length() const { return size(); }
};
#define RTC_NOINIT_ATTR
#define FW_VERSION "test"
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
unsigned long nowMs = 0;
unsigned long millis() { return nowMs; }
typedef enum { ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT,
  ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO, ESP_RST_USB, ESP_RST_JTAG } esp_reset_reason_t;
esp_reset_reason_t nextReason = ESP_RST_POWERON;
esp_reset_reason_t esp_reset_reason() { return nextReason; }
typedef int esp_ota_img_states_t;
#define ESP_OTA_IMG_PENDING_VERIFY 1
#define ESP_OTA_IMG_ABORTED 2
#define ESP_OTA_IMG_VALID 3
#define ESP_OK 0
struct esp_partition_t { int state; };
esp_partition_t running{ESP_OTA_IMG_VALID}, other{ESP_OTA_IMG_VALID};
const esp_partition_t* esp_ota_get_next_update_partition(const void*) { return &other; }
const esp_partition_t* esp_ota_get_running_partition() { return &running; }
int esp_ota_get_state_partition(const esp_partition_t* p, esp_ota_img_states_t* s) { *s = p->state; return ESP_OK; }
int esp_ota_mark_app_valid_cancel_rollback() { running.state = ESP_OTA_IMG_VALID; return ESP_OK; }
std::map<std::string, std::string> nvs; // the flash settings survive a restart
struct Preferences {
  void begin(const char*) {}
  void end() {}
  unsigned long getULong(const char* k, unsigned long d) { return nvs.count(k) ? std::stoul(nvs[k]) : d; }
  void putULong(const char* k, unsigned long v) { nvs[k] = std::to_string(v); }
  bool getBool(const char* k, bool d) { return nvs.count(k) ? nvs[k] == "1" : d; }
  void putBool(const char* k, bool v) { nvs[k] = v ? "1" : "0"; }
  String getString(const char* k, const char* d) { return nvs.count(k) ? String(nvs[k]) : String(d); }
  void putString(const char* k, const String& v) { nvs[k] = v; }
};
std::map<std::string, std::string> files;
struct File {
  std::string name; bool ok = false;
  explicit operator bool() const { return ok; }
  size_t size() { return files[name].size(); }
  void println(const char* t) { files[name] += t; files[name] += "\n"; }
  String readString() { return String(files[name]); }
  void close() {}
};
struct { File open(const char* n, const char* m) { File f; f.name = n; f.ok = std::string(m) != "r" || files.count(n); if (f.ok && !files.count(n)) files[n] = ""; return f; }
  void remove(const char* n) { files.erase(n); } void rename(const char* a, const char* b) { files[b] = files[a]; files.erase(a); } } LittleFS;
bool beepFlag = false, moving = false, cncActive = false;
volatile bool wifiFsBusy = false;
bool motionActive() { return moving; }
bool cncRunActive() { return cncActive; }
#include "../safety.ino"

int fails = 0, checks = 0;
void check(bool ok, const char* what) { checks++; if (!ok) { fails++; printf("FAIL %s\n", what); } }

// a restart: RAM is lost (except RTC_NOINIT), the flash stays
void restart(esp_reset_reason_t why, bool powerCut) {
  nextReason = why; nowMs = 0;
  restartUnconfirmed = false; restartText = ""; programFlagSet = false; firmwareAccepted = false; eventHead = eventTail = 0;
  if (powerCut) { rtcSafetyMagic = 12345; rtcMoving = 77; rtcCrashCount = 99; } // RTC memory is garbage after power on
  safetyBoot();
}
void run(unsigned long ms, bool mv) { moving = mv; for (unsigned long t = 0; t < ms; t += 100) { nowMs += 100; safetyLoop(); } }

int main() {
  eventsFsReady = true; // LittleFS mounted
  // first power on: nothing to report
  restart(ESP_RST_POWERON, true);
  check(!restartUnconfirmed && safetyStartRefusal() == NULL, "first start is clean");
  run(1000, false);
  check(files["/events.log"].find("cause: power on") != std::string::npos, "the start is in the lasting log");

  // a crash while moving: positions not reliable until confirmed / Home All
  run(500, true);
  restart(ESP_RST_PANIC, false);
  check(restartUnconfirmed && safetyStartRefusal() != NULL, "crash while moving refuses the GCODE start");
  check(restartText.find("crash") != std::string::npos, "the cause is named");
  safetyConfirmRestart("Home All done after the restart");
  check(!restartUnconfirmed && safetyStartRefusal() == NULL, "Home All clears it");

  // a crash while standing still: just logged (after a good run of FIRMWARE_OK_MS the counter starts again)
  run(FIRMWARE_OK_MS + 1000, false);
  restart(ESP_RST_TASK_WDT, false);
  check(!restartUnconfirmed, "a crash while standing still does not block");
  check(rtcCrashCount == 1, "abnormal restart counted");

  // brownout while moving
  run(300, true);
  restart(ESP_RST_BROWNOUT, false);
  check(restartUnconfirmed, "brownout while moving refuses");
  check(rtcCrashCount == 2, "two abnormal restarts in a row");
  safetyConfirmRestart("ok");

  // a normal restart (software) while moving is not a fault (the H4 restarts only when nothing moves)
  run(300, true);
  restart(ESP_RST_SW, false);
  check(!restartUnconfirmed, "software restart does not block");
  check(rtcCrashCount == 0, "counter reset by a normal restart");

  // power cut during a G-code program (RTC memory lost): known from the flash
  run(25000, false);
  safetyProgramStart(String("part.ngc"));
  check(nvs["run"] == "1", "program flag written");
  cncActive = true; run(2000, true);
  restart(ESP_RST_POWERON, true);
  check(restartUnconfirmed && restartText.find("part.ngc") != std::string::npos, "power cut during a program is reported with its name");
  check(nvs["run"] == "0", "the flag is cleared at the next start");
  cncActive = false; safetyConfirmRestart("ok");

  // a program that ends normally clears the flag once nothing moves
  safetyProgramStart(String("a.ngc"));
  cncActive = true; run(1000, true);
  check(nvs["run"] == "1", "flag kept while the program runs");
  cncActive = false; run(300, false);
  check(nvs["run"] == "0", "flag cleared after the end");
  restart(ESP_RST_POWERON, true);
  check(!restartUnconfirmed, "after a normal end nothing to report");

  // new firmware: accepted only after FIRMWARE_OK_MS
  running.state = ESP_OTA_IMG_PENDING_VERIFY;
  restart(ESP_RST_SW, false);
  run(FIRMWARE_OK_MS - 1000, false);
  check(running.state == ESP_OTA_IMG_PENDING_VERIFY, "not accepted too early");
  run(2000, false);
  check(running.state == ESP_OTA_IMG_VALID, "accepted after it ran long enough");
  // a refused update is reported
  other.state = ESP_OTA_IMG_ABORTED;
  restart(ESP_RST_SW, false);
  run(500, false);
  check(files["/events.log"].find("previous firmware runs again") != std::string::npos, "a refused update is in the log");
  other.state = ESP_OTA_IMG_VALID;

  // nothing written to the flash while moving; the log rotates
  size_t before = files["/events.log"].size();
  for (int i = 0; i < 5; i++) eventLog("while moving");
  run(500, true);
  check(files["/events.log"].size() == before, "no flash write while moving");
  run(200, false);
  check(files["/events.log"].size() > before, "written once still");
  for (int i = 0; i < 900; i++) { eventLog("filler line to make the log grow past its size"); if (i % 10 == 0) run(100, false); }
  run(200, false);
  check(files.count("/events.old.log") && files["/events.log"].size() <= EVENTS_MAX_BYTES + 2000, "the log rotates");
  check(eventsText().find("filler") != std::string::npos, "the log can be read back");
  printf("safety: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
