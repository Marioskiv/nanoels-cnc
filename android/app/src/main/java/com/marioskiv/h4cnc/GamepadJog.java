// PS4 / PS5 (or any Android) gamepad -> the H4's /jog. What each button does comes from the page (Settings,
// Gamepad): an action alone and one while the safety button is held. The default is the layout of the LinuxCNC
// gamepad.hal:
//   L1 (hold)          safety (deadman): nothing moves without it; releasing it stops at once
//   R1 (hold)          fast speed
//   Left stick <->     Z (right = +Z)
//   Right stick up/dn  X (down = +X)
//   L2 / R2            C+ / C- (press further = faster)
//   L1 + Share         Home All
//   L1 + D-pad down    touch off Z (G10 L20 P0 Z0)
//   Options            STOP
//   PS                 unlock ($X)
//   D-pad up / down    feed override +10 / -10 %; D-pad left: 100 %
// The page refuses a map without a STOP or a safety button; here a map without a safety button is not used either.
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

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

class GamepadJog {
    private static final long TICK_MS = 20;
    private static final long SEND_MS = 100;
    // dead zones (defaults; the page's Gamepad box sets them, in %): left stick, right stick, L2 / R2
    private static final float DEAD_L = 0.35f, DEAD_R = 0.45f, DEAD_TRIGGER = 0.05f;
    private volatile float deadL = DEAD_L, deadR = DEAD_R, deadT = DEAD_TRIGGER;

    private final MainActivity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService net = Executors.newSingleThreadExecutor();
    private volatile String address;
    private volatile boolean demo;

    // settings from the page's Gamepad box
    private volatile double speed = 400, fast = 1000, cFactor = 6;
    private volatile boolean revX, revZ, revC;
    private volatile String stickL = "z", stickR = "x";
    // button name -> {alone, with the safety button held}
    private volatile Map<String, String[]> map = defaults();

    private static Map<String, String[]> defaults() {
        Map<String, String[]> m = new HashMap<>();
        m.put("l1", new String[]{"enable", ""});
        m.put("r1", new String[]{"fast", ""});
        m.put("l2", new String[]{"cp", ""});
        m.put("r2", new String[]{"cm", ""});
        m.put("options", new String[]{"stop", "stop"});
        m.put("ps", new String[]{"unlock", ""});
        m.put("share", new String[]{"", "home"});
        m.put("up", new String[]{"fup", ""});
        m.put("down", new String[]{"fdn", "z0"});
        m.put("left", new String[]{"f100", ""});
        return m;
    }

    // the page's names of the buttons
    private static String name(int k) {
        switch (k) {
            case KeyEvent.KEYCODE_BUTTON_A: return "cross";
            case KeyEvent.KEYCODE_BUTTON_B: return "circle";
            case KeyEvent.KEYCODE_BUTTON_X: return "square";
            case KeyEvent.KEYCODE_BUTTON_Y: return "triangle";
            case KeyEvent.KEYCODE_BUTTON_L1: return "l1";
            case KeyEvent.KEYCODE_BUTTON_R1: return "r1";
            case KeyEvent.KEYCODE_BUTTON_L2: return "l2";
            case KeyEvent.KEYCODE_BUTTON_R2: return "r2";
            case KeyEvent.KEYCODE_BUTTON_SELECT: return "share";
            case KeyEvent.KEYCODE_BUTTON_START: return "options";
            case KeyEvent.KEYCODE_BUTTON_MODE: return "ps";
            case KeyEvent.KEYCODE_BUTTON_THUMBL: return "l3";
            case KeyEvent.KEYCODE_BUTTON_THUMBR: return "r3";
            case KeyEvent.KEYCODE_DPAD_UP: return "up";
            case KeyEvent.KEYCODE_DPAD_DOWN: return "down";
            case KeyEvent.KEYCODE_DPAD_LEFT: return "left";
            case KeyEvent.KEYCODE_DPAD_RIGHT: return "right";
            default: return "";
        }
    }

