#!/bin/sh
# Lathe screen vs the H4 (see compare.py). Needs g++, node with playwright, python3.
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
set -e
cd "$(dirname "$0")"
OUT=${OUT:-/tmp/h4-lathe-test}; mkdir -p "$OUT"
{ sh extract.sh ../../threadpass.ino "long threadPassPos"; sh extract.sh ../../h4.ino "long posFromSpindle" "long spindleFromPos" "long spindleModulo" "void modeTurn" "void modeCone" "void modeCut" "void modeEllipse"; } > "$OUT/modes.inc"
g++ -O2 -std=c++17 -I"$OUT" -o "$OUT/lathe_sim" lathe_sim.cpp
python3 -c "import sys;s=open('../../webpage.h').read();open(sys.argv[1],'w').write(s[s.index('R\"rawpage(')+10:s.rindex(')rawpage\"')])" "$OUT/page.html"
node plans.js "$OUT/page.html" "$OUT/plans.json"
python3 compare.py "$OUT/plans.json" "$OUT/lathe_sim"
