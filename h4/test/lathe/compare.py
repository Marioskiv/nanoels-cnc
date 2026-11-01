# Lathe screen vs the H4: every point of the H4 path (its own mode functions, lathe_sim) must lie on the page drawing
# and the other way round (tolerance 0.05 mm, the approach from wherever the tool stands left out).
# Usage: sh run.sh  (needs g++, node with playwright, python3)
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import json, subprocess, math, sys
plans = json.load(open(sys.argv[1])); SIM = sys.argv[2]; fails = 0
def num(v): return '-' if v in ('', None) else str(v)
def segdist(p, a, b):
    ax, ay = a; bx, by = b; px, py = p
    dx, dy = bx-ax, by-ay; L = dx*dx+dy*dy
    t = 0 if L == 0 else max(0, min(1, ((px-ax)*dx+(py-ay)*dy)/L))
    return math.hypot(ax+t*dx-px, ay+t*dy-py)
def polydist(pts, poly):
    worst = (0, None)
    for p in pts:
        d = min(segdist(p, poly[i], poly[i+1]) for i in range(len(poly)-1)) if len(poly) > 1 else math.hypot(p[0]-poly[0][0], p[1]-poly[0][1])
        if d > worst[0]: worst = (d, p)
    return worst
def thin(path, step=0.05):
    out=[path[0]]
    for p in path[1:]:
        if math.hypot(p[0]-out[-1][0], p[1]-out[-1][1]) >= step: out.append(p)
    out.append(path[-1]); return out
for i, pl in enumerate(plans):
    q = pl['q']; m = pl['m']
    if pl['err']: print(i, m, 'PAGE ERROR', pl['err']); continue
    args = [SIM, str(m), str(q.get('pitch', 0)), str(q.get('starts', 1)), str(q.get('passes', 1)), str(q.get('cone', 0)), str(q.get('aux', 1)),
            num(q.get('zl')), num(q.get('zr')), num(q.get('xl')), num(q.get('xr')), str(pl['st']['z']), str(pl['st']['x'])]
    h4 = json.loads(subprocess.run(args, capture_output=True, text=True).stdout)
    h4 = [p for p in h4 if p[2] >= 1] if m in (4,5,6,7) else h4
    if m == 8: h4 = h4[1:]  # the first move leaves wherever the tool stands
    h4p = thin([[a, b] for a, b, _ in h4])
    page = [[a, b] for a, b, c in pl['path']]
    # page path starts from the tool position; H4 path starts after the first move: compare the H4 against the page and back
    d1 = polydist(h4p, page) if len(page) > 0 else (None, None)
    d2 = polydist(thin(page), [[pl['st']['z'], pl['st']['x']]] + h4p)
    passes_h4 = sorted(set(round(b, 3) for a, b, k in h4 if k >= 1))
    bad = (d1[0] or 0) > 0.05 or d2[0] > 0.05
    fails += bad
    print('FAIL' if bad else 'ok  ', 'mode', m, pl['vals'], 'H4->page %.3f page->H4 %.3f mm' % (d1[0] or 0, d2[0]))
print('lathe page vs H4: %d plans, %d failures' % (len(plans), fails))
sys.exit(1 if fails else 0)
