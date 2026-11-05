// NanoEls H4 CNC app: why did the app close last time? Every 10 s, while it runs, the app notes that it is alive,
// whether it is on screen, the tablet's free memory and Android's last "free memory" request (onTrimMemory). An
// error in the app's code is noted with its place. If the app ends without the user closing it, the next start says
// what was seen last: an error (with where), the screen's browser part (WebView) gone, or Android closing the app
// (little free memory: memory; plenty: the maker's power saving, e.g. Huawei's). Kept only in this tablet.
// Every run is also written to a list (the last 30): when it started, how it ended. The page shows it (Settings,
// App log), so a closing that was not reported at the next start can still be looked up.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

final class ExitWatch {
    private static final String PREFS = "h4exit", LOG = "h4exitlog";
    private static final int LOG_MAX = 30;
    private static final long EVERY_MS = 10000;
    private static final Handler handler = new Handler(Looper.getMainLooper());
    private static Context app;
    private static boolean onScreen;
    private static int trimLevel = -1;
    private static long trimAt;
    private static boolean handlerSet;

    private ExitWatch() {
    }

    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    // At the app's start: the report of the last run (if it ended by itself), then the watch of this run.
    static void start(MainActivity activity) {
        app = activity.getApplicationContext();
        logLast(activity);
        report(activity);
        SharedPreferences.Editor e = prefs(app).edit().clear();
        e.putBoolean("running", true).putString("version", BuildInfo.version(activity))
                .putLong("startAt", System.currentTimeMillis()).commit();
        if (!handlerSet) setCrashHandler();
        handler.removeCallbacks(beat);
        handler.post(beat);
    }

    // An error that would stop the app is written down first (then Android stops the app as usual).
    private static void setCrashHandler() {
        handlerSet = true;
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((t, ex) -> {
            try {
                StringWriter w = new StringWriter();
                ex.printStackTrace(new PrintWriter(w));
                String s = w.toString();
                prefs(app).edit().putString("crash", s.length() > 1500 ? s.substring(0, 1500) : s)
                        .putLong("crashAt", System.currentTimeMillis()).commit();
            } catch (Throwable ignored) {
            }
            if (previous != null) previous.uncaughtException(t, ex);
        });
    }

    private static final Runnable beat = new Runnable() {
        @Override
        public void run() {
            note();
            handler.postDelayed(this, EVERY_MS);
        }
    };

    private static void note() {
        if (app == null) return;
        ActivityManager am = (ActivityManager) app.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
        if (am != null) am.getMemoryInfo(mi);
        Runtime rt = Runtime.getRuntime();
        prefs(app).edit().putLong("at", System.currentTimeMillis()).putBoolean("screen", onScreen)
                .putLong("availMb", mi.availMem >> 20).putLong("totalMb", mi.totalMem >> 20)
                .putLong("thresholdMb", mi.threshold >> 20).putBoolean("low", mi.lowMemory)
                .putLong("heapMb", (rt.totalMemory() - rt.freeMemory()) >> 20)
                .putInt("trim", trimLevel).putLong("trimAt", trimAt).apply();
    }

    static void visible(boolean v) {
        onScreen = v;
        note();
    }

    // Android asks the app to free memory (MainActivity.onTrimMemory).
    static void trim(int level) {
        trimLevel = level;
        trimAt = System.currentTimeMillis();
        note();
    }

    // The screen's browser part ended (Android 8 and newer tell it; the app then opens again).
    static void rendererGone(boolean crashed) {
        if (app != null) prefs(app).edit().putString("renderer", crashed ? "crash" : "memory").commit();
    }

    // The user closed the app (back key / swipe that finishes it): nothing to report next time.
    static void closed() {
        handler.removeCallbacks(beat);
        if (app != null) prefs(app).edit().putBoolean("running", false).putLong("at", System.currentTimeMillis()).commit();
    }

    // How the last run ended, into the list (before this run's watch starts over).
    private static void logLast(Context c) {
        SharedPreferences p = prefs(c);
        long start = p.getLong("startAt", 0);
        if (start == 0) return;  // first run with this list
        String kind;
        boolean running = p.getBoolean("running", false);
        boolean install = c.getSharedPreferences("h4", Context.MODE_PRIVATE).getString("installTried", null) != null;
        if (!running) kind = "user";
        else if (p.getString("crash", null) != null) kind = "crash";
        else if (p.getString("renderer", null) != null) kind = "renderer";
        else if (install) kind = "install";
        else {
            long avail = p.getLong("availMb", -1), thr = p.getLong("thresholdMb", -1);
            kind = (p.getBoolean("screen", false) ? "killedScreen" : "killedBg") + (memory(p) ? "Mem" : "");
        }
        try {
            SharedPreferences lp = c.getSharedPreferences(LOG, Context.MODE_PRIVATE);
            JSONArray a = new JSONArray(lp.getString("list", "[]")), b = new JSONArray();
            JSONObject o = new JSONObject();
            o.put("s", start).put("e", p.getLong(p.getString("crash", null) != null ? "crashAt" : "at", start))
                    .put("v", p.getString("version", "?")).put("k", kind).put("m", p.getLong("availMb", -1))
                    .put("t", p.getInt("trim", -1)).put("h", p.getLong("heapMb", -1));
            b.put(o);
            for (int i = 0; i < a.length() && b.length() < LOG_MAX; i++) b.put(a.get(i));
            lp.edit().putString("list", b.toString()).commit();
        } catch (Exception ignored) {
        }
    }

