// NanoEls H4 CNC app: Android finished installing an update of this app (MY_PACKAGE_REPLACED, sent only to this
// app by Android). The old version was closed to replace it; the new one opens by itself, so nobody has to guess
// when the install is over. Written to the App log.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class UpdatedReceiver extends BroadcastReceiver {
    static final String EXTRA = "h4.updated";
    @Override
    public void onReceive(Context c, Intent i) {
        if (!Intent.ACTION_MY_PACKAGE_REPLACED.equals(i.getAction())) return;
        c.getSharedPreferences("h4", Context.MODE_PRIVATE).edit().remove("installTried").apply();
        ExitWatch.event(c, "updated to " + BuildInfo.version(c), "ενημερώθηκε στην " + BuildInfo.version(c));
        try {
            c.startActivity(new Intent(c, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).putExtra(EXTRA, true));
        } catch (Exception ignored) {
        }
    }
}
