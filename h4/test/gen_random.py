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
    elif kind < 0.5:
        out.append(f'G94 G1 X{nx} Z{nz} C{nc} F{random.choice([30, 120, 600, 3000])}')
    else:
        out.append(f'G93 G1 X{nx} Z{nz} C{nc} F{round(random.uniform(0.5, 3000), 4)}')
    x, z, c = nx, nz, nc
    if random.random() < 0.03:
        out.append('G4 P0.2')
out += ['G94', 'G0 X0 Z0 C0', 'M30']
print('\n'.join(out))
