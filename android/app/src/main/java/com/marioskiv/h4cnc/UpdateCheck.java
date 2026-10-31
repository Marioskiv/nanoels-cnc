// NanoEls H4 CNC app: tells when a newer version of the app is on the repository's Releases page.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// Automatically each time the app opens (never while it is in use), and with the page's "Check for an app update"
// button (Settings, App update), in the background: GitHub's public list of releases (no account, no key), the newest
// "android-v<version>" release with an .apk. If it is newer than this app, a dialog offers to install it: the app
// downloads the APK itself (with progress) and hands it to Android's own installer (PackageInstaller), which asks
// the user to confirm. Handing the link to a browser left the download at 100 % with nothing opening it.
package com.marioskiv.h4cnc;

import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInstaller;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Scanner;

final class UpdateCheck {
    private static final String RELEASES = "https://api.github.com/repos/Marioskiv/nanoels-cnc/releases?per_page=30";
    private static final String TAG_PREFIX = "android-v";

    private UpdateCheck() {
    }

    private static volatile boolean busy;

    static void maybeCheck(final MainActivity activity) {
        check(activity, false);
    }

    // manual: always, and it also says when this is already the newest version or GitHub cannot be reached
    static void check(final MainActivity activity, final boolean manual) {
        final SharedPreferences prefs = activity.getSharedPreferences("h4", Context.MODE_PRIVATE);
        if (busy) return;
        busy = true;
        final String current = BuildInfo.version(activity);
        new Thread(() -> {
            try {
                HttpURLConnection c = (HttpURLConnection) new URL(RELEASES).openConnection();
                c.setConnectTimeout(5000);
                c.setReadTimeout(8000);
                c.setRequestProperty("Accept", "application/vnd.github+json");
                c.setRequestProperty("User-Agent", "H4CNC-app");
                if (c.getResponseCode() != 200) throw new Exception("GitHub answered " + c.getResponseCode());
                InputStream in = c.getInputStream();
                Scanner sc = new Scanner(in, "UTF-8").useDelimiter("\\A");
                String body = sc.hasNext() ? sc.next() : "[]";
                in.close();
                c.disconnect();
                JSONArray releases = new JSONArray(body);
                String best = null, bestUrl = null;
                for (int i = 0; i < releases.length(); i++) {
                    JSONObject r = releases.getJSONObject(i);
                    String tag = r.optString("tag_name", "");
                    if (!tag.startsWith(TAG_PREFIX) || r.optBoolean("draft") || r.optBoolean("prerelease")) continue;
                    String version = tag.substring(TAG_PREFIX.length());
                    if (!newer(version, current) || (best != null && !newer(version, best))) continue;
                    JSONArray assets = r.optJSONArray("assets");
                    String url = null;
                    for (int k = 0; assets != null && k < assets.length(); k++) {
                        String u = assets.getJSONObject(k).optString("browser_download_url", "");
                        if (u.endsWith(".apk")) url = u;
                    }
                    if (url == null) continue;
                    best = version;
                    bestUrl = url;
                }
                prefs.edit().putLong("updateCheck", System.currentTimeMillis()).apply();
                if (best == null) {
                    if (manual) tell(activity, "H4 CNC " + current + " is the newest version.");
                    return;
                }
                final String version = best, url = bestUrl;
                activity.runOnUiThread(() -> offer(activity, current, version, url));
            } catch (Exception e) {
                // no network or GitHub unreachable: the automatic check tries again next time
                if (manual) tell(activity, "Could not look for an update (no internet?): " + e.getMessage());
            } finally {
                busy = false;
            }
        }).start();
    }

    private static void tell(MainActivity activity, String text) {
        activity.runOnUiThread(() -> {
            if (activity.isFinishing()) return;
            new AlertDialog.Builder(activity).setTitle("App update").setMessage(text).setPositiveButton("OK", null).show();
        });
    }

