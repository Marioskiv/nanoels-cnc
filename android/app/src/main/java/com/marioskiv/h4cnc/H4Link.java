// NanoEls H4 CNC app: finding the H4 and joining its own WiFi network, all from the app (Settings, Connection).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// Two ways to work, chosen in the app:
// - direct: the tablet is on the H4's own network ("H4", address 192.168.4.1), no router. The app joins it itself:
//   up to Android 9 with a WiFi configuration it adds; from Android 10 with a network request, which Android shows as
//   a question once ("Connect to H4?") and which is used by this app only.
// - router: the H4 and the tablet on the same WiFi. The H4's address comes from the router; the app finds it by itself.
// Finding: the app sends "H4CNC?" by UDP broadcast (port 4210) on the WiFi, also straight to 192.168.4.1, every
// second; each H4 answers "H4CNC {json}" (wifi.ino discoveryLoop) and its address is the sender's.
package com.marioskiv.h4cnc;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

final class H4Link {
    private H4Link() {
    }

    static final String AP_ADDRESS = "192.168.4.1";
    private static final int PORT = 4210;
    private static final Handler ui = new Handler(Looper.getMainLooper());
    private static volatile int findRun = 0;
    private static ConnectivityManager.NetworkCallback joined;  // Android 10+: the request that keeps the H4's network

    interface Found {
        void found(String ip, JSONObject info);

        void done(int count);
    }

    interface Result {
        void result(boolean ok, String text);
    }

