#!/usr/bin/env python3
# Checks the BUILT firmware (the .elf): the GCODE step interrupt and everything it calls must be in IRAM (or the ROM),
# and the constants it loads must not point into the flash. Then the interrupt can run while the flash is read or
# written (ESP_INTR_FLAG_IRAM): no call or data access into the flash, where it would crash or wait.
#   python3 iram_check.py <h4.ino.elf> [objdump] [nm]
# The toolchain's objdump / nm are found in ~/.arduino15 when not given (Arduino ESP32 core, xtensa).
# https://github.com/Marioskiv/nanoels-cnc
# Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
import glob
import os
import re
import struct
import subprocess
import sys

ROOTS = ['cncTimerIsr']   # the IRAM step interrupt (cncStepTick, cnc_isr ... are followed from it, or inlined)
IRAM = (0x40370000, 0x403E0000)          # ESP32-S3 internal instruction RAM
ROM = (0x40000000, 0x40060000)           # ESP32-S3 ROM code
FLASH_CODE = (0x42000000, 0x44000000)    # code executed from the flash (cache)
FLASH_DATA = (0x3C000000, 0x3E000000)    # constants read from the flash (cache)


def tool(name, given):
    if given:
        return given
    for pattern in ['~/.arduino15/packages/esp32/tools/*/*/bin/xtensa-esp32s3-elf-' + name,
                    '~/.arduino15/packages/esp32/tools/*/*/bin/xtensa-esp-elf-' + name,
                    '~/.arduino15/packages/esp32/tools/*/*/*/bin/xtensa-esp32s3-elf-' + name,
                    '~/.arduino15/packages/esp32/tools/*/*/*/bin/xtensa-esp-elf-' + name]:
        found = sorted(glob.glob(os.path.expanduser(pattern)))
        if found:
            return found[-1]
    sys.exit('iram check: ' + name + ' of the xtensa toolchain not found')


def inside(addr, rng):
    return rng[0] <= addr < rng[1]


def elf_reader(path):
    """Returns read(addr) -> 32-bit word from the loadable sections of a 32-bit little-endian ELF."""
    data = open(path, 'rb').read()
    shoff, = struct.unpack_from('<I', data, 0x20)
    shentsize, shnum = struct.unpack_from('<HH', data, 0x2E)
    sections = []
    for i in range(shnum):
        name, typ, flags, addr, offset, size = struct.unpack_from('<IIIIII', data, shoff + i * shentsize)
        if typ == 1 and addr and size:  # PROGBITS with an address
            sections.append((addr, offset, size))

    def read(a):
        for addr, offset, size in sections:
            if addr <= a and a + 4 <= addr + size:
                return struct.unpack_from('<I', data, offset + a - addr)[0]
        return None
    return read


def main():
    elf = sys.argv[1]
    objdump = tool('objdump', sys.argv[2] if len(sys.argv) > 2 else None)
    nm = tool('nm', sys.argv[3] if len(sys.argv) > 3 else None)
    read = elf_reader(elf)
    # symbols: address, size, demangled name
    syms = {}
    by_addr = {}
    for line in subprocess.run([nm, '-C', '-S', '--defined-only', elf], capture_output=True, text=True, check=True).stdout.splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4 and parts[2] in 'tTwW':
            addr, size, name = int(parts[0], 16), int(parts[1], 16), parts[3]
            base = re.sub(r'\(.*$', '', name).split('::')[-1]
            syms.setdefault(base, (addr, size, name))
            by_addr[addr] = (addr, size, name)
    problems = []
    seen = set()
    todo = []
    for r in ROOTS:
        if r not in syms:
            problems.append('function not found in the firmware: ' + r)
        else:
            todo.append(syms[r])
    checked = []
    while todo:
        addr, size, name = todo.pop()
        if addr in seen:
            continue
        seen.add(addr)
        checked.append('%08x %s' % (addr, name))
        if not inside(addr, IRAM):
            problems.append('%s is at %08x, not in IRAM' % (name, addr))
            continue
        out = subprocess.run([objdump, '-d', '--no-show-raw-insn', '--start-address=0x%x' % addr,
                              '--stop-address=0x%x' % (addr + size), elf], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            m = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)$', line)
            if not m:
                continue
            op, args = m.group(2), m.group(3)
            if op.startswith('callx'):
                problems.append('%s: indirect call (%s %s): target cannot be checked' % (name, op, args))
            elif op.startswith('call') or op in ('j', 'j.l'):
                t = re.search(r'\b([0-9a-f]{8})\b', args)
                if not t:
                    continue
                target = int(t.group(1), 16)
                if inside(target, ROM):
                    continue
                if inside(target, FLASH_CODE):
                    problems.append('%s calls %s in the FLASH (%08x)' % (name, args, target))
                elif inside(target, IRAM):
                    if target in by_addr:  # a call, or a tail jump into another function
                        todo.append(by_addr[target])
                elif op.startswith('call'):
                    problems.append('%s calls %08x: neither IRAM, ROM nor flash' % (name, target))
            elif op == 'l32r':
                t = re.search(r',\s*([0-9a-f]{8})\b', args)
                if t:
                    value = read(int(t.group(1), 16))
                    if value is not None and (inside(value, FLASH_DATA) or inside(value, FLASH_CODE)):
                        problems.append('%s loads %08x, an address in the FLASH (%s)' % (name, value, args))
    print('iram check: functions checked (step interrupt and everything it calls):')
    for c in sorted(checked):
        print('  ' + c)
    for p in problems:
        print('PROBLEM: ' + p)
    print('iram: %d functions, %d problems' % (len(checked), len(problems)))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