    private static void offer(MainActivity activity, String current, String version, String url) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity)
                .setTitle("Update available")
                .setMessage("H4 CNC " + version + " is available (this one is " + current + ").\n\n"
                        + "Install downloads it and Android asks you to confirm. If Android says it cannot "
                        + "(an older app signed differently), uninstall H4 CNC first and install again.")
                .setPositiveButton("Install", (d, w) -> install(activity, url))
                .setNegativeButton("Later", null)
                .show();
    }

    static final String ACTION_INSTALLED = "com.marioskiv.h4cnc.INSTALL_STATUS";

    // Downloads the APK into a PackageInstaller session and commits it; Android shows its confirmation and the
    // result comes back to MainActivity (onNewIntent -> installResult).
    private static void install(final MainActivity activity, final String url) {
        if (Build.VERSION.SDK_INT >= 26 && !activity.getPackageManager().canRequestPackageInstalls()) {
            new AlertDialog.Builder(activity).setTitle("App update")
                    .setMessage("Android must allow H4 CNC to install apps (once). Switch it on in the next screen, "
                            + "come back and press Check for an app update again.")
                    .setPositiveButton("Open settings", (d, w) -> {
                        try {
                            activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                    Uri.parse("package:" + activity.getPackageName())));
                        } catch (Exception ignored) {
                        }
                    })
                    .setNegativeButton("Later", null).show();
            return;
        }
        final AlertDialog progress = new AlertDialog.Builder(activity).setTitle("App update")
                .setMessage("Downloading ...").setCancelable(false).show();
        new Thread(() -> {
            PackageInstaller.Session session = null;
            try {
                HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection(); // follows GitHub's redirect
                c.setConnectTimeout(8000);
                c.setReadTimeout(20000);
                c.setRequestProperty("User-Agent", "H4CNC-app");
                if (c.getResponseCode() != 200) throw new Exception("GitHub answered " + c.getResponseCode());
                final long total = c.getContentLength();
                PackageInstaller pi = activity.getPackageManager().getPackageInstaller();
                PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                params.setAppPackageName(activity.getPackageName());
                int id = pi.createSession(params);
                session = pi.openSession(id);
                try (InputStream in = c.getInputStream(); OutputStream out = session.openWrite("h4cnc.apk", 0, total)) {
                    byte[] buf = new byte[65536];
                    long done = 0;
                    int lastPct = -1;
                    for (int n; (n = in.read(buf)) > 0; ) {
                        out.write(buf, 0, n);
                        done += n;
                        final int pct = total > 0 ? (int) (100 * done / total) : -1;
                        if (pct != lastPct) {
                            lastPct = pct;
                            final long kb = done / 1024;
                            activity.runOnUiThread(() -> progress.setMessage(pct >= 0 ? "Downloading ... " + pct + " %" : "Downloading ... " + kb + " kB"));
                        }
                    }
                    session.fsync(out);
                }
                c.disconnect();
                activity.runOnUiThread(() -> progress.setMessage("Android is checking the app ..."));
                Intent back = new Intent(activity, MainActivity.class).setAction(ACTION_INSTALLED);
                int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
                session.commit(PendingIntent.getActivity(activity, 7, back, flags).getIntentSender());
                session.close();
                session = null;
                activity.runOnUiThread(progress::dismiss);
            } catch (Exception e) {
                if (session != null) session.abandon();
                activity.runOnUiThread(() -> {
                    progress.dismiss();
                    tell(activity, "The update was not installed: " + e.getMessage());
                });
            }
        }).start();
    }

    // The installer's answer (MainActivity.onNewIntent): ask the user to confirm, or say why it failed.
    static void installResult(MainActivity activity, Intent intent) {
        int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirm != null) {
                try {
                    activity.startActivity(confirm);
                } catch (Exception e) {
                    tell(activity, "Android's installer did not open: " + e.getMessage());
                }
            }
        } else if (status != PackageInstaller.STATUS_SUCCESS) {
            String m = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
            tell(activity, "The update was not installed" + (m != null ? ": " + m : ".")
                    + (status == PackageInstaller.STATUS_FAILURE_CONFLICT || status == PackageInstaller.STATUS_FAILURE_INCOMPATIBLE
                    ? "\n\nThe installed app is signed differently: uninstall H4 CNC first and install the APK again." : ""));
        }
    }

    // "1.10" > "1.9": compares the numbers between the dots.
    static boolean newer(String a, String b) {
        String[] x = a.split("\\."), y = b.split("\\.");
        for (int i = 0; i < Math.max(x.length, y.length); i++) {
            int p = i < x.length ? parse(x[i]) : 0, q = i < y.length ? parse(y[i]) : 0;
            if (p != q) return p > q;
        }
        return false;
    }

    private static int parse(String s) {
        try {
            return Integer.parseInt(s.replaceAll("[^0-9]", ""));
        } catch (Exception e) {
            return 0;
        }
    }
}
