// NanoEls H4 CNC app: the H4's own web page, full screen, with the gamepad read by Android itself.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;

import org.json.JSONObject;

public class MainActivity extends Activity {
    private static final String PREFS = "h4";
    private static final String KEY_ADDRESS = "address";
    private static final String KEY_ORIENTATION = "orientation";
    private static final int PICK_FILE = 1;

    private WebView web;
    private GamepadJog pad;
    private String address;
    private ValueCallback<Uri[]> fileCallback;
    private AlertDialog addressDialog;
    private boolean demo;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        applyOrientation(getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_ORIENTATION, "auto"));
        web = new WebView(this);
        setContentView(web);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);  // after a firmware update the page is the new one
        web.addJavascriptInterface(new Bridge(), "H4App");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return false;  // every link of the H4 page stays in the app
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                if (!demo && url != null && url.startsWith("http")) FirmwareUpdate.check(MainActivity.this, address, false);
            }

            @SuppressWarnings("deprecation")
            @Override
            public void onReceivedError(WebView view, int code, String description, String failingUrl) {
                if (demo) return;
                askAddress("Could not open " + failingUrl + " (" + description + ").");
            }
        });
        web.setWebChromeClient(new WebChromeClient() {  // also shows the page's confirm() questions
            @Override
            public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
                if (fileCallback != null) fileCallback.onReceiveValue(null);
                fileCallback = callback;
                Intent pick = new Intent(Intent.ACTION_GET_CONTENT);
                pick.addCategory(Intent.CATEGORY_OPENABLE);
                pick.setType("*/*");
                try {
                    startActivityForResult(Intent.createChooser(pick, "File"), PICK_FILE);
                } catch (Exception e) {
                    fileCallback = null;
                    return false;
                }
                return true;
            }
        });
        pad = new GamepadJog(this);
        address = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_ADDRESS, null);
        if (address == null) askAddress(null);
        else open();
        UpdateCheck.maybeCheck(this);  // each time the app opens; never while it is in use
    }

    // Screen orientation chosen on the page (Settings, Screen and jog): auto, landscape or portrait.
    private void applyOrientation(String o) {
        setRequestedOrientation("landscape".equals(o) ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
                : "portrait".equals(o) ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
                : ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    String address() {
        return address;
    }

    private void open() {
        demo = false;
        pad.setDemo(false);
        pad.setAddress(address);
        web.loadUrl("http://" + address + "/");
    }

    // The H4's address, as its display shows it (e.g. 192.168.1.50). Asked at the first start, when the page
    // cannot be opened, and from the page's "H4 address" button.
    void askAddress(String problem) {
        if (addressDialog != null && addressDialog.isShowing()) return;
        final EditText input = new EditText(this);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        input.setHint("192.168.1.50");
        if (address != null) input.setText(address);
        String text = "The address shown on the H4 display (the phone must be on the same WiFi).";
        if (problem != null) text = problem + "\n\n" + text;
        AlertDialog.Builder b = new AlertDialog.Builder(this)
                .setTitle("H4 address")
                .setMessage(text)
                .setView(input)
                .setCancelable(address != null)
                .setPositiveButton("Open", (d, w) -> {
                    String a = input.getText().toString().trim();
                    a = a.replaceFirst("^https?://", "").replaceAll("/+$", "");
                    if (a.isEmpty()) {
                        askAddress(null);
                        return;
                    }
                    address = a;
                    getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(KEY_ADDRESS, a).apply();
                    open();
                });
        if (address != null) b.setNeutralButton("Retry", (d, w) -> open());
        b.setNegativeButton("Demo (no H4)", (d, w) -> startDemo());
        addressDialog = b.show();
    }

    // The page with a simulated machine: everything can be seen and tried without an H4 (nothing moves).
    private void startDemo() {
        demo = true;
        pad.setDemo(true);
        web.loadUrl("file:///android_asset/h4/index.html?demo=1");
    }

    void reloadLater(long ms) {
        web.postDelayed(() -> {
            if (!demo) open();
        }, ms);
    }

    // The gamepad's requests go to the simulated machine of the page in the demo.
    void demoRequest(String path) {
        web.evaluateJavascript("window.h4DemoRequest&&h4DemoRequest(" + JSONObject.quote(path) + ")", null);
    }

    // Gamepad state for the page's Gamepad box (the page shows it when it runs in this app).
    void showGamepadState(String text) {
        web.evaluateJavascript("window.h4AppGamepad&&h4AppGamepad(" + JSONObject.quote(text) + ")", null);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (request == PICK_FILE && fileCallback != null) {
            fileCallback.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(result, data));
            fileCallback = null;
            return;
        }
        if (request == UpdateCheck.INSTALL_REQUEST) {  // Android's installer answers an app update
            UpdateCheck.installResult(this, result, data);
            return;
        }
        super.onActivityResult(request, result, data);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (pad.handleKey(e)) return true;
        if (e.getKeyCode() == KeyEvent.KEYCODE_BACK && e.getAction() == KeyEvent.ACTION_UP && web.canGoBack()) {
            web.goBack();
            return true;
        }
        return super.dispatchKeyEvent(e);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if (pad.handleMotion(e)) return true;
        return super.dispatchGenericMotionEvent(e);
    }

    @Override
    protected void onResume() {
        super.onResume();
        web.onResume();
        pad.resume();
    }

    @Override
    protected void onPause() {
        pad.pause();  // stops any jog at once
        web.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        pad.pause();
        web.destroy();
        super.onDestroy();
    }

    // Called by the page (window.H4App).
    private class Bridge {
        @JavascriptInterface
        public void settings(String json) {
            pad.setSettings(json);
        }

        @JavascriptInterface
        public void changeAddress() {
            runOnUiThread(() -> askAddress(null));
        }

        @JavascriptInterface
        public void checkFirmware() {
            runOnUiThread(() -> {
                if (demo) return;
                FirmwareUpdate.check(MainActivity.this, address, true);
            });
        }

        @JavascriptInterface
        public void checkAppUpdate() {
            runOnUiThread(() -> UpdateCheck.check(MainActivity.this, true));
        }

        @JavascriptInterface
        public String orientation() {
            return getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_ORIENTATION, "auto");
        }

        @JavascriptInterface
        public void setOrientation(String o) {
            if (!"landscape".equals(o) && !"portrait".equals(o)) o = "auto";
            final String v = o;
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(KEY_ORIENTATION, v).apply();
            runOnUiThread(() -> applyOrientation(v));
        }

        @JavascriptInterface
        public String version() {
            return BuildInfo.version(MainActivity.this);
        }
    }
}
