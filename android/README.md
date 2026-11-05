# H4 CNC — Android app

A small app for the NanoEls H4 CNC fork: it holds **the H4 screen** (files, Start / Pause / STOP, positions, Home
All, MDI, the lathe screen, settings, firmware update) and reads a **PS4 / PS5 gamepad**
through Android itself, so no browser setting is needed. Android 5.0 and newer.

Since app 1.18 the screen is part of the app (`app/src/main/assets/h4/index.html`), not of the firmware: the app
answers the H4's page address with it, so it talks to the H4 exactly as before, and a new screen comes with an app
update, without flashing the H4. The firmware (1.16 and newer) keeps only a small emergency page for any browser.
With an older firmware the screen still works and asks once for a firmware update.

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
the gamepad - and nothing moves. *H4 address* → *Open* goes back to the real machine.

## Updates

The app's own dialogs (updates, H4 address) follow the language chosen on the screen (Settings, Appearance). The
versions in use are always shown under the Settings list (*App version ... · firmware ...*). While Android installs
an update it closes the app: that is normal, open it again afterwards.

5 seconds after it opens, each time (once more a minute later if the network was not up yet; *Later* keeps that version quiet for a day), and at once with *Settings, App update, Check for an app update*, the app looks at this repository's Releases (public, no account or key needed). If an install that was started did not go through (the app opens again at the old version), it says so and how to retry. When a
newer *H4 CNC* is there, it says so and offers **Install**: the app downloads it (with progress), checks that it is
this app, newer and signed with the same key, and hands it to Android in an install session (from app 1.32): Android
shows its own "Install?" screen and installs the app itself, without depending on the app's screen (with the older
way, Huawei's installer on Android 7 could stay at "Installing..."; it remains the fallback). From app 1.38 the app
keeps both ways: when an install does not go through (the app opens again at the old version) or its confirmation
is left, the next try uses the other way (as F-Droid, which below Android 12 opens Android's installer screen
directly, because makers break the session on some devices); the way that worked stays. On one Huawei tablet
with Android 7 the session's *Install* button did nothing. From app 1.39 the first try below Android 13 is Android's installer
screen (Ackpine and F-Droid advise against the session there, above all on Huawei and Xiaomi installers); from
Android 13 on, the session.

**Full screen (from app 1.40).** The app hides the tablet's status bar and its Back / Home / Recent keys (a
swipe from the top or bottom edge shows them for a moment); the page shows the clock, the battery and the WiFi
signal beside STOP. *Settings, Screen and jog, Full screen* switches it off.

**Network (from app 1.39).** While the app is on screen it keeps the WiFi at full speed (Android's WiFi power
saving answers 100-300 ms late now and then, and the gamepad's jog sends every 0.1 s to an H4 that stops 0.3 s after
the last request). On a WiFi without internet (e.g. one only for the H4) the app keeps its traffic on that WiFi, so a
tablet with a SIM card does not send it over mobile data; the update check then needs a WiFi with internet. Web
pages other than the H4's open in the browser, not inside the app. Android asks to confirm (Android 8 and
newer first ask once to allow H4 CNC to install apps). If the installed app was signed with another key (the early
test versions 1.0–1.5), Android cannot update it: the app says so and offers to uninstall it; then install
`H4CNC.apk` once by hand. Versions up to 1.10 handed the download to the browser or to a newer installer path:
install 1.11 once by hand from the Releases page.
Older Androids (up to 7.1) lack the root certificate of GitHub's download servers; from 1.15 the app keeps the
needed public roots itself (ISRG Root X1 / X2, DigiCert Global Root G2, with the full chain and host name checked),
so updates download there too. On such a device install 1.15 once by hand.

**Huawei tablets with Android 7 (EMUI 5):** turn **Auto-rotate** on (or hold the tablet upright) while Android
installs the app, the first time and at every update. With the rotation locked in landscape, Huawei's installer
stays at "Installing..." and never finishes (found on a MediaPad T3 7 and a MediaPad M3 Lite 10); with Auto-rotate
on it finishes also with the tablet mounted sideways on the machine.

## When the app closed by itself

From app 1.34 the app notes every 10 s that it runs, whether it is on screen and the tablet's free memory, and
writes down any error in its own code. If it ended while on screen without being closed by the user, the next start
says what was seen last: an error (with where in the code), the screen's browser part (WebView) gone, or Android
closing it - with little free memory: memory (close other apps); with plenty: the maker's power saving (Huawei:
Phone Manager, Battery, Protected apps: H4 CNC on). Kept only in the tablet; a photo of that message tells the cause.

From app 1.37 every run is also written to a list, the last 30: when it started, when and how it ended (closed by
you, closed for an update, an error, the screen part ended, closed by Android on screen or in the background, with the
free memory then). *Settings, App log* shows it and can clear it. **Back** on the first screen asks before closing
the app (a Back pressed by mistake used to close it at once, which looked like the app throwing you out).

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

Speeds, sticks, reversed axes and what each button does: *Settings, Gamepad* (the same map in the app and in Chrome; Defaults = the layout above). The gamepad's state and any refusal of the H4 are shown
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

Copyright (c) 2026 Marioskiv. The app's Java code: MIT licence; the H4 screen it carries
(`app/src/main/assets/h4/index.html`, from the firmware's page): GPL-3.0-or-later, like the firmware.
