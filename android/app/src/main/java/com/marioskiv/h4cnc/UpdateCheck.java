// NanoEls H4 CNC app: tells when a newer version of the app is on the repository's Releases page.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// At most every 6 hours, in the background: GitHub's public list of releases (no account, no key), the newest
// "android-v<version>" release with an .apk. If it is newer than this app, a dialog offers to download it; the
// download opens Android's installer.
package com.marioskiv.h4cnc;

import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Scanner;

final class UpdateCheck {
    private static final String RELEASES = "https://api.github.com/repos/Marioskiv/nanoels-cnc/releases?per_page=30";
    private static final String TAG_PREFIX = "android-v";
    private static final long EVERY_MS = 6 * 3600 * 1000L;

    private UpdateCheck() {
    }

    static void maybeCheck(final MainActivity activity) {
        final SharedPreferences prefs = activity.getSharedPreferences("h4", Context.MODE_PRIVATE);
        long now = System.currentTimeMillis();
        if (now - prefs.getLong("updateCheck", 0) < EVERY_MS) return;
        final String current = BuildInfo.version(activity);
        new Thread(() -> {
            try {
                HttpURLConnection c = (HttpURLConnection) new URL(RELEASES).openConnection();
                c.setConnectTimeout(5000);
                c.setReadTimeout(8000);
                c.setRequestProperty("Accept", "application/vnd.github+json");
                c.setRequestProperty("User-Agent", "H4CNC-app");
                if (c.getResponseCode() != 200) return;
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
                if (best == null) return;
                final String version = best, url = bestUrl;
                activity.runOnUiThread(() -> offer(activity, current, version, url));
            } catch (Exception ignored) {
                // no network or GitHub unreachable: try again next time
            }
        }).start();
    }

    private static void offer(MainActivity activity, String current, String version, String url) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity)
                .setTitle("Update available")
                .setMessage("H4 CNC " + version + " is available (this one is " + current + ").\n\n"
                        + "Download opens the new APK; Android installs it over this one. If Android says it "
                        + "cannot (an older app signed differently), uninstall H4 CNC first and install again.")
                .setPositiveButton("Download", (d, w) -> {
                    try {
                        activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
                    } catch (Exception ignored) {
                    }
                })
                .setNegativeButton("Later", null)
                .show();
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
