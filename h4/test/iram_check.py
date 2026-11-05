#!/usr/bin/env python3
# Checks the BUILT firmware (the .elf): the GCODE step interrupt and everything it calls must be in IRAM (or the ROM),
# so that it can run while the flash is read or written (ESP_INTR_FLAG_IRAM).
# - Our own code on that path is checked instruction by instruction: no call into the flash, no indirect call, no
#   constant that points into the flash.
# - Any other function it calls belongs to ESP-IDF (portENTER_CRITICAL_ISR, esp_timer_get_time ...), which ESP-IDF
#   itself places in IRAM for use in interrupts: it must be in IRAM or the ROM and is not descended into (its own error
#   paths, e.g. an assertion message kept in the flash, only run when the system is already failing).
# - Self-test on the same firmware: the firmware 1.19 step timer (cncOnTimer, kept as the fallback) calls
#   gptimer_set_alarm_action() in the flash; the check must find that call, or it is not checking anything.
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

ROOT = 'cncTimerIsr'          # the IRAM step interrupt
OLD_ROOT = 'cncOnTimer'       # the firmware 1.19 step timer: must be found calling into the flash (self-test)
OURS = {'cncTimerIsr', 'cncStepTick', 'cnc_isr', 'cncAxis', 'cncOnTimer'}
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


def base_name(name):
    return re.sub(r'\(.*$', '', name).split('::')[-1]


def elf_reader(path):
    """read(addr) -> the 32-bit word at addr in the loadable sections of a 32-bit little-endian ELF, or None."""
    data = open(path, 'rb').read()
    shoff, = struct.unpack_from('<I', data, 0x20)
    shentsize, shnum = struct.unpack_from('<HH', data, 0x2E)
    sections = []
    for i in range(shnum):
        _, typ, _, addr, offset, size = struct.unpack_from('<IIIIII', data, shoff + i * shentsize)
        if typ == 1 and addr and size:  # PROGBITS with an address
            sections.append((addr, offset, size))

    def read(a):
        for addr, offset, size in sections:
            if addr <= a and a + 4 <= addr + size:
                return struct.unpack_from('<I', data, offset + a - addr)[0]
        return None
    return read


class Firmware:
    def __init__(self, elf, objdump, nm):
        self.elf, self.objdump = elf, objdump
        self.read = elf_reader(elf)
        self.syms, self.by_addr = {}, {}
        out = subprocess.run([nm, '-C', '-S', '--defined-only', elf], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            parts = line.split(None, 3)
            if len(parts) == 4 and parts[2] in 'tTwW':
                sym = (int(parts[0], 16), int(parts[1], 16), parts[3])
                self.syms.setdefault(base_name(parts[3]), sym)
                self.by_addr[sym[0]] = sym

    def analyze(self, root):
        """(own functions checked, ESP-IDF functions reached, problems) for the call tree from root."""
        own, library, problems = [], [], []
        if root not in self.syms:
            return own, library, ['function not found in the firmware: ' + root]
        seen, todo = set(), [self.syms[root]]
        while todo:
            addr, size, name = todo.pop()
            if addr in seen:
                continue
            seen.add(addr)
            if not inside(addr, IRAM):
                problems.append('%s is at %08x, not in IRAM' % (name, addr))
                continue
            if base_name(name) not in OURS:
                library.append('%08x %s' % (addr, name))
                continue
            own.append('%08x %s' % (addr, name))
            out = subprocess.run([self.objdump, '-d', '--no-show-raw-insn', '--start-address=0x%x' % addr,
                                  '--stop-address=0x%x' % (addr + size), self.elf],
                                 capture_output=True, text=True, check=True).stdout
            regs = {}  # register -> the constant last loaded into it with l32r (a far call is l32r + callx)
            for line in out.splitlines():
                m = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)$', line)
                if not m:
                    continue
                op, args = m.group(2), m.group(3)
                target = None
                if op.startswith('callx'):
                    reg = args.split(',')[0].strip()
                    if regs.get(reg) is None:
                        problems.append('%s: indirect call (%s %s): its target cannot be checked' % (name, op, args))
                        continue
                    target = regs[reg]
                elif op.startswith('call') or op in ('j', 'j.l'):
                    t = re.search(r'\b([0-9a-f]{8})\b', args)
                    if not t:
                        continue
                    target = int(t.group(1), 16)
                if target is not None:
                    if inside(target, ROM):
                        continue
                    if inside(target, FLASH_CODE):
                        problems.append('%s calls %s in the FLASH (%08x)' % (name, args, target))
                    elif inside(target, IRAM):
                        if target in self.by_addr:  # a call, or a tail jump into another function
                            todo.append(self.by_addr[target])
                    elif op.startswith('call'):
                        problems.append('%s calls %08x: neither IRAM, ROM nor flash' % (name, target))
                elif op == 'l32r':
                    t = re.search(r',\s*([0-9a-f]{8})\b', args)
                    if t:
                        value = self.read(int(t.group(1), 16))
                        regs[args.split(',')[0].strip()] = value
                        if value is not None and inside(value, FLASH_DATA):
                            problems.append('%s loads %08x, an address of data in the FLASH (%s)' % (name, value, args))
                else:
                    # any other instruction may change its first register: forget what was loaded there
                    regs.pop(args.split(',')[0].strip(), None)
        return own, library, problems


def main():
    elf = sys.argv[1]
    fw = Firmware(elf, tool('objdump', sys.argv[2] if len(sys.argv) > 2 else None),
                  tool('nm', sys.argv[3] if len(sys.argv) > 3 else None))
    own, library, problems = fw.analyze(ROOT)
    print('iram check: our functions on the step interrupt path, checked instruction by instruction:')
    for c in sorted(own):
        print('  ' + c)
    print('ESP-IDF functions they call (in IRAM, not descended into):')
    for c in sorted(library):
        print('  ' + c)
    _, _, old = fw.analyze(OLD_ROOT)
    found = any(' calls ' in p and 'in the FLASH' in p for p in old)
    print('self-test: the flash call of the firmware 1.19 step timer (%s) is %s' % (OLD_ROOT, 'found' if found else 'NOT found'))
    if not found:
        problems.append('self-test failed: the check did not find the flash call of ' + OLD_ROOT)
    for p in problems:
        print('PROBLEM: ' + p)
    print('iram: %d own functions, %d ESP-IDF functions in IRAM, %d problems' % (len(own), len(library), len(problems)))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