    // The list for the page (Settings, App log), newest first, in the screen's language.
    static String history(Context c) {
        StringBuilder sb = new StringBuilder();
        try {
            JSONArray a = new JSONArray(c.getSharedPreferences(LOG, Context.MODE_PRIVATE).getString("list", "[]"));
            SimpleDateFormat f = new SimpleDateFormat("dd/MM HH:mm", Locale.ROOT);
            for (int i = 0; i < a.length(); i++) {
                JSONObject o = a.getJSONObject(i);
                long s = o.optLong("s"), e = o.optLong("e");
                String k = o.optString("k");
                String how;
                switch (k) {
                    case "user": how = Tr.t(c, "closed by you (Back)", "το έκλεισες εσύ (Πίσω)"); break;
                    case "install": how = Tr.t(c, "closed by Android to install an update", "το έκλεισε το Android για να εγκαταστήσει ενημέρωση"); break;
                    case "crash": how = Tr.t(c, "ERROR in the app", "ΣΦΑΛΜΑ του app"); break;
                    case "renderer": how = Tr.t(c, "the screen part (WebView) ended", "σταμάτησε το κομμάτι της οθόνης (WebView)"); break;
                    case "killedScreen": how = Tr.t(c, "closed by Android WHILE ON SCREEN (power saving?)", "το έκλεισε το Android ΕΝΩ ΗΤΑΝ ΣΤΗΝ ΟΘΟΝΗ (εξοικονόμηση ενέργειας;)"); break;
                    case "killedScreenMem": how = Tr.t(c, "closed by Android WHILE ON SCREEN, little free memory", "το έκλεισε το Android ΕΝΩ ΗΤΑΝ ΣΤΗΝ ΟΘΟΝΗ, λίγη ελεύθερη μνήμη"); break;
                    case "killedBg": case "killedBgMem": how = Tr.t(c, "closed by Android in the background (normal)", "το έκλεισε το Android στο παρασκήνιο (κανονικό)"); break;
                    case "ev": how = Tr.greek(c) ? o.optString("el") : o.optString("en"); break;
                    default: how = k;
                }
                if ("ev".equals(k)) {  // a step of an app update
                    sb.append(f.format(new Date(s))).append(" · app ").append(o.optString("v")).append(" · ").append(how).append('\n');
                    continue;
                }
                sb.append(f.format(new Date(s))).append(" → ").append(f.format(new Date(e))).append(" · app ").append(o.optString("v"))
                        .append(" · ").append(how);
                long m = o.optLong("m", -1);
                if (m >= 0 && !"user".equals(k)) sb.append(Tr.t(c, " · free memory ", " · ελεύθερη μνήμη ")).append(m).append(" MB");
                // what Android last said to the app about memory (onTrimMemory level) and the app's own memory
                int t = o.optInt("t", -1);
                if (t >= 0 && !"user".equals(k)) sb.append(" · trim ").append(t);
                long h = o.optLong("h", -1);
                if (h >= 0 && !"user".equals(k)) sb.append(" · app ").append(h).append(" MB");
                sb.append('\n');
            }
        } catch (Exception ignored) {
        }
        return sb.length() == 0 ? Tr.t(c, "Nothing written yet: the list starts with app 1.37.", "Τίποτα ακόμα: η λίστα ξεκινά με το app 1.37.") : sb.toString();
    }

    // Little memory really: Android said so (lowMemory), or it asked the running app to free memory (onTrimMemory
    // 10 RUNNING_LOW, 15 RUNNING_CRITICAL) or the hidden app (60 MODERATE, 80 COMPLETE), or the free memory was at
    // Android's limit. Level 20 (UI_HIDDEN) only means the app went behind another screen: not memory (app 1.40;
    // 1.37-1.39 counted it as memory).
    private static boolean memory(SharedPreferences p) {
        int t = p.getInt("trim", -1);
        long avail = p.getLong("availMb", -1), thr = p.getLong("thresholdMb", -1);
        return p.getBoolean("low", false) || t == 10 || t == 15 || t >= 60 || (avail >= 0 && thr >= 0 && avail < thr + 64);
    }

