// PS4 / PS5 (or any Android) gamepad -> the H4's /jog, with the layout of the LinuxCNC gamepad.hal.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
//   L1 (hold)          jog enable (deadman): nothing moves without it; releasing it stops at once
//   R1 (hold)          fast speed
//   Left stick <->     Z (right = +Z)
//   Right stick up/dn  X (down = +X)
//   L2 / R2            C+ / C- (press further = faster)
//   L1 + Share         Home All
//   L1 + D-pad down    touch off Z (G10 L20 P0 Z0)
//   Options            STOP
//   PS                 unlock ($X)
//   D-pad up / down    feed override +10 / -10 % (without L1); D-pad left: 100 %
//
// The wanted speeds go to http://<H4>/jog every 100 ms while moving; the H4 turns them into short jogs and stops
// by itself 0.3 s after the last request. Releasing sends speed 0 three times. All requests go out one after the
// other on one thread, so a stop can never overtake an older move request.
package com.marioskiv.h4cnc;

import android.os.Handler;
import android.os.Looper;
import android.os.Vibrator;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.json.JSONObject;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.HashSet;
import java.util.Locale;
import java.util.Scanner;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

class GamepadJog {
    private static final long TICK_MS = 20;
    private static final long SEND_MS = 100;
    private static final float DEAD_Z = 0.35f, DEAD_X = 0.45f, DEAD_TRIGGER = 0.05f;

    private final MainActivity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService net = Executors.newSingleThreadExecutor();
    private volatile String address;

    // settings from the page's Gamepad box
    private volatile double speed = 400, fast = 1000, cFactor = 6;
    private volatile boolean revX, revZ, revC;

    // gamepad state (main thread)
    private int deviceId = -1;
    private String deviceName = "";
    private final Set<Integer> down = new HashSet<>();
    private float lx, ry, l2, r2;
    private boolean hatDown, hatUp, hatLeft, legacyTriggers;

    // sending (main thread)
    private boolean running, moving;
    private int stops, jogsInFlight;
    private long lastSendMs, lastStateMs;
    private String error = "", lastState = "";

    GamepadJog(MainActivity activity) {
        this.activity = activity;
    }

    void setAddress(String a) {
        address = a;
    }

    void setSettings(String json) {
        try {
            JSONObject o = new JSONObject(json);
            speed = o.optDouble("n", 400);
            fast = o.optDouble("f", 1000);
            cFactor = o.optDouble("c", 6);
            revX = o.optBoolean("ix", false);
            revZ = o.optBoolean("iz", false);
            revC = o.optBoolean("ic", false);
        } catch (Exception ignored) {
        }
    }

