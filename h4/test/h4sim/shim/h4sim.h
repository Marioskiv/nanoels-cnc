/*
  h4sim.h - the Arduino, ESP32 and library parts the NanoEls H4 firmware uses, for running the whole firmware on a PC
  (test/h4sim). Every shim header (Arduino.h, WiFi.h, WebServer.h, ...) includes this file; sim.cpp implements what
  is not inline. Only what the firmware uses is here, with the same names and behaviour as far as the firmware can
  see: real threads for its tasks, real time, a real HTTP server for its web page, settings and files kept on disk,
  and the step and direction pins counted as motors (see sim.cpp).
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <type_traits>
#include <memory>

// ------------------------------------------------------------------ attributes, basic types
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_NOINIT_ATTR
#define PROGMEM
#define PGM_P const char*
#define PSTR(s) (s)
#define F(s) (s)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define ESP_INTR_FLAG_IRAM 0x400
#define ESP_INTR_FLAG_LEVEL3 0x8
typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;
#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09
#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define radians(d) ((d) * DEG_TO_RAD)
#define degrees(r) ((r) * RAD_TO_DEG)
#define sq(x) ((x) * (x))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#include "binary.h"
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitWrite(value, bit, b) ((b) ? bitSet(value, bit) : bitClear(value, bit))
inline bool isDigit(int c) { return isdigit(c) != 0; }
inline bool isAlpha(int c) { return isalpha(c) != 0; }
inline bool isAlphaNumeric(int c) { return isalnum(c) != 0; }
inline bool isSpace(int c) { return isspace(c) != 0; }
inline bool isUpperCase(int c) { return isupper(c) != 0; }
inline bool isLowerCase(int c) { return islower(c) != 0; }
inline bool isPrintable(int c) { return isprint(c) != 0; }
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
using std::isnan;
using std::isinf;
// Arduino-ESP32 3.x: min / max work across types (the result has the common type)
template <class A, class B> constexpr typename std::common_type<A, B>::type min(A a, B b) { return b < a ? b : a; }
template <class A, class B> constexpr typename std::common_type<A, B>::type max(A a, B b) { return a < b ? b : a; }

// ------------------------------------------------------------------ String (Arduino's, on std::string)
class String : public std::string {
 public:
  String() {}
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
  String(const String& s) = default;
  String& operator=(const String& s) = default;
  String(char c) : std::string(1, c) {}
  explicit String(unsigned char v, unsigned char base = 10) { fromInt((unsigned long long)v, base, false); }
  explicit String(int v, unsigned char base = 10) { fromInt((long long)v, base, true); }
  explicit String(unsigned int v, unsigned char base = 10) { fromInt((unsigned long long)v, base, false); }
  explicit String(long v, unsigned char base = 10) { fromInt((long long)v, base, true); }
  explicit String(unsigned long v, unsigned char base = 10) { fromInt((unsigned long long)v, base, false); }
  explicit String(long long v, unsigned char base = 10) { fromInt(v, base, true); }
  explicit String(unsigned long long v, unsigned char base = 10) { fromInt(v, base, false); }
  explicit String(float v, unsigned int decimals = 2) { fromDouble(v, decimals); }
  explicit String(double v, unsigned int decimals = 2) { fromDouble(v, decimals); }
  unsigned int length() const { return (unsigned int)size(); }
  bool isEmpty() const { return empty(); }
  char charAt(unsigned int i) const { return i < size() ? (*this)[i] : 0; }
  void setCharAt(unsigned int i, char c) { if (i < size()) (*this)[i] = c; }
  String substring(unsigned int a) const { return a >= size() ? String() : String(std::string::substr(a)); }
  String substring(unsigned int a, unsigned int b) const {
    if (a > b) std::swap(a, b);
    if (a >= size()) return String();
    return String(std::string::substr(a, std::min((size_t)b, size()) - a));
  }
  int indexOf(char c, unsigned int from = 0) const { size_t p = find(c, from); return p == npos ? -1 : (int)p; }
  int indexOf(const String& s, unsigned int from = 0) const { size_t p = find(s, from); return p == npos ? -1 : (int)p; }
  int indexOf(const char* s, unsigned int from = 0) const { size_t p = find(s, from); return p == npos ? -1 : (int)p; }
  int lastIndexOf(char c) const { size_t p = rfind(c); return p == npos ? -1 : (int)p; }
  int lastIndexOf(const String& s) const { size_t p = rfind(s); return p == npos ? -1 : (int)p; }
  bool startsWith(const String& s) const { return compare(0, s.size(), s) == 0; }
  bool endsWith(const String& s) const { return size() >= s.size() && compare(size() - s.size(), s.size(), s) == 0; }
  bool equals(const String& s) const { return *this == s; }
  bool equalsIgnoreCase(const String& s) const {
    if (size() != s.size()) return false;
    for (size_t i = 0; i < size(); i++) if (tolower((unsigned char)(*this)[i]) != tolower((unsigned char)s[i])) return false;
    return true;
  }
  long toInt() const { return atol(c_str()); }
  float toFloat() const { return (float)atof(c_str()); }
  double toDouble() const { return atof(c_str()); }
  void trim() {
    size_t a = 0, b = size();
    while (a < b && isspace((unsigned char)(*this)[a])) a++;
    while (b > a && isspace((unsigned char)(*this)[b - 1])) b--;
    *this = String(std::string::substr(a, b - a));
  }
  void toUpperCase() { for (auto& c : *this) c = (char)toupper((unsigned char)c); }
  void toLowerCase() { for (auto& c : *this) c = (char)tolower((unsigned char)c); }
  void replace(const String& a, const String& b) {
    if (a.empty()) return;
    size_t p = 0;
    while ((p = find(a, p)) != npos) { std::string::replace(p, a.size(), b); p += b.size(); }
  }
  void replace(char a, char b) { for (auto& c : *this) if (c == a) c = b; }
  void remove(unsigned int i) { if (i < size()) erase(i); }
  void remove(unsigned int i, unsigned int n) { if (i < size()) erase(i, n); }
  bool reserve(unsigned int n) { std::string::reserve(n); return true; }
  bool concat(const String& s) { append(s); return true; }
  bool concat(const char* s) { if (s) append(s); return true; }
  bool concat(char c) { push_back(c); return true; }
  void getBytes(unsigned char* buf, unsigned int n, unsigned int from = 0) const {
    if (!n) return;
    size_t k = from < size() ? std::min((size_t)n - 1, size() - from) : 0;
    if (k) memcpy(buf, data() + from, k);
    buf[k] = 0;
  }
  void toCharArray(char* buf, unsigned int n, unsigned int from = 0) const { getBytes((unsigned char*)buf, n, from); }
  String& operator+=(const String& s) { append(s); return *this; }
  String& operator+=(const char* s) { if (s) append(s); return *this; }
  String& operator+=(char c) { push_back(c); return *this; }
  template <class T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value, int>::type = 0>
  String& operator+=(T v) { append(String(v)); return *this; }

 private:
  void fromInt(long long v, unsigned char base, bool sign) {
    if (base == 10) { char b[32]; snprintf(b, sizeof(b), sign ? "%lld" : "%llu", v); assign(b); return; }
    unsigned long long u = (unsigned long long)v;
    if (sign && v < 0) u = (unsigned long long)(-v);
    std::string s;
    do { int d = (int)(u % base); s.insert(s.begin(), (char)(d < 10 ? '0' + d : 'a' + d - 10)); u /= base; } while (u);
    if (sign && v < 0) s.insert(s.begin(), '-');
    assign(s);
  }
  void fromDouble(double v, unsigned int decimals) {
    if (isnan(v)) { assign("nan"); return; }
    if (isinf(v)) { assign(v > 0 ? "inf" : "-inf"); return; }
    char b[64]; snprintf(b, sizeof(b), "%.*f", (int)decimals, v); assign(b);
  }
};
inline String operator+(const String& a, const String& b) { String r(a); r.append(b); return r; }
inline String operator+(const String& a, const char* b) { String r(a); if (b) r.append(b); return r; }
inline String operator+(const char* a, const String& b) { String r(a); r.append(b); return r; }
inline String operator+(const String& a, char c) { String r(a); r.push_back(c); return r; }
inline String operator+(char c, const String& b) { String r; r.push_back(c); r.append(b); return r; }
template <class T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value, int>::type = 0>
inline String operator+(const String& a, T v) { return a + String(v); }
template <class T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value, int>::type = 0>
inline String operator+(T v, const String& b) { return String(v) + b; }

// ------------------------------------------------------------------ Print, Stream, Serial
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) { size_t k = 0; while (n--) k += write(*buf++); return k; }
  size_t write(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
  size_t write(const char* s, size_t n) { return write((const uint8_t*)s, n); }
  size_t print(const char* s) { return write(s); }
  size_t print(const String& s) { return write((const uint8_t*)s.data(), s.size()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char v, int base = 10) { return print(String((unsigned long)v, (unsigned char)base)); }
  size_t print(int v, int base = 10) { return print(String((long)v, (unsigned char)base)); }
  size_t print(unsigned int v, int base = 10) { return print(String((unsigned long)v, (unsigned char)base)); }
  size_t print(long v, int base = 10) { return print(String(v, (unsigned char)base)); }
  size_t print(unsigned long v, int base = 10) { return print(String(v, (unsigned char)base)); }
  size_t print(long long v, int base = 10) { return print(String(v, (unsigned char)base)); }
  size_t print(unsigned long long v, int base = 10) { return print(String(v, (unsigned char)base)); }
  size_t print(double v, int digits = 2) { return print(String(v, (unsigned int)digits)); }
  size_t println() { return write("\r\n"); }
  template <class T> size_t println(const T& v) { size_t n = print(v); return n + println(); }
  template <class T> size_t println(const T& v, int f) { size_t n = print(v, f); return n + println(); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char b[512]; va_list ap; va_start(ap, fmt); int n = vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    return write((const uint8_t*)b, (size_t)std::max(0, std::min(n, (int)sizeof(b) - 1)));
  }
  virtual void flush() {}
};
class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() { return -1; }
  String readString() { String s; int c; while ((c = read()) >= 0) s += (char)c; return s; }
  String readStringUntil(char t) { String s; int c; while ((c = read()) >= 0 && c != t) s += (char)c; return s; }
  size_t readBytes(char* buf, size_t n) { size_t k = 0; int c; while (k < n && (c = read()) >= 0) buf[k++] = (char)c; return k; }
  void setTimeout(unsigned long) {}
};
class HardwareSerial : public Stream {
 public:
  void begin(unsigned long, ...) {}
  void end() {}
  size_t write(uint8_t c) override;
  using Print::write;
  int available() override;
  int read() override;
  operator bool() const { return true; }
  void setRxBufferSize(size_t) {}
  void setTxTimeoutMs(uint32_t) {}
};
extern HardwareSerial Serial;
typedef HardwareSerial HWCDC;

// ------------------------------------------------------------------ time, pins, interrupts
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
inline void yield() {}
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t level);
int digitalRead(uint8_t pin);
inline int analogRead(uint8_t) { return 0; }
inline uint8_t digitalPinToInterrupt(uint8_t p) { return p; }
void attachInterrupt(uint8_t pin, void (*fn)(), int mode);
void detachInterrupt(uint8_t pin);
inline uint32_t getCpuFrequencyMhz() { return 240; }
inline bool psramFound() { return false; }
inline void* ps_malloc(size_t n) { return malloc(n); }
void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t pin);
inline long random(long a, long b) { return a + (b > a ? ::random() % (b - a) : 0); }
inline long random(long b) { return random(0, b); }
inline void randomSeed(unsigned long s) { srandom((unsigned)s); }

// the GPIO registers the step interrupt and the homing code use
#define GPIO_OUT_W1TS_REG 0x3FF44008u
#define GPIO_OUT_W1TC_REG 0x3FF4400Cu
#define GPIO_OUT1_W1TS_REG 0x3FF44014u
#define GPIO_OUT1_W1TC_REG 0x3FF44018u
#define GPIO_IN_REG 0x3FF4403Cu
#define GPIO_IN1_REG 0x3FF44040u
void sim_reg_write(uint32_t addr, uint32_t value);
uint32_t sim_reg_read(uint32_t addr);
#define REG_WRITE(addr, v) sim_reg_write((uint32_t)(addr), (uint32_t)(v))
#define REG_READ(addr) sim_reg_read((uint32_t)(addr))

// ------------------------------------------------------------------ FreeRTOS
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
struct SimQueue;
typedef SimQueue* QueueHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xffffffffu
#define portTICK_PERIOD_MS 1
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define tskIDLE_PRIORITY 0
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, uint32_t stack, void* param, UBaseType_t prio,
                                   TaskHandle_t* handle, BaseType_t core);
inline BaseType_t xTaskCreate(void (*fn)(void*), const char* name, uint32_t stack, void* param, UBaseType_t prio,
                              TaskHandle_t* handle) { return xTaskCreatePinnedToCore(fn, name, stack, param, prio, handle, 0); }
void vTaskDelay(TickType_t ticks);
void vTaskDelete(TaskHandle_t t);
TickType_t xTaskGetTickCount();
inline BaseType_t xPortGetCoreID() { return 0; }
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }
inline void taskYIELD() {}
QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize);
BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t wait);
BaseType_t xQueueReset(QueueHandle_t q);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q);
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t q);
#define xQueueSendToBack xQueueSend
// mutexes (recursive is fine: the firmware never takes one twice on the same task)
struct SimSemaphore { std::recursive_timed_mutex m; };
typedef SimSemaphore* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new SimSemaphore(); }
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return new SimSemaphore(); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait);
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { s->m.unlock(); return pdTRUE; }
#define xSemaphoreTakeRecursive xSemaphoreTake
#define xSemaphoreGiveRecursive xSemaphoreGive
// "interrupts off": the simulated interrupts (step timer, async timer, pin interrupts) run under this lock, so a task
// that switches interrupts off is not interrupted, as on one ESP32 core
void noInterrupts();
void interrupts();
#define portDISABLE_INTERRUPTS() noInterrupts()
#define portENABLE_INTERRUPTS() interrupts()
struct portMUX_TYPE { std::recursive_mutex m; };
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->m.lock()
#define portEXIT_CRITICAL(mux) (mux)->m.unlock()
#define portENTER_CRITICAL_ISR(mux) (mux)->m.lock()
#define portEXIT_CRITICAL_ISR(mux) (mux)->m.unlock()
#define taskENTER_CRITICAL(mux) (mux)->m.lock()
#define taskEXIT_CRITICAL(mux) (mux)->m.unlock()

// ------------------------------------------------------------------ ESP-IDF
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
inline const char* esp_err_to_name(esp_err_t e) { return e == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }
int64_t esp_timer_get_time();
uint32_t esp_cpu_get_cycle_count();
inline void esp_rom_delay_us(uint32_t us) { delayMicroseconds(us); }
typedef enum {
  ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT, ESP_RST_TASK_WDT,
  ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO, ESP_RST_USB, ESP_RST_JTAG
} esp_reset_reason_t;
esp_reset_reason_t esp_reset_reason();
void esp_restart();
typedef struct { const char* label; uint32_t address; uint32_t size; } esp_partition_t;
typedef enum {
  ESP_OTA_IMG_NEW, ESP_OTA_IMG_PENDING_VERIFY, ESP_OTA_IMG_VALID, ESP_OTA_IMG_INVALID, ESP_OTA_IMG_ABORTED,
  ESP_OTA_IMG_UNDEFINED
} esp_ota_img_states_t;
const esp_partition_t* esp_ota_get_running_partition();
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* from);
esp_err_t esp_ota_get_state_partition(const esp_partition_t* p, esp_ota_img_states_t* state);
esp_err_t esp_ota_mark_app_valid_cancel_rollback();
class EspClass {
 public:
  void restart();
  uint32_t getFreeHeap() { return 200000; }
  uint32_t getMinFreeHeap() { return 150000; }
  uint32_t getHeapSize() { return 320000; }
  uint32_t getFreePsram() { return 0; }
  uint32_t getPsramSize() { return 0; }
  uint32_t getMaxAllocHeap() { return 100000; }
  const char* getChipModel() { return "PC (h4sim)"; }
  uint32_t getFlashChipSize() { return 16u << 20; }
  const char* getSdkVersion() { return "h4sim"; }
};
extern EspClass ESP;

// general purpose timer (the step timer): the callback is called on a thread of its own at the alarm times
typedef struct SimGptimer* gptimer_handle_t;
typedef enum { GPTIMER_CLK_SRC_DEFAULT } gptimer_clock_source_t;
typedef enum { GPTIMER_COUNT_DOWN, GPTIMER_COUNT_UP } gptimer_count_direction_t;
typedef struct {
  gptimer_clock_source_t clk_src;
  gptimer_count_direction_t direction;
  uint32_t resolution_hz;
  int intr_priority;
  struct { uint32_t intr_shared : 1; uint32_t allow_pd : 1; uint32_t backup_before_sleep : 1; } flags;
} gptimer_config_t;
typedef struct { uint64_t count_value; uint64_t alarm_value; } gptimer_alarm_event_data_t;
typedef bool (*gptimer_alarm_cb_t)(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* ctx);
typedef struct { gptimer_alarm_cb_t on_alarm; } gptimer_event_callbacks_t;
typedef struct {
  uint64_t alarm_count;
  uint64_t reload_count;
  struct { uint32_t auto_reload_on_alarm : 1; } flags;
} gptimer_alarm_config_t;
esp_err_t gptimer_new_timer(const gptimer_config_t* config, gptimer_handle_t* ret);
esp_err_t gptimer_register_event_callbacks(gptimer_handle_t t, const gptimer_event_callbacks_t* cbs, void* ctx);
esp_err_t gptimer_enable(gptimer_handle_t t);
esp_err_t gptimer_disable(gptimer_handle_t t);
esp_err_t gptimer_start(gptimer_handle_t t);
esp_err_t gptimer_stop(gptimer_handle_t t);
esp_err_t gptimer_set_raw_count(gptimer_handle_t t, uint64_t value);
esp_err_t gptimer_get_raw_count(gptimer_handle_t t, uint64_t* value);
esp_err_t gptimer_set_alarm_action(gptimer_handle_t t, const gptimer_alarm_config_t* config);
// the timer group is not given out: the firmware then uses its 1.19 step timer (gptimer callback)
inline esp_err_t gptimer_get_group_id(gptimer_handle_t, int* group) { *group = -1; return ESP_FAIL; }
#define SOC_TIMER_GROUPS 2
typedef struct { struct { uint32_t val; } int_raw_timers; } timg_dev_t;
typedef void* intr_handle_t;
typedef void (*intr_handler_t)(void* arg);
extern timg_dev_t sim_timg[2];
#define TIMER_LL_GET_HW(g) (&sim_timg[g])
#define TIMER_LL_EVENT_ALARM(n) (1u << (n))
inline uint32_t timer_ll_get_intr_status(timg_dev_t*) { return 0; }
inline void timer_ll_trigger_soft_capture(timg_dev_t*, int) {}
inline uint64_t timer_ll_get_counter_value(timg_dev_t*, int) { return 0; }
inline void timer_ll_clear_intr_status(timg_dev_t*, uint32_t) {}
inline void timer_ll_set_alarm_value(timg_dev_t*, int, uint64_t) {}
inline void timer_ll_enable_alarm(timg_dev_t*, int, bool) {}
inline void timer_ll_enable_intr(timg_dev_t*, uint32_t, bool) {}
inline uint32_t timer_ll_get_intr_status_reg(timg_dev_t*) { return 0; }
struct SimTimerSignals { struct { int timer_irq_id[2]; } groups[2]; };
extern const SimTimerSignals timer_group_periph_signals;
inline esp_err_t esp_intr_alloc_intrstatus(int, int, uint32_t, uint32_t, intr_handler_t, void*, intr_handle_t*) { return ESP_FAIL; }

// hardware timer of the Arduino core (the asynchronous mode's timer): a thread of its own calls the handler
struct SimHwTimer;
typedef SimHwTimer hw_timer_t;
hw_timer_t* timerBegin(uint32_t frequency);
void timerEnd(hw_timer_t* t);
void timerAttachInterrupt(hw_timer_t* t, void (*fn)());
void timerDetachInterrupt(hw_timer_t* t);
void timerAlarm(hw_timer_t* t, uint64_t alarm, bool autoreload, uint64_t reloadCount);
void timerStart(hw_timer_t* t);
void timerStop(hw_timer_t* t);
void timerWrite(hw_timer_t* t, uint64_t value);
uint64_t timerRead(hw_timer_t* t);

// pulse counter (the spindle encoder): counts the simulated spindle (sim.cpp)
typedef enum { PCNT_UNIT_0, PCNT_UNIT_1, PCNT_UNIT_2, PCNT_UNIT_3 } pcnt_unit_t;
typedef enum { PCNT_CHANNEL_0, PCNT_CHANNEL_1 } pcnt_channel_t;
typedef enum { PCNT_COUNT_DIS, PCNT_COUNT_INC, PCNT_COUNT_DEC } pcnt_count_mode_t;
typedef enum { PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE } pcnt_ctrl_mode_t;
typedef struct {
  int pulse_gpio_num, ctrl_gpio_num;
  pcnt_ctrl_mode_t lctrl_mode, hctrl_mode;
  pcnt_count_mode_t pos_mode, neg_mode;
  int16_t counter_h_lim, counter_l_lim;
  pcnt_unit_t unit;
  pcnt_channel_t channel;
} pcnt_config_t;
esp_err_t pcnt_unit_config(const pcnt_config_t* c);
inline esp_err_t pcnt_set_filter_value(pcnt_unit_t, uint16_t) { return ESP_OK; }
inline esp_err_t pcnt_filter_enable(pcnt_unit_t) { return ESP_OK; }
inline esp_err_t pcnt_counter_pause(pcnt_unit_t) { return ESP_OK; }
esp_err_t pcnt_counter_clear(pcnt_unit_t u);
inline esp_err_t pcnt_counter_resume(pcnt_unit_t) { return ESP_OK; }
esp_err_t pcnt_get_counter_value(pcnt_unit_t u, int16_t* count);

// ------------------------------------------------------------------ Wire, SPI, LCD, keypad
class TwoWire {
 public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
  void setClock(uint32_t) {}
};
extern TwoWire Wire;
class LiquidCrystal : public Print {
 public:
  template <class... P> LiquidCrystal(P...) {}
  void begin(uint8_t cols, uint8_t rows, uint8_t = 0) { (void)cols; (void)rows; }
  void clear() { simCol = simRow = 0; }
  void home() { simCol = simRow = 0; }
  void setCursor(uint8_t c, uint8_t r) { simCol = c; simRow = r; }
  void createChar(uint8_t, uint8_t*) {}
  void createChar(uint8_t, const uint8_t*) {}
  void noCursor() {}
  void cursor() {}
  void noBlink() {}
  void blink() {}
  void display() {}
  void noDisplay() {}
  size_t write(uint8_t) override;
  using Print::write;
  uint8_t simCol = 0, simRow = 0;
};
#define TCA8418_DEFAULT_ADDR 0x34
class Adafruit_TCA8418 {
 public:
  template <class... P> bool begin(P...) { return true; }
  bool matrix(uint8_t, uint8_t) { return true; }
  void flush();
  int available();
  int getEvent();
  void enableInterrupts() {}
  void disableInterrupts() {}
  void enableDebounce() {}
};

// ------------------------------------------------------------------ Preferences (kept on disk)
class Preferences {
 public:
  bool begin(const char* ns, bool readOnly = false, const char* = nullptr);
  void end();
  bool clear();
  bool remove(const char* key);
  bool isKey(const char* key);
  size_t freeEntries() { return 500; }
  size_t putLong(const char* k, int32_t v) { return putS(k, std::to_string(v)); }
  int32_t getLong(const char* k, int32_t d = 0) { std::string s; return getS(k, s) ? (int32_t)atol(s.c_str()) : d; }
  size_t putInt(const char* k, int32_t v) { return putS(k, std::to_string(v)); }
  int32_t getInt(const char* k, int32_t d = 0) { return getLong(k, d); }
  size_t putUInt(const char* k, uint32_t v) { return putS(k, std::to_string(v)); }
  uint32_t getUInt(const char* k, uint32_t d = 0) { std::string s; return getS(k, s) ? (uint32_t)strtoul(s.c_str(), 0, 10) : d; }
  size_t putULong(const char* k, uint32_t v) { return putUInt(k, v); }
  uint32_t getULong(const char* k, uint32_t d = 0) { return getUInt(k, d); }
  size_t putChar(const char* k, int8_t v) { return putInt(k, v); }
  int8_t getChar(const char* k, int8_t d = 0) { return (int8_t)getLong(k, d); }
  size_t putUChar(const char* k, uint8_t v) { return putInt(k, v); }
  uint8_t getUChar(const char* k, uint8_t d = 0) { return (uint8_t)getLong(k, d); }
  size_t putShort(const char* k, int16_t v) { return putInt(k, v); }
  int16_t getShort(const char* k, int16_t d = 0) { return (int16_t)getLong(k, d); }
  size_t putBool(const char* k, bool v) { return putS(k, v ? "1" : "0"); }
  bool getBool(const char* k, bool d = false) { std::string s; return getS(k, s) ? s == "1" : d; }
  size_t putFloat(const char* k, float v) { char b[64]; snprintf(b, sizeof(b), "%.9g", v); return putS(k, b); }
  float getFloat(const char* k, float d = NAN) { std::string s; return getS(k, s) ? (float)atof(s.c_str()) : d; }
  size_t putDouble(const char* k, double v) { char b[64]; snprintf(b, sizeof(b), "%.17g", v); return putS(k, b); }
  double getDouble(const char* k, double d = NAN) { std::string s; return getS(k, s) ? atof(s.c_str()) : d; }
  size_t putString(const char* k, const char* v) { return putS(k, v ? v : ""); }
  size_t putString(const char* k, const String& v) { return putS(k, v); }
  String getString(const char* k, const String& d = String()) { std::string s; return getS(k, s) ? String(s) : d; }
  size_t getString(const char* k, char* buf, size_t n) {
    std::string s; if (!getS(k, s) || !n) return 0; size_t c = std::min(n - 1, s.size()); memcpy(buf, s.data(), c); buf[c] = 0; return c;
  }
  size_t putBytes(const char* k, const void* v, size_t n);
  size_t getBytes(const char* k, void* buf, size_t n);
  size_t getBytesLength(const char* k);

 private:
  size_t putS(const char* k, const std::string& v);
  bool getS(const char* k, std::string& out);
  std::string ns_;
  bool open_ = false, readOnly_ = false;
};

// ------------------------------------------------------------------ files (a folder on the PC)
#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"
enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };
class File : public Stream {
 public:
  File() {}
  File(const std::string& path, const std::string& name, const char* mode, bool dir);
  ~File() override { }
  operator bool() const { return fp_ != nullptr || isDir_; }
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t n) override;
  using Print::write;
  int available() override;
  int read() override;
  size_t read(uint8_t* buf, size_t n);
  int peek() override;
  bool seek(uint32_t pos, SeekMode mode = SeekSet);
  size_t position() const;
  size_t size() const;
  void close();
  void flush() override;
  const char* name() const { return name_.c_str(); }
  const char* path() const { return path_.c_str(); }
  bool isDirectory() const { return isDir_; }
  File openNextFile(const char* mode = "r");
  void rewindDirectory() { dirIndex_ = 0; }

 private:
  std::shared_ptr<FILE> file_;
  FILE* fp_ = nullptr;
  std::string path_, name_;
  bool isDir_ = false;
  size_t dirIndex_ = 0;
};
class LittleFSFS {
 public:
  bool begin(bool formatOnFail = false, const char* = "/littlefs", uint8_t = 10, const char* = "spiffs");
  void end() {}
  bool format();
  File open(const char* path, const char* mode = "r", bool create = false);
  File open(const String& path, const char* mode = "r", bool create = false) { return open(path.c_str(), mode, create); }
  bool exists(const char* path);
  bool exists(const String& path) { return exists(path.c_str()); }
  bool remove(const char* path);
  bool remove(const String& path) { return remove(path.c_str()); }
  bool rename(const char* a, const char* b);
  bool rename(const String& a, const String& b) { return rename(a.c_str(), b.c_str()); }
  bool mkdir(const char* path);
  bool mkdir(const String& path) { return mkdir(path.c_str()); }
  bool rmdir(const char* path);
  size_t totalBytes() { return 1536u * 1024u; }
  size_t usedBytes();
};
extern LittleFSFS LittleFS;
namespace fs { typedef ::File File; typedef LittleFSFS FS; }

// ------------------------------------------------------------------ firmware update (accepted and dropped)
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFFu
#define U_FLASH 0
class UpdateClass {
 public:
  bool begin(size_t size = UPDATE_SIZE_UNKNOWN, int command = U_FLASH, int = -1, uint8_t = 0, const char* = nullptr);
  size_t write(uint8_t* data, size_t len);
  bool end(bool evenIfRemaining = false);
  void abort();
  bool hasError() const { return error_ != 0; }
  bool isFinished() const { return finished_; }
  const char* errorString() const { return error_ ? "update failed (h4sim)" : "No Error"; }
  size_t progress() const { return written_; }
 private:
  size_t written_ = 0;
  int error_ = 0;
  bool finished_ = false, running_ = false;
};
extern UpdateClass Update;

// ------------------------------------------------------------------ WiFi
class IPAddress {
 public:
  IPAddress(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) : a_{a, b, c, d} {}
  String toString() const { char s[20]; snprintf(s, sizeof(s), "%u.%u.%u.%u", a_[0], a_[1], a_[2], a_[3]); return String(s); }
  uint8_t operator[](int i) const { return a_[i]; }
  operator uint32_t() const { return (uint32_t)a_[0] | (uint32_t)a_[1] << 8 | (uint32_t)a_[2] << 16 | (uint32_t)a_[3] << 24; }
 private:
  uint8_t a_[4];
};
typedef enum { WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3, WL_CONNECT_FAILED = 4, WL_DISCONNECTED = 6 } wl_status_t;
typedef enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 } wifi_mode_t;
typedef enum { WIFI_POWER_19_5dBm = 78 } wifi_power_t;
// Events and scan results as arduino-esp32 3.x names them (wifi.ino prints them while joining)
typedef enum { ARDUINO_EVENT_WIFI_STA_CONNECTED = 4, ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 5, ARDUINO_EVENT_WIFI_STA_GOT_IP = 7 } arduino_event_id_t;
typedef int wifi_err_reason_t;
typedef int wifi_auth_mode_t;
struct arduino_event_info_t { struct { int reason; } wifi_sta_disconnected; };
class WiFiClass {
 public:
  bool mode(wifi_mode_t m) { mode_ = m; return true; }
  wl_status_t begin(const char* ssid, const char* = nullptr, ...) { ssid_ = ssid ? ssid : ""; return WL_CONNECTED; }
  wl_status_t begin(const String& ssid, const String& pw) { return begin(ssid.c_str(), pw.c_str()); }
  wl_status_t status();
  bool disconnect(bool = false, bool = false) { return true; }
  bool softAP(const char*, const char* = nullptr, ...) { return true; }
  bool softAP(const String& s, const String& p) { return softAP(s.c_str(), p.c_str()); }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  IPAddress localIP() { return IPAddress(127, 0, 0, 1); }
  bool setSleep(bool) { return true; }
  void persistent(bool) {}
  bool setAutoReconnect(bool) { return true; }
  bool setTxPower(wifi_power_t) { return true; }
  bool setHostname(const char*) { return true; }
  int8_t RSSI() { return -50; }
  String SSID() { return String(ssid_.c_str()); }
  wifi_mode_t getMode() { return mode_; }
  uint8_t softAPgetStationNum() { return 0; }
  int onEvent(std::function<void(arduino_event_id_t, arduino_event_info_t)>) { return 0; }
  int16_t scanNetworks() { return 0; } // no networks around the PC
  String SSID(uint8_t) { return String(""); }
  int32_t RSSI(uint8_t) { return 0; }
  int32_t channel(uint8_t) { return 0; }
  wifi_auth_mode_t encryptionType(uint8_t) { return 0; }
  const char* disconnectReasonName(wifi_err_reason_t) { return "h4sim"; }
  String macAddress() { return String("02:00:00:00:00:01"); }
 private:
  wifi_mode_t mode_ = WIFI_OFF;
  std::string ssid_;
};
extern WiFiClass WiFi;
// TCP port 23 for Grbl senders: no sender connects in the simulation (the H4 is used from its app)
class WiFiClient : public Stream {
 public:
  operator bool() const { return false; }
  bool connected() { return false; }
  int available() override { return 0; }
  int read() override { return -1; }
  size_t write(uint8_t) override { return 1; }
  using Print::write;
  void stop() {}
  void setNoDelay(bool) {}
  IPAddress remoteIP() { return IPAddress(); }
};
class WiFiServer {
 public:
  WiFiServer(uint16_t = 23) {}
  void begin() {}
  void setNoDelay(bool) {}
  WiFiClient accept() { return WiFiClient(); }
  WiFiClient available() { return WiFiClient(); }
  bool hasClient() { return false; }
};

// ------------------------------------------------------------------ WebServer (a real HTTP server on the PC)
typedef enum { HTTP_ANY = 0, HTTP_GET = 1, HTTP_POST = 3, HTTP_PUT = 4, HTTP_DELETE = 2, HTTP_OPTIONS = 6 } HTTPMethod;
typedef enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED } HTTPUploadStatus;
#define HTTP_UPLOAD_BUFLEN 1436
struct HTTPUpload {
  HTTPUploadStatus status;
  String filename, name, type;
  size_t totalSize, currentSize;
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
};
class WebServer {
 public:
  typedef std::function<void(void)> THandlerFunction;
  WebServer(int port = 80);
  ~WebServer();
  void begin();
  void begin(uint16_t port) { port_ = port; begin(); }
  void stop();
  void close() { stop(); }
  void handleClient();
  void on(const String& uri, THandlerFunction fn) { on(uri, HTTP_ANY, fn); }
  void on(const String& uri, HTTPMethod m, THandlerFunction fn) { routes_.push_back({uri, m, fn, nullptr}); }
  void on(const String& uri, HTTPMethod m, THandlerFunction fn, THandlerFunction upload) { routes_.push_back({uri, m, fn, upload}); }
  void onNotFound(THandlerFunction fn) { notFound_ = fn; }
  String arg(const String& name) const;
  String arg(int i) const { return i < (int)args_.size() ? args_[i].second : String(); }
  String argName(int i) const { return i < (int)args_.size() ? args_[i].first : String(); }
  int args() const { return (int)args_.size(); }
  bool hasArg(const String& name) const;
  String header(const String& name) const;
  bool hasHeader(const String& name) const;
  String uri() const { return uri_; }
  HTTPMethod method() const { return method_; }
  HTTPUpload& upload() { return upload_; }
  void sendHeader(const String& name, const String& value, bool first = false);
  void send(int code, const char* type = nullptr, const String& content = String());
  void send(int code, const String& type, const String& content) { send(code, type.c_str(), content); }
  void send(int code, const char* type, const char* content) { send(code, type, String(content)); }
  void send_P(int code, const char* type, const char* content) { send(code, type, String(content)); }
  void send_P(int code, const char* type, const char* content, size_t len) { send(code, type, String(std::string(content, len))); }
  void setContentLength(size_t) {}
  size_t streamFile(File& f, const String& type, int code = 200);
  void enableCORS(bool on = true) { cors_ = on; }
  void collectHeaders(const char**, size_t) {}

 private:
  struct Route { String uri; HTTPMethod method; THandlerFunction fn, upload; };
  void serve(int fd);
  int port_, listen_ = -1, client_ = -1;
  std::vector<Route> routes_;
  THandlerFunction notFound_;
  std::vector<std::pair<String, String>> args_, reqHeaders_, respHeaders_;
  String uri_;
  HTTPMethod method_ = HTTP_GET;
  HTTPUpload upload_;
  bool sent_ = false, cors_ = false;
};

// ------------------------------------------------------------------ the simulated machine (sim.cpp)
// The sketch's own setup() / loop(), run by sim.cpp's main() as the Arduino core does.
void setup();
void loop();
