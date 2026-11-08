#!/usr/bin/env python3
"""scenario_test.py - the whole H4 firmware on a PC (test/h4sim), used through its web API as the app uses it.

Builds the firmware for the PC (build.py), starts it with empty settings (run.sh) and checks, in real time:
  - it starts: /status answers with the firmware's version, the LCD shows the H4's screen;
  - MDI moves: the motors, counted from the STEP / DIR pins, end exactly where the firmware says the tool is, with
    the steps per mm, direction and backlash of the firmware's own defaults (read from h4.ino);
  - a program uploaded as the app uploads it (/upload) runs to its end (/cmd?do=start) and the tool ends at its
    last point; Pause stops the motors on the path, Resume ends the program exactly;
  - the app's connection lost during a program pauses it (3 s), Resume goes on; lost while the spindle had already
    paused it: the message says so (it said "Spindle stopped" and starting the spindle did nothing - found here);
  - STOP in the middle of a move: the firmware's position is the motors' position.
Usage: python3 scenario_test.py [BUILD_DIR] [PORT]      (exit 1 on a failure)
Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
Copyright (c) 2026 Marioskiv
SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
H4INO = os.path.normpath(os.path.join(HERE, "..", "..", "h4.ino"))
BUILD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(tempfile.gettempdir(), "h4sim-test")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18180
B = "http://127.0.0.1:%d" % PORT
C = "http://127.0.0.1:%d" % (PORT + 1)
checks = failures = 0


def check(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(("ok  : " if ok else "FAIL: ") + what, flush=True)


def http(url, method="GET", data=None, ctype=None, timeout=10):
    req = urllib.request.Request(url, data=data, method=method)
    if ctype:
        req.add_header("Content-Type", ctype)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")


def status():
    return json.loads(http(B + "/status")[1])


def sim():
    return json.loads(http(C + "/state")[1])


def post(path, **args):
    return http(B + path + ("?" + urllib.parse.urlencode(args) if args else ""), "POST", b"")


def ctl(path, **args):
    return http(C + path + "?" + urllib.parse.urlencode(args), "POST", b"")


def mdi(line):
    return post("/mdi", line=line)


def wait_idle(timeout=30, poll=0.3):
    """As the app: asks for /status every 0.3 s until the machine is idle again (also keeps the link watch fed)."""
    end = time.time() + timeout
    time.sleep(0.5)
    quiet = 0
    while time.time() < end:
        s = status()
        # idle, not ON and nothing running (the firmware refuses uploads until then), twice in a row
        quiet = quiet + 1 if s["state"] == "Idle" and not s["on"] and not s.get("running") else 0
        if quiet >= 2:
            return s
        time.sleep(poll)
    return status()


def upload(name, text):
    boundary = "----h4simboundary"
    body = ("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: text/plain\r\n\r\n%s\r\n"
            "--%s--\r\n" % (boundary, name, text, boundary)).encode()
    end = time.time() + 2
    while True:  # just after a move the firmware still sees an axis settling for a few tens of ms: it refuses then
        code, text = http(B + "/upload", "POST", body, "multipart/form-data; boundary=" + boundary)
        if code == 200 or "ON or moving" not in text or time.time() > end:
            return code, text
        time.sleep(0.1)


def default(name):
    m = re.search(r"^\s*(?:long|int|bool|float)\s+%s\s*=\s*([^;]+);" % name, open(H4INO).read(), re.M)
    v = m.group(1).strip()
    return v == "true" if v in ("true", "false") else float(eval(v))


# the firmware's own defaults: steps per mm, direction, backlash (h4.ino)
SPM = {"X": default("MOTOR_STEPS_X") / (default("SCREW_X_DU") / 10000.0),
       "Z": default("MOTOR_STEPS_Z") / (default("SCREW_Z_DU") / 10000.0)}
SIGN = {"X": -1 if default("INVERT_X") else 1, "Z": -1 if default("INVERT_Z") else 1}
BACKLASH = {"X": round(default("BACKLASH_DU_X") / 10000.0 * SPM["X"]), "Z": round(default("BACKLASH_DU_Z") / 10000.0 * SPM["Z"])}


def motor_expected(axis, mm, last_negative):
    """The motor's step count with the tool at mm: after a move toward minus the backlash has been taken up."""
    return SIGN[axis] * (round(mm * SPM[axis]) - (BACKLASH[axis] if last_negative else 0))