    private static final int[] KEYS = {KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_X,
            KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_L2,
            KeyEvent.KEYCODE_BUTTON_R2, KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_START,
            KeyEvent.KEYCODE_BUTTON_MODE, KeyEvent.KEYCODE_BUTTON_THUMBL, KeyEvent.KEYCODE_BUTTON_THUMBR,
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT};

    // gamepad state (main thread)
    private int deviceId = -1;
    private String deviceName = "";
    private final Set<Integer> down = new HashSet<>();
    private float lx, ry, l2, r2;
    private boolean hatDown, hatUp, hatLeft, hatRight, legacyTriggers;

    // sending (main thread)
    private boolean running, moving;
    private int stops, jogsInFlight;
    private long lastSendMs, lastStateMs, lastAxesMs;
    private float[] shownAxes;
    private String error = "", lastState = "";

    GamepadJog(MainActivity activity) {
        this.activity = activity;
    }

    void setAddress(String a) {
        address = a;
    }

    void setDemo(boolean d) {
        demo = d;
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
            stickL = o.optString("sl", "z");
            stickR = o.optString("sr", "x");
            deadL = dead(o, "dl", DEAD_L);
            deadR = dead(o, "dr", DEAD_R);
            deadT = dead(o, "dt", DEAD_TRIGGER);
            JSONObject m = o.optJSONObject("map");
            if (m != null) {
                Map<String, String[]> n = new HashMap<>();
                boolean enable = false, stop = false;
                for (java.util.Iterator<String> it = m.keys(); it.hasNext(); ) {
                    String k = it.next();
                    JSONArray a = m.optJSONArray(k);
                    if (a == null) continue;
                    String[] v = {a.optString(0, ""), a.optString(1, "")};
                    n.put(k, v);
                    if ("enable".equals(v[0])) enable = true;
                    if ("stop".equals(v[0])) stop = true;
                }
                map = enable && stop ? n : defaults();  // never a map without the safety button or a STOP
            }
        } catch (Exception ignored) {
        }
    }

    // a dead zone in % from the page -> 0 .. 0.9
    private static float dead(JSONObject o, String k, float def) {
        double v = o.optDouble(k, Double.NaN);
        if (Double.isNaN(v)) return def;
        return (float) Math.min(0.9, Math.max(0, v / 100));
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
        boolean hd = hy > 0.5f, hu = hy < -0.5f, hl = hx < -0.5f, hr = hx > 0.5f;
        if (hd && !hatDown) pressed(KeyEvent.KEYCODE_DPAD_DOWN);
        if (hu && !hatUp) pressed(KeyEvent.KEYCODE_DPAD_UP);
        if (hl && !hatLeft) pressed(KeyEvent.KEYCODE_DPAD_LEFT);
        if (hr && !hatRight) pressed(KeyEvent.KEYCODE_DPAD_RIGHT);
        hatDown = hd;
        hatUp = hu;
        hatLeft = hl;
        hatRight = hr;
        return true;
    }

    private boolean isDown(int k) {
        if (k == KeyEvent.KEYCODE_DPAD_UP) return hatUp || down.contains(k);
        if (k == KeyEvent.KEYCODE_DPAD_DOWN) return hatDown || down.contains(k);
        if (k == KeyEvent.KEYCODE_DPAD_LEFT) return hatLeft || down.contains(k);
        if (k == KeyEvent.KEYCODE_DPAD_RIGHT) return hatRight || down.contains(k);
        return down.contains(k);
    }

    private String alone(int k) {
        String[] a = map.get(name(k));
        return a == null ? "" : a[0];
    }

    // a hold action (safety, fast, C+, C-): is one of its buttons held
    private boolean held(String action) {
        for (int k : KEYS) if (action.equals(alone(k)) && isDown(k)) return true;
        return false;
    }

    // how far a C+ / C- action is pressed: the triggers give 0 .. 1, the other buttons 0 or 1
    private float amount(String action, float left, float right) {
        float r = 0;
        for (int k : KEYS) {
            if (!action.equals(alone(k))) continue;
            float v = k == KeyEvent.KEYCODE_BUTTON_L2 ? left : k == KeyEvent.KEYCODE_BUTTON_R2 ? right : isDown(k) ? 1 : 0;
            r = Math.max(r, v);
        }
        return r;
    }

    private static boolean isHold(String a) {
        return "enable".equals(a) || "fast".equals(a) || "cp".equals(a) || "cm".equals(a);
    }

    // A button just pressed: its action with the safety button held (when it has one), else its own one.
    private void pressed(int k) {
        String[] m = map.get(name(k));
        if (m == null) return;
        boolean en = held("enable");
        String a = en && !m[1].isEmpty() ? m[1] : isHold(m[0]) ? "" : m[0];
        boolean needsSafety = "resume".equals(a) || "home".equals(a) || "z0".equals(a);
        if (needsSafety && !en) return;
        switch (a) {
            case "stop": post("/cmd?do=stop", false); break;
            case "pause": post("/cmd?do=pause", false); break;
            case "resume": post("/cmd?do=resume", false); break;
            case "home": post("/cmd?do=home", false); break;
            case "z0": post("/mdi?line=" + enc("G10 L20 P0 Z0"), false); break;
            case "unlock": post("/mdi?line=" + enc("$X"), false); break;
            case "fup": post("/cmd?do=ovr&code=145", false); break;  // feed +10 %
            case "fdn": post("/cmd?do=ovr&code=146", false); break;  // feed -10 %
            case "f100": post("/cmd?do=ovr&code=144", false); break;  // feed 100 %
            default: break;
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
            hatDown = hatUp = hatLeft = hatRight = false;
        }
        double sp = held("fast") ? fast : speed;
        double sl = dz(lx, deadL), sr = dz(ry, deadR);
        double vz = (revZ ? -1 : 1) * ("z".equals(stickL) ? sl : "z".equals(stickR) ? sr : 0) * sp;
        double vx = (revX ? -1 : 1) * ("x".equals(stickL) ? sl : "x".equals(stickR) ? sr : 0) * sp;
        float left = l2, right = r2;
        if (legacyTriggers) {  // RX / RY read 0 (= half pressed) until first touched: trust them only with the button
            if (!down.contains(KeyEvent.KEYCODE_BUTTON_L2)) left = 0;
            if (!down.contains(KeyEvent.KEYCODE_BUTTON_R2)) right = 0;
        }
        double vc = (revC ? -1 : 1) * (dz(amount("cp", left, right), deadT) - dz(amount("cm", left, right), deadT)) * sp * cFactor;
        showAxes(now, left, right);
        boolean en = held("enable");
        boolean want = en && (vx != 0 || vz != 0 || vc != 0);
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
                    : deviceName + (want ? " · moving" : en ? " · safety held" : " · hold the safety button to move")
                    + (error.isEmpty() ? "" : " · " + error);
            if (!s.equals(lastState)) {
                lastState = s;
                activity.showGamepadState(s);
            }
        }
    }

    // the page's indicator of the sticks and triggers: on a change, at most every 100 ms, and once a second anyway
    private void showAxes(long now, float left, float right) {
        float[] a = deviceId == -1 ? null : new float[]{lx, ry, left, right};
        boolean changed = (a == null) != (shownAxes == null);
        if (!changed && a != null) for (int i = 0; i < 4; i++) if (Math.abs(a[i] - shownAxes[i]) > 0.005f) changed = true;
        if (now - lastAxesMs < (changed ? 100 : 1000)) return;
        lastAxesMs = now;
        shownAxes = a;
        activity.showGamepadAxes(a == null ? "" : String.format(Locale.US, "%.3f,%.3f,%.3f,%.3f", a[0], a[1], a[2], a[3]));
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
        if (demo) {  // the simulated machine of the page answers
            activity.demoRequest(path);
            return;
        }
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
        hatDown = hatUp = hatLeft = hatRight = false;
        if (moving || stops > 0) {
            post("/jog?x=0&z=0&c=0", true);
            moving = false;
            stops = 0;
        }
    }
}
