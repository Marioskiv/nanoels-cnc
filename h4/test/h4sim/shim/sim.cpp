/*
  sim.cpp - runs the whole NanoEls H4 firmware on a PC (test/h4sim): the parts of h4sim.h that are not inline.

  What the firmware sees, and what it is on the PC:
  - its FreeRTOS tasks are threads; vTaskDelay sleeps; queues and mutexes are real ones; "interrupts off" holds the
    lock under which every simulated interrupt runs;
  - the step timer (gptimer) and the asynchronous mode's timer call their handlers from a thread of their own at the
    alarm times, in real time;
  - its step and direction pins drive simulated motors: a step is counted when a STEP pin goes LOW (the H4's drivers
    take the pulse when the pin is low, the firmware ends every pulse by setting it HIGH again), in the direction the
    DIR pin has then. So the motors' positions are measured from the pins, not taken from the firmware;
  - its input pins (home / limit switches, the driver alarm) are set by the test, by hand or from a motor's position;
  - the spindle encoder's pulse counter counts a simulated spindle turning at the rpm the test sets;
  - its web server is a real HTTP server on 127.0.0.1 (H4SIM_PORT, default 8080): the app's page talks to it as to
    the H4; settings (Preferences) and files (LittleFS) are kept in H4SIM_DATA (default ./h4sim-data);
  - the LCD and the keypad are a 20 x 4 text and a queue of key events.
  A second HTTP server, the test's own (H4SIM_CTL_PORT, default H4SIM_PORT + 1), shows the machine and changes it:
    GET  /state                          motors (steps), LCD, inputs, spindle, beeps, restarts asked
    POST /key?code=N[&down=1|0]          a key of the H4 (without down: press and release)
    POST /spindle?rpm=R                  the spindle's speed (encoder counts, 600 PPR x 2 edges by default)
    POST /input?pin=P&level=0|1          an input pin by hand (e.g. the driver alarm)
    POST /switch?pin=P&motor=X|Z|A1&op=le|ge&steps=S&pressed=0|1
                                         a switch closed while the motor's steps are <= / >= S (pressed = its level)
    GET  /serial                         what the firmware wrote to its serial port
    GET  /trace?from=MS                  every millisecond since MS: time, spindle (encoder counts), Z, X, A1 steps
  ESP.restart() ends the program with exit code 3 (the runner starts it again; RTC memory is not kept).

  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "h4sim.h"
#include <chrono>
#include <thread>
#include <condition_variable>
#include <deque>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

using Clock = std::chrono::steady_clock;
static const Clock::time_point t0 = Clock::now();
static int64_t nowNs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count(); }

static std::string envOr(const char* k, const char* d) { const char* v = getenv(k); return v && *v ? v : d; }
static int simPort() { return atoi(envOr("H4SIM_PORT", "8080").c_str()); }
static std::string dataDir() { return envOr("H4SIM_DATA", "h4sim-data"); }
static void mkdirs(const std::string& p) {
  std::string cur;
  std::stringstream ss(p);
  std::string part;
  if (!p.empty() && p[0] == '/') cur = "/";
  while (std::getline(ss, part, '/')) {
    if (part.empty()) continue;
    cur += part + "/";
    ::mkdir(cur.c_str(), 0755);
  }
}

// ------------------------------------------------------------------ globals of the libraries
HardwareSerial Serial;
EspClass ESP;
TwoWire Wire;
LittleFSFS LittleFS;
UpdateClass Update;
WiFiClass WiFi;
timg_dev_t sim_timg[2];
const SimTimerSignals timer_group_periph_signals = {};

// ------------------------------------------------------------------ "interrupts"
static std::recursive_mutex irqLock;
void noInterrupts() { irqLock.lock(); }
void interrupts() { irqLock.unlock(); }

// ------------------------------------------------------------------ serial
static std::mutex serialLock;
static std::string serialOut;  // kept for GET /serial
static FILE* serialFile() {
  static FILE* f = nullptr;
  if (!f) {
    std::string p = envOr("H4SIM_SERIAL", "");
    f = p.empty() ? stdout : fopen(p.c_str(), "a");
    if (!f) f = stdout;
  }
  return f;
}
size_t HardwareSerial::write(uint8_t c) {
  std::lock_guard<std::mutex> g(serialLock);
  fputc(c, serialFile());
  if (c == '\n') fflush(serialFile());
  serialOut.push_back((char)c);
  if (serialOut.size() > 200000) serialOut.erase(0, 100000);
  return 1;
}
int HardwareSerial::available() { return 0; }
int HardwareSerial::read() { return -1; }

// ------------------------------------------------------------------ time
unsigned long millis() { return (unsigned long)(nowNs() / 1000000); }
unsigned long micros() { return (unsigned long)(nowNs() / 1000); }
int64_t esp_timer_get_time() { return nowNs() / 1000; }
uint32_t esp_cpu_get_cycle_count() { return (uint32_t)(nowNs() * 240 / 1000); }
void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void delayMicroseconds(unsigned int us) {
  int64_t end = nowNs() + (int64_t)us * 1000;
  if (us > 200) std::this_thread::sleep_for(std::chrono::microseconds(us - 100));
  while (nowNs() < end) {}
}
TickType_t xTaskGetTickCount() { return (TickType_t)millis(); }

// ------------------------------------------------------------------ tasks, queues, mutexes
struct SimTaskExit {};
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, uint32_t, void* param, UBaseType_t, TaskHandle_t* handle,
                                   BaseType_t) {
  std::string n = name ? name : "task";
  std::thread([fn, param, n]() {
    try { fn(param); } catch (SimTaskExit&) {}
  }).detach();
  if (handle) *handle = (TaskHandle_t)1;
  return pdPASS;
}
void vTaskDelay(TickType_t ticks) {
  if (ticks == 0) std::this_thread::yield();
  else std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
void vTaskDelete(TaskHandle_t t) { if (t == NULL) throw SimTaskExit(); }

struct SimQueue {
  std::mutex m;
  std::condition_variable cv;
  std::deque<std::vector<uint8_t>> items;
  size_t length, size;
};
QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
  SimQueue* q = new SimQueue();
  q->length = length;
  q->size = itemSize;
  return q;
}
static bool waitFor(std::unique_lock<std::mutex>& l, std::condition_variable& cv, TickType_t wait, std::function<bool()> ok) {
  if (wait == portMAX_DELAY) { cv.wait(l, ok); return true; }
  return cv.wait_for(l, std::chrono::milliseconds(wait), ok);
}
BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t wait) {
  std::unique_lock<std::mutex> l(q->m);
  if (!waitFor(l, q->cv, wait, [q]() { return q->items.size() < q->length; })) return pdFALSE;
  q->items.emplace_back((const uint8_t*)item, (const uint8_t*)item + q->size);
  q->cv.notify_all();
  return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t wait) {
  std::unique_lock<std::mutex> l(q->m);
  if (!waitFor(l, q->cv, wait, [q]() { return !q->items.empty(); })) return pdFALSE;
  memcpy(item, q->items.front().data(), q->size);
  q->items.pop_front();
  q->cv.notify_all();
  return pdTRUE;
}
BaseType_t xQueueReset(QueueHandle_t q) {
  std::lock_guard<std::mutex> l(q->m);
  q->items.clear();
  q->cv.notify_all();
  return pdPASS;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { std::lock_guard<std::mutex> l(q->m); return (UBaseType_t)q->items.size(); }
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t q) {
  std::lock_guard<std::mutex> l(q->m);
  return (UBaseType_t)(q->length - q->items.size());
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
  if (wait == portMAX_DELAY) { s->m.lock(); return pdTRUE; }
  return s->m.try_lock_for(std::chrono::milliseconds(wait)) ? pdTRUE : pdFALSE;
}

// ------------------------------------------------------------------ the machine: pins, motors, switches, spindle
static std::mutex machineLock;  // the test's view; the pins themselves are atomics
static std::atomic<uint8_t> outLevel[64];
static std::atomic<int8_t> inForced[64];  // -1: follow the switch / pull-up
static uint8_t pinModes[64];
struct Motor { const char* name; int step, dir; std::atomic<int64_t> steps; };
// the H4's pins (h4.ino): Z STEP 18 DIR 17, X STEP 20 DIR 19, A1 (on A11..A13) STEP A13 = 11, DIR A12 = 10
static Motor motors[3] = {{"Z", 18, 17, {0}}, {"X", 20, 19, {0}}, {"A1", 11, 10, {0}}};
struct Switch { int pin; int motor; bool le; int64_t steps; int pressedLevel; };
static std::vector<Switch> switches;
static std::atomic<long> beeps{0};
static std::atomic<uint64_t> loopCalls{0};  // how often the sketch's loop() ran (the lathe modes step in it)

static void onOutput(int pin, int oldLevel, int level) {
  if (oldLevel == 1 && level == 0) {
    for (auto& m : motors) {
      if (m.step == pin) m.steps += outLevel[m.dir].load() ? 1 : -1;
    }
  }
}
static void setOut(int pin, int level) {
  if (pin < 0 || pin >= 64) return;
  int old = outLevel[pin].exchange((uint8_t)level);
  if (old != level) onOutput(pin, old, level);
}
static int inputLevel(int pin) {
  if (pin < 0 || pin >= 64) return 0;
  int8_t f = inForced[pin].load();
  if (f >= 0) return f;
  std::lock_guard<std::mutex> g(machineLock);
  for (auto& s : switches) {
    if (s.pin != pin) continue;
    int64_t p = motors[s.motor].steps.load();
    bool pressed = s.le ? p <= s.steps : p >= s.steps;
    return pressed ? s.pressedLevel : 1 - s.pressedLevel;
  }
  // nothing connected: a pull-up reads HIGH, a pull-down or a floating pin LOW
  return (pinModes[pin] & PULLUP) ? 1 : 0;
}
void pinMode(uint8_t pin, uint8_t mode) { if (pin < 64) pinModes[pin] = mode; }
void digitalWrite(uint8_t pin, uint8_t level) { setOut(pin, level ? 1 : 0); }
int digitalRead(uint8_t pin) {
  if (pin < 64 && (pinModes[pin] & OUTPUT) == OUTPUT) return outLevel[pin].load();
  return inputLevel(pin);
}
void sim_reg_write(uint32_t addr, uint32_t value) {
  int base = (addr == GPIO_OUT1_W1TS_REG || addr == GPIO_OUT1_W1TC_REG) ? 32 : 0;
  bool set = addr == GPIO_OUT_W1TS_REG || addr == GPIO_OUT1_W1TS_REG;
  for (int i = 0; i < 32; i++) {
    if (value & (1u << i)) setOut(base + i, set ? 1 : 0);
  }
}
uint32_t sim_reg_read(uint32_t addr) {
  int base = addr == GPIO_IN1_REG ? 32 : 0;
  uint32_t v = 0;
  for (int i = 0; i < 32; i++) {
    int pin = base + i;
    int level = (pinModes[pin] & OUTPUT) == OUTPUT ? outLevel[pin].load() : inputLevel(pin);
    if (level) v |= 1u << i;
  }
  return v;
}
void tone(uint8_t, unsigned int, unsigned long) { beeps++; }
void noTone(uint8_t) {}
struct PinIrq { void (*fn)(); int mode; };
static PinIrq pinIrqs[64];
void attachInterrupt(uint8_t pin, void (*fn)(), int mode) { if (pin < 64) pinIrqs[pin] = {fn, mode}; }
void detachInterrupt(uint8_t pin) { if (pin < 64) pinIrqs[pin] = {nullptr, 0}; }

// the spindle and its encoder: counts = rpm / 60 x countsPerTurn x time, integrated
static std::atomic<double> spindleRpm{0};
static double spindleCounts = 0;   // since the start
static int64_t spindleAtNs = 0;
static int64_t pcntBase = 0;
static int encoderCountsPerTurn() { return atoi(envOr("H4SIM_ENCODER_COUNTS", "1200").c_str()); }
static int64_t spindleNow() {
  std::lock_guard<std::mutex> g(machineLock);
  int64_t n = nowNs();
  spindleCounts += spindleRpm.load() / 60.0 * encoderCountsPerTurn() * (n - spindleAtNs) / 1e9;
  spindleAtNs = n;
  return (int64_t)floor(spindleCounts);
}
esp_err_t pcnt_unit_config(const pcnt_config_t*) { return ESP_OK; }
esp_err_t pcnt_counter_clear(pcnt_unit_t) { pcntBase = spindleNow(); return ESP_OK; }
esp_err_t pcnt_get_counter_value(pcnt_unit_t, int16_t* count) {
  int64_t c = spindleNow() - pcntBase;
  *count = (int16_t)std::max<int64_t>(-32767, std::min<int64_t>(32767, c));
  return ESP_OK;
}

// a record of the machine every millisecond (the last 3 minutes): GET /trace?from=MS
struct TraceRow { uint32_t ms; int64_t spindle; int64_t z, x, a1; };
static std::mutex traceLock;
static std::vector<TraceRow> traceRing(180000);
static size_t traceHead = 0, traceCount = 0;
static void traceThread() {
  int64_t next = nowNs();
  for (;;) {
    next += 1000000;
    std::this_thread::sleep_until(t0 + std::chrono::nanoseconds(next));
    TraceRow r = {(uint32_t)(next / 1000000), spindleNow(), motors[0].steps.load(), motors[1].steps.load(), motors[2].steps.load()};
    std::lock_guard<std::mutex> g(traceLock);
    traceRing[traceHead] = r;
    traceHead = (traceHead + 1) % traceRing.size();
    if (traceCount < traceRing.size()) traceCount++;
  }
}

// ------------------------------------------------------------------ LCD and keypad
static std::mutex lcdLock;
static char lcdText[4][21];
size_t LiquidCrystal::write(uint8_t c) {
  std::lock_guard<std::mutex> g(lcdLock);
  if (simRow < 4 && simCol < 20) lcdText[simRow][simCol] = c >= 32 && c < 127 ? (char)c : (c < 8 ? '#' : '?');
  simCol++;
  return 1;
}
static std::mutex keyLock;
static std::deque<int> keyEvents;
void Adafruit_TCA8418::flush() { std::lock_guard<std::mutex> g(keyLock); keyEvents.clear(); }
int Adafruit_TCA8418::available() { std::lock_guard<std::mutex> g(keyLock); return (int)keyEvents.size(); }
int Adafruit_TCA8418::getEvent() {
  std::lock_guard<std::mutex> g(keyLock);
  if (keyEvents.empty()) return 0;
  int e = keyEvents.front();
  keyEvents.pop_front();
  return e;
}

// ------------------------------------------------------------------ timers
struct SimGptimer {
  uint32_t resolution;
  gptimer_alarm_cb_t cb = nullptr;
  void* ctx = nullptr;
  std::atomic<bool> running{false}, alarmOn{false};
  std::atomic<uint64_t> alarm{0}, reload{0};
  std::atomic<bool> autoReload{false};
  std::atomic<int64_t> baseNs{0};
  std::atomic<uint64_t> baseCount{0};
  uint64_t count() const { return baseCount + (uint64_t)((nowNs() - baseNs) * (double)resolution / 1e9); }
  int64_t nsAt(uint64_t c) const { return baseNs + (int64_t)((double)(c - baseCount) * 1e9 / resolution); }
};
static void timerThread(SimGptimer* t) {
  for (;;) {
    if (!t->running || !t->alarmOn || !t->cb) { std::this_thread::sleep_for(std::chrono::microseconds(200)); continue; }
    uint64_t a = t->alarm;
    int64_t at = t->nsAt(a);
    int64_t wait = at - nowNs();
    if (wait > 150000) { std::this_thread::sleep_for(std::chrono::nanoseconds(wait - 100000)); continue; }  // spin only the last ~0.1 ms
    while (nowNs() < at) {}
    if (!t->running || !t->alarmOn || t->alarm != a) continue;
    if (!t->autoReload) t->alarmOn = false;  // the hardware switches the alarm off when it fires
    gptimer_alarm_event_data_t ed = {t->count(), a};
    {
      std::lock_guard<std::recursive_mutex> g(irqLock);
      t->cb((gptimer_handle_t)t, &ed, t->ctx);
    }
    if (t->autoReload && t->alarm == a) {
      t->baseCount = t->reload.load();
      t->baseNs = at;
    }
  }
}
esp_err_t gptimer_new_timer(const gptimer_config_t* config, gptimer_handle_t* ret) {
  SimGptimer* t = new SimGptimer();
  t->resolution = config->resolution_hz ? config->resolution_hz : 1000000;
  t->baseNs = nowNs();
  std::thread(timerThread, t).detach();
  *ret = (gptimer_handle_t)t;
  return ESP_OK;
}
esp_err_t gptimer_register_event_callbacks(gptimer_handle_t h, const gptimer_event_callbacks_t* cbs, void* ctx) {
  SimGptimer* t = (SimGptimer*)h;
  t->cb = cbs->on_alarm;
  t->ctx = ctx;
  return ESP_OK;
}
esp_err_t gptimer_enable(gptimer_handle_t) { return ESP_OK; }
esp_err_t gptimer_disable(gptimer_handle_t) { return ESP_OK; }
esp_err_t gptimer_start(gptimer_handle_t h) {
  SimGptimer* t = (SimGptimer*)h;
  if (!t->running) { t->baseNs = nowNs(); }
  t->running = true;
  return ESP_OK;
}
esp_err_t gptimer_stop(gptimer_handle_t h) {
  SimGptimer* t = (SimGptimer*)h;
  if (t->running) { t->baseCount = t->count(); t->baseNs = nowNs(); }
  t->running = false;
  return ESP_OK;
}
esp_err_t gptimer_set_raw_count(gptimer_handle_t h, uint64_t value) {
  SimGptimer* t = (SimGptimer*)h;
  t->baseCount = value;
  t->baseNs = nowNs();
  return ESP_OK;
}
esp_err_t gptimer_get_raw_count(gptimer_handle_t h, uint64_t* value) { *value = ((SimGptimer*)h)->count(); return ESP_OK; }
esp_err_t gptimer_set_alarm_action(gptimer_handle_t h, const gptimer_alarm_config_t* c) {
  SimGptimer* t = (SimGptimer*)h;
  if (!c) { t->alarmOn = false; return ESP_OK; }
  t->autoReload = c->flags.auto_reload_on_alarm;
  t->reload = c->reload_count;
  t->alarm = c->alarm_count;
  t->alarmOn = true;
  return ESP_OK;
}

struct SimHwTimer {
  uint32_t frequency;
  std::atomic<void (*)()> fn{nullptr};
  std::atomic<bool> running{false}, alarmOn{false}, autoReload{false};
  std::atomic<uint64_t> alarm{0};
  std::atomic<int64_t> startNs{0};
};
static void hwTimerThread(SimHwTimer* t) {
  int64_t next = 0;
  for (;;) {
    void (*fn)() = t->fn;
    if (!t->running || !t->alarmOn || !fn || t->alarm == 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(200));
      next = 0;
      continue;
    }
    int64_t period = (int64_t)((double)t->alarm * 1e9 / t->frequency);
    if (next == 0) next = std::max<int64_t>(t->startNs, nowNs()) + period;
    int64_t wait = next - nowNs();
    if (wait > 150000) { std::this_thread::sleep_for(std::chrono::nanoseconds(wait - 100000)); continue; }  // spin only the last ~0.1 ms
    while (nowNs() < next) {}
    {
      std::lock_guard<std::recursive_mutex> g(irqLock);
      fn();
    }
    if (!t->autoReload) t->alarmOn = false;
    next += period;
    if (next < nowNs() - 10 * period) next = nowNs() + period;  // fell far behind: do not catch up all at once
  }
}
hw_timer_t* timerBegin(uint32_t frequency) {
  SimHwTimer* t = new SimHwTimer();
  t->frequency = frequency ? frequency : 1000000;
  t->running = true;
  t->startNs = nowNs();
  std::thread(hwTimerThread, t).detach();
  return t;
}
void timerEnd(hw_timer_t* t) { t->running = false; }
void timerAttachInterrupt(hw_timer_t* t, void (*fn)()) { t->fn = fn; }
void timerDetachInterrupt(hw_timer_t* t) { t->fn = nullptr; }
void timerAlarm(hw_timer_t* t, uint64_t alarm, bool autoreload, uint64_t) {
  t->alarm = alarm;
  t->autoReload = autoreload;
  t->alarmOn = true;
}
void timerStart(hw_timer_t* t) { t->startNs = nowNs(); t->running = true; }
void timerStop(hw_timer_t* t) { t->running = false; }
void timerWrite(hw_timer_t* t, uint64_t) { t->startNs = nowNs(); }
uint64_t timerRead(hw_timer_t* t) { return (uint64_t)((nowNs() - t->startNs) * (double)t->frequency / 1e9); }

// ------------------------------------------------------------------ restart, reset reason, OTA
static void flushPreferences();
void esp_restart() {
  flushPreferences();
  fflush(nullptr);
  _exit(3);
}
void EspClass::restart() { esp_restart(); }
esp_reset_reason_t esp_reset_reason() {
  std::string r = envOr("H4SIM_RESET", "poweron");
  return r == "sw" ? ESP_RST_SW : r == "panic" ? ESP_RST_PANIC : ESP_RST_POWERON;
}
static const esp_partition_t partApp0 = {"app0", 0x10000, 0x300000}, partApp1 = {"app1", 0x310000, 0x300000};
const esp_partition_t* esp_ota_get_running_partition() { return &partApp0; }
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) { return &partApp1; }
esp_err_t esp_ota_get_state_partition(const esp_partition_t*, esp_ota_img_states_t* state) {
  *state = ESP_OTA_IMG_VALID;
  return ESP_OK;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback() { return ESP_OK; }
bool UpdateClass::begin(size_t, int, int, uint8_t, const char*) { written_ = 0; error_ = 0; finished_ = false; running_ = true; return true; }
size_t UpdateClass::write(uint8_t*, size_t len) { written_ += len; return len; }
bool UpdateClass::end(bool) { running_ = false; finished_ = true; return true; }
void UpdateClass::abort() { running_ = false; finished_ = false; }
wl_status_t WiFiClass::status() { return mode_ == WIFI_AP ? WL_DISCONNECTED : WL_CONNECTED; }

// ------------------------------------------------------------------ Preferences: one file per name space
static std::mutex prefLock;
static std::map<std::string, std::map<std::string, std::string>> prefs;
static std::map<std::string, bool> prefsLoaded;
static std::string prefPath(const std::string& ns) { return dataDir() + "/prefs/" + ns + ".txt"; }
static std::string escape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else o += c;
  }
  return o;
}
static std::string unescape(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\\' && i + 1 < s.size()) { o += s[i + 1] == 'n' ? '\n' : s[i + 1]; i++; }
    else o += s[i];
  }
  return o;
}
static void loadNs(const std::string& ns) {
  if (prefsLoaded[ns]) return;
  prefsLoaded[ns] = true;
  std::ifstream f(prefPath(ns));
  std::string line;
  while (std::getline(f, line)) {
    size_t eq = line.find('=');
    if (eq != std::string::npos) prefs[ns][line.substr(0, eq)] = unescape(line.substr(eq + 1));
  }
}
static void saveNs(const std::string& ns) {
  mkdirs(dataDir() + "/prefs");
  std::string tmp = prefPath(ns) + ".tmp";
  {
    std::ofstream f(tmp);
    for (auto& kv : prefs[ns]) f << kv.first << "=" << escape(kv.second) << "\n";
  }
  ::rename(tmp.c_str(), prefPath(ns).c_str());
}
static void flushPreferences() {}
bool Preferences::begin(const char* ns, bool readOnly, const char*) {
  std::lock_guard<std::mutex> g(prefLock);
  ns_ = ns ? ns : "";
  readOnly_ = readOnly;
  open_ = true;
  loadNs(ns_);
  return true;
}
void Preferences::end() { open_ = false; }
bool Preferences::clear() {
  std::lock_guard<std::mutex> g(prefLock);
  if (!open_ || readOnly_) return false;
  prefs[ns_].clear();
  saveNs(ns_);
  return true;
}
bool Preferences::remove(const char* key) {
  std::lock_guard<std::mutex> g(prefLock);
  if (!open_ || readOnly_) return false;
  bool had = prefs[ns_].erase(key) > 0;
  saveNs(ns_);
  return had;
}
bool Preferences::isKey(const char* key) {
  std::lock_guard<std::mutex> g(prefLock);
  return open_ && prefs[ns_].count(key) > 0;
}
size_t Preferences::putS(const char* k, const std::string& v) {
  std::lock_guard<std::mutex> g(prefLock);
  if (!open_ || readOnly_) return 0;
  prefs[ns_][k] = v;
  saveNs(ns_);
  return v.size() ? v.size() : 1;
}
bool Preferences::getS(const char* k, std::string& out) {
  std::lock_guard<std::mutex> g(prefLock);
  if (!open_) return false;
  auto it = prefs[ns_].find(k);
  if (it == prefs[ns_].end()) return false;
  out = it->second;
  return true;
}
static std::string toHex(const void* v, size_t n) {
  static const char* h = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; i++) { uint8_t b = ((const uint8_t*)v)[i]; s += h[b >> 4]; s += h[b & 15]; }
  return s;
}
size_t Preferences::putBytes(const char* k, const void* v, size_t n) { return putS(k, "hex:" + toHex(v, n)) ? n : 0; }
size_t Preferences::getBytes(const char* k, void* buf, size_t n) {
  std::string s;
  if (!getS(k, s) || s.compare(0, 4, "hex:") != 0) return 0;
  size_t len = std::min(n, (s.size() - 4) / 2);
  for (size_t i = 0; i < len; i++) ((uint8_t*)buf)[i] = (uint8_t)strtoul(s.substr(4 + 2 * i, 2).c_str(), 0, 16);
  return len;
}
size_t Preferences::getBytesLength(const char* k) {
  std::string s;
  return getS(k, s) && s.compare(0, 4, "hex:") == 0 ? (s.size() - 4) / 2 : 0;
}

// ------------------------------------------------------------------ files: a folder on the PC
static std::string fsRoot() { return dataDir() + "/fs"; }
static std::string fsPath(const std::string& p) { return fsRoot() + (p.empty() || p[0] != '/' ? "/" : "") + p; }
static bool isDirPath(const std::string& real) { struct stat st; return stat(real.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
File::File(const std::string& path, const std::string& name, const char* mode, bool dir) : path_(path), name_(name), isDir_(dir) {
  if (dir) return;
  std::string m = mode && *mode ? mode : "r";
  const char* fm = m == "w" ? "wb" : m == "a" ? "ab+" : m == "r+" ? "rb+" : m == "w+" ? "wb+" : "rb";
  FILE* f = fopen(fsPath(path).c_str(), fm);
  if (f) {
    file_ = std::shared_ptr<FILE>(f, [](FILE* p) { fclose(p); });
    fp_ = f;
  }
}
size_t File::write(uint8_t c) { return fp_ && fputc(c, fp_) != EOF ? 1 : 0; }
size_t File::write(const uint8_t* buf, size_t n) { return fp_ ? fwrite(buf, 1, n, fp_) : 0; }
int File::available() {
  if (!fp_) return 0;
  long here = ftell(fp_);
  return (int)std::max<long>(0, (long)size() - here);
}
int File::read() { if (!fp_) return -1; int c = fgetc(fp_); return c == EOF ? -1 : c; }
size_t File::read(uint8_t* buf, size_t n) { return fp_ ? fread(buf, 1, n, fp_) : 0; }
int File::peek() { if (!fp_) return -1; int c = fgetc(fp_); if (c != EOF) ungetc(c, fp_); return c == EOF ? -1 : c; }
bool File::seek(uint32_t pos, SeekMode mode) {
  return fp_ && fseek(fp_, (long)pos, mode == SeekSet ? SEEK_SET : mode == SeekCur ? SEEK_CUR : SEEK_END) == 0;
}
size_t File::position() const { return fp_ ? (size_t)ftell(fp_) : 0; }
size_t File::size() const {
  if (!fp_) return 0;
  fflush(fp_);
  struct stat st;
  return fstat(fileno(fp_), &st) == 0 ? (size_t)st.st_size : 0;
}
void File::close() { if (fp_) fflush(fp_); file_.reset(); fp_ = nullptr; isDir_ = false; }
void File::flush() { if (fp_) fflush(fp_); }
File File::openNextFile(const char* mode) {
  if (!isDir_) return File();
  std::vector<std::string> names;
  DIR* d = opendir(fsPath(path_).c_str());
  if (!d) return File();
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n != "." && n != "..") names.push_back(n);
  }
  closedir(d);
  std::sort(names.begin(), names.end());
  if (dirIndex_ >= names.size()) return File();
  std::string n = names[dirIndex_++];
  std::string p = (path_ == "/" ? "" : path_) + "/" + n;
  return File(p, n, mode, isDirPath(fsPath(p)));
}
bool LittleFSFS::begin(bool, const char*, uint8_t, const char*) { mkdirs(fsRoot()); return true; }
bool LittleFSFS::format() { return true; }
File LittleFSFS::open(const char* path, const char* mode, bool) {
  std::string p = path ? path : "/";
  std::string real = fsPath(p);
  std::string name = p.substr(p.find_last_of('/') + 1);
  if (isDirPath(real)) return File(p, name, mode, true);
  std::string m = mode && *mode ? mode : "r";
  if (m != "r") mkdirs(real.substr(0, real.find_last_of('/')));
  File f(p, name, mode, false);
  return f;
}
bool LittleFSFS::exists(const char* path) { struct stat st; return stat(fsPath(path).c_str(), &st) == 0; }
bool LittleFSFS::remove(const char* path) { return ::remove(fsPath(path).c_str()) == 0; }
bool LittleFSFS::rename(const char* a, const char* b) { return ::rename(fsPath(a).c_str(), fsPath(b).c_str()) == 0; }
bool LittleFSFS::mkdir(const char* path) { mkdirs(fsPath(path)); return true; }
bool LittleFSFS::rmdir(const char* path) { return ::rmdir(fsPath(path).c_str()) == 0; }
static size_t dirBytes(const std::string& real) {
  size_t n = 0;
  DIR* d = opendir(real.c_str());
  if (!d) return 0;
  while (dirent* e = readdir(d)) {
    std::string nm = e->d_name;
    if (nm == "." || nm == "..") continue;
    std::string p = real + "/" + nm;
    struct stat st;
    if (stat(p.c_str(), &st) != 0) continue;
    n += S_ISDIR(st.st_mode) ? dirBytes(p) : (size_t)st.st_size;
  }
  closedir(d);
  return n;
}
size_t LittleFSFS::usedBytes() { return dirBytes(fsRoot()); }

// ------------------------------------------------------------------ HTTP (the firmware's WebServer and the test's)
struct HttpRequest {
  std::string method, path, query, body;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string header(const std::string& k) const {
    for (auto& h : headers) if (strcasecmp(h.first.c_str(), k.c_str()) == 0) return h.second;
    return "";
  }
};
static std::string urlDecode(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), 0, 16); i += 2; }
    else o += s[i];
  }
  return o;
}
static void parseForm(const std::string& s, std::vector<std::pair<String, String>>& out) {
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find('&', a);
    if (b == std::string::npos) b = s.size();
    std::string kv = s.substr(a, b - a);
    if (!kv.empty()) {
      size_t eq = kv.find('=');
      out.push_back({String(urlDecode(kv.substr(0, eq))), String(eq == std::string::npos ? "" : urlDecode(kv.substr(eq + 1)))});
    }
    a = b + 1;
  }
}
static bool readRequest(int fd, HttpRequest& r) {
  std::string buf;
  char tmp[65536];
  size_t headEnd;
  while ((headEnd = buf.find("\r\n\r\n")) == std::string::npos) {
    ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0) return false;
    buf.append(tmp, (size_t)n);
    if (buf.size() > (1 << 20)) return false;
  }
  std::string head = buf.substr(0, headEnd);
  r.body = buf.substr(headEnd + 4);
  std::istringstream hs(head);
  std::string line;
  std::getline(hs, line);
  if (!line.empty() && line.back() == '\r') line.pop_back();
  std::istringstream rl(line);
  std::string target;
  rl >> r.method >> target;
  size_t q = target.find('?');
  r.path = urlDecode(target.substr(0, q));
  r.query = q == std::string::npos ? "" : target.substr(q + 1);
  while (std::getline(hs, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    size_t c = line.find(':');
    if (c == std::string::npos) continue;
    std::string v = line.substr(c + 1);
    while (!v.empty() && v[0] == ' ') v.erase(0, 1);
    r.headers.push_back({line.substr(0, c), v});
  }
  size_t len = (size_t)atol(r.header("Content-Length").c_str());
  while (r.body.size() < len) {
    ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0) return false;
    r.body.append(tmp, (size_t)n);
  }
  return true;
}
static void sendAll(int fd, const std::string& s) {
  size_t a = 0;
  while (a < s.size()) {
    ssize_t n = ::send(fd, s.data() + a, s.size() - a, MSG_NOSIGNAL);
    if (n <= 0) return;
    a += (size_t)n;
  }
}
static const char* reason(int code) {
  switch (code) {
    case 200: return "OK"; case 204: return "No Content"; case 302: return "Found"; case 400: return "Bad Request";
    case 403: return "Forbidden"; case 404: return "Not Found"; case 409: return "Conflict"; case 500: return "Internal Server Error";
    case 503: return "Service Unavailable"; default: return "Status";
  }
}
static int listenOn(int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(s, (sockaddr*)&a, sizeof(a)) != 0 || listen(s, 64) != 0) {
    fprintf(stderr, "h4sim: cannot listen on port %d\n", port);
    _exit(2);
  }
  return s;
}

WebServer::WebServer(int port) : port_(port) { upload_.status = UPLOAD_FILE_START; upload_.totalSize = upload_.currentSize = 0; }
WebServer::~WebServer() { stop(); }
void WebServer::begin() {
  if (listen_ >= 0) return;
  // the H4's port 80 is H4SIM_PORT on the PC
  listen_ = listenOn(port_ == 80 ? simPort() : port_);
  fcntl(listen_, F_SETFL, O_NONBLOCK);
}
void WebServer::stop() { if (listen_ >= 0) ::close(listen_); listen_ = -1; }
String WebServer::arg(const String& name) const {
  for (auto& a : args_) if (a.first == name) return a.second;
  return String();
}
bool WebServer::hasArg(const String& name) const {
  for (auto& a : args_) if (a.first == name) return true;
  return false;
}
String WebServer::header(const String& name) const {
  for (auto& h : reqHeaders_) if (strcasecmp(h.first.c_str(), name.c_str()) == 0) return h.second;
  return String();
}
bool WebServer::hasHeader(const String& name) const {
  for (auto& h : reqHeaders_) if (strcasecmp(h.first.c_str(), name.c_str()) == 0) return true;
  return false;
}
void WebServer::sendHeader(const String& name, const String& value, bool first) {
  if (first) respHeaders_.insert(respHeaders_.begin(), {name, value});
  else respHeaders_.push_back({name, value});
}
void WebServer::send(int code, const char* type, const String& content) {
  if (client_ < 0 || sent_) return;
  std::string r = "HTTP/1.1 " + std::to_string(code) + " " + reason(code) + "\r\n";
  if (type) r += std::string("Content-Type: ") + type + "\r\n";
  r += "Content-Length: " + std::to_string(content.size()) + "\r\n";
  for (auto& h : respHeaders_) r += h.first + ": " + h.second + "\r\n";
  r += "Connection: close\r\n\r\n";
  sendAll(client_, r + content);
  sent_ = true;
}
size_t WebServer::streamFile(File& f, const String& type, int code) {
  std::string s;
  uint8_t b[4096];
  size_t n;
  f.seek(0);
  while ((n = f.read(b, sizeof(b))) > 0) s.append((const char*)b, n);
  send(code, type.c_str(), String(s));
  return s.size();
}
void WebServer::handleClient() {
  if (listen_ < 0) return;
  int fd = accept(listen_, nullptr, nullptr);
  if (fd < 0) return;
  timeval tv = {3, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  serve(fd);
  ::close(fd);
}
void WebServer::serve(int fd) {
  HttpRequest req;
  if (!readRequest(fd, req)) return;
  client_ = fd;
  sent_ = false;
  args_.clear();
  reqHeaders_.clear();
  respHeaders_.clear();
  for (auto& h : req.headers) reqHeaders_.push_back({String(h.first), String(h.second)});
  uri_ = String(req.path);
  method_ = req.method == "POST" ? HTTP_POST : req.method == "PUT" ? HTTP_PUT : req.method == "DELETE" ? HTTP_DELETE
          : req.method == "OPTIONS" ? HTTP_OPTIONS : HTTP_GET;
  parseForm(req.query, args_);
  std::string ctype = req.header("Content-Type");
  const Route* route = nullptr;
  for (auto& r : routes_) {
    if (r.uri == uri_ && (r.method == HTTP_ANY || r.method == method_)) { route = &r; break; }
  }
  if (ctype.find("application/x-www-form-urlencoded") != std::string::npos) {
    parseForm(req.body, args_);
  } else if (ctype.find("multipart/form-data") != std::string::npos) {
    size_t bp = ctype.find("boundary=");
    std::string boundary = "--" + (bp == std::string::npos ? "" : ctype.substr(bp + 9));
    if (!boundary.empty() && boundary.front() == '"') boundary = boundary.substr(1, boundary.size() - 2);
    size_t pos = req.body.find(boundary);
    while (pos != std::string::npos) {
      size_t hs = pos + boundary.size();
      if (req.body.compare(hs, 2, "--") == 0) break;
      hs += 2;  // \r\n
      size_t he = req.body.find("\r\n\r\n", hs);
      if (he == std::string::npos) break;
      std::string ph = req.body.substr(hs, he - hs);
      size_t next = req.body.find("\r\n" + boundary, he + 4);
      if (next == std::string::npos) break;
      std::string data = req.body.substr(he + 4, next - he - 4);
      auto field = [&](const char* k) {
        size_t a = ph.find(std::string(k) + "=\"");
        if (a == std::string::npos) return std::string();
        a += strlen(k) + 2;
        return ph.substr(a, ph.find('"', a) - a);
      };
      std::string name = field("name"), filename = field("filename");
      if (ph.find("filename=") != std::string::npos && route && route->upload) {
        upload_.filename = String(filename);
        upload_.name = String(name);
        upload_.type = String("");
        upload_.totalSize = 0;
        upload_.currentSize = 0;
        upload_.status = UPLOAD_FILE_START;
        route->upload();
        for (size_t a = 0; a < data.size(); a += HTTP_UPLOAD_BUFLEN) {
          size_t n = std::min((size_t)HTTP_UPLOAD_BUFLEN, data.size() - a);
          memcpy(upload_.buf, data.data() + a, n);
          upload_.currentSize = n;
          upload_.totalSize += n;
          upload_.status = UPLOAD_FILE_WRITE;
          route->upload();
        }
        upload_.currentSize = 0;
        upload_.status = UPLOAD_FILE_END;
        route->upload();
      } else {
        args_.push_back({String(name), String(data)});
      }
      pos = next + 2;
    }
  } else if (!req.body.empty()) {
    args_.push_back({String("plain"), String(req.body)});
  }
  if (route) route->fn();
  else if (notFound_) notFound_();
  else send(404, "text/plain", String("Not found: ") + uri_);
  if (!sent_) send(500, "text/plain", String("no answer"));
  client_ = -1;
}

// ------------------------------------------------------------------ the test's own HTTP server
static std::string jsonStr(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if ((unsigned char)c < 32) o += ' ';
    else o += c;
  }
  return o + "\"";
}
static std::string qarg(const std::string& q, const std::string& k) {
  std::vector<std::pair<String, String>> a;
  parseForm(q, a);
  for (auto& p : a) if (p.first == k) return p.second;
  return "";
}
static std::string stateJson() {
  std::string j = "{\"ms\":" + std::to_string(millis()) + ",\"motors\":{";
  for (int i = 0; i < 3; i++) {
    j += (i ? "," : "") + jsonStr(motors[i].name) + ":" + std::to_string(motors[i].steps.load());
  }
  j += "},\"lcd\":[";
  {
    std::lock_guard<std::mutex> g(lcdLock);
    for (int r = 0; r < 4; r++) j += (r ? "," : "") + jsonStr(std::string(lcdText[r], 20));
  }
  j += "],\"inputs\":{";
  int pins[] = {12, 13, 14};
  for (int i = 0; i < 3; i++) j += (i ? "," : "") + jsonStr(std::to_string(pins[i])) + ":" + std::to_string(inputLevel(pins[i]));
  j += "},\"spindleCounts\":" + std::to_string(spindleNow()) + ",\"spindleRpm\":" + std::to_string(spindleRpm.load()) + ",\"beeps\":" + std::to_string(beeps.load()) + ",\"loops\":" + std::to_string(loopCalls.load()) + "}";
  return j;
}
static void controlServer() {
  int port = atoi(envOr("H4SIM_CTL_PORT", std::to_string(simPort() + 1).c_str()).c_str());
  int ls = listenOn(port);
  for (;;) {
    int fd = accept(ls, nullptr, nullptr);
    if (fd < 0) continue;
    timeval tv = {3, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    HttpRequest r;
    std::string body = "ok", type = "text/plain";
    int code = 200;
    if (readRequest(fd, r)) {
      std::string q = r.query;
      if (r.path == "/state") { body = stateJson(); type = "application/json"; }
      else if (r.path == "/trace") {
        // "ms spindle z x a1" per line, from the given millisecond on (the spindle in encoder counts since the start)
        uint32_t from = (uint32_t)atol(qarg(q, "from").c_str());
        std::string o;
        std::lock_guard<std::mutex> g(traceLock);
        size_t start = (traceHead + traceRing.size() - traceCount) % traceRing.size();
        for (size_t i = 0; i < traceCount; i++) {
          const TraceRow& t = traceRing[(start + i) % traceRing.size()];
          if (t.ms < from) continue;
          char line[96];
          snprintf(line, sizeof(line), "%u %lld %lld %lld %lld\n", t.ms, (long long)t.spindle, (long long)t.z, (long long)t.x, (long long)t.a1);
          o += line;
        }
        body = o;
      }
      else if (r.path == "/serial") { std::lock_guard<std::mutex> g(serialLock); body = serialOut; }
      else if (r.path == "/key") {
        int code7 = atoi(qarg(q, "code").c_str()) & 127;
        std::string down = qarg(q, "down");
        std::lock_guard<std::mutex> g(keyLock);
        if (down.empty() || down == "1") keyEvents.push_back(code7 | 128);
        if (down.empty() || down == "0") keyEvents.push_back(code7);
      } else if (r.path == "/spindle") {
        spindleNow();
        spindleRpm = atof(qarg(q, "rpm").c_str());
      } else if (r.path == "/input") {
        int pin = atoi(qarg(q, "pin").c_str());
        std::string lv = qarg(q, "level");
        if (pin >= 0 && pin < 64) inForced[pin] = lv.empty() ? -1 : (int8_t)atoi(lv.c_str());
      } else if (r.path == "/switch") {
        Switch s;
        s.pin = atoi(qarg(q, "pin").c_str());
        std::string m = qarg(q, "motor");
        s.motor = m == "X" ? 1 : m == "A1" ? 2 : 0;
        s.le = qarg(q, "op") != "ge";
        s.steps = atoll(qarg(q, "steps").c_str());
        s.pressedLevel = qarg(q, "pressed") == "1" ? 1 : 0;
        std::lock_guard<std::mutex> g(machineLock);
        switches.erase(std::remove_if(switches.begin(), switches.end(), [&](const Switch& o) { return o.pin == s.pin; }),
                       switches.end());
        switches.push_back(s);
      } else { code = 404; body = "unknown: " + r.path; }
    }
    sendAll(fd, "HTTP/1.1 " + std::to_string(code) + " " + reason(code) + "\r\nContent-Type: " + type +
                    "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    ::close(fd);
  }
}

// ------------------------------------------------------------------ the Arduino core's main()
int main() {
  signal(SIGPIPE, SIG_IGN);
  for (auto& l : outLevel) l = 0;
  for (auto& f : inForced) f = -1;
  for (auto& r : lcdText) { memset(r, ' ', 20); r[20] = 0; }
  mkdirs(dataDir());
  std::thread(controlServer).detach();
  std::thread(traceThread).detach();
  setup();
  for (;;) {
    loop();
    loopCalls++;
  }
}
