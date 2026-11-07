#!/usr/bin/env python3
# Planner stress programs for cnc_sim (work plan B4): long programs of small blocks as a polar converter writes them.
#   gen_stress.py spiral N   - N G93 moves along an Archimedean spiral (X radius, C angle), blocks of 0.05 .. 0.4 mm
#   gen_stress.py tiny N     - N G1 moves of 0.002 .. 0.02 mm (G94 F600 and G93), zig-zag and straight runs
#   gen_stress.py g93edge N  - G93 moves with F from very slow to far above what the axes allow (the planner limits them)
#   gen_stress.py arcs N     - N small G2 / G3 arcs (G18, X-Z) with C, radius 0.2 .. 5 mm
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import math, random, sys
kind, n = sys.argv[1], int(sys.argv[2])
random.seed(n * 7 + len(kind))
out = ['G21 G90 G94 G18', 'G0 X5 Z1 C0', 'G1 Z0 F300']
x, z, c = 5.0, 0.0, 0.0
if kind == 'spiral':
    out.append('G93')
    r, a = 5.0, 0.0
    for i in range(n):
        step = random.uniform(0.05, 0.4)            # mm along the path
        da = math.degrees(step / max(r, 0.5))       # angle for that arc length
        a += da
        r = 5 + 40 * (i / n)                        # out to 45 mm
        f = random.choice([600, 1200, 2400]) / step # G93: 1 / minutes for the block
        out.append(f'G1 X{r:.4f} C{a:.4f} F{f:.2f}')
elif kind == 'tiny':
    for i in range(n):
        d = random.uniform(0.002, 0.02)
        if random.random() < 0.5:
            x += d * random.choice([-1, 1]); z += d * random.choice([-1, 0, 1])   # zig-zag
        else:
            x += d                                                                  # a straight run of tiny pieces
        x = min(max(x, 0.5), 100)
        if i % 2:
            out.append(f'G94 G1 X{x:.4f} Z{z:.4f} F600')
        else:
            out.append(f'G93 G1 X{x:.4f} Z{z:.4f} C{c + i * 0.01:.4f} F{random.uniform(2000, 30000):.1f}')
elif kind == 'g93edge':
    out.append('G93')
    for i in range(n):
        x = min(max(x + random.uniform(-3, 3), 0.5), 100); z += random.uniform(-0.5, 0.5); c += random.uniform(-90, 90)
        f = random.choice([50, 200, 500, 5000, 50000, 500000])   # 1.2 s .. 0.12 ms per block
        out.append(f'G1 X{x:.4f} Z{z:.4f} C{c:.4f} F{f}')
elif kind == 'arcs':
    out.append('G94 F400')
    for i in range(n):
        rad = random.uniform(0.2, 5); ang = random.uniform(0.3, 3.0) * random.choice([-1, 1])
        a0 = random.uniform(0, 2 * math.pi)
        ci, ck = -rad * math.cos(a0), -rad * math.sin(a0)       # centre from the start point
        ex = x + ci + rad * math.cos(a0 + ang); ez = z + ck + rad * math.sin(a0 + ang)
        if not (0.5 <= ex <= 100 and 0.5 <= x + ci - rad and x + ci + rad <= 100):
            out.append(f'G1 X{min(max(x, 6), 90):.4f}')
            x = min(max(x, 6), 90)
            continue
        g = 3 if ang > 0 else 2
        c += random.uniform(-5, 5)
        out.append(f'G{g} X{ex:.4f} Z{ez:.4f} I{ci:.4f} K{ck:.4f} C{c:.4f}')
        x, z = float(f'{ex:.4f}'), float(f'{ez:.4f}')
out += ['G94', 'G0 Z1', 'M30']
print('\n'.join(out))
