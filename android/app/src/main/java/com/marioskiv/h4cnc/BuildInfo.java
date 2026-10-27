// NanoEls H4 CNC app - https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
package com.marioskiv.h4cnc;

import android.content.Context;

final class BuildInfo {
    private BuildInfo() {
    }

    // versionName of the installed app (build.gradle), e.g. "1.2".
    static String version(Context c) {
        try {
            return c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "0";
        }
    }
}