    private static boolean fromPad(int source) {
        return (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    private void seen(InputDevice d) {
        if (d == null) return;
        if (d.getId() != deviceId) {
            deviceId = d.getId();
            deviceName = d.getName();
            legacyTriggers = d.getMotionRange(MotionEvent.AXIS_LTRIGGER) == null
                    && d.getMotionRange(MotionEvent.AXIS_BRAKE) == null
                    && d.getMotionRange(MotionEvent.AXIS_RX) != null;
        }
    }

    boolean handleKey(KeyEvent e) {
        if (!fromPad(e.getSource())) return false;
        seen(e.getDevice());
        int k = e.getKeyCode();
        if (e.getAction() == KeyEvent.ACTION_DOWN) {
            if (e.getRepeatCount() == 0 && down.add(k)) pressed(k);
        } else if (e.getAction() == KeyEvent.ACTION_UP) {
            down.remove(k);
        }
        return true;  // gamepad keys never reach the page (B would be "back")
    }

    boolean handleMotion(MotionEvent e) {
        if ((e.getSource() & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK
                || e.getAction() != MotionEvent.ACTION_MOVE) return false;
        seen(e.getDevice());
        lx = e.getAxisValue(MotionEvent.AXIS_X);
        ry = e.getAxisValue(MotionEvent.AXIS_RZ);
        if (legacyTriggers) {
            // older Android versions give the DualShock 4 triggers as RX / RY, -1 released .. 1 pressed
            l2 = (e.getAxisValue(MotionEvent.AXIS_RX) + 1) / 2;
            r2 = (e.getAxisValue(MotionEvent.AXIS_RY) + 1) / 2;
        } else {
            l2 = Math.max(e.getAxisValue(MotionEvent.AXIS_LTRIGGER), e.getAxisValue(MotionEvent.AXIS_BRAKE));
            r2 = Math.max(e.getAxisValue(MotionEvent.AXIS_RTRIGGER), e.getAxisValue(MotionEvent.AXIS_GAS));
        }
        float hy = e.getAxisValue(MotionEvent.AXIS_HAT_Y), hx = e.getAxisValue(MotionEvent.AXIS_HAT_X);
        boolean hd = hy > 0.5f, hu = hy < -0.5f, hl = hx < -0.5f;
        if (hd && !hatDown) pressed(KeyEvent.KEYCODE_DPAD_DOWN);
        if (hu && !hatUp) pressed(KeyEvent.KEYCODE_DPAD_UP);
        if (hl && !hatLeft) pressed(KeyEvent.KEYCODE_DPAD_LEFT);
        hatDown = hd;
        hatUp = hu;
        hatLeft = hl;
        return true;
    }

    private boolean l1() {
        return down.contains(KeyEvent.KEYCODE_BUTTON_L1);
    }

    // A button just pressed.
    private void pressed(int k) {
        if (k == KeyEvent.KEYCODE_BUTTON_START) {
            post("/cmd?do=stop", false);
        } else if (k == KeyEvent.KEYCODE_BUTTON_MODE) {
            post("/mdi?line=" + enc("$X"), false);
        } else if (k == KeyEvent.KEYCODE_BUTTON_SELECT && l1()) {
            post("/cmd?do=home", false);
        } else if (k == KeyEvent.KEYCODE_DPAD_DOWN && l1()) {
            post("/mdi?line=" + enc("G10 L20 P0 Z0"), false);
        } else if (k == KeyEvent.KEYCODE_DPAD_UP && !l1()) {
            post("/cmd?do=ovr&code=145", false);  // feed +10 %
        } else if (k == KeyEvent.KEYCODE_DPAD_DOWN) {
            post("/cmd?do=ovr&code=146", false);  // feed -10 %
        } else if (k == KeyEvent.KEYCODE_DPAD_LEFT && !l1()) {
            post("/cmd?do=ovr&code=144", false);  // feed 100 %
        }
    }

    private static float dz(float v, float d) {
        float a = Math.abs(v);
        if (a <= d) return 0;
        return Math.signum(v) * (a - d) / (1 - d);
    }

    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            if (!running) return;
            step();
            handler.postDelayed(this, TICK_MS);
        }
    };

    private void step() {
        long now = System.currentTimeMillis();
        if (deviceId != -1 && InputDevice.getDevice(deviceId) == null) {
            // the gamepad is gone: nothing is held any more
            deviceId = -1;
            down.clear();
            lx = ry = l2 = r2 = 0;
            hatDown = hatUp = hatLeft = false;
        }
        double sp = down.contains(KeyEvent.KEYCODE_BUTTON_R1) ? fast : speed;
        double vz = (revZ ? -1 : 1) * dz(lx, DEAD_Z) * sp;
        double vx = (revX ? -1 : 1) * dz(ry, DEAD_X) * sp;
        float left = l2, right = r2;
        if (legacyTriggers) {  // RX / RY read 0 (= half pressed) until first touched: trust them only with the button
            if (!down.contains(KeyEvent.KEYCODE_BUTTON_L2)) left = 0;
            if (!down.contains(KeyEvent.KEYCODE_BUTTON_R2)) right = 0;
        }
        double vc = (revC ? -1 : 1) * (dz(left, DEAD_TRIGGER) - dz(right, DEAD_TRIGGER)) * sp * cFactor;
        boolean want = l1() && (vx != 0 || vz != 0 || vc != 0);
        if (want) {
            if (now - lastSendMs >= SEND_MS && jogsInFlight == 0) {
                moving = true;
                stops = 3;
                jog(vx, vz, vc, now);
            }
        } else if (stops > 0 && now - lastSendMs >= SEND_MS) {
            jog(0, 0, 0, now);
            stops = moving ? 2 : stops - 1;
            moving = false;
        }
        if (now - lastStateMs >= 250) {
            lastStateMs = now;
            String s = deviceId == -1 ? "No gamepad: connect it by Bluetooth (Share + PS to pair), then press a button."
                    : deviceName + (want ? " · moving" : l1() ? " · L1 held" : " · hold L1 to move")
                    + (error.isEmpty() ? "" : " · " + error);
            if (!s.equals(lastState)) {
                lastState = s;
                activity.showGamepadState(s);
            }
        }
    }

    private void jog(double x, double z, double c, long now) {
        lastSendMs = now;
        post(String.format(Locale.US, "/jog?x=%.1f&z=%.1f&c=%.1f", x, z, c), true);
    }

    private static String enc(String s) {
        try {
            return URLEncoder.encode(s, "UTF-8");
        } catch (Exception e) {
            return s;
        }
    }

    // POST to the H4 on the network thread; the answer comes back on the main thread.
    private void post(final String path, final boolean isJog) {
        final String a = address;
        if (a == null) return;
        if (isJog) jogsInFlight++;
        net.execute(() -> {
            int code = -1;
            String body = "";
            HttpURLConnection c = null;
            try {
                c = (HttpURLConnection) new URL("http://" + a + path).openConnection();
                c.setRequestMethod("POST");
                c.setConnectTimeout(isJog ? 400 : 2000);
                c.setReadTimeout(isJog ? 600 : 3000);
                c.setDoOutput(true);
                OutputStream o = c.getOutputStream();
                o.close();
                code = c.getResponseCode();
                InputStream in = code >= 400 ? c.getErrorStream() : c.getInputStream();
                if (in != null) {
                    Scanner sc = new Scanner(in, "UTF-8").useDelimiter("\\A");
                    body = sc.hasNext() ? sc.next() : "";
                    in.close();
                }
            } catch (Exception e) {
                body = "No answer from the H4";
            } finally {
                if (c != null) c.disconnect();
            }
            final int fc = code;
            final String fb = body.trim();
            handler.post(() -> answer(isJog, fc, fb));
        });
    }

    private void answer(boolean isJog, int code, String body) {
        if (isJog) jogsInFlight = Math.max(0, jogsInFlight - 1);
        if (code >= 200 && code < 300) {
            if (isJog) error = "";
            return;
        }
        String e = body.isEmpty() ? "Refused (" + code + ")" : body;
        if (!e.equals(error) || !isJog) rumble();
        error = e;
        lastState = "";  // show it now
    }

    private void rumble() {
        try {
            InputDevice d = deviceId == -1 ? null : InputDevice.getDevice(deviceId);
            Vibrator v = d != null ? d.getVibrator() : null;
            if (v != null && v.hasVibrator()) v.vibrate(200);
        } catch (Exception ignored) {
        }
    }

    void resume() {
        if (running) return;
        running = true;
        handler.post(tick);
    }

    // The app goes to the background or closes: stop any jog at once and forget what was held.
    void pause() {
        running = false;
        handler.removeCallbacks(tick);
        down.clear();
        lx = ry = l2 = r2 = 0;
        if (moving || stops > 0) {
            post("/jog?x=0&z=0&c=0", true);
            moving = false;
            stops = 0;
        }
    }
}
