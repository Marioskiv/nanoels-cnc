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
# Pause / Resume stress (work plan B3): 25 holds at random times per program, also right after a Resume, with
# overrides, backlash and the H4 interrupt order; each hold must stop within 2 s and the program end exactly
for i in $(seq 201 260); do
  python3 gen_random.py "$i" > "$OUT/r.ngc"
  bl="0,0,0"; [ $((i % 3)) = 0 ] && bl="40,80,3"
  d=""; [ $((i % 2)) = 0 ] && d="--delayed"
  m=marios; [ $((i % 5)) = 0 ] && m=h4default
  run "$OUT/r.ngc" --machine $m --backlash $bl $d --holds 25:$i --ovr 1.3:60:50 --ovr 3.9:150:100
done
run polar_sample.ngc --delayed --backlash 120,240,5 --holds 60:7 --ovr 5:60:50
# planner stress (work plan B4): 100 000 small G93 blocks of a spiral and 100 000 tiny G1 / G93 moves (no stop between
# them, no underrun), G93 feeds far beyond what the axes allow, thousands of small arcs; three machines, both interrupt orders
python3 gen_stress.py spiral 100000 > "$OUT/b4s.ngc"
python3 gen_stress.py tiny 100000 > "$OUT/b4t.ngc"
python3 gen_stress.py g93edge 3000 > "$OUT/b4g.ngc"
python3 gen_stress.py arcs 3000 > "$OUT/b4a.ngc"
run "$OUT/b4s.ngc" --stops none
run "$OUT/b4s.ngc" --delayed --stops none
run "$OUT/b4s.ngc" --machine fast --delayed --stops none
run "$OUT/b4t.ngc" --stops none
run "$OUT/b4t.ngc" --machine h4default --stops none
run "$OUT/b4g.ngc"
run "$OUT/b4g.ngc" --delayed --backlash 40,80,3
run "$OUT/b4a.ngc" --stops none
run "$OUT/b4a.ngc" --machine h4default --stops none
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
# direction hold (LinuxCNC DIRHOLD): a direction pin changes only H us after the axis' last pulse ended; with the
# author's driver timing (STEPLEN 5, DIRSETUP 10, DIRHOLD 10 us) and near the highest step rate
for h in 0 10 20; do run pins_sample.ngc --machine fast --delayed --pulse-us 5 --dir-us 10 --dirhold-us $h; done
run polar_sample.ngc --machine fast --delayed --pulse-us 5 --dir-us 10 --dirhold-us 10 --backlash 40,80,3
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
# G93: every move ends when the sum of 1/F says, without a drift over many moves (FluidNC issue 1372), also for
# moves shorter than one step and moves slower than a step every few seconds
python3 g93_timing.py "$OUT/cnc_sim" > "$OUT/g93t.txt" || { cat "$OUT/g93t.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/g93t.txt"
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
# Home All, home switches as limits, driver alarm / E-stop and soft limits: the firmware's own homing.ino on a
# simulated machine (switch bounce, noise, NC broken wire, disconnected switch, alarm and STOP during homing)
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/homing_test" homing_test.cpp
"$OUT/homing_test" || fail=$((fail + 1))
# moveAxis() step pulses (jog, Home All, G32 / G33, lathe modes): the firmware's own moveAxis() / setDir() on a simulated
# CPU clock; in the GCODE mode pulse >= CNC_STEP_PULSE_US, direction hold and set-up; lathe modes unchanged
{ awk '/^struct Axis \{/,/^};/' ../h4.ino; sh lathe/extract.sh ../h4.ino "uint32_t moveCyclesPerUs" "void setDir" "void moveAxis"; } > "$OUT/move_funcs.inc"
g++ -O2 -Wall -Wextra -std=c++17 -I"$OUT" -o "$OUT/move_pulse_test" move_pulse_test.cpp
"$OUT/move_pulse_test" > "$OUT/mp.txt" || { cat "$OUT/mp.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/mp.txt"
# the lathe modes' axis following the spindle (gearbox, threads): moveAxis() unchanged, exact simulated time
g++ -O2 -Wall -Wextra -std=c++17 -I"$OUT" -o "$OUT/follow_test" follow_test.cpp
"$OUT/follow_test" || fail=$((fail + 1))
# the line shown while a program runs: the one whose motion runs now, numbered as in the file (h4.ino, unchanged)
sh lathe/extract.sh ../h4.ino "void cncRingRecord" "uint32_t gcodeFileLineNo" "void cncExecLine" > "$OUT/exec_funcs.inc"
g++ -O2 -Wall -Wextra -std=c++17 -I"$OUT" -o "$OUT/exec_line_test" exec_line_test.cpp
"$OUT/exec_line_test" || fail=$((fail + 1))
# the chuck encoder against C (work plan B7, for C8): a simulated 600 PPR encoder with noise, bounce, backlash, stretch
# and slips, decoded as the ESP32 pulse counter; cnc_encwatch.cpp reports a skipped tooth / stall / slip, nothing else
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/encoder_test" encoder_test.cpp ../cnc_encwatch.cpp
"$OUT/encoder_test" "$(sed -n 's/^const int ENCODER_FILTER = \([0-9]*\);.*/\1/p' ../h4.ino)" > "$OUT/enc.txt" || { cat "$OUT/enc.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/enc.txt"
# a program the app was watching pauses when the app's connection is lost (h4.ino cncLinkWatch, compiled unchanged)
sh lathe/extract.sh ../h4.ino "void cncLinkWatch" > "$OUT/link_funcs.inc"
g++ -O2 -Wall -Wextra -std=c++17 -I"$OUT" -o "$OUT/link_test" link_test.cpp
"$OUT/link_test" || fail=$((fail + 1))
# at a corner no axis jumps in speed by more than its acceleration x 10 ms (cnc_core.cpp, firmware 1.36): every corner
# of random X Z C programs (G94 / G93, reversals, C alone) on three machines
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/junction_axis_test" junction_axis_test.cpp ../cnc_core.cpp
"$OUT/junction_axis_test" || fail=$((fail + 1))
# jogging from the app when the app stops talking (work plan B6): the gamepad's /jog with the real planner and the
# arrows' /key hold (grbl.ino, wifi.ino, compiled unchanged): stop within 300 ms + braking, exact position, arrows 500 ms
{ awk '/^#define WEB_JOG_PIECE_S/,/^unsigned long webJogStopMs/' ../grbl.ino; sh lathe/extract.sh ../grbl.ino "bool grblJog" "bool webJogRequest" "void webJogStop" "bool webJogService"; } > "$OUT/jog_funcs.inc"
{ grep -E "^(const unsigned long HELD_KEY_TIMEOUT_MS|int heldKey|unsigned long heldKeyMs)" ../wifi.ino; sh lathe/extract.sh ../wifi.ino "void releaseHeldKey" "void handleKey"; echo "void heldKeyFailSafe() {"; grep "HELD_KEY_TIMEOUT_MS) releaseHeldKey" ../wifi.ino; echo "}"; } > "$OUT/key_funcs.inc"
g++ -O2 -Wall -Wextra -std=c++17 -I"$OUT" -o "$OUT/jog_test" jog_test.cpp ../cnc_core.cpp ../cnc_gcode.cpp
"$OUT/jog_test" > "$OUT/jog.txt" || { cat "$OUT/jog.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/jog.txt"
# the lathe modes' zero and the GCODE mode's zero stay apart (zeros.ino)
g++ -O2 -Wall -Wextra -std=c++17 -x c++ -o "$OUT/zeros_test" zeros_test.cpp
"$OUT/zeros_test" || fail=$((fail + 1))
# how the lathe modes show Z and X: ISO / original H4, diameter / radius (latheview.ino)
g++ -O2 -Wall -Wextra -std=c++17 -x c++ -o "$OUT/latheview_test" latheview_test.cpp
"$OUT/latheview_test" || fail=$((fail + 1))
# the depth of each thread pass: smaller and smaller, the same material each pass (threadpass.ino)
g++ -O2 -Wall -Wextra -std=c++17 -x c++ -o "$OUT/threadpass_test" threadpass_test.cpp
"$OUT/threadpass_test" || fail=$((fail + 1))
# restarts while moving, power cut during a program, lasting log, accepting a new firmware (safety.ino)
g++ -O2 -Wall -Wextra -std=c++17 -Istubs -x c++ -o "$OUT/safety_test" safety_test.cpp
"$OUT/safety_test" || fail=$((fail + 1))
fail=$((fail + jfail))
# real CAM programs (test/cam): as written, and through the polar converter
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/gcode_trace" gcode_trace.cpp ../cnc_core.cpp ../cnc_gcode.cpp
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/cnc_bench_pc" cnc_bench_pc.cpp ../cnc_bench.cpp ../cnc_core.cpp ../cnc_gcode.cpp
python3 cam_check.py "$OUT" $RS274 > "$OUT/cam.txt" || { cat "$OUT/cam.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/cam.txt"
# the check before a program starts (cnc_bench.cpp as cncRunPreflight uses it): errors far in, soft limits, arcs, Y,
# notes that do not refuse, threading, a real program
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/preflight_test" preflight_test.cpp ../cnc_bench.cpp ../cnc_core.cpp ../cnc_gcode.cpp
"$OUT/preflight_test" > "$OUT/pf.txt" || { cat "$OUT/pf.txt"; fail=$((fail + 1)); }
tail -1 "$OUT/pf.txt"
# start from line N (cnc_runfrom.cpp): from every line of the samples, the same moves as a full run
g++ -O2 -Wall -Wextra -std=c++17 -o "$OUT/runfrom_test" runfrom_test.cpp ../cnc_runfrom.cpp ../cnc_bench.cpp ../cnc_core.cpp ../cnc_gcode.cpp
"$OUT/runfrom_test" || fail=$((fail + 1))
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