def main():
    # build and start
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
                status()
                break
            except Exception:
                time.sleep(0.1)
        run_checks()
    finally:
        os.killpg(proc.pid, signal.SIGTERM)
        proc.wait()
    print("h4sim scenario: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


def run_checks():
    fw = re.search(r'#define FW_VERSION "([^"]+)"', open(H4INO).read()).group(1)
    s = status()
    check(s["fw"] == fw and s["state"] == "Idle", "starts: /status answers, firmware %s, Idle" % s["fw"])
    check(any(line.strip() for line in sim()["lcd"]), "the LCD shows the H4's screen: %s" % sim()["lcd"][0].strip())

    # MDI moves with the spindle turning (the default pauses G-code while it stands)
    ctl("/spindle", rpm=500)
    check(mdi("$X")[0] == 200, "MDI $X (moves before Home All)")
    mdi("G0 X10 Z-5")
    s = wait_idle()
    m = sim()["motors"]
    check(abs(s["x"] - 10) < 1e-3 and abs(s["z"] + 5) < 1e-3, "G0 X10 Z-5: the firmware reports X %.4f Z %.4f" % (s["x"], s["z"]))
    check(m["X"] == motor_expected("X", 10, False) and m["Z"] == motor_expected("Z", -5, True),
          "the motors (from the STEP/DIR pins): X %d Z %d steps, expected %d %d (Z took up its backlash)"
          % (m["X"], m["Z"], motor_expected("X", 10, False), motor_expected("Z", -5, True)))
    mdi("G0 X0 Z0")
    s = wait_idle()
    m = sim()["motors"]
    check(m["X"] == motor_expected("X", 0, True) and m["Z"] == motor_expected("Z", 0, False),
          "back to X0 Z0: motors X %d Z %d, expected %d %d (X took up its backlash)"
          % (m["X"], m["Z"], motor_expected("X", 0, True), motor_expected("Z", 0, False)))

    # a program uploaded and started as the app does
    prog = "G21 G90\nG0 X5 Z0\nG1 Z-2 F600\nG1 X0\nG1 Z0\nG0 X3\nM2\n"
    code, text = upload("square.ngc", prog)
    check(code == 200, "upload square.ngc (%d %s)" % (code, text.strip()[:60]))
    code, text = post("/cmd", **{"do": "start", "name": "square.ngc"})
    check(code == 200, "start square.ngc (%d %s)" % (code, text.strip()[:60]))
    s = wait_idle(60)
    m = sim()["motors"]
    check(abs(s["x"] - 3) < 1e-3 and abs(s["z"]) < 1e-3 and m["X"] == motor_expected("X", 3, False) and m["Z"] == motor_expected("Z", 0, False),
          "the program ran to its end: tool X %.4f Z %.4f, motors X %d Z %d" % (s["x"], s["z"], m["X"], m["Z"]))

    # Pause on the path, Resume to the exact end
    upload("slow.ngc", "G21 G90\nG1 X8 F120\nM2\n")
    post("/cmd", **{"do": "start", "name": "slow.ngc"})
    time.sleep(1.0)
    post("/cmd", **{"do": "pause"})
    time.sleep(0.8)
    a = sim()["motors"]["X"]
    st = status()
    time.sleep(1.0)
    b = sim()["motors"]["X"]
    check(st["state"] == "Hold" and a == b and a != motor_expected("X", 3, False),
          "Pause: state %s, X motor stands at %d (moved, then stood still for 1 s)" % (st["state"], a))
    post("/cmd", **{"do": "resume"})
    s = wait_idle(30)
    check(abs(s["x"] - 8) < 1e-3 and sim()["motors"]["X"] == motor_expected("X", 8, False),
          "Resume: the program ends exactly at X 8 (motor %d)" % sim()["motors"]["X"])

    # the app's connection lost during a program: pause after 3 s, Resume goes on
    upload("away.ngc", "G21 G90\nG1 X2 F60\nM2\n")
    post("/cmd", **{"do": "start", "name": "away.ngc"})
    time.sleep(0.5)
    status()
    time.sleep(4.5)     # the app does not ask for 4.5 s
    st = status()
    check(st["state"] == "Hold" and "connection was lost" in st["text"],
          "the app away for 4.5 s: paused, \"%s\"" % st["text"][:60])
    post("/cmd", **{"do": "resume"})
    s = wait_idle(30)
    check(abs(s["x"] - 2) < 1e-3, "Resume with the app back: the program ends at X %.4f" % s["x"])

    # lost while the spindle had already paused the program: the message says why it will not go on by itself
    ctl("/spindle", rpm=0)
    mdi("G0 X6")
    time.sleep(0.5)
    st = status()
    check(st["state"] == "Hold" and "Spindle stopped" in st["text"], "spindle stopped: the program waits (%s)" % st["text"])
    time.sleep(4.5)
    ctl("/spindle", rpm=500)
    time.sleep(1.5)
    st = status()
    check(st["state"] == "Hold" and "connection was lost" in st["text"],
          "then the app was away and the spindle started: still paused, and the message says why (\"%s\")" % st["text"][:60])
    post("/cmd", **{"do": "resume"})
    s = wait_idle(30)
    check(abs(s["x"] - 6) < 1e-3, "Resume: the move ends at X %.4f" % s["x"])

    # STOP in the middle of a move: the firmware's position is where the motors are
    mdi("G1 X0 F120")
    time.sleep(1.2)
    post("/cmd", **{"do": "stop"})
    s = wait_idle(10)
    m = sim()["motors"]
    expected = motor_expected("X", s["x"], True)
    check(0.1 < s["x"] < 5.9 and abs(m["X"] - expected) <= 1,
          "STOP during a move: the tool stopped at X %.4f, the motor at %d steps (expected %d)" % (s["x"], m["X"], expected))


if __name__ == "__main__":
    sys.exit(main())
