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
- Stored programs (saved on the H4) run with look-ahead. The H4 stores at most about 4000 characters per
  program, so long polar programs are streamed over USB.

## Settings for a polar machine (X radial, Z axial, chuck on C)

In `h4.ino`: the `MOTOR_STEPS_*`, `SCREW_*_DU`, `SPEED_MANUAL_MOVE_*`, `ACCELERATION_*`, `INVERT_*` and
`BACKLASH_DU_*` of X, Z and A1 for the machine; `ACTIVE_A1 = true`, `NAME_A1 = 'C'`, `ROTARY_A1 = true`
(the C motor on terminal AUX1: enable 9, direction 10, step 11); `SPINDLE_PAUSES_GCODE = false` (the
chuck is turned by the C motor, the spindle encoder must not pause the program).

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
