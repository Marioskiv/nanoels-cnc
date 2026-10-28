# NanoEls H4 — GCODE mode as a CNC (fork)

This fork changes **only the GCODE mode** of the H4. Every other mode (gearbox, turning, facing, cone,
threads, ellipse, async, A1, cut-off) is the original code and works as before.

> **Status: compiled, and tested on a PC only.** The motion code has been run in a simulation on real
> polar programs and on random programs (see *Tests*), but not yet on a real H4. Test it in the air first,
> with a hand on the emergency stop.

## What the GCODE mode does now

- **Continuous motion with look-ahead** (the planner and step generator of
  [Grbl](https://github.com/gnea/grbl), ported to the ESP32-S3): the machine no longer stops at the end of
  every line; corners are taken at a limited speed (`CNC_JUNCTION_DEVIATION`, see *Corners* below). The tool
  always goes through every programmed point (G61, G64 and G64 P all mean that here; nothing is rounded off),
  and G61.1 stops completely at the end of every move.
- **X, Z and C move together**, step-synchronised (Bresenham with Grbl's multi-axis step smoothing),
  from one 20 MHz hardware timer.
- **G93 inverse time**: F gives the time of each move, 1/F minutes, as polar programs (X radius, C angle)
  need. That is the time the planner aims for, not a guarantee: a move takes longer when an axis would have to
  go faster than its maximum speed (then the whole move is slowed, all axes together, so the path does not
  change), while the machine accelerates or decelerates (start, stop, a corner that needs a lower speed), and
  with the feed override below 100 %. At 100 % it is never faster than 1/F. In the simulation of the author's
  polar programs the total time is 1.7 % to 7 % longer than programmed (F test: 820 s instead of 767 s).
  LinuxCNC does the same. G0 runs at the axes' rapid speed.
- Accelerations and top speeds per axis are the `ACCELERATION_*` and `SPEED_MANUAL_MOVE_*` values of the
  sketch, so no new settings are needed. Backlash (`BACKLASH_DU_*`) is taken up before a reversal, with
  the motor stopping before the carriage is driven.
- The H4 display shows the positions while the program runs; the H4 position bookkeeping is kept exactly,
  so every other mode continues from the true position.

### G-code read

The language is **RS274/NGC as LinuxCNC reads it** (Grbl reads a subset of the same language), for a lathe
with X, Z and C. Every line is checked as a whole before any of it runs; what cannot run exactly is refused
with the line and the reason. It is checked against LinuxCNC's own interpreter: 10 000 random programs (over 600 000
moves, 9 000 arcs, drilling cycles and G76 threading cycles, with G7, G20, G90.1, G10, G92, G28, G53, G43.1
mixed in) and the author's real programs give the same positions and refuse the same lines as LinuxCNC 2.9
(`RS274=/path/to/rs274 sh h4/test/run_tests.sh`).

| Group | Codes |
|---|---|
| motion | G0, G1, G2, G3 (arcs in the X-Z plane, I/K or R, P turns, C moves along), G33 K / G32 F (threading with the encoder), G80 |
| cycles | G81, G82, G83, G73, G85, G89 with R, Q, P, L, G98 / G99 (G17: drill along Z, e.g. centre drilling at X0; G19: along X); G76 threading cycle (P I J K, Q compound angle, H spring passes, R degression; every pass synchronised to the spindle) |
| plane | G18 (start); G17 / G19 accepted, but arcs there need a Y axis and are refused |
| distance | G90, G91; arc centres G91.1 (start), G90.1 |
| feed | G94 (start), G93 inverse time (F on every move), G95 per spindle turn (speed from the encoder) |
| units, diameter | G21 (start), G20; G8 (start), G7: X words are diameters |
| coordinates | G54 (= the H4's own zero, X0 / Z0 keys), G55, G56, G57, G58, G59, G59.1, G59.2, G59.3 |
| offsets | G10 L2 P, G10 L20 P, G92, G92.1, G92.2, G92.3, G28 / G28.1, G30 / G30.1, G53 (machine coordinates) |
| tool | G43.1 Z, G49; G43 H accepted with a note (no tool table yet: touch off Z); T, M6 pauses for the change, M61 |
| path | G61 and G64 [P Q] (start: G64): the path goes through every programmed point, corners are not rounded; G61.1: exact stop, every move ends at rest (an arc is one move) |
| overrides | M48 (start) and M50 / M50 P1: feed and rapid overrides apply; M49 and M50 P0: they do not, the moves run at the programmed feed and full rapid speed (as LinuxCNC, whose feed override switch covers rapids too); M2 / M30 turn them on again |
| other | G4 P (seconds), G40, G97, G96 (noted: the spindle speed is set by hand), M0, M1, M2, M30, M60, M3, M4, M5, M7, M8, M9, S, N, `/` |
| U, W | incremental X and Z, as Fanuc lathes and the CAM lathe posts use them (`G28 U0 W0`); U is a diameter in G7 |

As in LinuxCNC: two codes of one group on a line are an error; G94 and G95 clear the feed, so the next feed
move needs its F (a feed move before any F is refused, there is no hidden default feed); a program starts in
G1, so lathecode-style lines `X.. Z..` without a G word work as before. The G55-G59.3, G28 and G30 positions
are kept in the controller (machine coordinates: they mean something after Home All).

Refused with the reason: G41 / G42 cutter compensation (post with compensation *in computer*), the turning
cycles G70-G72 (post without cycles), G74 / G84 / G86-G88 (they stop or reverse the spindle), G76 with a taper
(E), G38 probing, M51 (spindle speed override: the spindle speed is set by hand), M52 (adaptive feed), M53 (feed
stop switch), Y A B V axes, parameters (`#`), expressions and O-word subroutines.

### Corners

At a corner the planner (Grbl's method) does not slow down to zero: it goes through at a *junction speed*
from the junction deviation and the accelerations, and the direction changes at once, so each axis changes
its speed at once by a small amount. The calculation uses all axes together, X and Z in mm and C in degrees.
Measured on the author's polar programs, on edge cases (lines through the centre, radius 0.02 to 102 mm,
C reversals, sharp star points, zig-zag facing) and on random programs (`test/cnc_junctions.cpp`): **every
axis stays within sqrt(8 x its acceleration x junction deviation), in its own units**, because the junction
speed is limited by each axis's own acceleration. With 0.01 and the author's settings that is at most
2.8 mm/s on X (2.1 motor turns/s), 4 mm/s on Z and 7.6 deg/s on C (0.22 motor turns/s): C, through its 10:1
belts, gets the smallest jump of the three. The one effect of the units: for C the junction deviation counts
in degrees, so the corner tolerance of C at a radius r is r x 0.01 x pi / 180 mm: 0.01 mm at r = 57 mm,
0.019 mm at r = 110 mm, less nearer the centre; X and Z keep 0.01 mm. The run-time test checks the bound on
every program (`cnc_junctions --check`). G61.1 avoids the jumps completely (every move from rest to rest).

**Posts.** Milling for polar / wrap work: any 3-axis post (Grbl, LinuxCNC, Fanuc) through the polar converter.
Turning straight on the H4: a LinuxCNC lathe post (G7, G18, U/W retracts are read).

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

## Grbl senders (UGS, gSender, CNCjs, bCNC ...)

The H4 also speaks the **Grbl 1.1 protocol**: on WiFi, TCP port **23** (in the sender choose a network /
telnet / WiFi connection to the H4's address, port 23), and on USB when *USB speaks the Grbl protocol* is on
(Machine settings; then the old H4 USB protocol is off). A sender sees the start message, `ok` / `error:N` for
every line (look-ahead: `ok` as soon as a line is planned), the real-time commands `?` `!` `~` Ctrl-X and jog
cancel, the status report `<Idle|WPos:X,0,Z,C|Bf:..|FS:..>` (Y is always 0), `$$` `$#` `$G` `$I` `$N`, `$X`
(run without Home All), `$H` (Home All), `$J=` jogs and `ALARM:1` for a limit or a driver alarm. The G-code goes
to the same interpreter as files. Machine settings are changed on the web page, not with `$x=` (a sender gets
`error:3` and a message). **Feed and rapid overrides** work as in Grbl 1.1 (real-time bytes `0x90`-`0x94` feed
100 % / +10 / -10 / +1 / -1, range 10-200 %; `0x95`-`0x97` rapids 100 / 50 / 25 %; `Ov:` in the status report):
what is already planned is re-planned at once, as Grbl does. A program with M49 or M50 P0 switches them off
for its following moves (the page shows *off*). Spindle overrides and check mode `$C` are not supported (the H4
does not drive the spindle).

**PS4 / PS5 gamepad.** A plain ESP32 board with the sketch `h4_ps4_pendant` (folder next to `h4`) turns the
gamepad into a jog pendant through this port (L1 deadman, sticks X/Z, triggers C, Home All, touch off Z,
STOP): see [`../h4_ps4_pendant/README.md`](../h4_ps4_pendant/README.md).

**Android app.** The simplest way on a phone: the *H4 CNC* app (folder `android/`, APK on the repository's
Releases page) shows this page full screen and reads the gamepad itself, with no browser setting. See
[`../android/README.md`](../android/README.md).

**Gamepad on the phone (no extra board).** The H4 web page also reads a PS4 / PS5 gamepad connected to the
phone or computer (Android: Settings > Bluetooth, pair it with Share + PS). Same layout; the page sends the wanted
speeds to the H4 every 100 ms and the H4 turns them into the same short jogs (0.05 s pieces, at most 0.2 s queued,
stop 0.3 s after the last request or at once on release). The page must stay open in front. Chrome only gives a
page a gamepad on a secure origin, and the H4 page is plain `http://`: on Android open `chrome://flags`, search
*Insecure origins treated as secure*, add the H4 address (e.g. `http://192.168.1.50`), Enabled, Relaunch. Speeds
and reversed axes are set in the *Gamepad* box of the page (kept in that browser). The MDI line also accepts `$X`
(allow moves before Home All).

## Your machine's values (my_machine.h)

Copy `my_machine.h.example` to `my_machine.h` in the `h4` folder and set there what your machine needs (steps,
screws, speeds, directions, home switches, limits ...; any setting at the top of `h4.ino`). They are applied at
start-up, before the values saved on the web page, and *Defaults from the firmware* on the page goes back to
them. `my_machine.h` is not in the repository (`.gitignore`), so a newer firmware copied over the folder keeps it.
From a LinuxCNC `.ini`: steps per mm = |STEP_SCALE|, speed = MAX_VELOCITY x steps per mm, acceleration =
MAX_ACCELERATION x steps per mm; HOME_* and *_LIMIT are the same numbers. Check every direction with a short move
first: the sign of STEP_SCALE does not translate reliably (the DIR signal differs between cards and boards).

## WiFi: files, status and controls on a phone or tablet

`WIFI_ENABLED = true` in `h4.ino` (as on the H5). The H4 joins your network; without a saved network, or if
it cannot connect, it opens its own access point **`NanoEls-H4-Setup`**, password **`nanoels-h4`**, page
`http://192.168.4.1`. The address it got on your network is shown **on the H4's own display**, bottom line
(for 30 s after start-up in every mode, and always in the GCODE mode while no program runs; the setup access
point shows its name, password and address in turn), printed on USB and shown on the page. Type it into the
browser of a phone, tablet (Android or iPhone) or computer on the same network, e.g. `http://192.168.1.50`.

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

## Home switches, limits and driver alarm (wiring)

Terminal **A2** becomes three inputs (it cannot be used for handwheel 2 at the same time). The settings are
on the web page (*Home switch and limits X / Z*, *Home All and driver alarm*) or at the top of `h4.ino`; they
have the meaning of the LinuxCNC `.ini` keys (`HOME_SEARCH_VEL`, `HOME_LATCH_VEL`, `HOME_FINAL_VEL`,
`HOME_OFFSET`, `HOME`, `MIN_LIMIT`, `MAX_LIMIT`). Everything is off by default.

| H4 terminal | Connect | |
|---|---|---|
| A2 `A21` | home switch X, wire 1 | switch normally open, closes to GND when pressed (or normally closed: setting) |
| A2 `A22` | home switch Z, wire 1 | same |
| A2 `A23` | driver alarm outputs (all drivers together, below) | |
| Encoder `GND` | one wire from a GND strip | the second wire of both switches and the `ALM-` / `A-` of every driver go to the strip |
| A2 `5V` | nothing (only for the measurement below) | |

The inputs are 5 V lines with pull-ups: **never connect more than 5.5 V** to them; only switches or
open-collector outputs that connect the input to GND.

**Driver alarms on one input.** A closed-loop driver's alarm output (`ALM+` / `ALM-`, an opto-isolated
open-collector transistor, JD57: at most 30 V, 50 mA) is a switch. Which way it switches is not written in the
JD57 manual; most drivers of this kind keep it **open** in normal running and **close** it on an alarm.
- open when running, closes on alarm: all drivers **in parallel**: every `ALM+` to `A23`, every `ALM-` to GND.
  Setting *Alarm connects A23 to GND* = on.
- closed when running, opens on alarm: all drivers **in series**: `A23` -> `ALM+` 1, `ALM-` 1 -> `ALM+` 2,
  `ALM-` 2 -> `ALM+` 3, `ALM-` 3 -> GND. Setting *Alarm connects A23 to GND* = off. The input must read below
  0.5 V with all three closed.
- Check before wiring them together: connect one driver at a time (`ALM+` to `A23`, `ALM-` to GND), power
  it, and read A23 on the web page (*Inputs*): `high` = open while running, `LOW` = closed while running.
  Drivers of one model behave the same.

An alarm stops all motion at once and the machine counts as not homed (a driver that stopped has lost its
position). A JD57 alarm is cleared only by switching the driver off and on. A home switch that is hit outside
homing (*The switch also stops everything*) stops all motion at once as well; the axis can then be jogged away
from the switch only.

**Home All** (button on the page, or `$H` in *Run line*): Z first, then X (setting), each one searching its
switch, leaving it slowly (latch), setting its machine coordinate to `HOME_OFFSET` there and moving to `HOME`;
A1 (C) takes machine 0 where it stands. Soft limits (`MIN_LIMIT` / `MAX_LIMIT`, machine coordinates) then
hold for programs and for jogging. With *Programs start only after Home All* a program does not start before.
OFF or STOP stops homing. After an emergency stop by cutting the drivers' power, home again.

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
  and the resume reaches the same end point. About 120 runs: lathecode style, polar samples and random
  programs with G0/G93/G94, holds, overrides and backlash, for two machine settings. `--stops all` proves
  that G61.1 ends every move at rest (the step generator reports speed 0 at the end of the move and at the
  start of the next one) and `--stops none` that the same path in G64 goes through without stopping;
- `cnc_junctions --check`: the corner bound above, on polar and random programs;
- `cnc_parse_test` also checks M48 / M49 / M50 (state and the planned speed with overrides at 50 % / 25 %),
  M51-M53 refused and the `%` start / end line;
- with `RS274=/path/to/rs274`: `lcnc_compare.py` (positions and refused lines against LinuxCNC's own
  interpreter, 1000 random programs and the samples) and `modes_compare.py` (M48 / M49 / M50 and G61 / G61.1 /
  G64 in force for the same moves as in LinuxCNC, 500 random programs). GitHub Actions run all of it on every
  change.

## License

The original NanoEls code is MIT (`LICENSE`, © Maxim Kachurovskiy). `cnc_core.cpp` is ported from Grbl
(GPL v3), so this fork as a whole is distributed under the **GNU GPL v3** (`COPYING`).
