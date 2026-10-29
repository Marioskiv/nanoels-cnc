# CLAUDE.md

Instructions for AI coding assistants working on this repository.

- The owner's working notes, rules and open items are kept privately (repository `Marioskiv/cnc-manuals`,
  folder `nanoels-h4/`: `CLAUDE.md`, `STATUS.md`, `DECISIONS.md`, `LOG.md`). Read them first when you have access;
  otherwise ask the owner. Keep notes there, not here.
- Ask the owner before changing code, tests, settings or versions.
- Work on a branch and open a pull request; merge only with green CI (`sh h4/test/run_tests.sh` with `RS274` set,
  the ESP32 build, the Android build).
- Never commit signing keys, passwords or a `my_machine.h` with real machine values.
- New source files carry `Copyright (c) 2026 Marioskiv`, the repository URL and `SPDX-License-Identifier:
  GPL-3.0-or-later`.
- Nothing is called working on a real H4 until it has been tested on one.