    // The WiFi network of the tablet, if it is on one (so the search does not go out over mobile data).
    @SuppressWarnings("deprecation")
    private static Network wifiNetwork(Context c) {
        if (Build.VERSION.SDK_INT < 23) return null;
        try {
            ConnectivityManager cm = (ConnectivityManager) c.getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
            for (Network n : cm.getAllNetworks()) {
                NetworkCapabilities nc = cm.getNetworkCapabilities(n);
                if (nc != null && nc.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return n;
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    // The broadcast address of the tablet's WiFi (e.g. 192.168.1.255), or null.
    @SuppressWarnings("deprecation")
    private static InetAddress subnetBroadcast(Context c) {
        try {
            WifiManager wm = (WifiManager) c.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            DhcpInfo d = wm != null ? wm.getDhcpInfo() : null;
            if (d == null || d.ipAddress == 0) return null;
            int mask = d.netmask != 0 ? d.netmask : 0x00ffffff;  // little-endian bytes, as Android gives them
            int b = (d.ipAddress & mask) | ~mask;
            byte[] q = {(byte) b, (byte) (b >> 8), (byte) (b >> 16), (byte) (b >> 24)};
            return InetAddress.getByAddress(q);
        } catch (Exception e) {
            return null;
        }
    }

    // Looks for H4s for `seconds`; every new one is reported at once (on the UI thread), then done(count).
    static void find(Context ctx, int seconds, Found cb) {
        final Context c = ctx.getApplicationContext();
        final int run = ++findRun;
        new Thread(() -> {
            Set<String> seen = new HashSet<>();
            DatagramSocket s = null;
            try {
                s = new DatagramSocket();
                s.setBroadcast(true);
                s.setSoTimeout(250);
                Network n = wifiNetwork(c);
                if (n != null && Build.VERSION.SDK_INT >= 23) {
                    try {
                        n.bindSocket(s);
                    } catch (Exception ignored) {
                    }
                }
                byte[] ask = "H4CNC?".getBytes(StandardCharsets.US_ASCII);
                InetAddress[] to = {InetAddress.getByName("255.255.255.255"), subnetBroadcast(c), InetAddress.getByName(AP_ADDRESS)};
                long end = System.currentTimeMillis() + seconds * 1000L, nextAsk = 0;
                byte[] buf = new byte[1500];
                while (System.currentTimeMillis() < end && run == findRun) {
                    if (System.currentTimeMillis() >= nextAsk) {
                        nextAsk = System.currentTimeMillis() + 1000;
                        for (InetAddress a : to) {
                            if (a == null) continue;
                            try {
                                s.send(new DatagramPacket(ask, ask.length, a, PORT));
                            } catch (Exception ignored) {  // e.g. no WiFi at all: tried again next second
                            }
                        }
                    }
                    DatagramPacket p = new DatagramPacket(buf, buf.length);
                    try {
                        s.receive(p);
                    } catch (SocketTimeoutException e) {
                        continue;
                    }
                    String text = new String(p.getData(), 0, p.getLength(), StandardCharsets.UTF_8);
                    if (!text.startsWith("H4CNC ")) continue;
                    String ip = p.getAddress().getHostAddress();
                    if (!seen.add(ip)) continue;
                    JSONObject info;
                    try {
                        info = new JSONObject(text.substring(6));
                    } catch (Exception e) {
                        info = new JSONObject();
                    }
                    final JSONObject i = info;
                    AppLog.add("LINK", "found an H4 at " + ip);
                    ui.post(() -> cb.found(ip, i));
                }
            } catch (Exception e) {
                AppLog.add("LINK", "search failed: " + e);
            } finally {
                if (s != null) s.close();
            }
            final int count = seen.size();
            if (run == findRun) ui.post(() -> cb.done(count));
        }, "h4-find").start();
    }

    static void stopFind() {
        findRun++;
    }

    // Does the H4's own network answer (http://192.168.4.1/wifi)?
    private static boolean apAnswers() {
        try {
            HttpURLConnection h = (HttpURLConnection) new URL("http://" + AP_ADDRESS + "/wifi").openConnection();
            h.setConnectTimeout(1500);
            h.setReadTimeout(1500);
            int code = h.getResponseCode();
            h.disconnect();
            return code == 200;
        } catch (Exception e) {
            return false;
        }
    }

    // The tablet joins the H4's own network (ssid, password); result after the H4 answers there (up to ~40 s).
    @SuppressLint("MissingPermission")
    @SuppressWarnings("deprecation")
    static void joinH4(Context ctx, String ssid, String password, Result cb) {
        final Context c = ctx.getApplicationContext();
        stopFind();
        if (Build.VERSION.SDK_INT >= 29) {
            ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
            release(c);
            final boolean[] answered = {false};
            joined = new ConnectivityManager.NetworkCallback() {
                @Override
                public void onAvailable(Network n) {
                    cm.bindProcessToNetwork(n);  // the app's traffic on the H4's network (it has no internet)
                    if (answered[0]) return;
                    answered[0] = true;
                    AppLog.add("LINK", "joined the H4's network " + ssid);
                    ui.post(() -> cb.result(true, ssid));
                }

                @Override
                public void onUnavailable() {
                    if (answered[0]) return;
                    answered[0] = true;
                    joined = null;
                    AppLog.add("LINK", "the H4's network " + ssid + " was not joined");
                    ui.post(() -> cb.result(false, "not joined"));
                }

                @Override
                public void onLost(Network n) {
                    AppLog.add("LINK", "the H4's network " + ssid + " was lost");
                }
            };
            try {
                WifiNetworkSpecifier spec = new WifiNetworkSpecifier.Builder().setSsid(ssid).setWpa2Passphrase(password).build();
                NetworkRequest req = new NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                        .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).setNetworkSpecifier(spec).build();
                cm.requestNetwork(req, joined, 60000);
            } catch (Exception e) {
                joined = null;
                cb.result(false, e.toString());
            }
            return;
        }
        new Thread(() -> {
            boolean ok = false;
            String why = "";
            try {
                WifiManager wm = (WifiManager) c.getSystemService(Context.WIFI_SERVICE);
                if (!wm.isWifiEnabled()) wm.setWifiEnabled(true);
                String quoted = "\"" + ssid + "\"";
                List<WifiConfiguration> list = wm.getConfiguredNetworks();
                if (list != null) for (WifiConfiguration w : list) if (quoted.equals(w.SSID)) wm.removeNetwork(w.networkId);  // ours: with the new password
                WifiConfiguration w = new WifiConfiguration();
                w.SSID = quoted;
                w.preSharedKey = "\"" + password + "\"";
                int id = wm.addNetwork(w);
                if (id < 0) {
                    // a network of the same name saved by the user in Android: use it as it is
                    if (list != null) for (WifiConfiguration x : list) if (quoted.equals(x.SSID)) id = x.networkId;
                }
                if (id < 0) throw new Exception("Android did not accept the network " + ssid);
                wm.saveConfiguration();
                wm.enableNetwork(id, true);
                wm.reconnect();
                for (int i = 0; i < 40 && !ok; i++) {
                    Thread.sleep(1000);
                    ok = apAnswers();
                }
                if (!ok) why = "the H4 did not answer on " + ssid;
            } catch (Exception e) {
                why = e.toString();
            }
            final boolean r = ok;
            final String t = ok ? ssid : why;
            AppLog.add("LINK", ok ? "joined the H4's network " + ssid : "joining " + ssid + " failed: " + why);
            ui.post(() -> cb.result(r, t));
        }, "h4-join").start();
    }

    // Back to the tablet's usual WiFi (the H4 goes on the router): the H4's network is let go.
    @SuppressLint("MissingPermission")
    @SuppressWarnings("deprecation")
    static void leaveH4(Context ctx, String ssid) {
        final Context c = ctx.getApplicationContext();
        if (Build.VERSION.SDK_INT >= 29) {
            release(c);
            return;
        }
        try {
            WifiManager wm = (WifiManager) c.getSystemService(Context.WIFI_SERVICE);
            String quoted = "\"" + ssid + "\"";
            List<WifiConfiguration> list = wm.getConfiguredNetworks();
            if (list != null) {
                for (WifiConfiguration w : list) {
                    if (quoted.equals(w.SSID)) wm.disableNetwork(w.networkId);
                    else wm.enableNetwork(w.networkId, false);  // the tablet's other networks may be joined again
                }
            }
            wm.reconnect();
        } catch (Exception e) {
            AppLog.add("LINK", "leaving the H4's network: " + e);
        }
    }

    private static void release(Context c) {
        if (joined == null || Build.VERSION.SDK_INT < 29) return;
        try {
            ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
            cm.unregisterNetworkCallback(joined);
            cm.bindProcessToNetwork(null);
        } catch (Exception ignored) {
        }
        joined = null;
    }
}
