# g93_timing.py - G93 inverse time: does the tool reach the end of every move when the program says?
# G93 promises that a move takes 1/F minutes (LinuxCNC: also when the move is shorter than one motor step).
# FluidNC issue 1372 found a drift that grew with the number of moves. Programs of many short G93 moves
# (1, 2, 3, 7, 60 steps each, a half and a third of a step, X alone and X Z C together, 2000 moves) are run in
# cnc_sim; the end of every line is compared with the sum of 1/F up to that line. Allowed: 0.6 of the time since the
# previous line that made a step (the Bresenham steps of a block fall inside it, a single step in its middle) + 30 ms
# (start), and the difference may not grow from the first to the last move by more than 30 ms.
#   python3 g93_timing.py /path/to/cnc_sim
# https://github.com/Marioskiv/nanoels-cnc - Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import os, subprocess, sys, tempfile

SIM = sys.argv[1]
SPM = {'X': 1200.0, 'Z': 400.0, 'C': 91.733333}  # cnc_sim --machine marios


def program(steps, n, f, axes):
    lines, pos, ends = ["G21 G90 G93"], {a: 0.0 for a in axes}, []
    t = 0.0
    for i in range(n):
        for a in axes: pos[a] += steps / SPM[a]
        lines.append("G1 " + " ".join("%s%.7f" % (a, pos[a]) for a in axes) + " F%g" % f)
        t += 60.0 / f
        ends.append((len(lines), t, 60.0 / f))
    lines.append("M2")
    return "\n".join(lines) + "\n", ends


cases = []
for k in (1, 2, 3, 7, 60):
    cases.append(("%g steps, X" % k, k, 200, 1440, "X"))
    cases.append(("%g steps, X Z C" % k, k, 200, 1440, "XZC"))
cases += [("half a step, X", 0.5, 200, 1440, "X"), ("a third of a step, X", 1 / 3, 300, 1440, "X"),
          ("a fifth of a step, C", 0.2, 200, 2400, "C"), ("1 step, 2000 moves", 1, 2000, 14400, "X"),
          ("0.7 step, X Z C", 0.7, 400, 2880, "XZC"), ("1 step every 0.5 s, X", 1, 20, 120, "X"),
          ("1 step every 3 s, C", 1, 5, 20, "C")]
fails = 0
work = tempfile.mkdtemp(prefix="h4g93")
for name, k, n, f, axes in cases:
    text, ends = program(k, n, f, axes)
    path = os.path.join(work, "p.ngc")
    open(path, "w").write(text)
    out = subprocess.run([SIM, path, "--reach-times"], capture_output=True, text=True).stdout
    reach = {}
    for ln in out.splitlines():
        if ln.startswith("R "):
            _, line, t = ln.split()
            reach[int(line)] = float(t)
    worst, first, last, checked = 0.0, None, None, 0
    bad, prevT = False, 0.0
    for line, t, dt in ends:
        if line not in reach: continue  # no step at the end of this line: the next line that steps is checked
        dt, prevT = t - prevT, t
        d = reach[line] - t
        checked += 1
        first = d if first is None else first
        last = d
        if abs(d) > abs(worst): worst = d
        if abs(d) > 0.6 * dt + 0.03: bad = True
    total = ends[-1][1]
    ok = not bad and checked > 0 and "OK" in out and abs((last or 0) - (first or 0)) <= 0.03
    fails += not ok
    print("%-22s %4d moves, %6.2f s programmed: worst %+7.1f ms, drift first->last %+6.1f ms  %s"
          % (name, n, total, worst * 1000, ((last or 0) - (first or 0)) * 1000, "OK" if ok else "FAIL"))
print("g93 timing: %d failure(s)" % fails)
sys.exit(1 if fails else 0)
