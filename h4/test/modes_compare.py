# modes_compare.py - checks that the feed / rapid override switch (M48, M49, M50 P) and the exact-stop mode
# (G61, G61.1, G64) are in force for the same moves as in LinuxCNC's own interpreter (rs274).
# For every move LinuxCNC makes (STRAIGHT_TRAVERSE, STRAIGHT_FEED, ARC_FEED) it records whether the feed override
# is enabled (ENABLE_ / DISABLE_FEED_OVERRIDE) and whether the motion control mode is CANON_EXACT_STOP; the H4
# trace (gcode_trace) gives the same two values after every line that moved. Random programs of moves mixed with
# those codes, plus any programs given.
#   python3 modes_compare.py /path/to/rs274 /path/to/gcode_trace [count] [seed] [programs.ngc ...]
# https://github.com/Marioskiv/nanoels-cnc - Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import os, random, re, subprocess, sys, tempfile

RS274, TRACE = sys.argv[1], sys.argv[2]
rest = sys.argv[3:]
count = int(rest.pop(0)) if rest and rest[0].isdigit() else 0
seed = int(rest.pop(0)) if rest and rest[0].isdigit() else 1


def lcnc_states(path):
    out = subprocess.run([RS274, "-g", path], capture_output=True, text=True, cwd=os.path.dirname(path)).stdout
    ovr, exact, states = True, False, []
    for ln in out.splitlines():
        if "DISABLE_FEED_OVERRIDE" in ln: ovr = False
        elif "ENABLE_FEED_OVERRIDE" in ln: ovr = True
        elif "SET_MOTION_CONTROL_MODE" in ln: exact = "CANON_EXACT_STOP" in ln
        elif re.search(r"\b(STRAIGHT_TRAVERSE|STRAIGHT_FEED|ARC_FEED)\(", ln): states.append((ovr, exact))
    return states


def h4_states(path):
    out = subprocess.run([TRACE, path], capture_output=True, text=True).stdout
    states, moved, err = [], False, None
    for ln in out.splitlines():
        f = ln.split()
        if f[0] == "P": moved = True
        elif f[0] == "L" and f[2] == "ERR": err = ln
        elif f[0] == "M":
            if moved: states.append((f[2] == "1", f[3] == "1"))
            moved = False
    return states, err


def random_program(rnd):
    codes = ["M48", "M49", "M50", "M50 P0", "M50 P1", "G61", "G61.1", "G64", "G64 P0.02", "M49 G61.1", "M48 G64"]
    lines, x, z = ["G21 G90 G18 G8", "G0 X10 Z5"], 10.0, 5.0
    for _ in range(rnd.randint(5, 25)):
        if rnd.random() < 0.4: lines.append(rnd.choice(codes))
        x, z = round(x + rnd.uniform(-5, 5), 3), round(z + rnd.uniform(-5, 5), 3)
        kind = rnd.random()
        pre = (rnd.choice(codes) + " ") if rnd.random() < 0.2 else ""
        if kind < 0.3: lines.append("%sG0 X%.3f Z%.3f" % (pre, x, z))
        elif kind < 0.8: lines.append("%sG1 X%.3f Z%.3f F%d" % (pre, x, z, rnd.randint(50, 600)))
        else: lines.append("%sG2 X%.3f Z%.3f R%.3f F200" % (pre, x, z, 20.0))
    lines.append("M2")
    return "\n".join(lines) + "\n"


rnd = random.Random(seed)
work = tempfile.mkdtemp(prefix="h4modes")
files = list(rest)
for i in range(count):
    p = os.path.join(work, "m%04d.ngc" % i)
    open(p, "w").write(random_program(rnd))
    files.append(p)
moves = differ = refused = 0
for p in files:
    a = lcnc_states(os.path.abspath(p))
    b, err = h4_states(os.path.abspath(p))
    if err:  # e.g. an arc LinuxCNC refuses as well: compare the moves made before it
        refused += 1
        a = a[:len(b)]
    moves += len(a)
    if a != b:
        differ += 1
        if differ <= 5:
            print("DIFFER", p, "LinuxCNC", a[:12], "H4", b[:12], err or "")
print("modes: compared %d programs, %d moves, %d stopped early by a refused line; %d differ" % (len(files), moves, refused, differ))
sys.exit(1 if differ else 0)
