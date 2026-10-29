# modes_compare.py - checks that the feed / rapid override switch (M48, M49, M50 P), the exact-stop mode
# (G61, G61.1, G64), the spindle (M3 / M4 / M5), the coolant (M7 / M8 / M9) and the tool in the spindle (T, M6,
# M61) are in force for the same moves as in LinuxCNC's own interpreter (rs274).
# For every move LinuxCNC makes (STRAIGHT_TRAVERSE, STRAIGHT_FEED, ARC_FEED) it records whether the feed override
# is enabled, whether the motion control mode is CANON_EXACT_STOP, the spindle, mist, flood and the tool; the H4
# trace (gcode_trace) gives the same values after every line that moved. Random programs of moves mixed with
# those codes, plus any programs given.
#   python3 modes_compare.py /path/to/rs274 /path/to/gcode_trace [count] [seed] [programs.ngc ...]
# https://github.com/Marioskiv/nanoels-cnc - Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import os, random, re, subprocess, sys, tempfile

RS274, TRACE = sys.argv[1], sys.argv[2]
rest = sys.argv[3:]
count = int(rest.pop(0)) if rest and rest[0].isdigit() else 0
seed = int(rest.pop(0)) if rest and rest[0].isdigit() else 1


def lcnc_states(path):
    # tools 1-99 in the table, as lcnc_compare.py (the H4 has no tool table: any tool number is taken)
    tbl = os.path.join(tempfile.gettempdir(), "h4-zero.tbl")
    open(tbl, "w").write("".join("T%d P%d Z0 D0\n" % (t, t) for t in range(1, 100)))
    res = subprocess.run([RS274, "-t", tbl, "-g", path], capture_output=True, text=True, cwd=os.path.dirname(path))
    out = res.stdout
    ovr, exact, spin, mist, flood, tool, states = True, False, 5, False, False, 0, []
    for ln in out.splitlines():
        if "DISABLE_FEED_OVERRIDE" in ln: ovr = False
        elif "ENABLE_FEED_OVERRIDE" in ln: ovr = True
        elif "SET_MOTION_CONTROL_MODE" in ln: exact = "CANON_EXACT_STOP" in ln
        elif "START_SPINDLE_CLOCKWISE" in ln: spin = 3
        elif "START_SPINDLE_COUNTERCLOCKWISE" in ln: spin = 4
        elif "STOP_SPINDLE_TURNING" in ln: spin = 5
        elif "MIST_ON" in ln: mist = True
        elif "MIST_OFF" in ln: mist = False
        elif "FLOOD_ON" in ln: flood = True
        elif "FLOOD_OFF" in ln: flood = False
        elif "CHANGE_TOOL(" in ln or "CHANGE_TOOL_NUMBER(" in ln: tool = int(re.search(r"\((-?\d+)", ln).group(1))
        elif re.search(r"\b(STRAIGHT_TRAVERSE|STRAIGHT_FEED|ARC_FEED)\(", ln): states.append((ovr, exact, spin, mist, flood, tool))
    return states


def h4_states(path):
    out = subprocess.run([TRACE, path], capture_output=True, text=True).stdout
    states, moved, err = [], False, None
    for ln in out.splitlines():
        f = ln.split()
        if f[0] == "P": moved = True
        elif f[0] == "L" and f[2] == "ERR": err = ln
        elif f[0] == "M":
            if moved: states.append((f[2] == "1", f[3] == "1", int(f[4]), f[5] == "1", f[6] == "1", int(f[7])))
            moved = False
    return states, err


def random_program(rnd):
    codes = ["M48", "M49", "M50", "M50 P0", "M50 P1", "G61", "G61.1", "G64", "G64 P0.02", "M49 G61.1", "M48 G64",
             "S12000 M3", "M4 S800", "M5", "M7", "M8", "M9", "T2", "M61 Q7"]
    # on a line of their own only: M6 (the H4 refuses M6 with a move on the same line, no post writes that);
    # "M7 M8" on one line is left out: LinuxCNC refuses it, the H4 accepts it as Grbl does
    alone = ["T3 M6", "M6 T4", "T5 M6 M8", "M9 M6"]
    lines, x, z = ["G21 G90 G18 G8", "G0 X10 Z5"], 10.0, 5.0
    for _ in range(rnd.randint(5, 25)):
        if rnd.random() < 0.4: lines.append(rnd.choice(codes + alone))
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
            k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            print("DIFFER", p, "at move %d of %d / %d:" % (k, len(a), len(b)), "LinuxCNC", a[k:k + 2], "H4", b[k:k + 2], err or "")
print("modes: compared %d programs, %d moves, %d stopped early by a refused line; %d differ" % (len(files), moves, refused, differ))
sys.exit(1 if differ else 0)
