#!/bin/sh
# Lathe screen vs the H4 (see compare.py). Needs g++, node with playwright, python3.
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
set -e
cd "$(dirname "$0")"
OUT=${OUT:-/tmp/h4-lathe-test}; mkdir -p "$OUT"
{ cat ../../threadpass.ino; sh extract.sh ../../h4.ino "long posFromSpindle" "long spindleFromPos" "long spindleModulo" "void modeTurn" "void modeCone" "void modeCut" "void modeEllipse"; } > "$OUT/modes.inc"
g++ -O2 -std=c++17 -I"$OUT" -o "$OUT/lathe_sim" lathe_sim.cpp
cp ../../../android/app/src/main/assets/h4/index.html "$OUT/page.html" # the H4 screen, part of the app
node threads.js "$OUT/page.html" # thread data and form: ISO 965-1, own thread, G, Tr, UN
node guard.js "$OUT/page.html" # touch-off guard: START waits for the zeros
node measure.js "$OUT/page.html" # measure and correct after a turning
node recipe.js "$OUT/page.html" # sequence of operations (recipe)
node advice.js "$OUT/page.html" # spindle speed / feed starting point
node cone.js "$OUT/page.html" # taper by angle, chamfer
node draw.js "$OUT/page.html" # half section and tool shapes
node nodef.js "$OUT/page.html" # empty forms, start mark
node zface.js "$OUT/page.html" # Z from the face, Part first, drawing at a fixed scale
node gamepad.js "$OUT/page.html" # gamepad map: alone / with the safety button, refusals, fake pad
node lang.js "$OUT/page.html" # every screen in Greek / English, switching back and forth
node settings.js "$OUT/page.html" # Settings: whole pages that scroll, App log
node dropdown.js "$OUT/page.html" # choice lists roll down under their button, second tap closes
node plans.js "$OUT/page.html" "$OUT/plans.json"
python3 compare.py "$OUT/plans.json" "$OUT/lathe_sim"
python3 flank.py "$OUT/lathe_sim" # thread flank infeed: the spindle sync of each pass
