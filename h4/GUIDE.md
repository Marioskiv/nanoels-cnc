# NanoEls H4 as a CNC controller — setup guide

Everything needed to wire, flash, set up and run the H4 with this fork's CNC firmware, from the box to the first
part. Details of the G-code language, the Grbl protocol and the settings are in [CNC.md](CNC.md); the original
lathe functions (threading, turning, ...) are in [README.md](README.md) and keep working as before.

**Status:** the firmware is tested on a PC (G-code compared line by line with LinuxCNC 2.9, motion simulated) but
not yet on many real machines. Do the first runs in the air, slowly, with a hand on the power switch.

Contents: [1 What you need](#1-what-you-need) · [2 Power](#2-power) · [3 Wiring](#3-wiring) ·
[4 Flashing](#4-flashing-the-firmware) · [5 Your machine's values](#5-your-machines-values) ·
[6 WiFi and the web page](#6-wifi-and-the-web-page) · [7 First start checklist](#7-first-start-checklist) ·
[8 G-code and CAM](#8-g-code-and-cam) · [9 Running a program](#9-running-a-program) ·
[10 Jogging and the gamepad](#10-jogging-and-the-gamepad) ·
[11 Self-test and diagnostics](#11-self-test-and-diagnostics) · [12 Safety](#12-safety) ·
[13 Troubleshooting](#13-troubleshooting) · [14 Limits of this controller](#14-limits-of-this-controller)

## 1 What you need

- A NanoEls H4 (see [parts.md](parts.md) to build one, [h4.pdf](h4.pdf) for its datasheet).
- Step/direction stepper drivers (open or closed loop) for X, Z and, for a rotary axis, C, with their own power
  supply. The H4 only gives the step, direction and enable signals.
- A USB-C phone charger (5 V, 1 A is plenty) to power the H4.
- Optional: home switches for X and Z, a spindle encoder (lathe functions), a phone, tablet or computer on the
  same WiFi (web page), an Android phone and a PS4 / PS5 gamepad (jogging).

## 2 Power

- The H4 is powered **only by 5 V on its USB-C port**, protected by a 300 mA fuse. With WiFi, programs are sent
  over the air, so USB is used for power alone; a charger is enough.
- The drivers have their own supply (12-80 V depending on the driver). Never connect that voltage to an H4
  terminal: **every H4 terminal takes at most 5.5 V**.
- Ground: the drivers' signal inputs are opto-isolated, so their power ground and the H4 ground are separate.
  Join grounds only where this guide says so (switches, alarm, external 5 V).

## 3 Wiring

![h4 back](https://github.com/kachurovskiy/nanoels/assets/517919/bd0fd006-5bda-4b00-a66b-ec7d7cab502e)

All terminals are on the back: pluggable screw terminals, 3.5 mm pitch.

### 3.1 Terminals

| Terminal | Pin | Use in CNC | ESP32 pin |
|---|---|---|---|
| **Z axis** | 5V / ENA / DIR / STEP | Z driver (axial, along the bed) | ENA 16, DIR 17, STEP 18 |
| **X axis** | 5V / ENA / DIR / STEP | X driver (cross slide, radial) | ENA 8, DIR 19, STEP 20 |
| **A1** | 5V / ENA / DIR / STEP | C driver (rotary: chuck or dividing head), or handwheel 1 | ENA 9, DIR 10, STEP 11 |
| **A2** | 5V / ENA / DIR / STEP | inputs: ENA = **A21 home X**, DIR = **A22 home Z**, STEP = **A23 driver alarm**; or handwheel 2 | 12, 13, 14 |
| **Encoder** | GND / 5V / ENCB / ENCA | spindle encoder (lathe modes, G33/G76/G95), and the GND for switches | 7, 15 |

A terminal is either an output or an input: A1 cannot drive the C motor and read handwheel 1 at the same time, A2
cannot take home switches and handwheel 2 at the same time (the settings refuse it).

### 3.2 Stepper drivers (Z, X, A1)

The H4 sinks the signals ("common anode"): the driver's `+` inputs go to 5 V, the `-` inputs to the H4 pins.

| H4 terminal pin | Driver input |
|---|---|
| 5V | `PUL+`, `DIR+`, `ENA+` (all three) |
| STEP | `PUL-` |
| DIR | `DIR-` |
| ENA | `ENA-` |

- The drivers' signal inputs must accept **5 V** (most do; drivers marked "24 V only" need resistors or a 5 V
  setting - see their manual).
- The 5V line of each terminal goes through a 200 Ω resistor, which is too weak for some drivers (three
  optocouplers on one line). If a motor misses steps, moves erratically or does not enable: feed `PUL+`, `DIR+`,
  `ENA+` from a separate regulated **5 V** supply instead, and join that supply's GND to the H4 encoder `GND`.
- **Enable**: the H4 keeps ENA high (no current) to enable the driver and pulls it low to release the motor
  (the setting "disable the driver when stopped" for open-loop drivers, or an axis switched off on the H4). If your driver works the other
  way round (motor free when it should hold), leave ENA unconnected: the driver is then always enabled.
- Step pulse length, direction set-up and direction hold time are settings (`CNC_STEP_PULSE_US`,
  `DIRECTION_SETUP_DELAY_US`, `DIRECTION_HOLD_US`, LinuxCNC `STEPLEN` / `DIRSETUP` / `DIRHOLD`); 5, 10 and 10 µs suit
  almost every driver. Take the values from a working LinuxCNC `.ini` or the driver's datasheet, not shorter.
- Use shielded cable for the signals, keep it away from motor wires, connect the shield to GND at the H4 end only.
- A wrong direction is fixed in the settings (*Invert direction*), not by rewiring.

### 3.3 Home switches and driver alarm (A2)

| A2 pin | Connect |
|---|---|
| ENA (A21) | home switch X, one wire; the other wire to GND |
| DIR (A22) | home switch Z, one wire; the other wire to GND |
| STEP (A23) | driver alarm outputs and the E-stop contact (all together, see [CNC.md](CNC.md#home-switches-limits-and-driver-alarm-wiring)) |
| 5V | nothing |

GND is the `GND` of the Encoder terminal (use a small terminal strip for the switch and alarm ground wires).

- The inputs have pull-ups to 5 V: they read a switch, an open-collector (NPN) sensor or a driver's alarm
  transistor that connects them to GND. **Never** connect a sensor that outputs 12 or 24 V (PNP sensors) directly.
- Switches may be normally open or normally closed: one setting per switch. Normally closed is safer (a broken
  wire reads as "pressed").
- A switch can also be the limit at its end of travel: *The switch also stops everything*.
- NPN inductive sensors: their supply (often 12-24 V) has its own ground; join that ground to the H4 GND.

### 3.4 Spindle encoder

| Encoder terminal | Encoder wire |
|---|---|
| GND | 0 V (usually black) and the shield |
| 5V | supply (usually red) - only for 5 V encoders, at most 100 mA |
| ENCA / ENCB | the two signal wires (swap them if the angle counts backwards) |

An encoder that needs more than 5 V (e.g. 12-24 V open-collector types) gets its own supply; join its 0 V to the
H4 GND and connect only A and B to the H4. The encoder is needed for the lathe functions, G33 / G76 threading and
G95; a polar milling machine (chuck turned by the C motor) does not need it.

### 3.5 What the H4 does not have

- **No spindle output**: `M3` / `M4` / `M5` / `S` are read and ignored; switch the spindle or router by hand
  (or with its own controller).
- **No emergency-stop input**: put a real emergency stop in the drivers' power (it must also stop the spindle).
  After it was pressed, home again: the drivers may have lost their position.
- No probe input, no coolant output.

## 4 Flashing the firmware

**Without the Arduino IDE** (general default values; your machine's values are then set on the page, section 5):
on a computer with Chrome or Edge, download `h4-firmware-<version>-usb.img` from the
[Releases](https://github.com/Marioskiv/nanoels-cnc/releases) entry *H4 CNC firmware*, plug the H4 in by USB, open
[esptool-js](https://espressif.github.io/esptool-js/), *Connect* (choose the H4's port), *Flash Address* `0x0`,
choose the file, *Program*, then unplug and replug the H4. This clears the H4's stored settings (not its G-code
files). If it does not connect, try another USB cable (some carry only power).

**With the Arduino IDE** (needed for your own `my_machine.h`):

1. Install the [Arduino IDE](https://docs.arduino.cc/software/ide-v2).
2. *File > Preferences > Additional boards manager URLs*: add
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`.
3. *Boards Manager*: install **esp32** (by Espressif). *Library Manager*: install **Adafruit TCA8418**.
4. Download this repository, open `h4/h4.ino` (the folder must stay named `h4`, with all its files).
5. *Tools*: Board **ESP32S3 Dev Module**, Flash Size **16MB (128Mb)**, PSRAM **QSPI PSRAM**, Partition Scheme
   **16M Flash (3MB APP/9.9MB FATFS)**, Port: the one that appears when the H4 is plugged in.
6. Upload. This first installation is over USB; **later updates go over WiFi**: the Android app compares the H4's
   firmware with the newest `firmware-v...` release of this repository and offers it (*Install*), or the page's
   *Firmware update* takes a `.bin` (the Releases entry *H4 CNC firmware*, or *Sketch > Export Compiled Binary*). The H4 keeps its
   machine settings through an update: with `my_machine.h` they are stored in the H4 at its first start, and
   *Save and restart* in *Machine settings* stores all of them.

## 5 Your machine's values

Put your machine's numbers in `h4/my_machine.h` (copy `my_machine.h.example`). It is not part of the repository,
so a newer firmware copied over the folder keeps it. Everything can also be changed later on the web page
(*Machine settings*, stored in the H4; *Defaults from the firmware* goes back to `my_machine.h`).

| Value | Meaning | From a LinuxCNC `.ini` |
|---|---|---|
| `MOTOR_STEPS_X`, `SCREW_X_DU` | steps per screw turn, travel per turn (deci-microns: 2 mm = 20000) | steps per mm = \|`STEP_SCALE`\| |
| `SPEED_MANUAL_MOVE_X` | top speed (G0, jogging), steps/s | `MAX_VELOCITY` x steps per mm |
| `ACCELERATION_X` | steps/s² | `MAX_ACCELERATION` x steps per mm |
| `INVERT_X` | direction | check with a move (the sign of `STEP_SCALE` does not carry over) |
| `BACKLASH_DU_X` | backlash, deci-microns | `BACKLASH` |
| (same for `_Z`, and `_A1` for C) | for a rotary A1, travel per turn is in degrees x 10000 | |
| `ACTIVE_A1`, `ROTARY_A1` | C motor connected to A1, rotary | |
| `HOME_X_*`, `LIMIT_X_*` (and Z) | home switch and soft limits | `HOME_SEARCH_VEL`, `HOME_LATCH_VEL`, `HOME_FINAL_VEL`, `HOME_OFFSET`, `HOME`, `MIN_LIMIT`, `MAX_LIMIT`: same numbers |
| `HOME_Z_FIRST` | Home All order | `HOME_SEQUENCE` |
| `SPINDLE_PAUSES_GCODE` | pause when the spindle stops | **false** for polar milling |

Keep the numbers in steps exact: for a rotary axis pick the smallest "screw" that gives whole steps
(e.g. 91.733333 steps/degree = 1376 steps per 15°).

## 6 WiFi and the web page

1. At the first start the H4 opens its own network **`NanoEls-H4-Setup`**, password **`nanoels-h4`**. Join it
   with a phone and open `http://192.168.4.1`. The H4 display shows the name, password and address in turn.
2. On the page, *WiFi*: enter your network and password. The H4 restarts and joins it.
3. The H4 display now shows **`IP 192.168.x.y`** (bottom line, for 30 s after start, and always in the GCODE mode
   while idle). Open `http://192.168.x.y` on any phone, tablet or computer on the same network. Give the H4 a fixed
   address in your router (DHCP reservation), so it does not change.
4. Android: the [H4 CNC app](../android/README.md) shows the same page full screen and reads a PS4 / PS5 gamepad.

The page **never scrolls**: every screen fits the phone or tablet, upright or turned. Screens are chosen with the
**tabs** (at the bottom when upright, on the left when turned; or swipe left / right; the phone's Back key goes to the
screen before). What does not fit is on sub-tabs (e.g. *Run | Files | Path*), on pages (‹ 1/2 ›) or in a pop-up.
On a tablet turned sideways the parts of a screen sit side by side. **STOP** is always at the top right. Messages
also appear for a few seconds; a red dot on *Machine* means a new message, *G-code* shows the progress while a
program runs.

- **Control**: X / Z / C in work coordinates with the machine position under them and a ZERO per axis (it asks
  first; on a polar machine X0 must stay the chuck centre); the step (.001 ... 10, or **Cont**) and the **jog
  lock** 🔒; the **jog buttons** X−/X+, Z+/Z−, C+/C− (a tap is one step; with *Cont* the axis moves while the
  button is held and stops when it is released) or a **jog wheel** (tap an axis, turn); START / HOLD, feed and
  rapid overrides (F / R open a pop-up), Home, Macros (your own one-line G-code buttons, kept in that phone or
  browser), MDI and Files. With the jog lock on, the jog buttons and the wheel do nothing until 🔒 is pressed, and
  lock again 15 s after the last move.
- **Lathe** (*Τόρνος*): the H4's own lathe modes (feed, turning, facing, taper, thread, parting, sphere, constant
  feed): large X / Z readout, spindle speed and angle, a side view with Z0, the stops and the tool, and what the
  H4 is set to (mode, feed or pitch, passes, starts, cone ratio, external / internal, stops, current pass).
  Updated about 7 times a second. Work is set from the part's zero, as on the H4 keys: **Z0** on the face, **X0**
  either on the surface (X values are depths) or on the centre line from a measured diameter (**Ø → X0**, X values
  are diameters); **How?** on the screen explains it. Pick an operation at the top and fill its form: Z values
  (green: start, length) and X values (orange: part Ø and final Ø, or the depth) side by side, or a preset.
  The side view draws the part from its zero (face at Z0, its diameter), the material that comes off and the
  tool's path pass by pass; **▶ Simulate** runs the tool along it. Presets: metric and UNC threads, Morse and other tapers, spheres; every value stays editable, with the
  page's own number pad. Filling the form moves nothing and sends nothing. **Hold START**: the page sends the values
  with the same functions the H4 keys use (only while the H4 is OFF and nothing moves) and presses ON as many times
  as the H4's own setup steps need; the H4 then moves the tool to the start point by itself and cuts, and refuses
  as it does from the keys. Taper, feed and parting start where the tool stands. **STOP** turns it OFF. Inch
  measure: set the values on the H4 keys.
- **G-code**: *Run* (start / pause / resume / **STOP**, overrides, one-line MDI), *Files* (upload, draw, test,
  delete) and *Path* (the drawing of the program).
- **H4 keys**: the H4 display and keys, and the gamepad.
- **Machine**: positions, Home All and the inputs; diagnostics; messages.
- **Settings**: *Appearance* (language English / Ελληνικά for the Lathe screen, theme, colours, number size), *Screen and jog* (jog buttons or wheel, jog lock on or
  off; in the Android app also the screen orientation: automatic, landscape or portrait), one page per group of
  machine settings, WiFi, firmware; in the Android app also *App update* (the app's version and a button that
  looks for a newer one at once; the app also looks by itself each time it opens).

## 7 First start checklist

Do it in this order, the motors free to move, nothing in the chuck, a hand on the power switch.

1. **Inputs**: on the page, *Inputs (terminal A2)* shows each switch. Free = not active; press each by hand =
   active. Wrong: change *Switch is normally closed* for that switch.
2. **Directions** (before homing): MDI `$X` (allows moves before Home All), then
   `G91 G1 X1 F100` - X must move away from the centre (positive X = larger radius),
   `G91 G1 Z1 F100` - Z must move away from the chuck / work,
   `G91 G1 C90 F1000` - the chuck must turn **clockwise seen from the tool** (ISO / LinuxCNC rule for a C axis
   turning the work), then `G90`. Wrong: *Invert direction* for that axis.
3. **Home All** (page button, L1 + Share on the gamepad, or `$H`): Z first, then X (setting). Check that each
   axis finds its switch and stops at `HOME`.
4. **Distances**: `G53 G1 X20 F300` and `G53 G1 Z-20 F300` - measure 20 mm (a wrong distance = wrong steps per
   mm or a driver microstep switch).
5. **Lost steps on C**: a mark on the chuck, `G0 C3600` then `G0 C0`: the mark must line up. If not, lower the
   C speed / acceleration (high C speeds mean tens of thousands of steps per second).
6. **Work offset**: touch the tool on the work and set zero: `G10 L20 P1 Z0` (or L1 + D-pad down on the gamepad);
   for a polar machine X 0 is the chuck centre and stays so.
7. **Self-test**: *Test* the program on the page (section 11).
8. **Air cut**: run the program with Z raised (e.g. `G10 L20 P1 Z-20` so the work zero is 20 mm lower), check
   *Diagnostics* (section 11), then cut.

## 8 G-code and CAM

- The H4 reads **RS274/NGC as LinuxCNC 2.9** does, for a lathe with X, Z and C: G0-G3 (arcs in the X-Z plane),
  G4, G7/G8, G10 L2/L20, G20/G21, G28/G30, G33, G43.1/G49, G53, G54-G59.3, the drilling cycles G73, G81, G82,
  G83, G85, G89 and the G76 threading cycle, G80, G90/G91, G90.1/G91.1, the G92 family, G93/G94/G95, M0-M2, M30,
  U/W. The full list is in [CNC.md](CNC.md#g-code-read). A line it cannot run exactly is refused with the line
  number and the reason - nothing is skipped silently.
- **Turning on the lathe**: in the CAM choose the **LinuxCNC turning** post (Fusion 360 "LinuxCNC Turning",
  FreeCAD's LinuxCNC lathe post): mm, tool nose compensation in the computer (not G41/G42), no turning cycles
  (G70-G72), arcs allowed.
- **Polar milling** (router on the cross slide, chuck on C): make a normal 3-axis XYZ milling program and convert
  it with the polar converter. Post: **LinuxCNC** (FreeCAD `linuxcnc`, Fusion 360 "LinuxCNC"; Generic GRBL works
  too): mm, cutter compensation in the computer (not G41/G42), arcs in the XY plane only, retracts to a clearance
  height. FreeCAD: set the feeds in the Tool Controller (0 gives moves without F, which are refused). Converter:
  [polar converter](https://marioskiv.github.io/polar-cnc-converter/) (output X Z C in G93). A milling program
  with Y moves cannot run on the H4 directly: it has no Y axis.
- Programs over WiFi: upload on the page (stored in the H4, up to several MB) or stream from a Grbl sender
  (UGS, gSender, CNCjs, bCNC) to the H4's address, port 23.

## 9 Running a program

1. Home All, set the work offset (section 7).
2. Upload the file on the page, press *Draw* to check the path, then *Start*.
3. **Feed override** (10-200 %, page buttons, gamepad D-pad up / down / left, Grbl senders): changes the feed of
   everything already planned at once; rapids 100 / 50 / 25 %. A program can switch the overrides off with M49
   or M50 P0 (as in LinuxCNC; the page then shows *off*) and on again with M48 or M50.
   *Pause* stops along the path and *Resume* continues; **STOP** brakes along the path and drops the rest of the
   program (the position stays known). The H4 keys work as well (ON / OFF).
4. Messages and refused lines appear under *Messages*. A home switch hit outside homing or a driver alarm stops
   all motion at once; after a driver alarm the machine counts as not homed (Home All again).

## 10 Jogging and the gamepad

- H4 keys or the page's keys: the arrows move X and Z, held keys stop when released or when the page loses its
  connection.
- PS4 / PS5 gamepad, with the layout of a LinuxCNC `gamepad.hal`: **hold L1** to move; left stick Z, right stick
  X, L2 / R2 C, R1 fast, L1 + Share Home All, L1 + D-pad down touch off Z, Options STOP, PS unlock (`$X`),
  D-pad up / down / left (without L1) feed override +10 / -10 / 100 %.
  - **Android app** (recommended): pair the gamepad with the phone, open the app. See
    [android/README.md](../android/README.md).
  - Chrome on a phone or computer: the page reads the gamepad after a one-time browser setting (see the
    *Gamepad* box on the page).
  - Without a phone: a plain ESP32 as a Bluetooth pendant, see [h4_ps4_pendant](../h4_ps4_pendant/README.md).
- On the page: the jog buttons or wheel of the *Control* screen (see section 6).
- Jogging respects the soft limits once homed. Speeds are set in the page's *Gamepad* box (also for the jog
  buttons with *Cont*).

## 11 Self-test and diagnostics

No instrument is needed to see whether the controller keeps up with a machine and a program: it measures itself.

- **Test a program** (page, *Files*, *Test*): the file is read and planned exactly as when it runs, but nothing
  moves. The message gives how many moves per second this H4 plans and how many the program asks for at most
  (any 32 moves in a row, at their programmed speeds), with the line. `OK, N times faster than needed`, or
  `TOO CLOSE`: then the machine would slow down there (the position stays exact); fewer, longer moves in the CAM
  or the converter help. The position, offsets and modes are the same afterwards.
- **Diagnostics** (page): measured while programs run, since the start of the last program (or *Clear*):
  - the longest step interrupt, next to the time between two steps at the highest step rate given - it must stay
    well below it;
  - how often an interrupt started more than 10 µs late (WiFi, flash, other interrupts) - should be 0;
  - the highest step rate given and the limit, 1 / (pulse + the longer of pulse and direction hold + set-up): with
    the defaults (pulse 4, hold 5, set-up 5 µs) 71 000 steps/s, with 5 / 10 / 10 µs (a typical LinuxCNC `.ini`)
    40 000 steps/s;
  - how often the step buffer ran empty while moves were waiting (the controller could not keep up) - should be 0.
  The page shows **OK** or **CHECK**.
- How to use them: *Test* the file, then run it **in the air** at full speed with the page open (WiFi in use) and
  look at *Diagnostics*. For fast rotary axes also do the mark test (section 7, step 5).

How the step interrupt works (learned from FluidNC's "timed" engine): it runs at the highest interrupt level a
handler may have (WiFi cannot delay it), puts out the step computed before at its very start, computes the next
one while the pulse is high and sets the direction pins right after the pulse, a whole step period before the
next step. A speed above what the pulse length allows is refused in the settings and lowered at start-up.

## 12 Safety

- The H4, the page, the app and the gamepad are conveniences, **not safety devices**. Wire a real emergency stop
  that cuts the drivers' power and the spindle.
- Keep the power switch within reach for every first run, every new program and every change of settings.
- Home switches as limits and soft limits only work when they are set up and the machine is homed.
- After an emergency stop, an alarm or a power cut: Home All before the next program.
- A program that is wrong in the CAM is run exactly as written: check it with *Draw* and an air cut.

## 13 Troubleshooting

| Symptom | Likely cause |
|---|---|
| Arduino IDE does not see the H4 | another USB cable (some are power-only); unplug and replug to find its port |
| Motor hums or misses steps | too high speed or acceleration; weak 5V signal line (use an external 5 V, section 3.2); step pulse too short |
| Axis moves the wrong way | *Invert direction* for that axis |
| Wrong distance | steps per turn or travel per turn; the driver's microstep switches |
| Home All stops at once / "on its switch" | switch type (normally open / closed) or wiring; check *Inputs* |
| "Home All first" | *Programs start only after Home All* is on: home, or `$X` to unlock |
| A line is refused | the message names the line and the reason; change the CAM post or the program |
| Page does not open | the address on the H4 display; the phone on the same WiFi; restart the H4 |
| "WiFi lost" on the display | the H4 lost the network; it reconnects by itself, programs on the H4 keep running |
| Gamepad does nothing | hold L1; in Chrome see the *Gamepad* box; in the app check that it is paired |
| Motion slows down or stutters | *Test* the file; *Diagnostics*: step buffer ran empty, interrupt late or too long |

## 14 Limits of this controller

- 3 axes: X, Z and C (A1). No Y axis.
- No spindle control, probe, coolant or emergency-stop input (section 3.5).
- G41 / G42 (cutter compensation in the controller), G70-G72 turning cycles, G38 probing, subprograms and
  parameters (`o` words, `#`) are refused. There is no tool table: `G43 H` is accepted with a note, touch off Z
  for every tool.
- Speeds: the step rate of the ESP32 is limited; very fast rotary axes (tens of thousands of steps per second)
  must be checked for lost steps (section 7, step 5).
