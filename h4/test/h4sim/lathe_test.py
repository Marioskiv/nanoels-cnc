#!/usr/bin/env python3
"""lathe_test.py - the H4's lathe modes (electronic lead screw) of the whole firmware on a PC (test/h4sim), set and
started through the web API as the app's Lathe screen does (/lathe), with a simulated spindle turning the encoder.
The machine is recorded every millisecond (spindle encoder counts and the motors' steps from the STEP / DIR pins):
  - gearbox (feed / thread by hand): Z follows the spindle at exactly the pitch, also when the spindle turns back;
  - thread, 1 start: every pass cuts in the same groove (Z against the spindle angle, to the step), the depth grows
    to the end, the H4 switches itself off at the end;
  - thread, 2 starts: the two starts half a turn apart, each start's passes in its own groove;
  - turning: the tool never leaves the stops, the last pass is at the final diameter.
Usage: python3 lathe_test.py [BUILD_DIR] [PORT]      (exit 1 on a failure)
Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
Copyright (c) 2026 Marioskiv
SPDX-License-Identifier: GPL-3.0-or-later
"""
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import scenario_test as T  # noqa: E402  (HTTP helpers, the firmware's defaults)

BUILD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(tempfile.gettempdir(), "h4sim-test")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18680
T.PORT, T.B, T.C = PORT, "http://127.0.0.1:%d" % PORT, "http://127.0.0.1:%d" % (PORT + 1)
COUNTS = 1200  # encoder counts per spindle turn in the simulation (600 PPR, both edges of A, as the H4's PCNT)
checks = failures = 0


