#!/usr/bin/env python3
"""
mymachine_test.py - firmware 1.52: a firmware built with my_machine.h (the machine's LinuxCNC values) stores them in the
H4 whenever they are new to it - also over a set stored earlier - and keeps corrections made in the app until
my_machine.h itself changes.

Usage: python3 mymachine_test.py [BUILD_DIR] [PORT]      (exit 1 on a failure)

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
import scenario_test as T  # noqa: E402  (HTTP helpers)

BUILD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(tempfile.gettempdir(), "h4sim-mm")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18780
T.PORT, T.B, T.C = PORT, "http://127.0.0.1:%d" % PORT, "http://127.0.0.1:%d" % (PORT + 1)
checks = failures = 0


def check(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(("ok  : " if ok else "FAIL: ") + what)


MACHINE = """void myMachine() {
  MOTOR_STEPS_X = 2400;
  SCREW_X_DU = 20000;
  SPEED_MANUAL_MOVE_X = 12 * 1200;
  ACCELERATION_X = 100 * 1200;
  SPEED_START_X = 1200;
  HOME_X_USE = true;
  HOME_X_SEARCH_VEL = -3;
  HOME_X_LATCH_VEL = 0.5;
  HOME_X_OFFSET = %s;
  HOME_X_HOME = 0;
  LIMIT_X_USE = true;
  LIMIT_X_MIN = -22.039;
  LIMIT_X_MAX = 108.961;
}
"""


def build(offset):
    path = os.path.join(BUILD, "test_my_machine.h")
    os.makedirs(BUILD, exist_ok=True)
    with open(path, "w") as f:
        f.write(MACHINE % offset)
    r = subprocess.run([sys.executable, os.path.join(HERE, "build.py"), BUILD, "--machine", path], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        sys.exit("h4sim: build failed")


def start():
    p = subprocess.Popen(["sh", os.path.join(HERE, "run.sh"), BUILD, str(PORT)], start_new_session=True,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        try:
            T.status()
            break
        except Exception:
            time.sleep(0.1)
    return p


def stop(p):
    os.killpg(p.pid, signal.SIGTERM)
    p.wait()


def cfg():
    return {i["k"]: i["v"] for i in json.loads(T.http(T.B + "/config")[1])["items"]}


def main():
    build("-24.039")
    data = os.path.join(BUILD, "data")
    if os.path.isdir(data):
        shutil.rmtree(data)
    # an H4 that has a set stored already (here: saved with the public firmware's numbers), as the owner's had
    os.makedirs(os.path.join(data, "prefs"))
    with open(os.path.join(data, "prefs", "h4cfg.txt"), "w") as f:
        f.write("v=1\nxSteps=800\nxScrew=20000\nhxOff=0\nhxUse=0\n")
    p = start()
    try:
        s, c = T.status(), cfg()
        check(s.get("cfgMy") == 1 and c["xSteps"] == 2400 and abs(c["hxOff"] + 24.039) < 1e-9 and c["hxUse"] == 1,
              "update with my_machine.h over a stored set: its values go in (cfgMy %s, xSteps %s, HOME_OFFSET %s)" % (s.get("cfgMy"), c["xSteps"], c["hxOff"]))
        stop(p)
        p = start()
        s = T.status()
        check(s.get("cfgMy") == 0 and abs(cfg()["hxOff"] + 24.039) < 1e-9, "next start: nothing new, the values kept (cfgMy %s)" % s.get("cfgMy"))
        # a correction in the app stays (my_machine.h unchanged)
        items = json.loads(T.http(T.B + "/config")[1])["items"]
        q = "&".join("%s=%s" % (i["k"], urllib.parse.quote(str(i["v"]))) for i in items if i["k"] != "hxOff") + "&hxOff=-24.1"
        code, text = T.http(T.B + "/config", "POST", q.encode(), "application/x-www-form-urlencoded")
        T.wait_back()
        stop(p)
        p = start()
        s = T.status()
        check(code == 200 and s.get("cfgMy") == 0 and abs(cfg()["hxOff"] + 24.1) < 1e-9,
              "a correction made in the app stays through a restart (%d, HOME_OFFSET %s)" % (code, cfg()["hxOff"]))
        stop(p)
        # my_machine.h changed (as the .ini did): the next update brings its values
        build("-23.5")
        p = start()
        s = T.status()
        check(s.get("cfgMy") == 1 and abs(cfg()["hxOff"] + 23.5) < 1e-9, "my_machine.h changed: its values go in again (HOME_OFFSET %s)" % cfg()["hxOff"])
    finally:
        stop(p)
    print("h4sim my_machine: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
