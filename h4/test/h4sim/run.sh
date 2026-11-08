#!/bin/sh
# run.sh - starts the H4 firmware on the PC (test/h4sim) and keeps it running: a restart the firmware asks for
# (ESP.restart(), exit code 3) starts it again, as the ESP32 would. Stops on any other exit or on SIGTERM.
#   sh run.sh BUILD_DIR [PORT]      (the firmware's web server on PORT, default 8080; the test's on PORT + 1)
# Data (settings, files) in BUILD_DIR/data; the serial output in BUILD_DIR/serial.log.
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
B=$1
PORT=${2:-8080}
export H4SIM_DATA="$B/data" H4SIM_PORT="$PORT" H4SIM_SERIAL="$B/serial.log"
reset=poweron
child=
trap 'kill $child 2>/dev/null; exit 0' TERM INT
while :; do
  H4SIM_RESET=$reset "$B/h4sim" &
  child=$!
  wait $child
  code=$?
  [ $code -eq 3 ] || exit $code
  reset=sw
done