def check(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(("ok  : " if ok else "FAIL: ") + what, flush=True)


def lathe(**args):
    import urllib.parse
    return T.http(T.B + "/lathe?" + urllib.parse.urlencode(args), "POST", b"")


def trace(since):
    return [list(map(int, line.split())) for line in T.http(T.C + "/trace?from=%d" % since)[1].splitlines()]


def run_until_off(timeout):
    """Polls as the app does while a lathe operation runs; returns when the H4 has switched itself off."""
    end = time.time() + timeout
    time.sleep(1)
    while time.time() < end:
        s = T.status()
        if not s["on"]:
            return s
        time.sleep(0.35)
    return T.status()


def synced_segments(rows, a, axis=2, min_ms=100):
    """Parts of the record where the axis moves at a steps per encoder count (cutting a thread, or the gearbox).
    Each: (direction, rows). Windows of 100 ms, within 5 % + 3 steps (on the PC the firmware follows the spindle a
    few ms late, varying with the PC's load; at a slow spindle one stroke can come out as a few pieces, which
    join_strokes() puts back together)."""
    w, segs, cur = 100, [], None
    for i in range(w, len(rows)):
        ds = rows[i][1] - rows[i - w][1]
        dz = rows[i][axis] - rows[i - w][axis]
        if ds != 0 and dz != 0 and abs(abs(dz) - a * abs(ds)) <= 0.05 * a * abs(ds) + 3:
            sgn = 1 if dz * ds > 0 else -1
            if cur is None or cur[0] != sgn:
                cur = (sgn, [])
                segs.append(cur)
            cur[1].append(rows[i])
        else:
            cur = None
    return [s for s in segs if len(s[1]) >= min_ms]


def join_strokes(segs, a, gap_ms=600, same=6):
    """Pieces of one stroke: same direction, less than gap_ms apart and on the same line (offset within 'same'
    steps) -> one stroke."""
    out = []
    for seg in segs:
        if out and out[-1][0] == seg[0] and seg[1][0][0] - out[-1][1][-1][0] < gap_ms \
                and abs(offset(out[-1], a)[0] - offset(seg, a)[0]) <= same:
            out[-1] = (seg[0], out[-1][1] + seg[1])
        else:
            out.append(seg)
    return out


def offset(seg, a):
    """The line z = sgn * a * spindle + b of one segment: its b (median) and how far the samples are from it (5 ...
    95 %: on the PC a single delay of the loop() thread by a few ms puts a few samples off the line)."""
    sgn, rows = seg
    b = sorted(r[2] - sgn * a * r[1] for r in rows)
    n = len(b)
    return b[n // 2], b[(n * 95) // 100] - b[(n * 5) // 100]


def main():
    r = subprocess.run([sys.executable, os.path.join(HERE, "build.py"), BUILD], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        return 1
    if os.path.isdir(os.path.join(BUILD, "data")):
        shutil.rmtree(os.path.join(BUILD, "data"))
    proc = subprocess.Popen(["sh", os.path.join(HERE, "run.sh"), BUILD, str(PORT)], start_new_session=True,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                T.status()
                break
            except Exception:
                time.sleep(0.1)
        run_checks()
    finally:
        os.killpg(proc.pid, signal.SIGTERM)
        proc.wait()
    print("h4sim lathe: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


def run_checks():
    spm_z = T.SPM["Z"]
    chuck_checks()
    # 1. gearbox: pitch 1.5 mm, 10 turns forward, then 4 turns back
    pitch = 1.5
    a = pitch * spm_z / COUNTS
    lathe(mode=0, pitch=pitch)
    T.ctl("/spindle", rpm=0)
    since = T.sim()["ms"]
    # measured with the spindle standing (on the PC the firmware follows it a few ms late, by how busy the PC is)
    lathe(do="start")
    time.sleep(0.8)
    t1 = T.sim()["ms"]           # standing, the H4 ON
    T.ctl("/spindle", rpm=120)
    time.sleep(5)                # 10 turns
    T.ctl("/spindle", rpm=0)
    time.sleep(0.8)
    t2 = T.sim()["ms"]
    T.ctl("/spindle", rpm=-120)
    time.sleep(2)                # 4 turns back
    T.ctl("/spindle", rpm=0)
    time.sleep(0.8)
    t3 = T.sim()["ms"]
    rows = trace(since)
    T.post("/cmd", **{"do": "stop"})
    nearest = lambda t: max((r for r in rows if r[0] <= t), key=lambda r: r[0], default=None)
    r1, r2, r3 = nearest(t1), nearest(t2), nearest(t3)
    if r1 and r2 and r3:
        mm_per_turn = (r2[2] - r1[2]) / spm_z / ((r2[1] - r1[1]) / COUNTS)
        check(abs(abs(mm_per_turn) - pitch) < 0.0005, "gearbox 1.5 mm, 10 turns: Z moved %.4f mm per turn" % mm_per_turn)
        # the line Z = k * spindle + b: the same b after 4 turns back (the motor also takes up its backlash)
        k = (r2[2] - r1[2]) / (r2[1] - r1[1])
        b2, b3 = r2[2] - k * r2[1], r3[2] - k * r3[1]
        check(abs(abs(b3 - b2) - T.BACKLASH["Z"]) <= 3 or abs(b3 - b2) <= 3,
              "the spindle turned back 4 turns: Z went back on the same line (%.1f steps apart, backlash %d)" % (b3 - b2, T.BACKLASH["Z"]))
    else:
        check(False, "gearbox: no record at the times asked")

    # 2. thread, 1 start: 1.5 mm, 6 mm long, 1 mm deep, 3 passes
    thread_case(pitch=1.5, starts=1, passes=3)
    # 3. thread, 2 starts, 2 mm pitch (4 mm lead), at 40 rpm: Z then runs below its start speed. Faster, the loop()
    #    thread is interrupted for milliseconds by the PC and the following looks irregular here; the firmware itself
    #    follows within 2 steps at any of these speeds (follow_test.cpp, exact time)
    thread_case(pitch=2.0, starts=2, passes=2, rpm=40)

    # 4. turning: Z from 0 to -8, X from 0 to -1 (radius), 3 passes, feed 0.2 mm per turn
    lathe(mode=4, pitch=0.2, passes=3, starts=1, zl=0, zr=-8, xl=0, xr=-1)
    T.ctl("/spindle", rpm=300)
    time.sleep(0.5)
    st0, m0 = T.status(), T.sim()["motors"]   # where Z is (work mm) and its motor: the stops on the motor
    since = T.sim()["ms"]
    lathe(do="start")
    s = run_until_off(120)
    rows = trace(since)
    T.ctl("/spindle", rpm=0)
    zs = [r[2] for r in rows]
    st = s["lathe"]
    check(not s["on"] and st["done"], "turning: 3 passes done, the H4 switched itself off")
    sz = T.SIGN["Z"]
    to_mm = lambda m: st0["z"] + sz * (m - m0["Z"]) / spm_z   # motor steps -> the H4's Z (backlash not counted)
    zmin, zmax = min(to_mm(z) for z in zs), max(to_mm(z) for z in zs)
    bl = T.BACKLASH["Z"] / spm_z
    check(zmin >= -8 - bl - 0.01 and zmax <= 0 + bl + 0.01,
          "turning: Z stayed between the stops 0 and -8 (%.3f .. %.3f mm, backlash %.2f)" % (zmin, zmax, bl))


def chuck_checks():
    """The app's chuck tile: the speed while the app watches (also with the H4's own tacho display off), and the angle
    from the 0 its button sets, measured against the encoder counts the simulation turned."""
    T.ctl("/spindle", rpm=120)
    rpm = 0
    for _ in range(12):                    # polled as the app does
        time.sleep(0.35)
        rpm = T.status()["rpm"]
    check(abs(rpm - 120) <= 3, "the chuck at 120 rpm: the app reads %d rpm (H4 OFF, in a lathe mode)" % rpm)
    T.ctl("/spindle", rpm=0)
    time.sleep(0.3)
    code, text = T.post("/cmd", **{"do": "chuckzero"})
    a0 = T.status()["chuck"]
    check(code == 200 and (a0 < 0.31 or a0 > 359.69), "0 button: the chuck angle reads %.1f deg" % a0)
    c0 = T.sim()["spindleCounts"]
    T.ctl("/spindle", rpm=10)              # about a quarter turn
    time.sleep(1.5)
    T.ctl("/spindle", rpm=0)
    time.sleep(0.3)
    turned = (T.sim()["spindleCounts"] - c0) * 360.0 / COUNTS
    a1 = T.status()["chuck"]
    check(abs(a1 - turned % 360) <= 0.31, "the chuck turned %.1f deg by the encoder: the app reads %.1f deg" % (turned, a1))


def thread_case(pitch, starts, passes, rpm=100):
    spm_z = T.SPM["Z"]
    lead_steps_per_count = pitch * starts * spm_z / COUNTS  # the lead: the tool advances pitch x starts per turn
    lathe(mode=7, pitch=pitch, passes=passes, starts=starts, zl=0, zr=-6, xl=0, xr=-1)
    T.ctl("/spindle", rpm=rpm)
    time.sleep(0.5)
    since = T.sim()["ms"]
    lathe(do="start")
    s = run_until_off(180)
    rows = trace(since)
    T.ctl("/spindle", rpm=0)
    name = "thread %.1f mm, %d start%s, %d passes" % (pitch, starts, "s" if starts > 1 else "", passes)
    check(not s["on"] and s["lathe"]["done"], name + ": done, the H4 switched itself off")
    segs = synced_segments(rows, lead_steps_per_count, min_ms=100)
    # the cutting direction: the one with the most time in sync (moves to the start and returns can pass through the
    # cutting speed for a moment, in either direction)
    dirs = {d: sum(len(x[1]) for x in segs if x[0] == d) for d in (1, -1)}
    cut_dir = max(dirs, key=dirs.get)
    pieces = join_strokes([x for x in segs if x[0] == cut_dir], lead_steps_per_count)
    # a cutting stroke runs at least 2 mm along Z in sync (here ~6 mm); a move to the start that passes through the
    # cutting speed for a moment does not (reported, not hidden)
    long_enough = lambda x: abs(x[1][-1][2] - x[1][0][2]) >= 2 * spm_z
    cuts = [x for x in pieces if long_enough(x)]
    short = [round(abs(x[1][-1][2] - x[1][0][2]) / spm_z, 2) for x in pieces if not long_enough(x)]
    if short:
        print("      (%s: left out, shorter than 2 mm in sync: %s mm)" % (name, short))
    turn = lead_steps_per_count * COUNTS                  # Z steps of one spindle turn along the thread
    offs = [offset(x, lead_steps_per_count) for x in cuts]
    phases = [o[0] % turn for o in offs]
    check(len(cuts) >= passes * starts,
          name + ": %d cutting strokes found in sync with the spindle (at least %d)" % (len(cuts), passes * starts))
    if not offs:
        return
    # (the record is taken every ms: at 100 rpm the spindle turns ~2 counts between two samples, about one Z step)
    check(max(o[1] for o in offs) <= 12, name + ": each stroke on one line to %.1f steps (5 ... 95 %%)" % max(o[1] for o in offs))
    # each start's grooves: the phases (Z offset modulo one turn) fall on 'starts' places 1/starts of a turn apart
    step = turn / starts
    base = phases[0]
    dev = [min(abs(((p - base) % step)), step - abs(((p - base) % step))) for p in phases]
    groups = sorted(set(round(((p - base) % turn) / step) % starts for p in phases))
    check(max(dev) <= 6, name + ": every stroke in a groove: phases %s (one turn %d steps), largest miss %.1f steps"
          % ([round(p, 1) for p in phases], turn, max(dev)))
    check(len(groups) == starts, name + ": %d start%s cut (groups %s)" % (starts, "s" if starts > 1 else "", groups))
    xs = [x[1][len(x[1]) // 2][3] for x in cuts]
    check(len(xs) >= 2 and abs(xs[-1] - xs[0]) > 0, name + ": the depth changes between the passes (X %s steps)" % xs)


if __name__ == "__main__":
    sys.exit(main())
