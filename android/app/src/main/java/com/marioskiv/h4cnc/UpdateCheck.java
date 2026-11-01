// NanoEls H4 CNC app: tells when a newer version of the app is on the repository's Releases page.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// Automatically each time the app opens (never while it is in use), and with the page's "Check for an app update"
// button (Settings, App update), in the background: GitHub's public list of releases (no account, no key), the newest
// "android-v<version>" release with an .apk. If it is newer than this app, a dialog offers to install it: the app
// downloads the APK itself (with progress), checks it and opens Android's own installer on it, which asks the user
// to confirm. Handing the link to a browser left the download at 100 % with nothing opening it.
package com.marioskiv.h4cnc;

import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
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

    static final long DELAY_MS = 15000;              // the automatic check waits this long after the app opens
    static final long EVERY_MS = 12L * 3600 * 1000;  // and runs at most this often
    static final long LATER_MS = 24L * 3600 * 1000;  // "Later" keeps that version quiet this long

    // When the app opens: first, did the last install go through? Then, after a short while and not more often than
    // every 12 hours, look for a newer version (a manual check, Settings, App update, always looks).
    static void maybeCheck(final MainActivity activity) {
        final SharedPreferences prefs = activity.getSharedPreferences("h4", Context.MODE_PRIVATE);
        final String current = BuildInfo.version(activity);
        final String tried = prefs.getString("installTried", null);
        if (tried != null) {
            prefs.edit().remove("installTried").apply();
            if (newer(tried, current)) {
                // the installer was opened for that version, but this is still the old one
                activity.web().postDelayed(() -> notInstalled(activity, current, tried), 3000);
                return;
            }
        }
        if (System.currentTimeMillis() - prefs.getLong("updateCheck", 0) < EVERY_MS) return;
        activity.web().postDelayed(() -> {
            if (!activity.isFinishing()) check(activity, false);
        }, DELAY_MS);
    }

    private static void notInstalled(MainActivity activity, String current, String tried) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity)
                .setTitle(Tr.t(activity, "App update not installed", "Η ενημέρωση δεν εγκαταστάθηκε"))
                .setMessage(Tr.t(activity,
                        "Android did not finish installing H4 CNC " + tried + ": this is still " + current + ".\n\n"
                        + "Try again with Auto-rotate on (Huawei: or hold the tablet upright) and wait until Android "
                        + "says the app is installed; Android closes the app meanwhile, that is normal. If it still "
                        + "does not install: Settings, Apps, show system, Package installer, Storage, Clear data, "
                        + "restart the tablet and try again.",
                        "Το Android δεν ολοκλήρωσε την εγκατάσταση του H4 CNC " + tried + ": αυτό είναι ακόμα το " + current + ".\n\n"
                        + "Δοκίμασε ξανά με την Αυτόματη περιστροφή ενεργή (Huawei: ή κράτα το tablet κάθετα) και "
                        + "περίμενε να πει το Android ότι εγκαταστάθηκε· στο μεταξύ κλείνει το app, είναι κανονικό. Αν "
                        + "πάλι δεν μπει: Ρυθμίσεις, Εφαρμογές, εμφάνιση συστήματος, Πρόγραμμα εγκατάστασης πακέτων, "
                        + "Αποθηκευτικός χώρος, Διαγραφή δεδομένων, επανεκκίνηση του tablet και ξανά."))
                .setPositiveButton(Tr.t(activity, "Try again", "Ξανά"), (d, w) -> check(activity, true))
                .setNegativeButton(Tr.t(activity, "Later", "Αργότερα"), null)
                .show();
    }

    // manual: always, and it also says when this is already the newest version or GitHub cannot be reached
    static void check(final MainActivity activity, final boolean manual) {
        final SharedPreferences prefs = activity.getSharedPreferences("h4", Context.MODE_PRIVATE);
        if (busy) return;
        busy = true;
        final String current = BuildInfo.version(activity);
        new Thread(() -> {
            try {
                HttpURLConnection c = Net.get(RELEASES, 5000, 8000, "Accept", "application/vnd.github+json");
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
                if (best != null && !manual && best.equals(prefs.getString("laterVersion", ""))
                        && System.currentTimeMillis() - prefs.getLong("laterAt", 0) < LATER_MS) best = null;
                if (best == null) {
                    if (manual) tell(activity, Tr.t(activity, "H4 CNC " + current + " is the newest version.", "Το H4 CNC " + current + " είναι η νεότερη έκδοση."));
                    return;
                }
                final String version = best, url = bestUrl;
                activity.runOnUiThread(() -> offer(activity, current, version, url, prefs));
            } catch (Exception e) {
                // no network or GitHub unreachable: the automatic check tries again next time
                if (manual) tell(activity, Tr.t(activity, "Could not look for an update (no internet?): ", "Δεν έγινε έλεγχος για ενημέρωση (χωρίς internet;): ") + e.getMessage());
            } finally {
                busy = false;
            }
        }).start();
    }

    private static void tell(MainActivity activity, String text) {
        activity.runOnUiThread(() -> {
            if (activity.isFinishing()) return;
            new AlertDialog.Builder(activity).setTitle(Tr.t(activity, "App update", "Ενημέρωση app")).setMessage(text).setPositiveButton("OK", null).show();
        });
    }

    private static void offer(MainActivity activity, String current, String version, String url, SharedPreferences prefs) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity)
                .setTitle(Tr.t(activity, "Update available", "Υπάρχει ενημέρωση"))
                .setMessage(Tr.t(activity,
                        "Installed: " + current + "\nNew: " + version + "\n\n"
                        + "Install downloads it and Android asks you to confirm. Android closes the app while it "
                        + "installs it: that is normal, open it again afterwards. If Android says it cannot (an older "
                        + "app signed differently), uninstall H4 CNC first and install again.\n\n"
                        + "Huawei tablets (Android 7): keep Auto-rotate on (or hold the tablet upright) while Android "
                        + "installs; with the rotation locked sideways its installer stays at \"Installing...\".",
                        "Εγκατεστημένη: " + current + "\nΝέα: " + version + "\n\n"
                        + "Το Εγκατάσταση το κατεβάζει και το Android ζητά επιβεβαίωση. Στην εγκατάσταση το Android "
                        + "κλείνει το app: είναι κανονικό, άνοιξέ το ξανά μετά. Αν το Android λέει ότι δεν μπορεί "
                        + "(παλιό app με άλλη υπογραφή), απεγκατάστησε πρώτα το H4 CNC και εγκατάστησε ξανά.\n\n"
                        + "Tablet Huawei (Android 7): άφησε την Αυτόματη περιστροφή ενεργή (ή κράτα το tablet κάθετα) "
                        + "όσο εγκαθιστά· με κλειδωμένη πλάγια περιστροφή ο εγκαταστάτης μένει στο «Εγκατάσταση...»."))
                .setPositiveButton(Tr.t(activity, "Install", "Εγκατάσταση"), (d, w) -> install(activity, url, version))
                .setNegativeButton(Tr.t(activity, "Later", "Αργότερα"), (d, w) -> prefs.edit()
                        .putString("laterVersion", version).putLong("laterAt", System.currentTimeMillis()).apply())
                .show();
    }

    static final int INSTALL_REQUEST = 7;

    // Downloads the APK into the app's own storage, checks it (this app, a newer version, the same signature as
    // the installed app) and opens Android's installer on it, the way that exists on every Android from 5 on
    // (a file on 5 and 6, a content:// address from 7). A file signed with another key is caught before the
    // installer opens: some installers only hang on it instead of saying why.
    private static void install(final MainActivity activity, final String url, final String version) {
        if (Build.VERSION.SDK_INT >= 26 && !activity.getPackageManager().canRequestPackageInstalls()) {
            new AlertDialog.Builder(activity).setTitle(Tr.t(activity, "App update", "Ενημέρωση app"))
                    .setMessage(Tr.greek(activity) ? "Το Android πρέπει να επιτρέψει στο H4 CNC να εγκαθιστά εφαρμογές (μία φορά). "
                            + "Ενεργοποίησέ το στην επόμενη οθόνη, γύρνα πίσω και πάτα ξανά Έλεγχος για ενημέρωση." : "Android must allow H4 CNC to install apps (once). Switch it on in the next screen, "
                            + "come back and press Check for an app update again.")
                    .setPositiveButton(Tr.t(activity, "Open settings", "Άνοιγμα ρυθμίσεων"), (d, w) -> {
                        try {
                            activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                    Uri.parse("package:" + activity.getPackageName())));
                        } catch (Exception ignored) {
                        }
                    })
                    .setNegativeButton(Tr.t(activity, "Later", "Αργότερα"), null).show();
            return;
        }
        final AlertDialog progress = new AlertDialog.Builder(activity).setTitle(Tr.t(activity, "App update", "Ενημέρωση app"))
                .setMessage(Tr.t(activity, "Downloading ...", "Λήψη ...")).setCancelable(false).show();
        new Thread(() -> {
            try {
                final File apk = ApkProvider.file(activity);
                HttpURLConnection c = Net.get(url, 8000, 20000);  // follows GitHub's redirect to its download host
                if (c.getResponseCode() != 200) throw new Exception("GitHub answered " + c.getResponseCode());
                final long total = c.getContentLength();
                try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(apk)) {
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
                            activity.runOnUiThread(() -> progress.setMessage(Tr.t(activity, "Downloading ... ", "Λήψη ... ") + (pct >= 0 ? pct + " %" : kb + " kB")));
                        }
                    }
                }
                c.disconnect();
                if (total > 0 && apk.length() != total) throw new Exception("the download was cut off");
                final String problem = check(activity, apk);
                activity.runOnUiThread(() -> {
                    progress.dismiss();
                    if (problem == null) openInstaller(activity, apk, version);
                    else if (problem.equals(SIGNED_DIFFERENTLY)) offerUninstall(activity);
                    else tell(activity, Tr.t(activity, "The update was not installed: ", "Η ενημέρωση δεν εγκαταστάθηκε: ") + problem);
                });
            } catch (Exception e) {
                activity.runOnUiThread(() -> {
                    progress.dismiss();
                    tell(activity, Tr.t(activity, "The update was not installed: ", "Η ενημέρωση δεν εγκαταστάθηκε: ") + e.getMessage());
                });
            }
        }).start();
    }

    private static final String SIGNED_DIFFERENTLY = "signed differently";

    // null when the file is this app, newer, and signed like the installed app (Android refuses anything else)
    @SuppressWarnings("deprecation")
    private static String check(MainActivity activity, File apk) {
        PackageManager pm = activity.getPackageManager();
        PackageInfo n = pm.getPackageArchiveInfo(apk.getPath(), PackageManager.GET_SIGNATURES);
        if (n == null) return "the downloaded file is not a readable app";
        if (!activity.getPackageName().equals(n.packageName)) return "the downloaded file is another app";
        try {
            PackageInfo o = pm.getPackageInfo(activity.getPackageName(), PackageManager.GET_SIGNATURES);
            if (n.versionCode <= o.versionCode) return "the downloaded app is not newer";
            // older Androids may not read an archive's signatures: then the installer itself decides
            if (n.signatures != null && n.signatures.length > 0 && o.signatures != null && o.signatures.length > 0
                    && !n.signatures[0].equals(o.signatures[0])) return SIGNED_DIFFERENTLY;
        } catch (Exception ignored) {
        }
        return null;
    }

    @SuppressWarnings("deprecation")
    private static void openInstaller(MainActivity activity, File apk, String version) {
        Intent i;
        if (Build.VERSION.SDK_INT >= 24) {
            i = new Intent(Intent.ACTION_INSTALL_PACKAGE, ApkProvider.uri(activity));
            i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } else {
            // Android 5 and 6 take a plain file: make it readable by the installer
            apk.setReadable(true, false);
            apk.getParentFile().setExecutable(true, false);
            i = new Intent(Intent.ACTION_INSTALL_PACKAGE, Uri.fromFile(apk));
        }
        i.putExtra(Intent.EXTRA_NOT_UNKNOWN_SOURCE, true);
        i.putExtra(Intent.EXTRA_RETURN_RESULT, true);
        try {
            // remembered: if the app opens again still at the old version, it says the install did not go through
            activity.getSharedPreferences("h4", Context.MODE_PRIVATE).edit().putString("installTried", version).commit();
            activity.startActivityForResult(i, INSTALL_REQUEST);
        } catch (Exception e) {
            tell(activity, Tr.t(activity, "Android's installer did not open: ", "Ο εγκαταστάτης του Android δεν άνοιξε: ") + e.getMessage());
        }
    }

    // The installed app was signed with another key (the early test versions): Android can only install the new
    // one after this one is removed. Uninstalling loses only the app's own settings (the H4 address, screen).
    private static void offerUninstall(MainActivity activity) {
        if (activity.isFinishing()) return;
        new AlertDialog.Builder(activity).setTitle(Tr.t(activity, "App update", "Ενημέρωση app"))
                .setMessage(Tr.greek(activity) ? "Το εγκατεστημένο H4 CNC έχει άλλη υπογραφή (παλιά δοκιμαστική έκδοση), οπότε το Android "
                        + "δεν μπορεί να το ενημερώσει.\n\nΑπεγκατάστησέ το τώρα, κατέβασε ξανά το H4CNC.apk από\n" + LATEST
                        + "\nκαι άνοιξέ το. Χάνονται μόνο οι ρυθμίσεις του app (διεύθυνση H4, οθόνη)· το H4 τα κρατά όλα." : "The installed H4 CNC was signed with another key (an early test version), so Android "
                        + "cannot update it.\n\nUninstall it now, then download H4CNC.apk once more from\n"
                        + LATEST + "\nand open it. Only the app's own settings are lost (the H4 address, the "
                        + "screen); the H4 keeps everything.")
                .setPositiveButton(Tr.t(activity, "Uninstall", "Απεγκατάσταση"), (d, w) -> {
                    try {
                        activity.startActivity(new Intent(Intent.ACTION_DELETE, Uri.parse("package:" + activity.getPackageName())));
                    } catch (Exception ignored) {
                    }
                })
                .setNegativeButton(Tr.t(activity, "Later", "Αργότερα"), null).show();
    }

    static final String LATEST = "https://github.com/Marioskiv/nanoels-cnc/releases/latest/download/H4CNC.apk";

    // The installer's answer (MainActivity.onActivityResult). On success Android replaces and restarts the app.
    static void installResult(MainActivity activity, int result, Intent data) {
        // cancelled in the installer: not a failed install, no reminder at the next start
        if (result == android.app.Activity.RESULT_CANCELED)
            activity.getSharedPreferences("h4", Context.MODE_PRIVATE).edit().remove("installTried").apply();
        if (result == android.app.Activity.RESULT_OK || result == android.app.Activity.RESULT_CANCELED) return;
        int code = data != null ? data.getIntExtra("android.intent.extra.INSTALL_RESULT", 0) : 0;
        // -7 INSTALL_FAILED_UPDATE_INCOMPATIBLE: another signature
        if (code == -7) offerUninstall(activity);
        else tell(activity, Tr.t(activity, "The update was not installed (Android code " + code + ").", "Η ενημέρωση δεν εγκαταστάθηκε (κωδικός Android " + code + ")."));
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
