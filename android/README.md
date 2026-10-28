# H4 CNC — Android app

A small app for the NanoEls H4 CNC fork: it opens the H4's own web page full screen (files, Start / Pause /
STOP, positions, Home All, MDI, the H4 keys and display, settings, firmware update) and reads a **PS4 / PS5
gamepad** through Android itself, so no browser setting is needed. Android 5.0 and newer.

## Install

1. On the phone open **https://github.com/Marioskiv/nanoels-cnc/releases/latest/download/H4CNC.apk** (always the
   newest app; in the browser, not the GitHub app). If the browser warns about an APK file, choose *Download anyway*.
2. Open the downloaded file. Android asks to allow *install unknown apps* for the browser or the file manager:
   allow it (only for this install if you like). Play Protect may warn that the app is unknown: *Install anyway*.
3. Open **H4 CNC** and type the address shown on the H4 display (e.g. `192.168.1.50`). The phone must be on the
   same WiFi as the H4. Wrong address later: the *H4 address* button in the page's Gamepad box.

Updating: see *Signing* below (without the signing secret, uninstall the old app first).

## Demo (no H4 needed)

In the first dialog (or *H4 address* later) press **Demo (no H4)**: the app shows the H4 page with a simulated
machine. Everything can be tried - files, Draw, Start / Pause / STOP, feed override, Test, Diagnostics, settings,
the gamepad - and nothing moves. *H4 address* → *Open* goes back to the real machine. The same demo opens in any
browser with `?demo` after the page address.

## Updates

At each start (at most every 10 minutes) the app looks at this repository's Releases (public, no account or key needed). When a
newer *H4 CNC* is there, it says so and offers **Download**; Android then installs it.

## H4 firmware updates

When the app opens the H4 page it compares the H4's firmware with the newest `firmware-v...` release here (at most
every 6 hours; the page's *Check for a firmware update* button asks at once). A newer one is offered: **Install**
downloads it and sends it to the H4 over WiFi. Nothing may run; the H4 restarts and keeps its machine settings, and
it keeps its present firmware if anything goes wrong on the way. An H4 with a firmware older than this feature is
never offered an update: install the new firmware once over USB.

## Gamepad

Pair it once: Android Settings > Bluetooth > pair new device, hold **Share + PS** on the gamepad until the
light flashes quickly. Then the PS button alone connects it. The layout is the one of the LinuxCNC `gamepad.hal`:

| Button | Action |
|---|---|
| **L1 (hold)** | jog enable (deadman): nothing moves without it; releasing it stops at once |
| R1 (hold) | fast speed |
| Left stick left/right | Z (right = +Z) |
| Right stick up/down | X (down = +X) |
| L2 / R2 | C+ / C- (press further = faster) |
| L1 + Share | Home All |
| L1 + D-pad down | touch off Z: the tool's current point becomes Z0 of the active work offset |
| Options | **STOP** |
| PS | unlock (`$X`, moves before Home All) |
| D-pad up / down (without L1) | feed override +10 / -10 % (10-200 %) |
| D-pad left | feed override back to 100 % |

Speeds and reversed axes: the page's *Gamepad* box. The gamepad's state and any refusal of the H4 are shown
there; a refusal also rumbles the gamepad.

Safety: the app sends the wanted speeds every 0.1 s; the H4 turns them into short jogs (0.05 s pieces, never
more than 0.2 s queued) and stops by itself 0.3 s after the last request: when L1 or the stick is released, the
app goes to the background, the screen turns off, the gamepad or the WiFi is lost. The screen stays on while the
app is open. **This is not an emergency stop**: keep the power-supply switch within reach.

## Build

GitHub builds the APK (workflow `.github/workflows/android.yml`) on every change of this folder and publishes it
as a release from `main`. Locally: Android SDK, JDK 17, Gradle 8.7: `gradle -p android assembleRelease`.

Signing: Android installs a newer APK over the old one only when both are signed with the same key. Without a key
the build signs with a temporary debug key, so before installing a newer release uninstall the app (only the saved
address is lost). For updates in place, make one key once and give it to GitHub as two repository secrets
(Settings > Secrets and variables > Actions): `ANDROID_KEYSTORE_BASE64` (the keystore file in base64) and
`ANDROID_KEYSTORE_PASSWORD`, key alias `h4cnc`. The key never goes into the repository.

Copyright (c) 2026 Marioskiv. MIT licence.
