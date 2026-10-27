/*
  cnc_bench_pc.cpp - the controller's self-test (cnc_bench.cpp) on a PC: what a program asks for.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
  Usage: cnc_bench_pc program.ngc ...   (machine values of the author's XZC lathe, as cnc_sim --machine marios)
*/
#include "../cnc_bench.h"
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <string>

static uint64_t nowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
  int fails = 0;
  for (int f = 1; f < argc; f++) {
    cnc_settings_t s;
    s.stepsPerUnit[CNC_X] = 1200; s.stepsPerUnit[CNC_Z] = 400; s.stepsPerUnit[CNC_C] = 91.733333f;
    s.maxRate[CNC_X] = 12 * 60; s.maxRate[CNC_Z] = 25 * 60; s.maxRate[CNC_C] = 360 * 60;
    s.acceleration[CNC_X] = 100 * 3600; s.acceleration[CNC_Z] = 100 * 3600; s.acceleration[CNC_C] = 720 * 3600;
    s.junctionDeviation = 0.01f;
    s.timerHz = 20000000;
    cnc_core_settings(&s);
    cnc_machine_t m;
    memset(&m, 0, sizeof(m));
    m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
    for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
    m.rotary[CNC_C] = true;
    m.arcTolerance = 0.002f;
    cnc_gc_init(&m);
    cnc_plan_reset();
    int32_t zero[CNC_N_AXIS] = {0, 0, 0};
    bool neg[CNC_N_AXIS] = {false, false, false};
    cnc_gc_sync(zero, zero, neg);
    FILE* fp = fopen(argv[f], "rb");
    if (!fp) { perror(argv[f]); return 2; }
    std::string text;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) text.append(buf, n);
    fclose(fp);
    cnc_bench_t r;
    cnc_bench_run(text.data(), text.size(), &r, nowUs, nullptr);
    double plannedPerSecond = r.busyUs > 0 ? r.blocks / (r.busyUs / 1e6) : 0;
    printf("%s: %u lines, %u blocks, %.1f min at programmed speeds; this PC plans %.0f blocks/s; the program needs up to %.0f blocks/s (line %u)%s%s\n",
           argv[f], r.lines, r.blocks, r.programMinutes, plannedPerSecond, r.neededPerSecond, r.neededAtLine,
           r.stopLine ? "; stopped: " : "", r.stopLine ? r.stopReason : "");
    if (r.stopLine && strncmp(r.stopReason, "threading", 9) != 0) fails++;
  }
  return fails ? 1 : 0;
}
