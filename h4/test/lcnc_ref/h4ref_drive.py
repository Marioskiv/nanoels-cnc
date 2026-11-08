#!/usr/bin/env python3
# h4ref_drive.py - the "display" of h4ref.ini (found on the PATH: lcnc_traj.py puts this directory first): runs one program in LinuxCNC with no screen and records the joint positions
# every servo period. Program: $H4REF_PROGRAM, samples to $H4REF_OUT ("t x z c" per line, t in seconds from the start
# of the program). Exits when the program has ended (LinuxCNC then shuts down). Errors go to $H4REF_OUT.err.
# Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import subprocess
import sys
import time

import linuxcnc

program = os.environ["H4REF_PROGRAM"]
out = os.environ["H4REF_OUT"]
timeout = float(os.environ.get("H4REF_TIMEOUT", "3600"))
s = linuxcnc.stat()
c = linuxcnc.command()
e = linuxcnc.error_channel()
errors = []


def poll():
    s.poll()
    while True:
        m = e.poll()
        if not m:
            break
        errors.append(m[1])


def wait(cond, t=10.0):
    end = time.time() + t
    while time.time() < end:
        poll()
        if cond():
            return True
        time.sleep(0.01)
    return False


ok = wait(lambda: s.task_state == linuxcnc.STATE_ESTOP, 20)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
ok = ok and wait(lambda: s.task_state == linuxcnc.STATE_ON)
for mdi in [m for m in os.environ.get("H4REF_MDI", "").split(";") if m.strip()]:
    # MDI commands before the program, as a user types them. lcnc_traj.py gives "G64 P0.025": LinuxCNC 2.9.10 does
    # not pass the G64 P of RS274NGC_STARTUP_CODE to the motion controller (its message is queued at the
    # interpreter's start and never sent), so without it corners are blended with no tolerance (1.2 mm seen)
    c.mode(linuxcnc.MODE_MDI)
    wait(lambda: s.task_mode == linuxcnc.MODE_MDI)
    c.mdi(mdi)
    c.wait_complete(10)
c.mode(linuxcnc.MODE_AUTO)
ok = ok and wait(lambda: s.task_mode == linuxcnc.MODE_AUTO)
c.program_open(program)
wait(lambda: s.file == program, 5)
# the sampler runs from the start, straight into a file (a pipe would fill and stop it); its own sample count (-t)
# gives the time
raw = out + ".raw"
with open(raw, "w") as rawFile:
    sampler = subprocess.Popen(["halsampler", "-t"], stdout=rawFile)
    time.sleep(0.2)
    c.auto(linuxcnc.AUTO_RUN, 0)
    started = wait(lambda: s.interp_state != linuxcnc.INTERP_IDLE, 5)
    # the end: the interpreter idle, nothing queued, and the joints standing still for 0.3 s
    end = time.time() + timeout
    still, last = 0, None
    while time.time() < end:
        poll()
        pos = tuple(round(v, 6) for v in s.joint_actual_position[:3])
        still = still + 1 if pos == last else 0
        last = pos
        if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and still >= 15:
            break
        time.sleep(0.02)
    else:
        errors.append("timeout: the program did not end in %.0f s" % timeout)
    sampler.terminate()
    sampler.wait()
c.state(linuxcnc.STATE_OFF)
with open(raw) as f:
    rows = f.read().splitlines()
os.remove(raw)
# t from the first sample that differs from the start position, minus one period
data = []
for r in rows:
    p = r.split()
    if len(p) == 4:
        data.append([int(p[0])] + [float(v) for v in p[1:]])
start = 0
while start + 1 < len(data) and data[start + 1][1:] == data[0][1:]:
    start += 1
with open(out, "w") as f:
    for d in data[start:]:
        f.write("%.3f %.6f %.6f %.6f\n" % ((d[0] - data[start][0]) * 0.001, d[1], d[2], d[3]))
with open(out + ".err", "w") as f:
    if not ok:
        f.write("LinuxCNC did not get ready\n")
    if not started:
        f.write("the program did not start\n")
    for m in errors:
        f.write(m + "\n")
sys.exit(0)
