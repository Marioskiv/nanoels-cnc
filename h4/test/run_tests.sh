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
# feed / rapid overrides changed while moving (Grbl 1.1 real-time commands), also during a hold
run polar_sample.ngc --ovr 0.5:50:50 --ovr 2:150:25 --ovr 4:10:100 --ovr 6:200:100
for i in $(seq 61 80); do
  python3 gen_random.py "$i" > "$OUT/r.ngc"
  hold=""; [ $((i % 2)) = 0 ] && hold="--hold-at 2.3"
  run "$OUT/r.ngc" --ovr 0.7:30:25 --ovr 2.1:180:50 --ovr 3.3:100:100 --ovr 5:70:100 $hold
done
# the H4 step interrupt order (step put out by the next interrupt), with holds and overrides
run polar_sample.ngc --delayed --backlash 120,240,5 --hold-at 3 --ovr 5:60:50
run lathecode_sample.ngc --machine h4default --delayed --hold-at 1.5
for i in $(seq 81 110); do
  python3 gen_random.py "$i" > "$OUT/r.ngc"
  bl="0,0,0"; [ $((i % 3)) = 0 ] && bl="40,80,3"
  hold=""; [ $((i % 2)) = 0 ] && hold="--hold-at $((i % 5 + 1)).7"
  run "$OUT/r.ngc" --delayed --backlash $bl $hold --ovr 1.1:40:25 --ovr 4.2:160:100
done
# G61.1 exact stop: every move ends at rest (step generator speed 0 at its end and at the next start);
# the same smooth path in G64 goes through without stopping
for m in marios h4default; do
  run g611_sample.ngc --machine $m --stops all
  run g611_sample.ngc --machine $m --stops all --delayed --backlash 40,80,3
  run g64_sample.ngc --machine $m --stops none
done
# step and direction pins as the H4 drives them, near the highest step rate, direction set-up 5 / 10 / 20 us
for d in 5 10 20; do run pins_sample.ngc --machine fast --delayed --dir-us $d; done
for i in 111 112 113; do python3 gen_random.py "$i" > "$OUT/r.ngc"; run "$OUT/r.ngc" --machine fast --delayed --dir-us 10 --backlash 40,80,3; done
# STOP in the middle of a program: stops on the path, the rest is dropped, a new move from there is exact
run polar_sample.ngc --stop-at 20
run polar_sample.ngc --stop-at 33 --delayed --backlash 120,240,5
# a STOP that ends exactly where a backlash take-up block starts (found a step count of -1 and a period of 0)
run polar_sample.ngc --stop-at 33.04 --backlash 120,240,5
run lathecode_sample.ngc --machine h4default --stop-at 1
# G93 hard cases (1 ms blocks, F above what the axes can do, very slow, near the centre, a G93 arc), and
# 1500 blocks of 1 ms (1000 blocks/s) with the feeding task pausing up to 50 ms: no stutter
run g93_hard.ngc --machine marios
run g93_hard.ngc --machine h4default --delayed --dir-us 10
python3 -c "
L=['G21 G90 G18 G8','G0 X20 Z1 C0','G93']
for i in range(1500): L.append('G1 X%.3f C%.2f F60000' % (20 + 0.01 * (i + 1), (i + 1) * 0.05))
open('$OUT/rate.ngc', 'w').write('\\n'.join(L + ['G94', 'M2']) + '\\n')"
run "$OUT/rate.ngc" --machine marios --task-gap-ms 50
for f in "$@"; do run "$f"; done
echo "simulation: $fail failure(s)"
# corners: no axis changes its speed at once by more than sqrt(8 x its acceleration x junction deviation),
# in its own units (X, Z mm, C degrees), on polar and random programs
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/cnc_junctions" cnc_junctions.cpp ../cnc_gcode.cpp
jfail=0
for f in polar_sample.ngc g611_sample.ngc g64_sample.ngc modes_sample.ngc g93_hard.ngc; do
  "$OUT/cnc_junctions" "$f" --check > "$OUT/j.txt" || { cat "$OUT/j.txt"; jfail=$((jfail + 1)); }
done
for i in $(seq 1 30); do
  python3 gen_random.py "$i" > "$OUT/r.ngc"
  m=marios; [ $((i % 2)) = 0 ] && m=h4default
  "$OUT/cnc_junctions" "$OUT/r.ngc" --machine $m --check > "$OUT/j.txt" || { cat "$OUT/j.txt"; jfail=$((jfail + 1)); }
done
echo "corners: $jfail failure(s)"
fail=$((fail + jfail))
# real CAM programs (test/cam): as written, and through the polar converter
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/gcode_trace" gcode_trace.cpp ../cnc_core.cpp ../cnc_gcode.cpp
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/cnc_bench_pc" cnc_bench_pc.cpp ../cnc_bench.cpp ../cnc_core.cpp ../cnc_gcode.cpp
python3 cam_check.py "$OUT" $RS274 > "$OUT/cam.txt" || { cat "$OUT/cam.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/cam.txt"
# the controller's self-test (cnc_bench.cpp) on the sample programs
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/cnc_bench_pc" cnc_bench_pc.cpp ../cnc_bench.cpp ../cnc_core.cpp ../cnc_gcode.cpp
"$OUT/cnc_bench_pc" polar_sample.ngc lathecode_sample.ngc || fail=$((fail + 1))
# Second opinion: LinuxCNC's own interpreter, when its rs274 is given (RS274=/path/to/rs274)
if [ -n "$RS274" ]; then
  g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/gcode_trace" gcode_trace.cpp ../cnc_core.cpp ../cnc_gcode.cpp
  python3 lcnc_compare.py "$RS274" "$OUT/gcode_trace" 1000 1 || fail=$((fail + 1))
  python3 lcnc_compare.py "$RS274" "$OUT/gcode_trace" polar_sample.ngc modes_sample.ngc g611_sample.ngc || fail=$((fail + 1))
  # M48 / M49 / M50 and G61 / G61.1 / G64 in force for the same moves as in LinuxCNC
  python3 modes_compare.py "$RS274" "$OUT/gcode_trace" 500 1 modes_sample.ngc g611_sample.ngc g64_sample.ngc || fail=$((fail + 1))
fi
[ $fail = 0 ]
