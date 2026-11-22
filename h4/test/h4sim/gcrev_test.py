#!/usr/bin/env python3
"""
gcrev_test.py - firmware 1.53, GCODE_REV_X / _Z: GCODE counts X and Z the other way than the H4's own (lathe)
positions, as LinuxCNC / ISO does while the H4's lathe modes count Z+ towards the chuck and X+ towards the centre.
The whole firmware on the PC (h4sim): G-code moves go the other way on the motors, with the backlash taken up as the
H4's own moves take it (the planner's play on its minus side = the H4's plus side), the screen shows GCODE's numbers,
a zero set in G-code stays where it was set, and the app's jog in a lathe mode goes the same way on the machine as the
same button in GCODE mode.

Usage: python3 gcrev_test.py [BUILD_DIR] [PORT]      (exit 1 on a failure)

Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
Copyright (c) 2026 Marioskiv
SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import scenario_test as T  # noqa: E402  (HTTP helpers, the firmware's defaults)

BUILD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(tempfile.gettempdir(), "h4sim-rev")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18880
T.PORT, T.B, T.C = PORT, "http://127.0.0.1:%d" % PORT, "http://127.0.0.1:%d" % (PORT + 1)
checks = failures = 0
SPM, SIGN, BL = T.SPM, T.SIGN, T.BACKLASH


def check(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(("ok  : " if ok else "FAIL: ") + what)


def expected(axis, mm, last_gcode_positive):
    """Motor steps with the tool at GCODE mm on a reversed axis: the H4's own position is -mm; after a G-code move
    towards plus (the H4's minus) the motor is the backlash below the tool, as after the H4's own moves down."""
    return SIGN[axis] * (-round(mm * SPM[axis]) - (BL[axis] if last_gcode_positive else 0))


def run_checks():
    items = json.loads(T.http(T.B + "/config")[1])["items"]
    q = "&".join("%s=%s" % (i["k"], urllib.parse.quote(str((1 if i["v"] else 0) if isinstance(i["v"], bool) else i["v"])))
                 for i in items if i["k"] not in ("xGcRev", "zGcRev")) + "&xGcRev=1&zGcRev=1"
    code, text = T.http(T.B + "/config", "POST", q.encode(), "application/x-www-form-urlencoded")
    st = T.wait_back()
    c = {i["k"]: i["v"] for i in json.loads(T.http(T.B + "/config")[1])["items"]}
    check(code == 200 and st is not None and c.get("xGcRev") == 1 and c.get("zGcRev") == 1,
          "saved: GCODE counts X and Z the other way (%d %s)" % (code, text.strip()[:40]))
    T.post("/cmd", **{"do": "gcode"})
    time.sleep(0.3)
    T.ctl("/spindle", rpm=500)
    T.mdi("$X")
    T.mdi("G0 X10 Z-5")
    s = T.wait_idle()
    m = T.sim()["motors"]
    check(abs(s["x"] - 10) < 1e-3 and abs(s["z"] + 5) < 1e-3, "G0 X10 Z-5: the screen shows X %.4f Z %.4f" % (s["x"], s["z"]))
    ex, ez = expected("X", 10, True), expected("Z", -5, False)
    check(m["X"] == ex and m["Z"] == ez, "the motors the other way: X %d Z %d steps, expected %d %d" % (m["X"], m["Z"], ex, ez))
    T.mdi("G0 X0 Z0")
    s = T.wait_idle()
    m = T.sim()["motors"]
    ex, ez = expected("X", 0, False), expected("Z", 0, True)
    check(abs(s["x"]) < 1e-3 and abs(s["z"]) < 1e-3 and m["X"] == ex and m["Z"] == ez,
          "back to X0 Z0: screen %.4f %.4f, motors %d %d, expected %d %d (the play taken up as the H4 takes it)" % (s["x"], s["z"], m["X"], m["Z"], ex, ez))
    # many reversals in a program: the tool ends where G-code says, the play never lost
    prog = "G21 G90\nG1 Z-2 F900\nG1 Z1\nG1 Z-3\nG1 X4\nG1 X1\nG1 X6 Z-1\nG0 X3 Z2\nM2\n"
    T.upload("rev.ngc", prog)
    T.post("/cmd", **{"do": "start", "name": "rev.ngc"})
    time.sleep(1)
    s = T.wait_idle(60)
    m = T.sim()["motors"]
    ex, ez = expected("X", 3, False), expected("Z", 2, True)
    check(abs(s["x"] - 3) < 1e-3 and abs(s["z"] - 2) < 1e-3 and m["X"] == ex and m["Z"] == ez,
          "a program with reversals ends at X %.4f Z %.4f, motors %d %d (expected %d %d)" % (s["x"], s["z"], m["X"], m["Z"], ex, ez))
    # a zero set in G-code stays where it was set; a move from it goes the same way as before
    T.mdi("G10 L20 P0 X0 Z0")
    time.sleep(0.3)
    s0 = T.status()
    T.mdi("G0 X1.5 Z-1")
    s = T.wait_idle()
    m = T.sim()["motors"]
    ex, ez = expected("X", 4.5, True), expected("Z", 1, False)
    check(abs(s0["x"]) < 1e-3 and abs(s0["z"]) < 1e-3 and abs(s["x"] - 1.5) < 1e-3 and abs(s["z"] + 1) < 1e-3 and m["X"] == ex and m["Z"] == ez,
          "G10 L20 zero here, then G0 X1.5 Z-1: screen %.4f %.4f, motors %d %d (expected %d %d)" % (s["x"], s["z"], m["X"], m["Z"], ex, ez))
    # the app's jog in GCODE mode: Z+ goes as G-code Z+ (motor the other way than the H4's own plus)
    z0 = T.sim()["motors"]["Z"]
    end = time.time() + 0.8
    while time.time() < end:
        T.post("/jog", x="0", z="300", c="0")
        time.sleep(0.1)
    for _ in range(3):
        T.post("/jog", x="0", z="0", c="0")
        time.sleep(0.1)
    T.wait_idle()
    zg = T.sim()["motors"]["Z"] - z0
    # the same button in a lathe mode: the H4's own arrow key, the same way on the machine
    T.http(T.B + "/lathe", "POST", b"mode=0", "application/x-www-form-urlencoded")
    time.sleep(0.5)
    z1 = T.sim()["motors"]["Z"]
    end = time.time() + 1.0
    while time.time() < end:
        T.post("/jog", x="0", z="600", c="0")
        time.sleep(0.1)
    for _ in range(3):
        T.post("/jog", x="0", z="0", c="0")
        time.sleep(0.1)
    time.sleep(1.5)
    zl = T.sim()["motors"]["Z"] - z1
    check(zg * SIGN["Z"] < 0 and zl * zg > 0, "the app's Z+ goes the same way on the machine in GCODE (%d steps) and in a lathe mode (%d)" % (zg, zl))


def main():
    r = subprocess.run([sys.executable, os.path.join(HERE, "build.py"), BUILD], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        print("h4sim: build failed")
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
    print("h4sim GCODE the other way: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
