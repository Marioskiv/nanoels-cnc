// NanoEls H4 CNC app: why did the app close last time? Every 10 s, while it runs, the app notes that it is alive,
// whether it is on screen, the tablet's free memory and Android's last "free memory" request (onTrimMemory). An
// error in the app's code is noted with its place. If the app ends without the user closing it, the next start says
// what was seen last: an error (with where), the screen's browser part (WebView) gone, or Android closing the app
// (little free memory: memory; plenty: the maker's power saving, e.g. Huawei's). Kept only in this tablet.
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

import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

final class ExitWatch {
    private static final String PREFS = "h4exit";
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
        report(activity);
        SharedPreferences.Editor e = prefs(app).edit().clear();
        e.putBoolean("running", true).putString("version", BuildInfo.version(activity)).commit();
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
        if (app != null) prefs(app).edit().putBoolean("running", false).commit();
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
            boolean memory = low || trim >= 10 || (avail >= 0 && thr >= 0 && avail < thr + 64);
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