    // A step of an app update (offered, handed to Android, answered, installed) into the same list.
    static void event(Context c, String en, String el) {
        try {
            SharedPreferences lp = c.getSharedPreferences(LOG, Context.MODE_PRIVATE);
            JSONArray a = new JSONArray(lp.getString("list", "[]")), b = new JSONArray();
            JSONObject o = new JSONObject();
            o.put("s", System.currentTimeMillis()).put("v", BuildInfo.version(c)).put("k", "ev").put("en", en).put("el", el);
            b.put(o);
            for (int i = 0; i < a.length() && b.length() < LOG_MAX; i++) b.put(a.get(i));
            lp.edit().putString("list", b.toString()).commit();
        } catch (Exception ignored) {
        }
    }

    static void clearHistory(Context c) {
        c.getSharedPreferences(LOG, Context.MODE_PRIVATE).edit().clear().commit();
    }

    private static void report(MainActivity a) {
        SharedPreferences p = prefs(a);
        if (!p.getBoolean("running", false)) return;
        String crash = p.getString("crash", null), renderer = p.getString("renderer", null);
        long at = p.getLong(crash != null ? "crashAt" : "at", 0);
        if (at == 0) return;
        // closed by Android while in the background (e.g. swiped away, another app on screen): normal, not told
        if (crash == null && renderer == null && !p.getBoolean("screen", false)) return;
        String when = new SimpleDateFormat("dd/MM HH:mm", Locale.ROOT).format(new Date(at));
        long avail = p.getLong("availMb", -1), total = p.getLong("totalMb", -1), thr = p.getLong("thresholdMb", -1);
        boolean low = p.getBoolean("low", false), screen = p.getBoolean("screen", false);
        int trim = p.getInt("trim", -1);
        long trimAgo = p.getLong("trimAt", 0) > 0 ? (at - p.getLong("trimAt", 0)) / 1000 : -1;
        String mem = Tr.t(a, "Free memory then: " + avail + " of " + total + " MB" + (low ? " (low)" : "") + ", the app's own "
                + p.getLong("heapMb", -1) + " MB.", "Ελεύθερη μνήμη τότε: " + avail + " από " + total + " MB" + (low ? " (χαμηλή)" : "")
                + ", του ίδιου του app " + p.getLong("heapMb", -1) + " MB.")
                + (trim >= 0 ? Tr.t(a, " Android had asked to free memory (level " + trim + ") " + trimAgo + " s before.",
                " Το Android είχε ζητήσει να ελευθερωθεί μνήμη (επίπεδο " + trim + ") " + trimAgo + " δευτ. πριν.") : "");
        String text;
        if (crash != null) {
            text = Tr.t(a, "H4 CNC stopped on " + when + " because of an error in the app:\n\n", "Το H4 CNC σταμάτησε στις " + when + " από σφάλμα του app:\n\n") + crash;
        } else if (renderer != null) {
            text = Tr.t(a, "On " + when + " the screen's browser part (WebView) ended (" + renderer + "). " + mem,
                    "Στις " + when + " σταμάτησε το κομμάτι του app που δείχνει την οθόνη (WebView, " + renderer + "). " + mem);
        } else {
            boolean memory = memory(p);
            text = Tr.t(a, "Android closed H4 CNC (last sign of life " + when + ", " + (screen ? "on screen" : "in the background") + "). " + mem + "\n\n"
                    + (memory ? "Likely cause: not enough memory in the tablet. Close other apps (the square key)."
                    : "Likely cause: the maker's power saving (Huawei: Phone Manager, Battery, Protected apps: H4 CNC on)."),
                    "Το Android έκλεισε το H4 CNC (τελευταίο σημάδι ζωής " + when + ", " + (screen ? "στην οθόνη" : "στο παρασκήνιο") + "). " + mem + "\n\n"
                    + (memory ? "Πιθανή αιτία: λίγη μνήμη στο tablet. Κλείσε τα άλλα apps (το τετράγωνο κουμπί)."
                    : "Πιθανή αιτία: η εξοικονόμηση ενέργειας του κατασκευαστή (Huawei: Phone Manager, Battery, Protected apps: H4 CNC ενεργό)."));
        }
        final String msg = text + Tr.t(a, "\n\n(app " + p.getString("version", "?") + ")", "\n\n(app " + p.getString("version", "?") + ")");
        a.web().postDelayed(() -> {
            if (a.isFinishing()) return;
            new AlertDialog.Builder(a).setTitle(Tr.t(a, "H4 CNC closed last time", "Το H4 CNC έκλεισε την προηγούμενη φορά"))
                    .setMessage(msg).setPositiveButton("OK", null).show();
        }, 1500);
    }
}
