#!/usr/bin/env python3
"""lcnc_traj.py - runs G-code programs in real LinuxCNC 2.9 (its interpreter, task, motion controller and trajectory
planner, no hardware, lcnc_ref/h4ref.ini) and in the H4's motion core on a PC (cnc_sim --samples), both on the
author's machine, and compares what the tool does, sampled every millisecond:
  - path: the largest distance of the H4's tool from LinuxCNC's path ON THE PART (x = X cos C, y = X sin C, z = Z: what
    a polar machine cuts), in feed moves (checked) and in rapids (shown: LinuxCNC rounds the corners between G0
    moves more, in the air);
    LinuxCNC rounds corners within G64 P0.025, the H4 goes through every point, so up to ~0.035 mm is expected.
    "G64 P0.025" is given by MDI before every program: LinuxCNC 2.9.10 does not pass the G64 P of the .ini's
    startup code to its motion controller (found with this tool), corners would be rounded with no limit;
  - end point: both must end at the same place;
  - time: the H4's time / LinuxCNC's time for the whole program;
  - speed and acceleration of every axis (over 20 ms), against the axis limits: LinuxCNC never exceeds them.
Usage:
  python3 lcnc_traj.py --linuxcnc <dir of a built LinuxCNC or "installed"> --sim <cnc_sim> [--user U] prog.ngc ...
LinuxCNC refuses to run its realtime part as root: with --user it runs as that user (the programs and the config
are copied to a directory that user can read). Exit 1 when a program differs more than the limits below.
Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
Copyright (c) 2026 Marioskiv
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import math
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LIMITS = {"x": (12.0, 100.0), "z": (25.0, 100.0), "c": (360.0, 720.0)}  # units/s, units/s^2 (h4ref.ini)
STEPS = {"x": 1200.0, "z": 400.0, "c": 91.733333}
PATH_LIMIT_MM = 0.035       # G64 P0.025 + steps
TIME_RATIO = (0.70, 1.30)   # H4 time / LinuxCNC time
ACC_WINDOW_S = 0.02
PATH_PREFIX = None          # --path: directory put first on the PATH (e.g. the python3 LinuxCNC was built for)


def read(path):
    rows = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) >= 4:
                rows.append([float(v) for v in p[:4]] + [int(p[4]) if len(p) > 4 else 0])
    return rows


def trim(rows):
    """From the first sample that moved to the last one that moved, t from 0."""
    if not rows:
        return rows
    first, last = 0, len(rows) - 1
    while first + 1 < len(rows) and rows[first + 1][1:4] == rows[0][1:4]:
        first += 1
    while last > first and rows[last - 1][1:4] == rows[-1][1:4]:
        last -= 1
    t0 = rows[first][0]
    return [[r[0] - t0] + r[1:] for r in rows[first:last + 1]]


def path_deviation(h4, ref):
    """Largest distance of an H4 sample from LinuxCNC's sampled path, [during feed moves, during rapids]. Both go
    forward along the path, so each H4 sample is looked for on the segments just behind and ahead of where the
    previous one was found."""
    import numpy as np

    def onPart(rows):
        a = np.array([r[1:4] for r in rows])
        c = np.radians(a[:, 2])
        return np.column_stack([a[:, 0] * np.cos(c), a[:, 0] * np.sin(c), a[:, 1]])

    pts = onPart(ref)
    keep = np.ones(len(pts), dtype=bool)
    keep[1:] = np.any(pts[1:] != pts[:-1], axis=1)
    poly = pts[keep]
    if len(poly) < 2:
        poly = np.vstack([poly, poly])
    a, b = poly[:-1], poly[1:]
    ab = b - a
    l2 = np.einsum("ij,ij->i", ab, ab)
    l2[l2 == 0] = 1e-30
    hp = onPart(h4)
    worst, worstAt, j = [0.0, 0.0], [0.0, 0.0], 0  # [feed moves, rapids]
    n = len(a)
    for idx, p in enumerate(hp):
        lo, hi = max(0, j - 50), min(n, j + 400)
        ap = p - a[lo:hi]
        t = np.clip(np.einsum("ij,ij->i", ap, ab[lo:hi]) / l2[lo:hi], 0, 1)
        d = np.linalg.norm(ap - t[:, None] * ab[lo:hi], axis=1)
        m = int(np.argmin(d))
        best = d[m]
        if best > PATH_LIMIT_MM:  # lost the place (a long stop on one side): look along the whole path
            ap = p - a
            t = np.clip(np.einsum("ij,ij->i", ap, ab) / l2, 0, 1)
            d = np.linalg.norm(ap - t[:, None] * ab, axis=1)
            m2 = int(np.argmin(d))
            if d[m2] < best:
                best, m, lo = d[m2], m2, 0
        j = lo + m
        kind = 1 if h4[idx][4] else 0
        if best > worst[kind]:
            worst[kind], worstAt[kind] = float(best), h4[idx][0]
    return worst, worstAt


def dynamics(rows):
    """Largest speed and acceleration of each axis, over ACC_WINDOW_S windows (one step of quantisation off)."""
    import numpy as np
    n = int(round(ACC_WINDOW_S / 0.001))
    arr = np.array([r[1:4] for r in rows])
    out = {}
    for i, a in enumerate("xzc"):
        x = arr[:, i]
        if len(x) <= 2 * n:
            out[a] = (0.0, 0.0)
            continue
        v = np.abs(x[2 * n:] - x[n:-n]) / ACC_WINDOW_S
        acc = np.abs(x[2 * n:] - 2 * x[n:-n] + x[:-2 * n]) / ACC_WINDOW_S ** 2 - 4 / STEPS[a] / ACC_WINDOW_S ** 2
        out[a] = (float(v.max()), float(max(acc.max(), 0)))
    return out


def run_linuxcnc(lcnc, user, prog, workdir, reuse=False, mdi="G64 P0.025", tag=""):
    ref = os.path.join(workdir, "ref")
    if not os.path.isdir(ref):
        shutil.copytree(os.path.join(HERE, "lcnc_ref"), ref)
    p = os.path.join(ref, os.path.basename(prog))
    shutil.copy(prog, p)
    out = p + tag + ".lcnc"
    if reuse and os.path.exists(out):
        err = open(out + ".err").read().strip() if os.path.exists(out + ".err") else ""
        return read(out), err
    env_setup = "" if lcnc == "installed" else ". '%s/scripts/rip-environment'; " % lcnc
    pre = "export PATH='%s':$PATH; " % PATH_PREFIX if PATH_PREFIX else ""
    cmd = "%s%sexport PATH='%s':$PATH; cd '%s' && H4REF_MDI='%s' H4REF_PROGRAM='%s' H4REF_OUT='%s' timeout 900 linuxcnc h4ref.ini > '%s.log' 2>&1 || { e=$?; for f in ~/linuxcnc_print.txt ~/linuxcnc_debug.txt; do [ -f $f ] && grep -v '^+' $f | tail -40 >> '%s.log'; done; exit $e; }" % (
        pre, env_setup, ref, ref, mdi, p, out, out, out)
    if user:
        subprocess.run(["chown", "-R", user, workdir], check=True)
        r = subprocess.run(["su", user, "-s", "/bin/bash", "-c", cmd])
    else:
        r = subprocess.run(["bash", "-c", cmd])
    if not os.path.exists(out):
        log = open(out + ".log").read() if os.path.exists(out + ".log") else ""
        raise RuntimeError("LinuxCNC did not run (exit %d):\n%s" % (r.returncode, log[-2000:]))
    err = open(out + ".err").read().strip() if os.path.exists(out + ".err") else ""
    return read(out), err


def run_h4(sim, prog, workdir):
    out = os.path.join(workdir, os.path.basename(prog) + ".h4")
    r = subprocess.run([sim, "--samples", out, prog], capture_output=True, text=True)
    return read(out), r.stdout.strip().splitlines()[0] if r.stdout else r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--linuxcnc", required=True)
    ap.add_argument("--sim", required=True)
    ap.add_argument("--user", default=None)
    ap.add_argument("--path", default=None, help="directory put first on the PATH for LinuxCNC (its python3)")
    ap.add_argument("--keep", default=None, help="directory for the sampled trajectories (kept)")
    ap.add_argument("--reuse", action="store_true", help="with --keep: use LinuxCNC's trajectories found there")
    ap.add_argument("--mdi", default="G64 P0.025", help="MDI commands before each program, ';' between them")
    ap.add_argument("programs", nargs="+")
    a = ap.parse_args()
    global PATH_PREFIX
    PATH_PREFIX = a.path
    workdir = a.keep or tempfile.mkdtemp(prefix="lcnc_traj_")
    os.makedirs(workdir, exist_ok=True)
    os.chmod(workdir, 0o755)
    bad = 0
    for prog in a.programs:
        name = os.path.basename(prog)
        tag = "" if a.mdi == "G64 P0.025" else "." + "".join(ch for ch in a.mdi if ch.isalnum())
        ref, err = run_linuxcnc(a.linuxcnc, a.user, prog, workdir, a.reuse, a.mdi, tag)
        h4, simLine = run_h4(a.sim, prog, workdir)
        if err:
            print("%s: LinuxCNC reported: %s" % (name, err.replace("\n", " | ")))
        ref, h4 = trim(ref), trim(h4)
        if not ref or not h4:
            print("%s: FAIL no motion (LinuxCNC %d samples, H4 %d)" % (name, len(ref), len(h4)))
            bad += 1
            continue
        dev, devAt = path_deviation(h4, ref)
        # the end point: within one motor step of each axis (the H4 rounds to steps, LinuxCNC does not)
        endSteps = max(abs(h4[-1][i + 1] - ref[-1][i + 1]) * STEPS[a] for i, a in enumerate("xzc"))
        endDiff = max(abs(h4[-1][i] - ref[-1][i]) for i in (1, 2, 3))
        tH4, tRef = h4[-1][0], ref[-1][0]
        ratio = tH4 / tRef if tRef > 0 else float("inf")
        dH4, dRef = dynamics(h4), dynamics(ref)
        fails = []
        if dev[0] > PATH_LIMIT_MM:
            fails.append("path %.4f mm off LinuxCNC's at %.3f s (feed move)" % (dev[0], devAt[0]))
        if endSteps > 1.0:
            fails.append("ends %.4f away (%.1f steps)" % (endDiff, endSteps))
        if not TIME_RATIO[0] <= ratio <= TIME_RATIO[1]:
            fails.append("time %.3f x LinuxCNC's" % ratio)
        notes = []
        for ax in "xzc":
            vl, al = LIMITS[ax]
            vh, ah = dH4[ax]
            vr, ar = dRef[ax]
            if vh > vl * 1.02 + 2 / STEPS[ax] / ACC_WINDOW_S:
                fails.append("%s speed %.2f > %.2f" % (ax.upper(), vh, vl))
            if ah > al * 1.5 + 4 / STEPS[ax] / ACC_WINDOW_S ** 2:  # a corner may add a jump of a x 10 ms
                notes.append("%s accel %.0f > %.0f (LinuxCNC %.0f)" % (ax.upper(), ah, al, ar))
        print("%s: %s time H4 %.3f s / LinuxCNC %.3f s = %.3f, path %.4f mm (rapids %.4f), end %.4f; "
              "max speed/accel H4 X %.1f/%.0f Z %.1f/%.0f C %.0f/%.0f, LinuxCNC X %.1f/%.0f Z %.1f/%.0f C %.0f/%.0f%s%s" % (
                  name, "FAIL" if fails else "ok", tH4, tRef, ratio, dev[0], dev[1], endDiff,
                  dH4["x"][0], dH4["x"][1], dH4["z"][0], dH4["z"][1], dH4["c"][0], dH4["c"][1],
                  dRef["x"][0], dRef["x"][1], dRef["z"][0], dRef["z"][1], dRef["c"][0], dRef["c"][1],
                  ("; " + ", ".join(fails)) if fails else "", ("; NOTE " + ", ".join(notes)) if notes else ""))
        if fails:
            bad += 1
    print("lcnc_traj: %d programs, %d differ" % (len(a.programs), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
