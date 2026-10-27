#!/usr/bin/env python3
# lcnc_compare.py - runs random lathe programs through LinuxCNC's own interpreter (rs274) and through the
# H4 interpreter (gcode_trace) and compares every move: end points in machine coordinates, arc centres,
# directions and turns, and that both refuse the same line.
#   python3 lcnc_compare.py /path/to/rs274 /path/to/gcode_trace [count] [seed]
# rs274 is built from the LinuxCNC source (2.9): ./configure --with-realtime=uspace && make ../bin/rs274
# Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import math, os, random, re, subprocess, sys, tempfile

RS274, TRACE = sys.argv[1], sys.argv[2]
FILES = [a for a in sys.argv[3:] if not a.isdigit()]  # real programs instead of random ones
NUMS = [a for a in sys.argv[3:] if a.isdigit()]
COUNT = int(NUMS[0]) if NUMS else 300
SEED = int(NUMS[1]) if len(NUMS) > 1 else 1
TOL = 2e-4

HEADER = "G18 G21 G90 G94 G40 G49 G80 G54 G91.1 G8 M3 S500"


def gen(rng):
    lines = [HEADER]
    x, z = 0.0, 0.0  # rough program position, only to keep arcs sensible
    for _ in range(rng.randint(5, 40)):
        r = rng.random()
        num = lambda a, b: "%.3f" % rng.uniform(a, b)
        if r < 0.25:
            lines.append("G%d X%s Z%s" % (rng.choice([0, 1]), num(-30, 30), num(-60, 10)) + (" F%s" % num(10, 500) if rng.random() < 0.5 else "") + (" C%s" % num(-400, 400) if rng.random() < 0.3 else ""))
        elif r < 0.45:
            g = rng.choice([2, 3])
            ex, ez = rng.uniform(-30, 30), rng.uniform(-60, 10)
            if rng.random() < 0.5:
                rad = rng.uniform(1, 60) * rng.choice([1, -1]) if rng.random() < 0.3 else rng.uniform(50, 120) * rng.choice([1, -1])
                lines.append("G%d X%.3f Z%.3f R%.3f F120" % (g, ex, ez, rad))
            elif rng.random() < 0.7:
                # a valid I K arc: incremental, end point = start turned about the centre
                ci, ck = rng.uniform(-20, 20), rng.uniform(-20, 20)
                ang = rng.uniform(-6, 6)
                sx, sz = -ci, -ck  # start relative to the centre
                ex_ = sx * math.cos(ang) - sz * math.sin(ang)
                ez_ = sx * math.sin(ang) + sz * math.cos(ang)
                lines.append("G8 G91 G91.1 G%d X%.4f Z%.4f I%.4f K%.4f F%s" % (g, ex_ + ci, ez_ + ck, ci, ck, num(20, 300)) + (" P%d" % rng.randint(1, 3) if rng.random() < 0.15 else "") + (" C%s" % num(-90, 90) if rng.random() < 0.2 else ""))
            else:
                # a random centre (often refused)
                lines.append("G%d X%.3f Z%.3f I%s K%s F120" % (g, ex, ez, num(-20, 20), num(-20, 20)) + (" P%d" % rng.randint(1, 3) if rng.random() < 0.15 else "") + (" C%s" % num(-90, 90) if rng.random() < 0.2 else ""))
        elif r < 0.55:
            # (G49 with a unit change: LinuxCNC 2.9 keeps a G43.1 offset in the old units after G20/G21,
            # its G53 moves then differ from the true machine position; the H4 keeps it in mm)
            lines.append(rng.choice(["G90", "G91", "G7", "G8", "G49\nG20", "G49\nG21", "G90.1", "G91.1", "G93", "G94"]))
        elif r < 0.65:
            lines.append(rng.choice(["G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3"]))
        elif r < 0.72:
            lines.append("G10 L%d P%d X%s Z%s" % (rng.choice([2, 20]), rng.randint(0, 9), num(-20, 20), num(-20, 20)))
        elif r < 0.78:
            lines.append(rng.choice(["G92 X%s Z%s" % (num(-20, 20), num(-20, 20)), "G92.1", "G92.2", "G92.3", "G92 C%s" % num(-90, 90)]))
        elif r < 0.84:
            lines.append(rng.choice(["G28", "G30", "G28.1", "G30.1", "G28 X%s" % num(-10, 10), "G30 Z%s" % num(-10, 10)]))
        elif r < 0.89:
            lines.append("G53 G%d X%s Z%s" % (rng.choice([0, 1]), num(-30, 30), num(-60, 10)))
        elif r < 0.93:
            lines.append(rng.choice(["G43.1 Z%s" % num(-5, 5), "G49"]))
        elif r < 0.97:
            # canned cycles (G17: drill along Z, G19: along X), repeated positions, G98 / G99
            pl = rng.choice(["G17", "G19", "G17", "G18"])
            cyc = rng.choice(["G81", "G82", "G83", "G73", "G85", "G89"])
            words = [pl, rng.choice(["", "G98", "G99"]), cyc]
            dz, pz = ("Z", "X") if pl != "G19" else ("X", "Z")
            if rng.random() < 0.8: words.append("%s%s" % (pz, num(-20, 20)))
            if rng.random() < 0.85: words.append("%s%s" % (dz, num(-30, 0)))
            if rng.random() < 0.85: words.append("R%s" % num(-5, 10))
            if rng.random() < 0.7: words.append("Q%s" % num(0.5, 5))
            if rng.random() < 0.6: words.append("P%s" % num(0, 1))
            if rng.random() < 0.2: words.append("L%d" % rng.randint(1, 4))
            words.append("F%s" % num(20, 300))
            lines.append(" ".join(w for w in words if w))
            for _ in range(rng.randint(0, 3)):
                lines.append("%s%s" % (pz, num(-20, 20)))
            if rng.random() < 0.6: lines.append("G80")
            lines.append("G18")
        else:
            # G76 threading cycle in G18, from a start point outside the part
            lines.append("G18 G0 X%s Z%s" % (num(5, 20), num(1, 5)))
            lines.append("G76 P%s Z%s I%s J%s K%s" % (num(0.5, 3), num(-30, -5), rng.choice(["-", ""]) + num(0.5, 2), num(0.05, 0.4), num(0.5, 2))
                         + (" Q%s" % num(0, 30) if rng.random() < 0.5 else "") + (" H%d" % rng.randint(0, 2) if rng.random() < 0.4 else "")
                         + (" R%s" % num(1, 3) if rng.random() < 0.4 else "") + (" X%s" % num(5, 25) if rng.random() < 0.3 else ""))
    lines.append("M2")
    # G93 needs F on every feed move in both interpreters; keep the programs mostly G94
    return "\n".join(lines) + "\n"


