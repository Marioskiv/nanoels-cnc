#!/bin/sh
# Runs the H4 PC tests and keeps a record of the results: h4/test/results/<UTC date>-<commit>.txt
# (what was tested, on which commit, with which tools, every "checks / failures" line), then says what changed
# since the previous record. The tests themselves are not changed; this only runs them and writes down the result.
#   sh h4/test/record.sh            the firmware tests (run_tests.sh; with RS274=/path/to/rs274 also against LinuxCNC)
#   sh h4/test/record.sh --lathe    also the H4 screen tests (lathe/run.sh: needs node with playwright)
# Exit code: 0 only when every test run passed.
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
cd "$(dirname "$0")"
LATHE=0
for a in "$@"; do [ "$a" = "--lathe" ] && LATHE=1; done
RES=results
mkdir -p "$RES"
OUT=${TMPDIR:-/tmp}/h4cnc-record
mkdir -p "$OUT"
commit=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
dirty=""; [ -n "$(git status --porcelain -- .. ../../android 2>/dev/null | grep -v ' h4/test/results/')" ] && dirty="+changes"
stamp=$(date -u +%Y-%m-%dT%H%M%SZ)
prev=$(ls "$RES"/*.txt 2>/dev/null | sort | tail -1)
file="$RES/$stamp-$commit$dirty.txt"

status=0
sh run_tests.sh > "$OUT/firmware.log" 2>&1; fw=$?
[ $fw = 0 ] || status=1
lathe="not run (add --lathe)"
if [ $LATHE = 1 ]; then
  sh lathe/run.sh > "$OUT/lathe.log" 2>&1; lt=$?
  [ $lt = 0 ] || status=1
  lathe="exit $lt"
fi

# the result lines: "N checks, M failures", "M failure(s)", "N plans, M failures", the self-test speeds
summary() { grep -E '[0-9]+ checks?, [0-9]+ failures?|: [0-9]+ failure\(s\)|plans, [0-9]+ failures|programs?, [0-9]+ diff|blocks/s' "$1" | sed 's/[[:space:]]*$//'; }
{
  echo "# H4 test record"
  echo "date (UTC): $stamp"
  echo "commit: $commit$dirty"
  echo "firmware: $(sed -n 's/^#define FW_VERSION "\(.*\)".*/\1/p' ../h4.ino)  app: $(sed -n "s/.*versionName '\(.*\)'.*/\1/p" ../../android/app/build.gradle)"
  echo "tools: $(g++ --version | head -1); $(python3 --version 2>&1); node $(node --version 2>/dev/null || echo none)"
  echo "LinuxCNC rs274: ${RS274:-not used (set RS274=/path/to/rs274)}"
  echo "firmware tests (run_tests.sh): exit $fw"
  echo "screen tests (lathe/run.sh): $lathe"
  echo "result: $([ $status = 0 ] && echo PASS || echo FAIL)"
  echo
  echo "## firmware tests"
  summary "$OUT/firmware.log"
  if [ $LATHE = 1 ]; then
    echo
    echo "## screen tests"
    summary "$OUT/lathe.log"
  fi
} > "$file"
cat "$file"
if [ $status != 0 ]; then
  echo
  echo "Full output: $OUT/firmware.log$([ $LATHE = 1 ] && echo " $OUT/lathe.log")"
fi
# what changed since the previous record (the result lines only, not the header; the planning speed of this PC
# changes on every run and is left out of the comparison)
if [ -n "$prev" ] && [ "$prev" != "$file" ]; then
  sed '1,/^$/d' "$prev" | sed 's/; this PC plans [0-9]* blocks\/s//' > "$OUT/prev.txt"
  sed '1,/^$/d' "$file" | sed 's/; this PC plans [0-9]* blocks\/s//' > "$OUT/now.txt"
  echo
  if cmp -s "$OUT/prev.txt" "$OUT/now.txt"; then
    echo "Same results as $(basename "$prev")."
  else
    echo "Changed since $(basename "$prev") (- before, + now):"
    diff "$OUT/prev.txt" "$OUT/now.txt" | grep '^[<>]' | sed 's/^</-/; s/^>/+/'
  fi
fi
exit $status
