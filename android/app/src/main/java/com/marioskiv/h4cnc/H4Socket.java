// NanoEls H4 CNC app: the gamepad's lasting link to the H4 (WebSocket, RFC 6455, port 81; firmware 1.48 wslink.ino).
// https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: MIT
//
// One connection that stays open instead of a new HTTP connection for every jog: over a weak WiFi a new connection
// takes 100-300 ms and sometimes fails, a lasting one only sends its few bytes. Opened when the gamepad is used, kept
// with an "alive" every second, closed when the app goes to the background. While it is not open the gamepad uses
// HTTP as before (an older firmware has no port 81). Text messages only; the H4's answers come back on the main thread.
package com.marioskiv.h4cnc;

import android.os.Handler;
import android.os.Looper;
import android.util.Base64;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

final class H4Socket {
    interface Listener {
        void message(String text);
    }

    static final int PORT = 81;
    private static final long RETRY_MS = 3000;   // a failed open is tried again after this
    private static final long ALIVE_MS = 1000;   // the H4 closes a connection silent for 3 s

    private final Listener listener;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final ExecutorService out = Executors.newSingleThreadExecutor();
    private final SecureRandom random = new SecureRandom();
    private volatile Socket socket;
    private volatile boolean open;
    private volatile boolean opening;
    private volatile long lastTry, lastSent;
    private String address;

    H4Socket(Listener listener) {
        this.listener = listener;
    }

    boolean isOpen() {
        return open;
    }

    // Opens the link to the H4 at address (if not open and not tried just now).
    void want(String address) {
        if (address == null) return;
        if (!address.equals(this.address)) {
            close();
            this.address = address;
        }
        if (open || opening || System.currentTimeMillis() - lastTry < RETRY_MS) return;
        lastTry = System.currentTimeMillis();
        opening = true;
        final String a = address;
        new Thread(() -> run(a), "h4-socket").start();
    }

    // Sends a text message (false when the link is not open).
    boolean send(final String text) {
        if (!open) return false;
        lastSent = System.currentTimeMillis();
        out.execute(() -> {
            try {
                Socket s = socket;
                if (s == null) return;
                OutputStream o = s.getOutputStream();
                o.write(frame(text.getBytes(StandardCharsets.UTF_8)));
                o.flush();
            } catch (Exception e) {
                fail("send: " + e);
            }
        });
        return true;
    }

    // Every gamepad tick: keeps the link from being closed as silent.
    void tick() {
        if (open && System.currentTimeMillis() - lastSent >= ALIVE_MS) send("{\"t\":\"alive\"}");
    }

    void close() {
        Socket s = socket;
        socket = null;
        if (open) AppLog.add("PAD", "lasting link closed");
        open = false;
        if (s != null) {
            try {
                s.close();
            } catch (Exception ignored) {
            }
        }
    }

    private void fail(String why) {
        if (open) AppLog.add("PAD", "lasting link lost (" + why + "), the gamepad goes on over HTTP");
        close();
    }

    private void run(String a) {
        Socket s = new Socket();
        try {
            String host = a;
            int port = PORT;
            int colon = a.lastIndexOf(':');
            if (colon > 0 && a.indexOf(':') == colon) host = a.substring(0, colon);  // "ip:80" -> the ip
            s.setTcpNoDelay(true);
            s.connect(new InetSocketAddress(host, port), 1500);
            s.setSoTimeout(5000);
            byte[] keyBytes = new byte[16];
            random.nextBytes(keyBytes);
            String key = Base64.encodeToString(keyBytes, Base64.NO_WRAP);
            String req = "GET / HTTP/1.1\r\nHost: " + host + ":" + port + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                    + "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
            OutputStream o = s.getOutputStream();
            o.write(req.getBytes(StandardCharsets.US_ASCII));
            o.flush();
            InputStream in = s.getInputStream();
            StringBuilder head = new StringBuilder();
            while (head.indexOf("\r\n\r\n") < 0) {
                int c = in.read();
                if (c < 0 || head.length() > 2000) throw new Exception("no answer to the opening request");
                head.append((char) c);
            }
            MessageDigest sha = MessageDigest.getInstance("SHA-1");
            String accept = Base64.encodeToString(sha.digest((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").getBytes(StandardCharsets.US_ASCII)), Base64.NO_WRAP);
            String h = head.toString();
            if (!h.startsWith("HTTP/1.1 101") || !h.contains(accept)) throw new Exception("refused: " + h.split("\r\n")[0]);
            s.setSoTimeout(0);
            socket = s;
            open = true;
            opening = false;
            lastSent = System.currentTimeMillis();
            AppLog.add("PAD", "lasting link to the H4 open (port " + port + ")");
            read(s, in);
        } catch (Exception e) {
            opening = false;
            if (socket == s) fail(e.toString());
            else {
                try {
                    s.close();
                } catch (Exception ignored) {
                }
            }
        }
    }

    // The H4's frames (never masked): text to the listener, ping answered, close ends it.
    private void read(Socket s, InputStream in) throws Exception {
        java.io.ByteArrayOutputStream msg = new java.io.ByteArrayOutputStream();
        while (socket == s) {
            int b0 = in.read(), b1 = in.read();
            if (b0 < 0 || b1 < 0) throw new Exception("closed by the H4");
            int op = b0 & 0x0F;
            long len = b1 & 0x7F;
            if (len == 126) len = (readByte(in) << 8) | readByte(in);
            else if (len == 127) {
                len = 0;
                for (int i = 0; i < 8; i++) len = (len << 8) | readByte(in);
            }
            if (len > 65536) throw new Exception("frame too long");
            byte[] data = new byte[(int) len];
            int got = 0;
            while (got < len) {
                int n = in.read(data, got, (int) len - got);
                if (n < 0) throw new Exception("closed by the H4");
                got += n;
            }
            if (op == 0x8) throw new Exception("closed by the H4");
            if (op == 0x9) {
                pong(data);
                continue;
            }
            if (op == 0x1 || op == 0x0) {
                msg.write(data);
                if ((b0 & 0x80) != 0) {
                    final String text = new String(msg.toByteArray(), StandardCharsets.UTF_8);
                    msg.reset();
                    ui.post(() -> listener.message(text));
                }
            }
        }
    }

    private static int readByte(InputStream in) throws Exception {
        int b = in.read();
        if (b < 0) throw new Exception("closed by the H4");
        return b;
    }

    private void pong(final byte[] data) {
        out.execute(() -> {
            try {
                Socket s = socket;
                if (s != null) s.getOutputStream().write(frame(data, 0xA));
            } catch (Exception ignored) {
            }
        });
    }

    private byte[] frame(byte[] data) {
        return frame(data, 0x1);
    }

    // A client frame: always masked (RFC 6455 5.3).
    private byte[] frame(byte[] data, int op) {
        int n = data.length;
        int head = n < 126 ? 2 : 4;
        byte[] f = new byte[head + 4 + n];
        f[0] = (byte) (0x80 | op);
        if (n < 126) {
            f[1] = (byte) (0x80 | n);
        } else {
            f[1] = (byte) (0x80 | 126);
            f[2] = (byte) (n >> 8);
            f[3] = (byte) n;
        }
        byte[] mask = new byte[4];
        random.nextBytes(mask);
        System.arraycopy(mask, 0, f, head, 4);
        for (int i = 0; i < n; i++) f[head + 4 + i] = (byte) (data[i] ^ mask[i & 3]);
        return f;
    }
}