def run_rs274(path):
    # tools 1-99 of length 0: G43 H adds nothing (the H4 has no tool table yet)
    tbl = os.path.join(tempfile.gettempdir(), "h4-zero.tbl")
    open(tbl, "w").write("".join("T%d P%d Z0 D0\n" % (t, t) for t in range(1, 100)))
    res = subprocess.run([RS274, "-t", tbl, "-g", path], capture_output=True, text=True, cwd=os.path.dirname(path)); out = res.stdout + "\n" + res.stderr
    moves, err = [], None
    g5x = [0.0] * 9
    g92 = [0.0] * 9
    tlo = [0.0] * 9
    unit = 1.0  # canonical positions are in the program's units; offsets are kept in mm here
    for ln in out.splitlines():
        if "USE_LENGTH_UNITS(CANON_UNITS_INCHES)" in ln:
            unit = 25.4
        elif "USE_LENGTH_UNITS(CANON_UNITS_MM)" in ln:
            unit = 1.0
        m = re.search(r"(STRAIGHT_TRAVERSE|STRAIGHT_FEED|ARC_FEED|SET_G5X_OFFSET|SET_G92_OFFSET|USE_TOOL_LENGTH_OFFSET)\((.*)\)", ln)
        if not m:
            if "N....." not in ln and ln.strip() not in ("executing", ""):
                err = err or ln
            continue
        kind, args = m.group(1), [a.strip() for a in m.group(2).split(",")]
        if kind == "SET_G5X_OFFSET":
            g5x = [float(a) * (unit if k != 5 else 1) for k, a in enumerate(args[1:])] + [0.0] * 3
        elif kind == "SET_G92_OFFSET":
            g92 = [float(a) * (unit if k != 5 else 1) for k, a in enumerate(args)] + [0.0] * 3
        elif kind == "USE_TOOL_LENGTH_OFFSET":
            nums = re.findall(r"-?\d+\.\d+", m.group(2))
            tlo = [float(v) * unit for v in nums] + [0.0] * 9
        elif kind in ("STRAIGHT_TRAVERSE", "STRAIGHT_FEED"):
            v = [float(a) * (unit if k < 3 else 1) for k, a in enumerate(args[:6])]
            moves.append(("L", v[0] + g5x[0] + g92[0] + tlo[0], v[2] + g5x[2] + g92[2] + tlo[2], v[5] + g5x[5] + g92[5], unit))
        else:
            # XZ plane: first = Z, second = X; rotation; then the end of the third axis (Y), then a, b, c
            fz, fx, cz, cx, rot = float(args[0]) * unit, float(args[1]) * unit, float(args[2]) * unit, float(args[3]) * unit, int(args[4])
            c = float(args[8])
            oz, ox, oc = g5x[2] + g92[2] + tlo[2], g5x[0] + g92[0] + tlo[0], g5x[5] + g92[5]
            moves.append(("A", fx + ox, fz + oz, c + oc, cx + ox, cz + oz, rot, unit))
    return moves, err


