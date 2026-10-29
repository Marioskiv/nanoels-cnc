# cam_check.py - runs the real CAM programs of test/cam (README.md there) through the H4 tests.
#   python3 cam_check.py OUT_DIR [RS274]
# https://github.com/Marioskiv/nanoels-cnc - Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import glob, gzip, os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
out, rs274 = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else None)
fails = 0
for gz in sorted(glob.glob(os.path.join(here, "cam", "*.gz"))):
    name = os.path.basename(gz)[:-3]
    path = os.path.join(out, name)
    open(path, "wb").write(gzip.open(gz).read())
    if not name.endswith("_polar.ngc"):
        # X Y Z milling as written by the CAM: everything up to the first Y move is read, that line stops it
        trace = subprocess.run([os.path.join(out, "gcode_trace"), path], capture_output=True, text=True).stdout
        err = [l for l in trace.splitlines() if " ERR " in l]
        lines = open(path).read().split("\n")
        first_y = next(i + 1 for i, l in enumerate(lines) if re.search(r"(^|[^A-Z(])Y\s*[-+.0-9]", re.sub(r"\(.*?\)", "", l).upper()))
        ok = len(err) == 1 and err[0].startswith("L %d ERR" % first_y) and "no Y axis" in err[0]
        print("%s: %s" % (name, "read up to line %d, the first Y move: refused, no Y axis" % first_y if ok else "UNEXPECTED: %s (first Y line %d)" % (err[:1], first_y)))
        fails += 0 if ok else 1
        continue
    # the converted X Z C program: simulation, corners, self-test, LinuxCNC
    r = subprocess.run([os.path.join(out, "cnc_sim"), path, "--machine", "marios"], capture_output=True, text=True)
    print("%s: %s" % (name, r.stdout.splitlines()[0].split(": ", 1)[1] if r.stdout else r.stderr))
    if r.returncode: print(r.stdout); fails += 1
    r = subprocess.run([os.path.join(out, "cnc_junctions"), path, "--check"], capture_output=True, text=True)
    if r.returncode: print(r.stdout); fails += 1
    r = subprocess.run([os.path.join(out, "cnc_bench_pc"), path], capture_output=True, text=True)
    print("  self-test:", r.stdout.strip().split(": ", 1)[-1])
    if r.returncode: fails += 1
    if rs274:
        r = subprocess.run([sys.executable, os.path.join(here, "lcnc_compare.py"), rs274, os.path.join(out, "gcode_trace"), path], capture_output=True, text=True)
        print("  LinuxCNC:", r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr)
        if r.returncode: fails += 1
print("cam: %d failure(s)" % fails)
sys.exit(1 if fails else 0)
