#!/bin/sh
# Lathe screen vs the H4 (see compare.py). Needs g++, node with playwright, python3.
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
set -e
cd "$(dirname "$0")"
OUT=${OUT:-/tmp/h4-lathe-test}; mkdir -p "$OUT"
{ sh extract.sh ../../threadpass.ino "long threadPassPos"; sh extract.sh ../../h4.ino "long posFromSpindle" "long spindleFromPos" "long spindleModulo" "void modeTurn" "void modeCone" "void modeCut" "void modeEllipse"; } > "$OUT/modes.inc"
g++ -O2 -std=c++17 -I"$OUT" -o "$OUT/lathe_sim" lathe_sim.cpp
cp ../../../android/app/src/main/assets/h4/index.html "$OUT/page.html" # the H4 screen, part of the app
node threads.js "$OUT/page.html" # thread data and form: ISO 965-1, own thread, G, Tr, UN
node plans.js "$OUT/page.html" "$OUT/plans.json"
python3 compare.py "$OUT/plans.json" "$OUT/lathe_sim"