_rs_cache = {}
def rs_out_lines(path):
    res = subprocess.run([RS274, "-t", os.path.join(tempfile.gettempdir(), "h4-zero.tbl"), "-g", path], capture_output=True, text=True, cwd=os.path.dirname(path))
    return [l.strip() for l in (res.stdout + "\n" + res.stderr).splitlines()]


def run_trace(path):
    out = subprocess.run([TRACE, path], capture_output=True, text=True).stdout
    lines, err = [], None
    pts = {}
    for ln in out.splitlines():
        f = ln.split()
        if f[0] == "P":
            pts.setdefault(int(f[1]), []).append(tuple(float(v) for v in f[2:5]))
        elif f[2] == "ERR":
            err = (int(f[1]), " ".join(f[3:]))
        else:
            lines.append(int(f[1]))
    return lines, pts, err


def main():
    rng = random.Random(SEED)
    bad = 0
    stats = {"programs": 0, "moves": 0, "arcs": 0, "errors_agree": 0}
    tmp = os.environ.get("KEEP_DIR") or tempfile.mkdtemp()
    os.makedirs(tmp, exist_ok=True)
    for n in range(len(FILES) if FILES else COUNT):
        if FILES:
            path = os.path.abspath(FILES[n])
            prog = "(file %s)" % path
        else:
            prog = gen(rng)
            path = os.path.join(tmp, "p%d.ngc" % n)
            open(path, "w").write(prog)
        rs_moves, rs_err = run_rs274(path)
        h_lines, h_pts, h_err = run_trace(path)
        stats["programs"] += 1
        problems = []
        # both refuse the same line: LinuxCNC may have moved part of it first (it checks some words late, e.g.
        # it rises to R before it misses Q); the H4 refuses the whole line before it moves at all
        same_line = False
        if rs_err is not None and h_err is not None:
            src = open(path).read().split("\n")
            bad_line = src[h_err[0] - 1].strip() if 0 < h_err[0] <= len(src) else None
            if bad_line and bad_line in rs_out_lines(path):
                same_line = True
        if (rs_err is None) != (h_err is None):
            problems.append("error mismatch: rs274=%s h4=%s" % (rs_err, h_err))
        # H4 points per line in order; an arc line has several, a G28 line two (intermediate, home)
        h_seq = []
        for ln in sorted(h_pts):
            h_seq.append((ln, h_pts[ln]))
        # walk the rs274 moves and consume H4 points
        flat = []
        for ln, ps in h_seq:
            flat.append([ln, ps])
        i = 0  # index in flat
        j = 0  # point inside flat[i]
        prev = (0.0, 0.0, 0.0)
        for mv in rs_moves:
            if i >= len(flat):
                if not same_line:
                    problems.append("rs274 has more moves than H4: %s" % (mv,))
                break
            ln, ps = flat[i]
            # rs274 prints 4 decimals in the program's units (offsets too): allow that rounding
            tol = TOL + 3 * 0.00005 * 25.4  # offsets may have been printed in inches earlier
            if mv[0] == "L":
                p = ps[j]
                if max(abs(p[0] - mv[1]), abs(p[1] - mv[2]), abs(p[2] - mv[3])) > tol:
                    problems.append("line %d: H4 %s, LinuxCNC %s" % (ln, p, mv[1:]))
                    break
                prev = p
                j += 1
            else:
                stats["arcs"] += 1
                x, z, c, cx, cz, rot = mv[1:7]
                seg = ps[j:]
                end = seg[-1]
                if max(abs(end[0] - x), abs(end[1] - z), abs(end[2] - c)) > tol:
                    problems.append("arc line %d end: H4 %s, LinuxCNC %s" % (ln, end, (x, z, c)))
                    break
                r0 = math.hypot(prev[0] - cx, prev[1] - cz)
                sweep = 0.0
                a_prev = math.atan2(prev[0] - cx, prev[1] - cz)
                for q in seg:
                    rq = math.hypot(q[0] - cx, q[1] - cz)
                    if abs(rq - r0) > max(TOL, 0.002 * r0) + 0.01:
                        problems.append("arc line %d: point %s off the LinuxCNC circle (r %.4f vs %.4f)" % (ln, q, rq, r0))
                        break
                    a = math.atan2(q[0] - cx, q[1] - cz)
                    d = a - a_prev
                    while d > math.pi: d -= 2 * math.pi
                    while d < -math.pi: d += 2 * math.pi
                    sweep += d
                    a_prev = a
                if problems:
                    break
                turns = abs(rot)
                if rot > 0 and sweep < 0 or rot < 0 and sweep > 0:
                    problems.append("arc line %d: direction differs (LinuxCNC %d, H4 sweep %.3f)" % (ln, rot, sweep))
                    break
                if abs(sweep) > 2 * math.pi * turns + 1e-6 or abs(sweep) <= 2 * math.pi * (turns - 1) - 1e-6:
                    problems.append("arc line %d: turns differ (LinuxCNC %d, H4 sweep %.3f rad)" % (ln, turns, sweep))
                    break
                prev = end
                j = len(ps)
            stats["moves"] += 1
            if j >= len(flat[i][1]):
                i += 1
                j = 0
        else:
            if i < len(flat) and not problems:
                problems.append("H4 has more moves than rs274 from line %d" % flat[i][0])
        if rs_err is not None and h_err is not None:
            stats["errors_agree"] += 1
        if problems:
            bad += 1
            if bad <= 5:
                print("---- program %d (%s)" % (n, path))
                print(prog)
                for p in problems:
                    print("  " + p)
                print("  rs274 error: %s" % rs_err)
    print("compared %(programs)d programs, %(moves)d moves, %(arcs)d arcs, both refused %(errors_agree)d;" % stats, "%d differ" % bad)
    sys.exit(1 if bad else 0)


main()
