/*
  exec_line_test.cpp - the line shown while a program runs (firmware 1.30): the line whose motion runs now, not the
  one read ahead, with its number in the stored program as Test counts it. h4.ino's own cncRingRecord(),
  cncExecLine() and gcodeFileLineNo() are compiled unchanged; the program is read here as taskGcode reads it
  (comments, empty lines, CRLF), the line numbers compared with cnc_bench's (the check before the start).
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static uint32_t runningLine = 0;
uint32_t cnc_st_current_line() { return runningLine; }
#define CNC_LINE_RING 128
struct CncRingLine { uint32_t no; char text[44]; };
CncRingLine cncLineRing[CNC_LINE_RING];
#include "exec_funcs.inc"

static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) { failures++; printf("FAIL: %s\n", what); }
}

// taskGcode's reading of a stored program: chars, ( ) and ; comments dropped, a line ends at any control char,
// empty lines skipped; returns (file line number, text) of every line given to cncProcessLine
static std::vector<std::pair<uint32_t, std::string>> readProgram(const std::string& p) {
  std::vector<std::pair<uint32_t, std::string>> out;
  uint32_t newlines = 0;
  std::string cmd;
  bool brace = false, semi = false;
  for (char c : p) {
    if (c == '\n') newlines++;
    if (brace) { if (c == ')') brace = false; continue; }
    if (c == '(') { brace = true; continue; }
    if (c == ';') { semi = true; continue; }
    if (semi && (unsigned char)c >= 32) continue;
    if ((unsigned char)c < 32) {
      std::string line = cmd;
      while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
      while (!line.empty() && (line[0] == ' ' || line[0] == '\t')) line.erase(0, 1);
      cmd.clear();
      semi = false;
      if (!line.empty()) out.push_back({gcodeFileLineNo(newlines, c), line});
    } else {
      cmd += c;
    }
  }
  return out;
}

int main() {
  char buf[96], what[200];
  // 1. line numbers = the file's, as Test counts them: comments, empty lines, CRLF, a comment-only line
  {
    std::string p = "(header)\nG21 G90\n\n; note\nG0 X10\r\nG1 Z-1 F100 (cut)\r\n\r\nG1 X20\nM2\n";
    auto lines = readProgram(p);
    const uint32_t want[] = {2, 5, 6, 8, 9};
    bool ok = lines.size() == 5;
    for (size_t i = 0; ok && i < 5; i++) ok = lines[i].first == want[i];
    snprintf(what, sizeof(what), "file line numbers 2 5 6 8 9 (got %zu lines, first %u)", lines.size(), lines.empty() ? 0 : lines[0].first);
    check(ok && lines[2].second == "G1 Z-1 F100", what);
  }
  // 2. the shown line is the running one, with its number; the one read ahead is not shown
  {
    memset(cncLineRing, 0, sizeof(cncLineRing));
    for (uint32_t n = 1; n <= 60; n++) { snprintf(buf, sizeof(buf), "G1 X%u F600", n); cncRingRecord(n, buf); }
    runningLine = 3;
    cncExecLine(buf, sizeof(buf), true);
    check(!strcmp(buf, "3: G1 X3 F600"), "running line 3 shown while 60 are read");
    cncExecLine(buf, sizeof(buf), false);
    check(!strcmp(buf, "G1 X3 F600"), "screen: text only");
    runningLine = 0;
    cncExecLine(buf, sizeof(buf), true);
    check(buf[0] == 0, "before the first move: nothing");
  }
  // 3. a line whose text is no longer kept (more than 128 lines read ahead): its number only
  {
    for (uint32_t n = 61; n <= 300; n++) { snprintf(buf, sizeof(buf), "G1 X%u", n); cncRingRecord(n, buf); }
    runningLine = 100;
    cncExecLine(buf, sizeof(buf), true);
    check(!strcmp(buf, "100"), "text overwritten: number only");
    runningLine = 290;
    cncExecLine(buf, sizeof(buf), true);
    check(!strcmp(buf, "290: G1 X290"), "recent line: number and text");
  }
  // 4. a long line is cut to fit, never past the buffer
  {
    std::string longLine(200, 'X');
    cncRingRecord(301, longLine.c_str());
    runningLine = 301;
    char small[16];
    cncExecLine(small, sizeof(small), true);
    check(strlen(small) == 15 && !strncmp(small, "301: XXXX", 9), "long line cut to the buffer");
  }
  printf("exec line: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
