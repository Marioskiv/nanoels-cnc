// NanoEls H4 CNC app: hands a downloaded app update to Android's installer.
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// Android 7 and newer only let another app (the installer) read a file through a content:// address. This serves
// the one file the update downloads, read only, to whoever the app grants it (FLAG_GRANT_READ_URI_PERMISSION);
// it is not exported. A minimal stand-in for androidx FileProvider, so the app keeps no library dependencies.
package com.marioskiv.h4cnc;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

public final class ApkProvider extends ContentProvider {
    private static final String NAME = "update.apk";

    static File file(Context c) {
        File dir = c.getExternalCacheDir();  // Android 5 and 6 hand the installer this file directly
        if (dir == null) dir = c.getCacheDir();
        return new File(dir, NAME);
    }

    static Uri uri(Context c) {
        return Uri.parse("content://" + c.getPackageName() + ".apk/" + NAME);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!NAME.equals(uri.getLastPathSegment())) throw new FileNotFoundException();
        return ParcelFileDescriptor.open(file(getContext()), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return "application/vnd.android.package-archive";
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] args, String sort) {
        MatrixCursor c = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        c.addRow(new Object[] {NAME, file(getContext()).length()});
        return c;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] args) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] args) {
        return 0;
    }
}
