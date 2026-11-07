/*
  link_test.cpp - a program the app was watching pauses when the app's connection is lost (firmware 1.26,
  h4.ino cncLinkWatch, compiled unchanged from h4.ino). The app asks for /status about every 0.35 s; here a simulated
  clock, the app's requests and cncService() every 20 ms. Checked: no pause while the app answers (also with slow
  answers), a pause 3 s after it stops, one log line, Resume while the app is still away does not pause again at once,
  armed again when the app is back; never for a program started with no app, with the setting 0, during a Grbl
  sender's session, during an M0 wait (but after it, if the app is still away), or when the program has ended.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <stdio.h>
#include <string>
#include <vector>

#define MODE_GCODE 9
static unsigned long nowMs = 0;
unsigned long millis() { return nowMs; }
bool isOn = false;
int mode = MODE_GCODE;
volatile bool grblSession = false;
long CNC_LINK_LOSS_PAUSE_S = 3;
volatile unsigned long cncAppSeenMs = 0;
bool cncLinkArmed = false;
volatile bool cncLinkHold = false;
volatile bool cncUserHold = false;
volatile bool cncPaused = false;
std::vector<std::string> logLines;
void cncLog(const char* t) { logLines.push_back(t); }

#include "link_funcs.inc"

static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) { failures++; printf("FAIL: %s\n", what); }
}

// runs for ms milliseconds: cncService() every 20 ms, the app's request every pollMs (0: none);
// returns when the program paused, counted from the app's last request, or -1
static long run(unsigned long ms, unsigned long pollMs) {
  long pausedAt = -1;
  unsigned long start = nowMs, nextPoll = nowMs;
  for (unsigned long t = 0; t < ms; t += 20) {
    nowMs = start + t;
    if (pollMs && nowMs >= nextPoll) { cncAppSeenMs = nowMs; nextPoll = nowMs + pollMs; }
    bool before = cncUserHold;
    cncLinkWatch();
    if (!before && cncUserHold && pausedAt < 0) pausedAt = (long)(nowMs - cncAppSeenMs);
  }
  nowMs = start + ms;
  return pausedAt;
}

static void reset() {
  nowMs += 100000; isOn = true; mode = MODE_GCODE; grblSession = false; CNC_LINK_LOSS_PAUSE_S = 3;
  cncLinkArmed = false; cncLinkHold = false; cncUserHold = false; cncPaused = false; logLines.clear();
  cncAppSeenMs = 0;
}

int main() {
  char what[160];
  // 1. the app answers every 0.35 s for 10 minutes: no pause
  reset();
  check(run(600000, 350) < 0 && !cncUserHold, "app watching for 10 min: no pause");
  // 2. slow WiFi: requests 2.5 s apart (under 3 s): no pause
  check(run(60000, 2500) < 0, "requests 2.5 s apart: no pause");
  // 3. the app stops: pause after 3 s (within one service period), its message, one log line
  run(5000, 350);
  long at = run(10000, 0);
  snprintf(what, sizeof(what), "app gone: paused 3 s after its last request (at %ld ms)", at);
  check(at >= 3000 && at <= 3040, what);
  check(cncLinkHold && logLines.size() == 1, "the pause says the connection was lost, logged once");
  // 4. Resume (ON on the H4) while the app is still away: no new pause at once; the app back: armed again
  cncUserHold = false; cncLinkHold = false;
  check(run(20000, 0) < 0, "resumed with the app still away: it goes on (not paused again)");
  run(2000, 350);
  at = run(10000, 0);
  check(at >= 3000 && at <= 3040, "the app back, then gone again: paused again after 3 s");
  // 5. a program started with no app watching (keys of the H4): never paused
  reset();
  check(run(60000, 0) < 0, "no app watching: never paused");
  // 6. setting 0: never
  reset(); CNC_LINK_LOSS_PAUSE_S = 0;
  run(2000, 350);
  check(run(20000, 0) < 0, "setting 0: never paused");
  // 7. a Grbl sender's session: not paused (the sender drives it)
  reset(); grblSession = true;
  run(2000, 350);
  check(run(20000, 0) < 0, "Grbl session: not paused");
  // 8. during an M0 / M1 / M6 wait the app goes away: no pause while waiting; after ON it pauses (app still away)
  reset();
  run(2000, 350);
  cncPaused = true;
  check(run(10000, 0) < 0, "during an M0 wait: no pause");
  cncPaused = false;
  at = run(1000, 0);
  check(at >= 10000 && at <= 10400, "after the M0 wait, the app still away: paused at once");
  // 9. the program ended (isOn false), the app gone: nothing; the next program started without the app: not paused
  reset();
  run(2000, 350);
  isOn = false;
  run(5000, 0);
  check(!cncUserHold && !cncLinkArmed, "program ended: disarmed, no pause");
  isOn = true;
  check(run(20000, 0) < 0, "next program started without the app: not paused");
  // 10. another setting: 10 s
  reset(); CNC_LINK_LOSS_PAUSE_S = 10;
  run(2000, 350);
  at = run(20000, 0);
  snprintf(what, sizeof(what), "setting 10 s: paused 10 s after the last request (at %ld ms)", at);
  check(at >= 10000 && at <= 10040, what);
  printf("link: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
