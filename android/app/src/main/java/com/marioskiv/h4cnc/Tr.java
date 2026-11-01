// NanoEls H4 CNC app - https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.content.Context;

import java.util.Locale;

// The app's own dialogs (updates, H4 address) in the language chosen on the screen (Settings, Appearance), which the
// page passes on (H4App.setLang); before that, in the tablet's language when it is Greek, else in English.
final class Tr {
    private Tr() {
    }

    static void set(Context c, String lang) {
        c.getSharedPreferences("h4", Context.MODE_PRIVATE).edit().putString("lang", "el".equals(lang) ? "el" : "en").apply();
    }

    static boolean greek(Context c) {
        String l = c.getSharedPreferences("h4", Context.MODE_PRIVATE).getString("lang", null);
        if (l == null) l = Locale.getDefault().getLanguage();
        return "el".equals(l);
    }

    static String t(Context c, String en, String el) {
        return greek(c) ? el : en;
    }
}
