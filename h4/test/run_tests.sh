#!/bin/sh
# Tests of the GCODE-mode motion core of NanoEls H4, on a PC (g++ and python3 needed).
#   sh h4/test/run_tests.sh [more programs.ngc ...]
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
set -e
cd "$(dirname "$0")"
OUT=${TMPDIR:-/tmp}/h4cnc-test
mkdir -p "$OUT"
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/cnc_parse_test" cnc_parse_test.cpp ../cnc_core.cpp ../cnc_gcode.cpp
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/cnc_sim" cnc_sim.cpp ../cnc_core.cpp ../cnc_gcode.cpp
"$OUT/cnc_parse_test"
fail=0
run() { if ! "$OUT/cnc_sim" "$@" > "$OUT/sim.txt"; then cat "$OUT/sim.txt"; fail=$((fail + 1)); fi; }
run lathecode_sample.ngc --machine h4default
run lathecode_sample.ngc --machine h4default --backlash 360,260,0
run polar_sample.ngc
run polar_sample.ngc --backlash 120,240,5 --hold-at 3
for i in $(seq 1 60); do
  python3 gen_random.py "$i" > "$OUT/r.ngc"
  bl="0,0,0"; [ $((i % 3)) = 0 ] && bl="40,80,3"
  hold=""; [ $((i % 4)) = 0 ] && hold="--hold-at $((i % 7 + 1)).3"
  m=marios; [ $((i % 5)) = 0 ] && m=h4default
  run "$OUT/r.ngc" --machine $m --backlash $bl $hold
done
for f in "$@"; do run "$f"; done
echo "simulation: $fail failure(s)"
[ $fail = 0 ]
