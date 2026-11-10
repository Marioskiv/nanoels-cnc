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
  LinuxCNC does the same. G0 runs at the axes' rapid speed. As in LinuxCNC, a move shorter than one motor step
  still takes its 1/F: its time is added to the next G93 move that makes a step (a G0 or G94 move in between
  drops it), and slow moves are not sped up (lowest planned speed 0.001 units/min, Grbl used 1). Over many
  moves the time does not drift (`test/g93_timing.py`, the case of FluidNC issue 1372).
- **G94 / G95 with C** (firmware 1.35), as LinuxCNC: when X or Z moves, F is the speed along X and Z and C turns
  in the same time (`G1 X10 C90 F100` takes 6 s); C alone moves at F degrees per minute (`G1 C90 F100`: 54 s); an
  axis that would have to go faster than its maximum slows the whole move. Before 1.35 F was taken along X, Z and C
  together, degrees counted as mm (Grbl's way), so that move took 54 s and the gamepad moved X/Z about 13 times
  slower while C turned. G93 programs (the polar converter) did not change. Test: `cnc_parse_test.cpp`, `jog_test.cpp`.
- **Corners** (firmware 1.36): the planner keeps Grbl's junction deviation, and in addition no axis may change its
  speed at a corner by more than its acceleration x 10 ms (what the step generator itself changes in one segment while
  accelerating). Grbl's rule works on the direction of all axes together, where C degrees count as mm, so a fast C
  hid a reversal of X or Z: X jumped by ~2 mm/s, 2.3 x its acceleration measured over 20 ms in the author's polar
  programs (5 x in a FreeCAD polar program). Now at most 1.5 x over 20 ms; the programs take 0-5.5 % longer (the
  author's: 0.7-4 %). LinuxCNC 2.9 measured 0.9-1.4 x at such corners (`lcnc_traj.py`). Test: `junction_axis_test.cpp`.
- Accelerations and top speeds per axis are the `ACCELERATION_*` and `SPEED_MANUAL_MOVE_*` values of the
  sketch, so no new settings are needed. Backlash (`BACKLASH_DU_*`) is taken up before a reversal, with
  the motor stopping before the carriage is driven. A STOP in the middle of a take-up is remembered: the next
  move takes up only what is still missing, so the tool position stays exact.
- **Step look-ahead by time**: the step generator keeps at least 0.1 s of motion prepared
  (`CNC_SEGMENT_AHEAD_S`, up to 128 segments), so a run of very short G93 blocks does not empty it while the
  feeding task is busy.
- **Step and direction timing** as LinuxCNC's `STEPLEN`, `STEPSPACE` (= the pulse length), `DIRSETUP` and
  `DIRHOLD`: settings *Step pulse length*, *Direction set-up time* and *Direction hold time*. A direction pin
  changes only after the hold time since the last pulse, and at least the set-up time before the next step; after
  an interrupt that came late the pin still rests a pulse length before the next pulse.
- **Step interrupt in IRAM** (firmware 1.20, setting *Step interrupt keeps running while the flash is busy*, on by
  default): the GCODE step interrupt is allocated with `ESP_INTR_FLAG_IRAM` and sets the timer's alarm with its
  registers (`timer_ll_*`), as the step timers of FluidNC and grblHAL do; the ESP-IDF gptimer driver only reserves and
  clocks the timer. So the steps go on while the flash is read or written (before, with the gptimer callback, they
  waited; the firmware avoids flash access while moving either way). `h4/test/iram_check.py` checks every build in CI:
  the interrupt and every function it calls are in IRAM or the ROM and it loads no address in the flash.
  *Diagnostics* shows which step timer runs; if the IRAM timer cannot be set up the H4 says so in the log and uses
  the firmware 1.19 timer. Switching the setting off also goes back to it (after a restart).
- **Jog, handwheels, Home All, G32 / G33** (firmware 1.19): these steps come from the H4's own `moveAxis()`, not
  from the step interrupt. In the GCODE mode their pulse also lasts at least the *Step pulse length* and their
  direction pin also keeps its level the *Direction hold time* after the last pulse (before 1.19 the pulse was only
  as long as the code between the two pin writes). The lathe modes keep the original pulse. *Diagnostics* shows the
  shortest and longest of these pulses as the H4 measures them itself (CPU cycle counter: from just before the pin
  goes low to just after it goes high, a little longer than the pin is low); test `move_pulse_test.cpp`.
- **Step rate limit**: the highest step rate per axis is 1 / (pulse + the longer of pulse and hold + set-up); a
  speed setting above it is lowered with a message in the log, so a direction change never comes too close to the
  next step.
- The H4 display shows the positions while the program runs; the H4 position bookkeeping is kept exactly,
  so every other mode continues from the true position.

### The line shown while a program runs (firmware 1.30)

The H4's screen and the page show the line whose **motion runs now** (the planner reads up to 64 moves ahead; before
1.30 the line just read was shown). The page shows it with its number in the file, counted as *Test* and the check
before the start count (comments and empty lines included), e.g. `1234: G1 X20.5 C90 F600`; an error message gives
the same number. STOP, OFF, a limit or an alarm during a program writes `Stopped at line N` to *Messages*: the line
from which a run-from-line would start. Test: `exec_line_test.cpp`.

### Start from a line (firmware 1.31)

*Program*, *Run*, **From line…** (the line where the last program stopped is proposed). Not a jump: the whole program
is checked first, then lines 1 to N-1 go through the interpreter and the planner without moving (as *Test*), so line
N starts with the program's own modes, work offset, G10 / G92 values, F, S, tool and position, exactly as in a full
run. Then the machine moves in machine coordinates: **up** to the highest Z the program reached before line N (or
stays where it is if higher), **across** to X and C of the start of line N (rapid, above the work), **pauses**
("switch the router on, then Resume"), goes **down** to the start at *Start from a line: feed down* (100 mm/min) and
the program goes on at line N. Refused, with nothing moved: line N past the end or after M2 / M30, an error or
threading (G33 / G76) before it. It assumes Z+ is away from the work (as on a mill and the polar machine). Test:
`runfrom_test.cpp` (from every line of the sample programs and of a program with G55, G10, G91, G92, G20, G93 and
arcs, the moves after line N equal those of the full run).

After a power cut during a program (app 1.59): the H4 has no memory that survives a power cut while it moves (no
FRAM / EEPROM on the board; a flash sector erase can take up to ~0.4 s and would stall the motion), so the **app keeps
the line** on the tablet: the file, its size, the line that runs (asked every 0.35 s) and the H4's boot number, cleared
when the program ends or stops while the H4 stays on. When the H4 comes back saying it restarted during the program,
the message also says where it was cut, and **From line…** offers that line (and warns if the file's size changed).
The tablet's line is never later than the real one, so the restart repeats a little rather than skipping material.
It needs the app to have been watching (a program started from the H4's keys is not kept). First Home All (X, Z) and
line C up with the mark on the chuck (C has no home). Since firmware 1.41 a start that would be refused anyway (not
homed, restarted while moving, an alarm, a jog still braking) says "Not started: ..." at once; before, the check said
"Started." and the start was then refused with only a beep. Test: `test/h4sim/app_test.js` (the H4 killed in the
middle of a program and started again with its flash).

### Check before a program starts (firmware 1.22)

START on the page (or ON on the H4 with a stored program) first sends the whole program through the interpreter and
the planner, as *Test* does; nothing moves. If a line cannot run (a code the H4 refuses, a move or an arc outside the
soft limits, a bad arc, a Y word ...) the program does not start and the page says which line and why; so a program
never stops half way in the material for such a reason. Notes (M3, M6, G43 H ...) do not refuse. G32 / G33 / G76
threading cannot be planned without the spindle: the check ends at the first threading line and says so. A long
program takes a moment to check; STOP during the check cancels the start. A Grbl sender's stream cannot be checked
before (it arrives line by line). Test: `preflight_test.cpp`.

Since 1.40 the check also says, when the program starts, what the operator will meet: how many times it will wait for
them and where the first is (a tool change M6, the router to switch on at M3 / M4, M0 / M1), and its first note, a
G43 H before any other (the H4 stores no tool lengths: touch off Z for every tool). The program still says each of
them when it gets there; the check no longer uses up the one-time spindle / coolant notes (it puts them back with the
modes). Tests: `preflight_test.cpp`, `cnc_parse_test.cpp`, `test/h4sim/scenario_test.py`.

Since firmware 1.25 a program (stored, from the page, or a Grbl sender's) also does not start while an axis moved by
the keys or a handwheel is still moving or braking ("X is still moving: start again when it has stopped"): the
program would be planned from a position the axis then leaves, and the end of that jog switches the driver off.
Test: `homing_test.cpp`.

### Numbers from the app are checked whole (firmware 1.42)

The Lathe screen's numbers (pitch, cone, stops, the safe approach's position, the measured diameter) must be a whole
finite number in range, or the request is refused (400) and nothing changes - all values are checked before any is
applied. Before, the text "abc" was read as 0 (a stop at 0, a move to 0) and "nan", "inf" or "1e40" became the
smallest long, whose absolute value is negative, so an absurd pitch passed the range check. Found by sending the
simulated H4 thousands of odd requests with the address and undefined-behaviour sanitizers on (`test/h4sim`); test:
`lathe_test.py`. The app itself always sent proper numbers.

### Home All searches in pieces (firmware 1.38)

The H4 stops everything when an axis is asked to go further than its longest move at once (*MAX_TRAVEL_MM*:
emergency stop "Requested position outside machine"). The search for a home switch is longer than that (the soft
limits' span + 10 mm, or 1.2 x the longest move + 20 mm without soft limits) and was asked as one move: Home All ended
in that emergency stop at once, on every machine (on the author's: Z 607 mm searched, 600 allowed; X 141 / 135). The
search is now given in pieces of half the longest move, the target moving on ahead of the axis, so it does not stop
between them; every other move keeps the protection. Found with the H4 on a PC; `homing_test.cpp` now has the main
loop's check too (41 of its 91 checks fail on 1.37).

### The web page after an emergency stop (firmware 1.38)

After an emergency stop (driver alarm / E-stop contact, a position outside the machine ...) the H4's web server
stopped answering: the app showed "NO CONNECTION" instead of the emergency stop and why, and the lasting log could not
be read. It keeps answering now; every request that would move refuses itself ("Emergency stop: restart the
controller"), Home All and the home check included.

### Homing against noise (firmware 1.34)

After the search the axis stops past the point where its switch closed, so a real switch is still pressed. If it is
not, the search was stopped by electrical noise longer than the 0.5 ms debounce, not by the switch: Home All is
refused ("the switch was not pressed any more after the search"), as LinuxCNC does. Before 1.34 machine zero was set
where the noise happened (the test found X 25 mm off). Also tested (`homing_test.cpp`, work plan B2): 300 Home All
from random places with 0.5 .. 3 ms contact bounce and noise shorter than the debounce: all homed, the switch found
within 0.0017 mm (X) and 0.0075 mm (Z); a normally open switch stuck closed is refused.

### Encoder filter (firmware 1.33)

The spindle / chuck encoder input ignores pulses shorter than 5 us (ESP32 pulse counter filter, 400 clocks; before:
25 ns). With electrical noise from the router and the drivers, the short filter let glitches through and counts were
lost (2.4 deg in the test); 5 us removes them and still follows the encoder up to about 5000 rpm (600 PPR). Test:
`encoder_test.cpp` (a simulated encoder with noise, bounce, backlash and slips).

### The chuck in the app's Lathe screen (firmware 1.39, app 1.57)

The tile **Chuck** shows the chuck's angle, 0 ... 360°, from the spindle encoder, also while it turns; a tap on it
shows its speed (rpm) instead, and back. The button **0° here** next to it makes the angle 0 where the chuck stands now
(kept as the H4 keeps its positions). Both are for the display only: the lathe operations (threads, gearbox, ...) sync
to the spindle as before and do not use this 0. The speed is now measured while the app is watching, in every mode:
before, in the lathe modes the app showed 0 rpm unless the H4's own screen showed the tacho. The encoder has no index:
after the H4 was off, the angle is right only if the chuck was not turned meanwhile. In GCODE mode the chuck is the C
axis: its 0 is the **ZERO** button of C on the Control screen (G10 L20 P0 C0). Tests: `lathe_test.py`, `app_test.js`.

### Pause right after Resume (firmware 1.32)

Pause (feed hold) decelerates along the path and stops, as Grbl and LinuxCNC do; a Pause given while the tool stands
(e.g. right after a Resume, before it has moved) is complete at once. Before 1.32 such a Pause left one last step whose
time came out as distance / speed with the speed at zero: the machine crept for up to about 20 s before it counted as
stopped (found by the Pause / Resume stress test, work plan B3: `cnc_sim --holds`, 25 holds at random times in each
of 60 random programs, each must stop within 2 s and the program must end exactly where it should).

How far the tool goes on after Pause: the motion the step generator had already prepared (0.1 s,
`CNC_SEGMENT_AHEAD_S` in `cnc_core.h`, as Grbl's segment buffer), then the deceleration v² / 2a - at 2 mm/s and X
12.5 mm/s² (the defaults) about 0.20 + 0.16 mm (the app test on a PC measures 0.27 - 0.35 mm).

### Lost connection to the app (firmware 1.26)

A program the app or web page was watching (it asks for the status about every 0.35 s) **pauses** when that connection
has been lost for 3 s (setting *Pause the program when the app's connection is lost for*, 0 = never): WiFi lost, tablet
switched off, app closed or put in the background. It decelerates on the path, as Pause does, and keeps its place: once
the app is back, **Resume** on the page goes on from there; **ON on the H4** does the same without the app. Not during an
M0 / M1 / M6 wait (nothing moves then; if the app is still away afterwards, the program pauses then). A program started
with no app watching (keys of the H4) and a Grbl sender's lines are not affected: a sender that is lost runs the lines it
has already sent and stops (as Grbl). Test: `link_test.cpp`. Since 1.37 a connection lost while another pause is on
(the spindle stopped, with *Pause G-code when the spindle stops*) says so: the message was still "Spindle stopped" and
starting the spindle did nothing until Resume (found with the H4 on a PC, below). Since 1.38 the other order is right
too: Resume given while the spindle is still stopped shows "Spindle stopped" again (it went on showing the lost
connection) and the program goes on when the spindle turns.

### G-code read

The language is **RS274/NGC as LinuxCNC reads it** (Grbl reads a subset of the same language), for a lathe
with X, Z and C. Every line is checked as a whole before any of it runs; what cannot run exactly is refused
with the line and the reason. It is checked against LinuxCNC's own interpreter: 10 000 random programs (over 600 000
moves, 9 000 arcs, drilling cycles and G76 threading cycles, with G7, G20, G90.1, G10, G92, G28, G53, G43.1
mixed in) and the author's real programs give the same positions and refuse the same lines as LinuxCNC 2.9
(`RS274=/path/to/rs274 sh h4/test/run_tests.sh`). A larger run (2026-11-08, firmware 1.42) of 10 000 more random
programs found two lines the H4 took and LinuxCNC refuses, now refused alike: an arc code alone (`G2`, no R, I or K)
and a drilling cycle code without an axis word (`G81 F100`). One difference stays on purpose: a drilling cycle after a
plane and unit change (`G17` cycle, then `G20 G19` cycle without `G80`, in `G91`) keeps the first cycle's initial
level; LinuxCNC keeps its number without converting mm to inch, the H4 converts it. Moves of no length (a cycle
already at R) are not compared (`lcnc_compare.py`).

| Group | Codes |
|---|---|
| motion | G0, G1, G2, G3 (arcs in the X-Z plane, I/K or R, P turns, C moves along), G33 K / G32 F (threading with the encoder), G80 |
| cycles | G81, G82, G83, G73, G85, G89 with R, Q, P, L, G98 / G99 (G17: drill along Z, e.g. centre drilling at X0; G19: along X); G76 threading cycle (P I J K, Q compound angle, H spring passes, R degression; every pass synchronised to the spindle) |
| plane | G18 (start); G17 / G19 accepted, but arcs there need a Y axis and are refused |
| distance | G90, G91; arc centres G91.1 (start), G90.1 |
| feed | G94 (start), G93 inverse time (F on every move), G95 per spindle turn (speed from the encoder); with X/Z and C together F is along X/Z, as LinuxCNC |
| units, diameter | G21 (start), G20; G8 (start), G7: X words are diameters |
| coordinates | G54 (= the H4's own zero, X0 / Z0 keys), G55, G56, G57, G58, G59, G59.1, G59.2, G59.3 |
| offsets | G10 L2 P, G10 L20 P, G92, G92.1, G92.2, G92.3, G28 / G28.1, G30 / G30.1, G53 (machine coordinates) |
| tool | G43.1 Z, G49; G43 H accepted with a note (no tool table yet: touch off Z); T selects, M6 stops the spindle and pauses with "tool change: put in tool T.., touch off Z, then ON / Resume"; M61 Q sets the tool without a change |
| path | G61 and G64 [P Q] (start: G64): the path goes through every programmed point, corners are not rounded; G61.1: exact stop, every move ends at rest (an arc is one move) |
| overrides | M48 (start) and M50 / M50 P1: feed and rapid overrides apply; M49 and M50 P0: they do not, the moves run at the programmed feed and full rapid speed (as LinuxCNC, whose feed override switch covers rapids too); M2 / M30 turn them on again |
| spindle, coolant | M3, M4, M5, S, M7, M8, M9 (also M7 M8 on one line, as Grbl): the H4 does not switch a spindle motor or coolant, so the state is kept and shown (screen, web page, Grbl `$G`) and a pause or note tells the operator what to switch by hand; M2 / M30 stop both. Since firmware 1.24 an M3 / M4 that starts (or reverses) the spindle **pauses** the program: switch the spindle / router on by hand, then Resume (setting *M3 / M4 pause*, on; not used when the spindle encoder already pauses the program). With it, M3 / M4 and a move on one line are refused, as M6 and a move (the move would come first, with the router still off) |
| other | G4 P (seconds), G40, G97, G96 (noted: the spindle speed is set by hand), M0, M1 (optional stop: setting *M1 stops*, on by default), M2, M30, M60, N, `/`, `%` |
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
it cannot connect, it opens its own network **`H4`**, password **`12345678`** (firmware 1.44; changeable in the app),
address `192.168.4.1`. The connection (direct with the H4, or through the router) is set in the app only:
*Settings, Connection*; the app finds the H4 by itself (UDP "H4CNC?" to port 4210, the H4 answers with its
address) and joins the H4's network by itself. The address it got on your network is shown **on the H4's own display**, bottom line
(for 30 s after start-up in every mode, and always in the GCODE mode while no program runs; the setup access
point shows its name, password and address in turn), printed on USB and shown on the page. Type it into the
browser of a phone, tablet (Android or iPhone) or computer on the same network, e.g. `http://192.168.1.50`.

**Since firmware 1.16 the full screen is part of the Android app** (H4 CNC, app 1.18 and newer): the app serves it
for the H4's address, so every request still goes to the H4, and it is updated with the app, without flashing.
The firmware itself keeps a small **emergency page** for any browser (status, STOP, *Zeros checked* after a
restart, WiFi, firmware update, lasting log) - enough to recover the H4 if the app is not at hand. The app's screen
shows the following:

The page shows X, Z and C, the program name, a progress bar and the current line, Start / Pause / Resume and
a large STOP, a drawing of the program (polar programs as seen from the cutter, the done part in blue, the
tool as a red dot), the files, the 25 H4 keys, the WiFi settings and a firmware update. The page is a
convenience, **not** an emergency stop: keep a real one within reach.

- **Files** are uploaded to the H4's own flash (about 9.9 MB, folder `/gcode`), streamed, so the size of the
  upload is not limited by RAM.
- **Running**: Start reads the whole program into memory first (PSRAM, up to about 2 MB; 120 kB on a board
  without PSRAM) and then runs it from memory with look-ahead. Nothing is read from or written to the flash
  while the machine moves: a flash read or write turns the CPU cache off, and the step interrupt (not cache-safe in
  the Arduino core) and the step tasks of the other modes wait meanwhile. So the file list, *Draw*, uploads,
  deletions, settings and WiFi changes are refused while a program runs, while the H4 is ON in any mode (turning,
  threading, ...) and while an axis is being jogged.
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
position). A JD57 alarm is cleared only by switching the driver off and on.

**Emergency stop.** The E-stop must stop the machine without the H4: a normally closed mushroom button in the coil
circuit of a relay or contactor that switches off the drivers' supply and the spindle / router (with a separate
start button, so that nothing restarts by itself when the button is released). The H4 is only told, through A23
together with the alarms: a second contact of the same button (or of the relay)
- alarms in parallel (they close on alarm): a contact that **closes** when the E-stop is pressed, in parallel;
- alarms in series (they open on alarm): a contact that **opens** when the E-stop is pressed, in series (fail-safe:
  a broken wire also stops).
The page then says *Driver alarm or E-stop*; the program stops and the machine counts as not homed. After the
button is released and the drivers are powered again: Home All. A home switch that is hit outside
homing (*The switch also stops everything*) stops all motion at once as well; the axis can then be jogged away
from the switch only. Since firmware 1.28 the axes then count as not homed (setting *After a limit switch stopped the
machine, Home All again*, on, as Grbl's hard limit): the stop is immediate, without deceleration, so a motor may not
have followed it or a belt may have skipped; jog off the switch and Home All before the next program. Off: they stay
homed, as LinuxCNC.

**Home check** (firmware 1.29, page *Machine*, *Position*): each axis goes at its search speed to 2 mm before the point
where Home All saw its switch, then makes Home All's last approach (latch speed), compares where the switch is seen
now with that point and goes back. Within the setting (0.05 mm) OK; further off, found more than 2 mm early or not
found within 4 mm: the axis counts as not homed. Nothing is corrected. Test: `homing_test.cpp`.

Since firmware 1.21 the step interrupt reads these inputs too, on every tick and with the same 0.5 ms debounce (as
FluidNC and grblHAL stop on their limit pins): a switch used as a limit or the alarm that becomes active while a
program (or a gamepad jog) moves stops the steps there, whatever the rest of the firmware is doing, and they stay
stopped until the message, OFF and the log are done. Before, only the main loop read them, so a main loop held up
would have let the axis go on. Home All, keys and handwheels step from the main loop and are watched there as
before. Test: `homing_test.cpp` (a program driven onto the X switch and a driver alarm, with the main loop running
and stuck: stopped within the bounce + 0.5 ms; without this check the X axis went 176 mm past its switch).

**Home All** (button on the page, or `$H` in *Run line*): Z first, then X (setting), each one searching its
switch (with soft limits for at most their span + 10 mm, firmware 1.23: a switch that never answers, e.g. a normally
open switch with a broken wire, does not keep the axis pushing against its end for long; a normally closed switch is
safer still, as a broken wire then reads as pressed), leaving it slowly (latch), setting its machine coordinate to `HOME_OFFSET` there and moving to `HOME`;
A1 (C) takes machine 0 where it stands. Soft limits (`MIN_LIMIT` / `MAX_LIMIT`, machine coordinates) then
hold for programs and for jogging. With *Programs start only after Home All* a program does not start before.
OFF or STOP stops homing. After an emergency stop by cutting the drivers' power, home again.

## Restarts, firmware updates and the lasting log (safety.ino)

- **Restart while moving.** A crash, a watchdog or a voltage drop (brownout) restarts the ESP32 at once. If it
  happened while the machine moved, or during a G-code program (a power cut too: the H4 notes in its flash that a
  program runs, before the first move), the positions it restores are not where the axes are. The H4 then says
  so (the page shows *Restart while moving* with the cause) and **the GCODE mode does not start** until Home All,
  or, on a machine without home switches, until *Zeros checked* is pressed after checking the zeros.
- **A new firmware has to prove itself.** After an update over WiFi the new firmware is accepted only after it has
  run for 20 s; if it restarts before (it crashes), the ESP32 starts the previous firmware again and the log says
  so. (Do not switch the H4 off in the first 20 s after an update: that also counts as a failed start.)
- **Lasting log** (page: *Messages*, *Lasting log*; or `/events`): every start with its cause, restarts in a row,
  faults, alarms, Home All, programs started, written to the flash only while nothing moves (24 KB, then the older
  half is kept as `events.old.log`).
- The ESP32's own watchdogs stay as they are: a core 0 task that blocks for 5 s, or interrupts blocked for 300 ms,
  restart the H4 (and then the points above apply).

## Settings for a polar machine (X radial, Z axial, chuck on C)

On the web page (*Machine settings*) or in `h4.ino`: steps, travel per turn, speeds, accelerations,
direction and backlash of X, Z and A1 for the machine; A1 *Connected* and *Rotary* (the C motor on terminal
A1: enable 9, direction 10, step 11, letter `NAME_A1 = 'C'` in `h4.ino`); *Pause the program when the
spindle stops* off (the chuck is turned by the C motor, the spindle encoder must not pause the program).

## The H4 on a PC (`test/h4sim`)

The whole firmware, unchanged, runs on a PC: `python3 h4/test/h4sim/build.py OUT` joins the `.ino` files as the
Arduino builder does and compiles them with stand-ins for the ESP32, Arduino and library parts (`test/h4sim/shim`);
`sh h4/test/h4sim/run.sh OUT 8080` starts it. Its tasks are threads, its timers call their handlers on threads of
their own in real time, its web server is a real HTTP server (port 8080; the app's page can talk to it as to the H4),
settings and files are kept in `OUT/data`. Its STEP and DIR pins drive simulated motors (a step when STEP goes low, in
the direction DIR has then), so where the tool is is measured from the pins, not taken from the firmware; the home /
limit / alarm inputs and the spindle encoder are set by the test (its own HTTP port, 8081: `/state`, `/key`,
`/spindle`, `/input`, `/switch`). The step timer's timing figures on the Diagnostics page mean nothing on a PC.
It is built for 32 bits (`-m32`, `g++-multilib`), so int, long and pointers have the ESP32's sizes (the firmware
relies on it). The simulated switches act on the motor's position (the backlash between motor and carriage is not
simulated). Every millisecond the spindle and the motors are recorded (`/trace`).
`lathe_test.py` sets and starts the lathe modes through `/lathe` as the app's Lathe screen does, with a simulated
spindle: the gearbox moves Z exactly the pitch per turn (1.4999 mm for 1.5 over 10 turns, measured standing) and back
on the same line when the spindle turns back; in a thread every cutting stroke falls in the same groove (Z against the
spindle angle, within a few steps of one turn), the depth grows, the H4 switches itself off at the end; a 2-start
thread cuts its starts half a turn apart; turning stays between the stops. The PC interrupts the firmware's loop()
for milliseconds, so the following is checked at speeds where that does not hide it; `follow_test.cpp` checks the
firmware's moveAxis() itself in exact simulated time: within 2 steps of the spindle's place up to 6000 steps/s.
`app_test.js` opens the app's real page (`android/app/src/main/assets/h4/index.html`) in Chromium, connected to it,
and uses its buttons as the operator does: machine settings saved (the H4 restarts and keeps them), Home All with
switches, MDI, a program uploaded with the page's form and started with Start, Pause / Resume, the tablet's WiFi lost
during a program, STOP; every position is checked on the motors.
`scenario_test.py` uses it as the app does (about a minute): MDI moves end with the motors exactly where the
firmware says the tool is (steps per mm, direction and backlash from `h4.ino`); a program uploaded and started as the
app does runs to its end; Pause stops on the path and Resume ends exactly; a lost connection pauses after 3 s; STOP
leaves the firmware's position equal to the motors'. GitHub Actions run it on every change.

## Tests (on a PC)

The app's page is checked too: `node h4/test/app_syntax.js` (it must parse as ECMAScript 2017: Android 7 tablets may
have an old WebView, where newer syntax such as `??` leaves the screen blank) and `node h4/test/app_crawl.js` (every
button of every screen pressed in demo mode, English and Greek, three screen sizes; any JavaScript error fails). All
tests also run clean with `-fsanitize=address,undefined` (2026-11-08); the only report is the original firmware's
LCD redraw hash (`h4.ino`, a sum of stop positions that wraps, harmless and left as it is).

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
- `cnc_sim --delayed --pulse-us --dir-us --dirhold-us`: the STEP / DIR pin timing as the interrupt drives the pins
  (pulse width, spacing, direction set before the next step and held after the last one) at 5, 10 and 20 us
  direction setup and 0, 10 and 20 us hold;
- `homing_test`: the firmware's own `homing.ino` on a simulated machine: Home All with NO and NC switches that
  bounce for 2 ms (switch point = `HOME_OFFSET`, axis at `HOME`), starting on the switch, an NC switch with a
  broken wire, a disconnected switch, noise shorter / longer than the 0.5 ms debounce during a program, a driver
  alarm or E-stop and a STOP during homing, homing and programs refused while an alarm is on, soft limits and
  jogging (only back inside when outside them, never further into a pressed switch), Z or X first;
- `g93_timing.py`: G93 moves of 1, 2, 3, 7, 60 steps, shorter than a step and slower than a step every few
  seconds, on X alone and X Z C together, up to 2000 moves: every move ends when the sum of 1/F says, no drift;
- `cnc_sim --stop-at S`: STOP in the middle of a program (also inside a backlash take-up), then a new move
  must reach its end point exactly; soft limits and G-code errors in `cnc_parse_test`;
- `g93_hard.ngc`: G93 blocks of one step, very short and very long blocks, C-only and X-only moves;
- `cam_check.py`: real programs written by FreeCAD 26.3 (GRBL and LinuxCNC posts, `test/cam/`): straight to the
  H4 they are refused at the first Y move with nothing moved before; through the polar converter they run in
  the simulation and match LinuxCNC's interpreter;
- `cnc_parse_test` also checks M48 / M49 / M50 (state and the planned speed with overrides at 50 % / 25 %),
  M51-M53 refused and the `%` start / end line;
- with `RS274=/path/to/rs274`: `lcnc_compare.py` (positions and refused lines against LinuxCNC's own
  interpreter, 1000 random programs and the samples) and `modes_compare.py` (M48 / M49 / M50, G61 / G61.1 /
  G64, spindle, coolant and tool in force for the same moves as in LinuxCNC, 500 random programs). GitHub Actions run all of it on every
  change.
- `lcnc_traj.py` with `lcnc_ref/`: the whole of LinuxCNC 2.9 (interpreter, task, motion controller, trajectory
  planner; no hardware, no screen) runs a program on the author's machine values, the joint positions are sampled every
  millisecond, and the H4's motion core runs the same program (`cnc_sim --samples`). Compared: the path on the part
  (x = X cos C, y = X sin C, z = Z) in feed moves and in rapids, the end point, the time, each axis' speed and
  acceleration. GitHub Actions run it on four sample programs with LinuxCNC in G61. Found with it (2026-11-08):
  in LinuxCNC 2.9.10 the `G64 P` of `RS274NGC_STARTUP_CODE` does not reach the motion controller (corners blended
  with no limit until a program or MDI gives `G64 P`); with `G64 P0.025` LinuxCNC still rounds corners where C turns by
  up to 0.44 mm on the part (its tolerance counts X/Z only) and drove X at 17.8 mm/s with a limit of 12 in a blend of
  rapids; with G61 it keeps to 0.009 mm and 12 mm/s. The H4 does not blend: it went within 0.011 mm of every programmed
  point of the polar sample. Run it: `python3 lcnc_traj.py --linuxcnc <built tree or installed> --sim <cnc_sim>
  --user <non-root user> --mdi G61 programs.ngc` (LinuxCNC refuses to run its realtime part as root).

`sh h4/test/record.sh [--lathe]` runs the same tests (with `--lathe` also the H4 screen tests of `test/lathe/run.sh`)
and keeps a record in `h4/test/results/<UTC date>-<commit>.txt`: commit, firmware and app versions, tools, whether
LinuxCNC's `rs274` was used, every "checks / failures" line, PASS or FAIL. It then shows what changed since the
previous record. GitHub Actions keep the record of every run as a download (artifact *test-record*). These are PC
results (simulation), never a test on a real H4.

## License

The original NanoEls code is MIT (`LICENSE`, © Maxim Kachurovskiy). `cnc_core.cpp` is ported from Grbl
(GPL v3), so this fork as a whole is distributed under the **GNU GPL v3** (`COPYING`).
