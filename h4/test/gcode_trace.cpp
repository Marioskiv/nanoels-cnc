/*
  gcode_trace.cpp - runs a program through the GCODE-mode interpreter of NanoEls H4 and prints, per line,
  the position reached (or the error) and every planned point, for lcnc_compare.py
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../cnc_core.h"
#include "../cnc_gcode.h"
#include <stdio.h>
#include <string.h>
#include <string>

static const double SPU = 10000;  // steps per mm / degree: fine enough to compare with LinuxCNC

static void trace(const int32_t t[CNC_N_AXIS], uint32_t line) {
  printf("P %u %.5f %.5f %.5f\n", line, t[CNC_X] / SPU, t[CNC_Z] / SPU, t[CNC_C] / SPU);
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: gcode_trace program.ngc\n"); return 2; }
  cnc_settings_t s;
  for (int i = 0; i < CNC_N_AXIS; i++) { s.stepsPerUnit[i] = SPU; s.maxRate[i] = 6000; s.acceleration[i] = 360000; }
  s.junctionDeviation = 0.01f;
  s.timerHz = 20000000;
  cnc_core_settings(&s);
  cnc_machine_t m;
  memset(&m, 0, sizeof(m));
  m.name[0] = 'X'; m.name[1] = 'Z'; m.name[2] = 'C';
  for (int i = 0; i < CNC_N_AXIS; i++) m.active[i] = true;
  m.rotary[CNC_C] = true;
  m.defaultFeed = 100;
  m.arcTolerance = 0.002f;
  cnc_gc_init(&m);
  cnc_plan_reset();
  cnc_st_reset();
  int32_t z[3] = {0, 0, 0};
  bool n[3] = {false, false, false};
  cnc_gc_sync(z, z, n);
  cnc_plan_trace = trace;
  FILE* f = fopen(argv[1], "r");
  if (!f) { perror(argv[1]); return 2; }
  char buf[1024];
  uint32_t lineNo = 0;
  while (fgets(buf, sizeof(buf), f)) {
    lineNo++;
    std::string l;
    bool paren = false;
    for (char* c = buf; *c; c++) {
      if (paren) { if (*c == ')') paren = false; continue; }
      if (*c == '(') { paren = true; continue; }
      if (*c == ';' || *c == '\n' || *c == '\r') break;
      l += *c;
    }
    if (l.find_first_not_of(" \t") == std::string::npos || l == "%") continue;
    char msg[128];
    float dwell = 0;
    int r;
    while ((r = cnc_gc_line(l.c_str(), lineNo, msg, sizeof(msg), &dwell)) == CNC_WAIT) {
      int32_t keep[CNC_N_AXIS];  // the planner is emptied as if the machine had moved
      memcpy(keep, cnc_plan_position(), sizeof(keep));
      cnc_plan_reset();
      cnc_plan_sync_position(keep);
    }
    if (r == CNC_ERROR) { printf("L %u ERR %s\n", lineNo, msg); return 0; }
    const int32_t* p = cnc_plan_position();
    printf("L %u OK %.5f %.5f %.5f\n", lineNo, p[CNC_X] / SPU, p[CNC_Z] / SPU, p[CNC_C] / SPU);
    if (r == CNC_END) return 0;
  }
  return 0;
}
