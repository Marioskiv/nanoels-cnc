# NanoEls H4 — GCODE mode as a CNC (fork)

This fork changes **only the GCODE mode** of the H4. Every other mode (gearbox, turning, facing, cone,
threads, ellipse, async, A1, cut-off) is the original code and works as before.

> **Status: compiled, and tested on a PC only.** The motion code has been run in a simulation on real
> polar programs and on random programs (see *Tests*), but not yet on a real H4. Test it in the air first,
> with a hand on the emergency stop.

## What the GCODE mode does now

- **Continuous motion with look-ahead** (the planner and step generator of
  [Grbl](https://github.com/gnea/grbl), ported to the ESP32-S3): the machine no longer stops at the end of
  every line; corners are taken at a safe speed (`CNC_JUNCTION_DEVIATION`).
- **X, Z and C move together**, step-synchronised (Bresenham with Grbl's multi-axis step smoothing),
  from one 20 MHz hardware timer.
- **G93 inverse time**: each move takes exactly 1/F minutes, as polar programs (X radius, C angle) need.
  G0 runs at the axes' rapid speed.
- Accelerations and top speeds per axis are the `ACCELERATION_*` and `SPEED_MANUAL_MOVE_*` values of the
  sketch, so no new settings are needed. Backlash (`BACKLASH_DU_*`) is taken up before a reversal, with
  the motor stopping before the carriage is driven.
- The H4 display shows the positions while the program runs; the H4 position bookkeeping is kept exactly,
  so every other mode continues from the true position.

### G-code read

| Code | Meaning |
|---|---|
| G0, G1 | rapid / feed move on X, Z and C. A program starts in G1, so `X.. Z..` lines without a G word move at the feed rate (lathecode programs and the old H4 behaviour) |
| G4 P | dwell, seconds |
| G20, G21 | inch / mm (C stays in degrees) |
| G90, G91 | absolute / incremental |
| G93, G94 | inverse time / units per minute. After G93 a G94 move must give its own F |
| G8, G17, G18, G19, G40, G49, G54, G61, G64 (P), G80 | accepted, nothing to do |
| G43 H | accepted with a note: set tool lengths by touching off Z |
| G32 X.. Z.. F*lead* / G33 X.. Z.. K*lead* | threading move synchronised to the spindle encoder, as `G32` on the NanoEls H5. The lead is per spindle turn (current units); G33 with K is the LinuxCNC form. Each pass waits for the same spindle angle, so repeated passes cut the same groove. Not modal; F of a G32 line does not change the feed rate. Like the threading mode of the H4, the axis needs a short run-in to reach the spindle speed: start each pass a few mm before the thread |
| M0, M1, M6 | pause; **ON** on the H4 (or `~` over USB) continues |
| M2, M30 | end of program, after the motion has finished |
| M3, M4, M5, M7, M8, M9, S, T, N | accepted (spindle and coolant are switched by hand) |

Refused with a reason (the program stops): arcs G2/G3, G7, G28/G30/G53, G10/G92, canned cycles, cutter
compensation, any other axis letter, numbers that cannot be read.

### USB protocol

- `~` starts (as before), `!` stops: the machine **decelerates along the path** and the rest of the program
  is dropped; the position stays exact.
- G94 programs streamed over USB get `ok` only after their move, as in the old firmware, so senders that
  send `!` right after the last `ok` (the H4 sender, lathecode) keep working.
- G93 programs get `ok` as soon as the line is in the planner (look-ahead). They must end with `M2` or
  `M30` (the polar converter writes `M30`); the `ok` for that line comes after the last move.
- `?` reports `<Run|WPos:X,0.000,Z,C|FS:feed,rpm|Un:0|Id:H4V12CNC>`. `Un` counts how often the step buffer
  ran empty while moving: it must stay 0.
- Programs saved with the H4 keys (the original stored programs, about 4000 characters each) also run with
  look-ahead. Long programs go over WiFi (below).

## WiFi: files, status and controls on a phone or tablet

`WIFI_ENABLED = true` in `h4.ino` (as on the H5). The H4 joins your network; without a saved network, or if
it cannot connect, it opens its own access point **`NanoEls-H4-Setup`**, password **`nanoels-h4`**, page
`http://192.168.4.1`. The address it got on your network is printed on USB at start-up and shown on the page.

The page shows X, Z and C, the program name, a progress bar and the current line, Start / Pause / Resume and
a large STOP, a drawing of the program (polar programs as seen from the cutter, the done part in blue, the
tool as a red dot), the files, the 25 H4 keys, the WiFi settings and a firmware update. The page is a
convenience, **not** an emergency stop: keep a real one within reach.

- **Files** are uploaded to the H4's own flash (about 9.9 MB, folder `/gcode`), streamed, so the size of the
  upload is not limited by RAM.
- **Running**: Start reads the whole program into memory first (PSRAM, up to about 2 MB; 120 kB on a board
  without PSRAM) and then runs it from memory with look-ahead. Nothing is read from or written to the flash
  while the machine moves (the step timer must not stop during a flash write), so uploads, deletions and
  settings are refused while a program runs.
- **H4 display and keys**: the page shows the 4 lines of the H4 display and all its keys. The arrows (and
  GEARS / TURN, which move A1 in its mode) move while held; the page repeats the press every 150 ms and the
  controller releases the key by itself if the repeats stop for half a second (page closed, phone asleep,
  WiFi lost).
- **Run line** (MDI): one G-code line typed on the page runs as a one-line program in the GCODE mode, for
  example `G0 C90` or `G91 G0 X-1`. Every line starts with the program modes (G90, G1, mm, default feed).
- **Messages**: errors and notes of the GCODE mode (which line, why).
- **Machine settings**: encoder, Z / X / A1 axes (steps, travel per turn, speeds, acceleration, inverted,
  driver release, backlash), handwheels, manual stepping and the GCODE-mode settings, shown in mm, mm/s and
  mm/s². They are kept in the controller (Preferences) and used after the automatic restart; the values
  at the top of `h4.ino` are the defaults (button *Defaults from the firmware*). Pins and axis letters stay
  in `h4.ino`. If the steps or the travel per turn of an axis, or the encoder, change, the saved positions
  and stops would be wrong, so they are cleared with the other saved H4 values.
- **Firmware updates** go over WiFi after the first flash (`.bin` from *Sketch → Export Compiled Binary*).
  USB is needed only once, for the first flash with the partition table below.

### Arduino IDE settings (needed once, for the first flash over USB)

Board *ESP32S3 Dev Module*, **Flash Size: 16MB**, **PSRAM: QSPI PSRAM**, **Partition Scheme: 16M Flash
(3MB APP/9.9MB FATFS)**. The `partitions.csv` next to the sketch then gives two 3 MB firmware slots (for the
WiFi update) and 9.9 MB for programs; the settings area (`nvs`) stays where it was, so saved H4 settings
are kept.

## Settings for a polar machine (X radial, Z axial, chuck on C)

On the web page (*Machine settings*) or in `h4.ino`: steps, travel per turn, speeds, accelerations,
direction and backlash of X, Z and A1 for the machine; A1 *Connected* and *Rotary* (the C motor on terminal
A1: enable 9, direction 10, step 11, letter `NAME_A1 = 'C'` in `h4.ino`); *Pause the program when the
spindle stops* off (the chuck is turned by the C motor, the spindle encoder must not pause the program).

## Tests (on a PC)

`sh h4/test/run_tests.sh [programs.ngc]` compiles the motion core with g++ and runs:

- `cnc_parse_test`: what is accepted and refused, positions, modes, backlash blocks;
- `cnc_sim`: every program is executed on simulated time exactly as the firmware does (line reader,
  planner, step segments, step interrupt). Every step is checked: the tool passes through every
  programmed point and never leaves the straight line between them by more than one step; no axis is
  faster or accelerates harder than its setting; the step buffer never runs empty (with the feeding task
  pausing up to 50 ms); backlash take-up moves the motor and not the tool; a feed hold stops on the path
  and the resume reaches the same end point. 64 programs: lathecode style, polar samples and random
  programs with G0/G93/G94, holds and backlash, for two machine settings.

## License

The original NanoEls code is MIT (`LICENSE`, © Maxim Kachurovskiy). `cnc_core.cpp` is ported from Grbl
(GPL v3), so this fork as a whole is distributed under the **GNU GPL v3** (`COPYING`).
