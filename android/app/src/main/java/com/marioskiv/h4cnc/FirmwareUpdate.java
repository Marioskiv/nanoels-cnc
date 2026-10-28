// NanoEls H4 CNC app: offers a newer H4 firmware from the repository's Releases and installs it over WiFi.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// The H4 tells its firmware version in /status ("fw"); GitHub's public release list has "firmware-v<version>"
// releases with a .bin. A newer one is offered; "Install" downloads it and sends it to the H4's own firmware
// update (/firmware, as the page's Firmware update does). The H4 writes it to its second firmware slot, switches
// only to a complete, checked image and keeps its machine settings. An H4 without "fw" (older firmware, whose
// settings may live only in its my_machine.h) is never offered an update.
package com.marioskiv.h4cnc;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Scanner;

final class FirmwareUpdate {
    private static final String RELEASES = "https://api.github.com/repos/Marioskiv/nanoels-cnc/releases?per_page=30";
    private static final String TAG_PREFIX = "firmware-v";
    private static final long EVERY_MS = 6 * 3600 * 1000L;
    private static volatile boolean busy;

    private FirmwareUpdate() {
    }

    // automatic: at most every 6 hours; manual (button on the page): always, and says when there is nothing new
    static void check(final MainActivity activity, final String address, final boolean manual) {
        if (address == null || busy) return;
        final SharedPreferences prefs = activity.getSharedPreferences("h4", Context.MODE_PRIVATE);
        long now = System.currentTimeMillis();
        if (!manual && now - prefs.getLong("fwCheck", 0) < EVERY_MS) return;
        busy = true;
        new Thread(() -> {
            String problem = null;
            try {
                JSONObject status = new JSONObject(get("http://" + address + "/status", 3000));
                String fw = status.optString("fw", "");
                if (fw.isEmpty()) {
                    problem = "This H4 firmware is too old for updates from the app: install the new firmware once over USB.";
                } else if (status.optInt("running", 0) != 0) {
                    problem = "The machine is running a program: try again when it has stopped.";
                } else {
                    JSONArray releases = new JSONArray(get(RELEASES, 8000));
                    String best = null, bestUrl = null;
                    for (int i = 0; i < releases.length(); i++) {
                        JSONObject r = releases.getJSONObject(i);
                        String tag = r.optString("tag_name", "");
                        if (!tag.startsWith(TAG_PREFIX) || r.optBoolean("draft") || r.optBoolean("prerelease")) continue;
                        String version = tag.substring(TAG_PREFIX.length());
                        if (!UpdateCheck.newer(version, fw) || (best != null && !UpdateCheck.newer(version, best))) continue;
                        JSONArray assets = r.optJSONArray("assets");
                        for (int k = 0; assets != null && k < assets.length(); k++) {
                            String u = assets.getJSONObject(k).optString("browser_download_url", "");
                            if (u.endsWith(".bin")) {
                                best = version;
                                bestUrl = u;
                            }
                        }
                    }
                    prefs.edit().putLong("fwCheck", System.currentTimeMillis()).apply();
                    if (best != null) {
                        final String version = best, url = bestUrl;
                        activity.runOnUiThread(() -> offer(activity, address, fw, version, url));
                        return;
                    }
                    if (manual) problem = "The H4 has the newest firmware (" + fw + ").";
                }
            } catch (Exception e) {
                if (manual) problem = "Could not check: " + e.getMessage();
            } finally {
                busy = false;
            }
            if (problem != null && manual) {
                final String text = problem;
                activity.runOnUiThread(() -> info(activity, "Firmware", text));
            }
        }).start();
    }

