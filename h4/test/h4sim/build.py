#!/usr/bin/env python3
"""build.py - builds the whole NanoEls H4 firmware for a PC (the "H4 on a PC", test/h4sim).

The sketch is put together as the Arduino builder does: h4.ino first, then the other .ino files in alphabetical
order, joined into one C++ file, with a declaration of every top-level function inserted before the first function
(the Arduino builder generates these with ctags; here they are read with a small parser). Nothing in the firmware is
changed: the ESP32, Arduino and library parts it uses are stand-ins in shim/ (sim.cpp runs them on the PC).

  python3 build.py OUT_DIR [--machine my_machine.h] [--cxx g++]   -> OUT_DIR/h4sim

Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
Copyright (c) 2026 Marioskiv
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
H4 = os.path.normpath(os.path.join(HERE, "..", ".."))


def code_only(text):
    """The text with comments, strings and character literals blanked (same length, newlines kept)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif text.startswith('R"', i) and re.match(r'R"([^(\s]*)\(', text[i:]):
            tag = re.match(r'R"([^(\s]*)\(', text[i:]).group(1)
            j = text.find(")" + tag + '"', i)
            j = n if j < 0 else j + len(tag) + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(n, j + 1)
            out.append(c + " " * (j - i - 2) + c if j - i >= 2 else " " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


SIG = re.compile(r"^([A-Za-z_][\w\s\*&:<>,]*?[\s\*&])(\w+)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(const\s*)?\{",
                 re.M)
SKIP_NAMES = {"if", "for", "while", "switch", "return", "else", "do", "sizeof"}


def strip_defaults(params):
    """'int a = 1, bool b = true' -> 'int a, bool b' (a declaration may not repeat default arguments)."""
    parts, depth, cur = [], 0, ""
    for ch in params:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    parts.append(cur)
    return ", ".join(p.split("=")[0].strip() for p in parts if p.strip())


def prototypes(text):
    """Declarations of the functions defined at the top level of one .ino file."""
    code = code_only(text)
    protos, depth, start = [], 0, 0
    # the top-level parts: everything at brace depth 0
    top = []
    for i, ch in enumerate(code):
        if ch == "{":
            if depth == 0:
                top.append((start, i + 1))
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                start = i + 1
    for a, b in top:
        chunk = code[a:b]
        m = None
        for m in SIG.finditer(chunk):
            pass
        if not m or m.end() != len(chunk):
            continue
        ret, name, params = m.group(1).strip(), m.group(2), m.group(3)
        if name in SKIP_NAMES or ret.startswith(("struct", "class", "enum", "union", "typedef", "#", "template")):
            continue
        if re.search(r"\b(struct|class|enum|union)\s*$", ret):
            continue
        ret = re.sub(r"\s+", " ", ret)
        protos.append("%s %s(%s)%s;" % (ret, name, re.sub(r"\s+", " ", strip_defaults(params)),
                                        " const" if m.group(4) else ""))
    return protos


def first_function_offset(text):
    """Offset of the line where the first top-level function starts (the builder puts the declarations there)."""
    code = code_only(text)
    depth = 0
    starts = [0]
    for i, ch in enumerate(code):
        if ch == "{":
            if depth == 0:
                chunk = code[starts[-1]:i + 1]
                m = None
                for m in SIG.finditer(chunk):
                    pass
                if m and m.end() == len(chunk) and m.group(2) not in SKIP_NAMES \
                        and not m.group(1).strip().startswith(("struct", "class", "enum", "union", "typedef")):
                    return text.rfind("\n", 0, starts[-1] + m.start()) + 1
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                starts.append(i + 1)
        elif ch == ";" and depth == 0:
            starts.append(i + 1)
    return len(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--machine", default=None, help="a my_machine.h for the simulated machine")
    ap.add_argument("--cxx", default="g++")
    ap.add_argument("--no-compile", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    src = os.path.join(a.out, "src")
    if os.path.isdir(src):
        shutil.rmtree(src)
    os.makedirs(src)
    inos = [os.path.join(H4, "h4.ino")] + sorted(f for f in glob.glob(os.path.join(H4, "*.ino"))
                                                  if os.path.basename(f) != "h4.ino")
    texts = [(f, open(f, encoding="utf-8").read()) for f in inos]
    protos = []
    for f, t in texts:
        protos += prototypes(t)
    main_text = texts[0][1]
    at = first_function_offset(main_text)
    joined = ['#include <Arduino.h>\n#line 1 "%s"\n' % texts[0][0], main_text[:at],
              "// declarations of every function of the sketch (as the Arduino builder adds them)\n",
              "\n".join(protos), '\n#line %d "%s"\n' % (main_text[:at].count("\n") + 1, texts[0][0]), main_text[at:]]
    for f, t in texts[1:]:
        joined.append('\n#line 1 "%s"\n' % f)
        joined.append(t)
    sketch = os.path.join(src, "sketch.cpp")
    open(sketch, "w").write("".join(joined))
    for f in glob.glob(os.path.join(H4, "*.cpp")) + glob.glob(os.path.join(H4, "*.h")):
        shutil.copy(f, src)
    if a.machine:
        shutil.copy(a.machine, os.path.join(src, "my_machine.h"))
    elif os.path.exists(os.path.join(src, "my_machine.h")):
        os.remove(os.path.join(src, "my_machine.h"))
    print("sketch: %d .ino files, %d function declarations -> %s" % (len(texts), len(protos), sketch))
    if a.no_compile:
        return 0
    cpps = [sketch] + [os.path.join(src, os.path.basename(f)) for f in glob.glob(os.path.join(H4, "*.cpp"))] \
        + [os.path.join(HERE, "shim", "sim.cpp")]
    cmd = [a.cxx, "-std=gnu++17", "-O1", "-g", "-pthread", "-DARDUINO=10819", "-DH4SIM",
           "-I" + os.path.join(HERE, "shim"), "-I" + src, "-w", "-o", os.path.join(a.out, "h4sim")] + cpps
    r = subprocess.run(cmd)
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
