// NanoEls H4 CNC app: the answer of Android's package installer to an app update made with an install session
// (UpdateCheck.sessionInstall). Android asks the user through this receiver (STATUS_PENDING_USER_ACTION: the
// confirmation screen is opened from here), then reports the end. On success Android replaces the app and stops this
// process, so nothing comes back; a failure or a cancel is told here.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.os.Build;
import android.widget.Toast;

public final class InstallReceiver extends BroadcastReceiver {
    static final String ACTION = "com.marioskiv.h4cnc.INSTALL_RESULT";

    @Override
    @SuppressWarnings("deprecation")
    public void onReceive(Context c, Intent i) {
        int status = i.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            // Android's own "Install this update?" screen, in a task of its own
            Intent confirm = Build.VERSION.SDK_INT >= 33 ? i.getParcelableExtra(Intent.EXTRA_INTENT, Intent.class)
                    : (Intent) i.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirm != null) {
                confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                try {
                    c.startActivity(confirm);
                } catch (Exception e) {
                    tell(c, Tr.t(c, "Android's installer did not open: ", "Ο εγκαταστάτης του Android δεν άνοιξε: ") + e.getMessage());
                }
            }
            return;
        }
        if (status == PackageInstaller.STATUS_SUCCESS) return;   // the new app replaces this one
        // cancelled or failed: no "did not go through" reminder at the next start, this message says it now
        c.getSharedPreferences("h4", Context.MODE_PRIVATE).edit().remove("installTried").apply();
        // left or said no (also when the Install button did nothing and Back was pressed): the next try uses the
        // other way
        if (status == PackageInstaller.STATUS_FAILURE_ABORTED) {
            UpdateCheck.flipWay(c);
            return;
        }
        UpdateCheck.flipWay(c);
        String msg = i.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
        if (status == PackageInstaller.STATUS_FAILURE_CONFLICT || status == PackageInstaller.STATUS_FAILURE_INCOMPATIBLE)
            tell(c, Tr.t(c, "The update was not installed: the installed app is signed differently or does not fit. "
                    + "Uninstall H4 CNC and install H4CNC.apk once from the Releases page.",
                    "Η ενημέρωση δεν εγκαταστάθηκε: το εγκατεστημένο app έχει άλλη υπογραφή ή δεν ταιριάζει. "
                    + "Απεγκατάστησε το H4 CNC και βάλε μία φορά το H4CNC.apk από τα Releases."));
        else tell(c, Tr.t(c, "The update was not installed (Android " + status + "): ", "Η ενημέρωση δεν εγκαταστάθηκε (Android " + status + "): ")
                + (msg != null ? msg : "") + Tr.t(c, " - install H4CNC.apk from Files instead.", " - βάλε το H4CNC.apk από τα Αρχεία."));
    }

    private static void tell(Context c, String text) {
        Toast.makeText(c, text, Toast.LENGTH_LONG).show();
    }
}