    private static void offer(MainActivity activity, String address, String current, String version, String url) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity)
                .setTitle("H4 firmware " + version)
                .setMessage("A newer firmware for the H4 is available (it has " + current + ").\n\n"
                        + "Install it now over WiFi? Nothing may run; the H4 restarts and keeps its machine settings. "
                        + "If anything goes wrong on the way, it keeps its present firmware.")
                .setPositiveButton("Install", (d, w) -> install(activity, address, version, url))
                .setNegativeButton("Later", null)
                .show();
    }

    private static void install(final MainActivity activity, final String address, final String version, final String url) {
        final AlertDialog progress = new AlertDialog.Builder(activity).setTitle("H4 firmware " + version)
                .setMessage("Downloading...").setCancelable(false).show();
        new Thread(() -> {
            String result;
            boolean ok = false;
            try {
                byte[] bin = download(url);
                activity.runOnUiThread(() -> progress.setMessage("Sending to the H4 (" + bin.length / 1024 + " kB)..."));
                result = upload("http://" + address + "/firmware", "h4-firmware-" + version + ".bin", bin);
                ok = result.startsWith("Firmware updated");
            } catch (Exception e) {
                result = "Failed: " + e.getMessage();
            }
            final String text = result;
            final boolean done = ok;
            activity.runOnUiThread(() -> {
                progress.dismiss();
                info(activity, "H4 firmware " + version, done ? text + "\n\nThe page reloads in a few seconds." : text);
                if (done) activity.reloadLater(9000);
            });
        }).start();
    }

    private static void info(MainActivity activity, String title, String text) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity).setTitle(title).setMessage(text).setPositiveButton("OK", null).show();
    }

    private static String get(String url, int timeout) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(timeout);
        c.setReadTimeout(timeout);
        c.setRequestProperty("User-Agent", "H4CNC-app");
        try {
            if (c.getResponseCode() != 200) throw new Exception("HTTP " + c.getResponseCode());
            InputStream in = c.getInputStream();
            Scanner sc = new Scanner(in, "UTF-8").useDelimiter("\\A");
            String body = sc.hasNext() ? sc.next() : "";
            in.close();
            return body;
        } finally {
            c.disconnect();
        }
    }

    private static byte[] download(String url) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();  // GitHub redirects: followed
        c.setConnectTimeout(10000);
        c.setReadTimeout(30000);
        c.setRequestProperty("User-Agent", "H4CNC-app");
        try {
            if (c.getResponseCode() != 200) throw new Exception("download HTTP " + c.getResponseCode());
            InputStream in = c.getInputStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            in.close();
            byte[] bin = out.toByteArray();
            if (bin.length < 100000 || (bin[0] & 0xff) != 0xE9) throw new Exception("not an ESP32 firmware image");
            return bin;
        } finally {
            c.disconnect();
        }
    }

    // multipart/form-data with one file field "fw", as the page's Firmware update sends it
    private static String upload(String url, String fileName, byte[] bin) throws Exception {
        String boundary = "----h4cnc" + System.currentTimeMillis();
        byte[] head = ("--" + boundary + "\r\nContent-Disposition: form-data; name=\"fw\"; filename=\"" + fileName
                + "\"\r\nContent-Type: application/octet-stream\r\n\r\n").getBytes("UTF-8");
        byte[] tail = ("\r\n--" + boundary + "--\r\n").getBytes("UTF-8");
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(5000);
        c.setReadTimeout(90000);
        c.setDoOutput(true);
        c.setRequestMethod("POST");
        c.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
        c.setFixedLengthStreamingMode(head.length + bin.length + tail.length);
        try {
            OutputStream o = c.getOutputStream();
            o.write(head);
            for (int i = 0; i < bin.length; i += 8192) o.write(bin, i, Math.min(8192, bin.length - i));
            o.write(tail);
            o.close();
            int code = c.getResponseCode();
            InputStream in = code >= 400 ? c.getErrorStream() : c.getInputStream();
            String body = "";
            if (in != null) {
                Scanner sc = new Scanner(in, "UTF-8").useDelimiter("\\A");
                body = sc.hasNext() ? sc.next() : "";
                in.close();
            }
            return body.isEmpty() ? "HTTP " + code : body.trim();
        } finally {
            c.disconnect();
        }
    }
}
