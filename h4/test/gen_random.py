#!/usr/bin/env python3
# Random X/Z/C programs for cnc_sim: rapids, G94 and G93 moves, short and long, G91 parts.
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import random, sys
seed = int(sys.argv[1]); random.seed(seed)
out = ['G21 G90 G94', 'F300']
x, z, c = 0.0, 0.0, 0.0
for i in range(random.randint(20, 300)):
    kind = random.random()
    nx = round(max(0, min(100, x + random.uniform(-20, 20) * random.choice([0, 0.01, 1]))), 4)
    nz = round(z + random.uniform(-5, 5) * random.choice([0, 0.01, 1]), 4)
    nc = round(c + random.uniform(-400, 400) * random.choice([0, 0.001, 0.1, 1]), 4)
    if kind < 0.15:
        out.append(f'G0 X{nx} Z{nz} C{nc}')
    elif kind < 0.25:
        # an arc in the X-Z plane: incremental centre, end point = start turned about it
        import math
        ci, ck = random.uniform(-10, 10), random.uniform(-10, 10)
        ang = random.uniform(-3, 3)
        sx, sz = -ci, -ck
        ex = sx * math.cos(ang) - sz * math.sin(ang) + ci
        ez = sx * math.sin(ang) + sz * math.cos(ang) + ck
        if 0 <= x + ex <= 100 and min(x + ci - math.hypot(ci, ck), 0) >= -1e9:
            g = random.choice([2, 3])
            nx, nz = round(x + ex, 4), round(z + ez, 4)
            # the end point is rounded: recompute I K so both radii match within the tolerance
            out.append(f'G94 G{g} X{nx} Z{nz} I{round(ci, 4)} K{round(ck, 4)} C{nc} F{random.choice([60, 300, 1200])}')
        else:
            out.append(f'G0 X{nx} Z{nz} C{nc}')
    elif kind < 0.5:
        out.append(f'G94 G1 X{nx} Z{nz} C{nc} F{random.choice([30, 120, 600, 3000])}')
    else:
        out.append(f'G93 G1 X{nx} Z{nz} C{nc} F{round(random.uniform(0.5, 3000), 4)}')
    x, z, c = nx, nz, nc
    if random.random() < 0.03:
        out.append('G4 P0.2')
    if random.random() < 0.03:
        # a drilling cycle along Z at the current X (G17), then back to G18 and the old position
        cyc = random.choice(['G81', f'G82 P0.1', f'G83 Q{round(random.uniform(0.5, 3), 3)}', f'G73 Q{round(random.uniform(0.5, 3), 3)}', 'G85', 'G89 P0.1'])
        out.append(f'G94 G17 G99 {cyc} X{x} Z{round(z - random.uniform(1, 8), 4)} R{round(z + 1, 4)} F{random.choice([60, 200])}')
        out.append('G80 G18')
        out.append(f'G0 X{x} Z{z} C{c}')
out += ['G94', 'G0 X0 Z0 C0', 'M30']
print('\n'.join(out))
