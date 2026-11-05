// NanoEls H4 CNC app: the full app log. Everything the app does that helps to find a problem is written, one line
// each, with the time: start and end, on/off screen, the tablet's screen on/off, memory, WiFi, the H4 link, the
// screens opened, commands sent, messages shown, page errors, updates, and every time the app or its screen
// stopped answering (a freeze, with where the app was stuck). Kept in the app's own files on this tablet only
// (about 2 × 256 KB, the oldest part goes first). Settings, App log shows it and can copy or send it.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

final class AppLog {
    private static final String FILE = "applog.txt", OLD = "applog.1.txt";
    private static final long MAX = 256 * 1024;
    private static final ExecutorService io = Executors.newSingleThreadExecutor();
    private static File dir;
    private static Context ctx;
    private static String version = "?";

    private AppLog() {
    }

    static void init(Context c) {
        if (dir == null) {
            ctx = c.getApplicationContext();
            dir = ctx.getFilesDir();
            version = BuildInfo.version(c);
        }
    }

    // One line: "05/11 12:08:31.250 1.44 TAG text". Written on its own thread, so it never holds the screen up.
    static void add(String tag, String text) {
        if (dir == null) return;
        final String line = new SimpleDateFormat("dd/MM HH:mm:ss.SSS", Locale.ROOT).format(new Date()) + " " + version + " "
                + tag + " " + (text == null ? "" : text.replace('\n', ' ').replace('\r', ' ')) + "\n";
        try {
            io.execute(() -> write(line));
        } catch (Exception ignored) {
        }
    }

    // At once, on this thread: for an error that is about to stop the app.
    static void addNow(String tag, String text) {
        if (dir == null) return;
        write(new SimpleDateFormat("dd/MM HH:mm:ss.SSS", Locale.ROOT).format(new Date()) + " " + version + " " + tag + " "
                + (text == null ? "" : text.replace('\n', ' ').replace('\r', ' ')) + "\n");
    }

    static void add(Context c, String tag, String en, String el) {
        init(c);
        add(tag, Tr.t(c, en, el));
    }

    private static String t(String en, String el) {
        return ctx == null ? en : Tr.t(ctx, en, el);
    }

    private static synchronized void write(String line) {
        try {
            File f = new File(dir, FILE);
            if (f.length() > MAX) {
                File o = new File(dir, OLD);
                if (o.exists()) o.delete();
                f.renameTo(o);
                f = new File(dir, FILE);
            }
            try (Writer w = new OutputStreamWriter(new FileOutputStream(f, true), StandardCharsets.UTF_8)) {
                w.write(line);
            }
        } catch (Exception ignored) {
        }
    }

