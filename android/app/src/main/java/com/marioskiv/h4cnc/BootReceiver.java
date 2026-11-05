// NanoEls H4 CNC app: opens the app when the tablet has started (BOOT_COMPLETED, sent by Android only), so a tablet
// fixed to the machine shows the H4 screen without a tap. Switched off on the page (Settings, Screen and jog,
// Open when the tablet starts). Some makers block it until it is allowed in their own settings (Huawei: Phone
// Manager, Startup manager / App launch: H4 CNC).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class BootReceiver extends BroadcastReceiver {
    static final String KEY = "openAtBoot";

    @Override
    public void onReceive(Context c, Intent i) {
        String a = i.getAction();
        if (!Intent.ACTION_BOOT_COMPLETED.equals(a) && !"android.intent.action.QUICKBOOT_POWERON".equals(a)) return;
        if (!c.getSharedPreferences("h4", Context.MODE_PRIVATE).getBoolean(KEY, true)) return;
        ExitWatch.event(c, "opened at the tablet's start", "άνοιξε με την εκκίνηση του tablet");
        try {
            c.startActivity(new Intent(c, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (Exception ignored) {
        }
    }
}
