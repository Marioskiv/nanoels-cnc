# PS4 / PS5 gamepad pendant for the NanoEls H4 CNC fork

Jog X, Z and C of the H4 with a PS4 (DualShock 4) or PS5 (DualSense) gamepad. Nothing changes in the H4
firmware: a small **plain ESP32** board (about 5 €, e.g. "ESP32 DevKit V1 / ESP32-WROOM-32") receives the
gamepad over Bluetooth and sends the H4 Grbl jog commands over WiFi (TCP port 23).

Without a second board: the H4 web page can read the gamepad connected to an Android phone (see `h4/CNC.md`,
*Gamepad on the phone*). This pendant does not need the phone or a browser setting.

Why a second board: the H4's ESP32-S3 has only Bluetooth Low Energy, and the PS4/PS5 gamepads use Bluetooth
Classic. An ESP32-**S3**, **C3** or **S2** board will not work here; it must be the original ESP32.

## Buttons

The same layout as the LinuxCNC `gamepad.hal` of the XZC machine.

| Button | Action |
|---|---|
| **L1 (hold)** | jog enable (deadman): nothing moves without it; releasing it stops at once |
| R1 (hold) | fast speed (1000 mm/min instead of 400) |
| Left stick left/right | Z (right = +Z) |
| Right stick up/down | X (down = +X) |
| L2 / R2 | C+ / C- (press further = faster; 40 deg/s, with R1 100 deg/s) |
| L1 + Share | Home All |
| L1 + D-pad down | touch off Z: the tool's current point becomes Z0 of the active work offset |
| Options | **STOP**: stops all motion of the H4, a running program too |
| PS | clear an alarm |
| D-pad up / down (without L1) | feed override +10 / -10 % (10-200 %) |
| D-pad left | feed override back to 100 % |

Cross, Circle, Triangle and Square are not used (the H4 does not switch the machine or the spindle).
The H4 still limits every axis to its own maximum speed and its soft limits; with *Home required* on, jogging
needs Home All first.

Light bar: **white** = no WiFi, **green** = ready, **blue** = jogging, **red** = the H4 does not answer, an
alarm, or a refused command (the gamepad also rumbles; the H4's reason is printed on USB and on the H4 page).

## Safety

Each jog command is a short piece (0.05 s of motion) and the H4 never holds more than 0.2 s of them. When the
stick returns to the centre, L1 is released, the gamepad is lost (or sends nothing for 0.25 s) or the WiFi
drops, the motion stops: at once with a jog cancel when the pendant can still send it, otherwise after at most
0.2 s of travel plus braking. **This is not an emergency stop**: keep the power-supply switch within reach.

The pendant uses the H4's Grbl port only while L1 is held and for 10 s after; the H4 takes one Grbl connection
at a time, so do not use the pendant while a G-code sender (UGS, gSender, ...) is connected over WiFi. The H4
web page is not affected.

## Install (Arduino IDE)

1. *File > Preferences > Additional boards manager URLs*, add
   `https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json`
2. *Tools > Board > Boards Manager*: install **esp32_bluepad32** (Bluepad32 by Ricardo Quesada, version 4.1.0
   was used). Choose the board **esp32_bluepad32 > ESP32 Dev Module**. (This is a separate board package
   from the H4's normal *esp32*; for the H4 itself keep using *esp32 > ESP32S3 Dev Module*.)
3. Open `h4_ps4_pendant/h4_ps4_pendant.ino`. At the top set `WIFI_SSID`, `WIFI_PASSWORD` (the same network as
   the H4) and `H4_ADDRESS` (shown on the H4 display after start-up, e.g. `192.168.1.50`). Give the H4 a fixed
   address in the router (DHCP reservation), otherwise it may change.
4. Upload. Power the ESP32 from any USB charger.
5. Pair: hold **Share + PS** on the gamepad until the light bar flashes quickly. Later the gamepad reconnects
   with the PS button alone.

Settings at the top of the file: speeds (`JOG_SPEED`, `JOG_SPEED_FAST`, `C_SPEED_FACTOR`), dead zones,
`DIR_X` / `DIR_Z` / `DIR_C` (set -1 when an axis goes the wrong way), `USE_C` (false without a C axis).

## Status

Compiled with esp32_bluepad32 4.1.0 (board ESP32 Dev Module) without errors or warnings. **Not yet tested with
a real gamepad and H4.** First test: wheels or belts off or the machine in the air, low speed, a hand on the
power switch. Known risk: WiFi and Bluetooth share one radio on the ESP32; an occasional reset of the pendant
board has been reported by other Bluepad32 users. A reset drops the connection, so the H4 stops as above.

Licences: this sketch is MIT. Bluepad32 is Apache-2.0 (https://github.com/ricardoquesada/bluepad32); the
Bluetooth stack inside it, BTstack (BlueKitchen), is free for personal / non-commercial use and needs a
licence from BlueKitchen for a commercial product.
