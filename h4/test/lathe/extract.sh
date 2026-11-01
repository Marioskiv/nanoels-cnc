#!/bin/sh
# Extracts the named top-level functions from h4.ino, unchanged (for lathe_sim.cpp).
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
F=$1; shift
for n in "$@"; do awk -v n="$n" 'index($0,n"(")>0 && $0 ~ /^[a-z].*\) \{$/ && p==0 {p=1} p{print} p && /^}/ {p=0; print ""}' "$F"; done
