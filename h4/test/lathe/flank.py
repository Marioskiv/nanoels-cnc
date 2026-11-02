# Flank infeed (threadFlank): runs the H4's modeTurn (extracted unchanged) in lathe_sim for a thread with and without
# the option and checks the spindle sync of each pass: later by depth * tan(29.5) / lead * encoder steps, nothing else
# changes (the same path of the tool, the same depths). Usage: python3 flank.py <lathe_sim>
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import subprocess, sys, math, json
sim = sys.argv[1]; ENC = 2400; T = math.tan(math.radians(29.5)); fails = 0; checks = 0
def run(pitch, starts, passes, flank, xs=-5.0, xe=-5.92):
    # thread 20 mm long towards the chuck (z 0 -> 20), the cross slide from xs to xe (H4 frame, mm), H4 sim units: 1 um
    a = [sim, '7', str(pitch), str(starts), str(passes), '0', '1', '20', '0', str(max(xs, xe)), str(min(xs, xe)), '0', str(xs), str(flank)]
    r = subprocess.run(a, capture_output=True, text=True)
    syncs = [tuple(map(int, l.split()[1:])) for l in r.stderr.splitlines() if l.startswith('sync ')]
    return json.loads(r.stdout), syncs
def ok(c, what):
    global fails, checks
    checks += 1
    if not c: fails += 1; print('FAIL', what)
for pitch, starts, passes in [(1.5, 1, 10), (2.0, 1, 6), (1.0, 2, 8), (-1.5, 1, 10)]:
    p0, s0 = run(pitch, starts, passes, 0); p1, s1 = run(pitch, starts, passes, 1)
    ok(p0 == p1, f'same tool path {pitch} {starts}')
    ok(len(s0) == len(s1) == passes * starts, f'one sync per pass {len(s0)} {len(s1)}')
    lead = abs(pitch) * starts * 10000  # du per spindle turn
    for (op0, v0), (op1, v1) in zip(s0, s1):
        k = math.ceil(op0 / starts); xs, xe = -5.0, -5.92
        # depth of pass k from the start (sqrt rule, threadpass.ino), in du (sim: 1 step = 10 du)
        d_um = abs(round((xe - xs) * 1000 * math.sqrt(k / passes))) if k < passes else abs(round((xe - xs) * 1000))
        want = round(d_um * 10 * T / lead * ENC)
        ok((v1 - v0) % ENC == want % ENC, f'pass {op0} pitch {pitch}: flank sync {(v1 - v0) % ENC} want {want}')
    # the last pass: shift along the thread = full depth * tan 29.5
    last = (s1[-1][1] - s0[-1][1]) % ENC / ENC * lead / 10000
    ok(abs(last - 0.92 * T) < abs(pitch) * starts / ENC * 2, f'last pass shift {last:.4f} mm, want {0.92 * T:.4f}')
print(f'flank: {checks} checks, {fails} failures'); sys.exit(1 if fails else 0)