    // The last lines, newest first (after the lines still waiting to be written).
    static String read(int maxLines) {
        if (dir == null) return "";
        try {
            io.submit(() -> {
            }).get(2, TimeUnit.SECONDS);
        } catch (Exception ignored) {
        }
        List<String> all = new ArrayList<>();
        for (String n : new String[]{OLD, FILE}) {
            File f = new File(dir, n);
            if (!f.exists()) continue;
            try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(f), StandardCharsets.UTF_8))) {
                String s;
                while ((s = r.readLine()) != null) all.add(s);
            } catch (Exception ignored) {
            }
        }
        StringBuilder sb = new StringBuilder();
        for (int i = all.size() - 1, n = 0; i >= 0 && n < maxLines; i--, n++) sb.append(all.get(i)).append('\n');
        return sb.toString();
    }

    static void clear() {
        if (dir == null) return;
        try {
            io.submit(() -> {
                new File(dir, FILE).delete();
                new File(dir, OLD).delete();
            }).get(2, TimeUnit.SECONDS);
        } catch (Exception ignored) {
        }
    }

    // ------------------------------------------------------------------ freezes
    // A watch thread asks the app's main thread every half second to answer. While the app is on screen, no answer
    // for 2 s is written as a freeze, with the place in the code where the main thread was (the top of its stack),
    // and again when it answers, with how long it took. The page (the screen) says it is alive every second
    // (H4App.alive); no word from it for 4 s while the app itself answers is written as a freeze of the screen.
    private static final Handler main = new Handler(Looper.getMainLooper());
    private static volatile long mainAt, pageAt;
    private static volatile boolean visible, pageStarted;
    private static volatile String pageWhere = "";
    private static Thread watch;

    static void visible(boolean v) {
        long now = SystemClock.uptimeMillis();
        mainAt = now;
        pageAt = now;
        visible = v;
        if (v && watch == null) {
            watch = new Thread(AppLog::watchLoop, "h4-freeze-watch");
            watch.setDaemon(true);
            watch.start();
        }
    }

    // a page is loading: no word from it is expected until it runs
    static void pageLoading() {
        pageStarted = false;
    }

    static void pageAlive(String where) {
        pageAt = SystemClock.uptimeMillis();
        pageStarted = true;
        if (where != null) pageWhere = where;
    }

    private static void watchLoop() {
        boolean mainStuck = false, pageStuck = false;
        long mainSince = 0, pageSince = 0;
        final Runnable ping = () -> mainAt = SystemClock.uptimeMillis();
        while (true) {
            main.post(ping);
            try {
                Thread.sleep(visible ? 500 : 2000);
            } catch (InterruptedException e) {
                return;
            }
            long now = SystemClock.uptimeMillis();
            if (!visible) {
                if (mainStuck) add("FREEZE", t("app answered again (left the screen) after ", "το app απάντησε ξανά (βγήκε από την οθόνη) μετά από ") + secs(now - mainSince));
                if (pageStuck) add("FREEZE", t("screen answered again (left the screen) after ", "η οθόνη απάντησε ξανά (βγήκε από την οθόνη) μετά από ") + secs(now - pageSince));
                mainStuck = pageStuck = false;
                continue;
            }
            long mGap = now - mainAt, pGap = now - pageAt;
            if (!mainStuck && mGap > 2000) {
                mainStuck = true;
                mainSince = mainAt;
                add("FREEZE", t("APP NOT ANSWERING for 2 s, stuck at: ", "ΤΟ APP ΔΕΝ ΑΠΑΝΤΑ για 2 s, κόλλησε στο: ") + stack(Looper.getMainLooper().getThread()) + memory());
            } else if (mainStuck && mGap < 1000) {
                mainStuck = false;
                add("FREEZE", t("app answers again after ", "το app απαντά ξανά μετά από ") + secs(mainAt - mainSince));
            }
            if (!pageStuck && pageStarted && pGap > 4000 && !mainStuck) {
                pageStuck = true;
                pageSince = pageAt;
                add("FREEZE", t("SCREEN (page) NOT ANSWERING for 4 s, last on: ", "Η ΟΘΟΝΗ (σελίδα) ΔΕΝ ΑΠΑΝΤΑ για 4 s, τελευταία στο: ") + pageWhere + memory());
            } else if (pageStuck && pGap < 2000) {
                pageStuck = false;
                add("FREEZE", t("screen answers again after ", "η οθόνη απαντά ξανά μετά από ") + secs(pageAt - pageSince));
            }
        }
    }

    private static String stack(Thread t) {
        StringBuilder sb = new StringBuilder();
        StackTraceElement[] s = t.getStackTrace();
        for (int i = 0; i < s.length && i < 12; i++) sb.append(i == 0 ? "" : " < ").append(s[i].getClassName().replaceFirst("^.*\\.", ""))
                .append('.').append(s[i].getMethodName()).append(':').append(s[i].getLineNumber());
        return sb.length() == 0 ? "?" : sb.toString();
    }

    private static String secs(long ms) {
        return String.format(Locale.ROOT, "%.1f s", ms / 1000.0);
    }

    // free memory of the tablet and the app's own, for the freeze lines
    static String memory() {
        try {
            Runtime rt = Runtime.getRuntime();
            String m = "";
            android.app.ActivityManager am = ctx == null ? null : (android.app.ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
            if (am != null) {
                android.app.ActivityManager.MemoryInfo mi = new android.app.ActivityManager.MemoryInfo();
                am.getMemoryInfo(mi);
                m = t(" · free ", " · ελεύθερη ") + (mi.availMem >> 20) + " MB" + (mi.lowMemory ? t(" (low)", " (χαμηλή)") : "");
            }
            return m + " · app " + ((rt.totalMemory() - rt.freeMemory()) >> 20) + " MB";
        } catch (Exception e) {
            return "";
        }
    }
}
