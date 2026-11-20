// The app's lasting link (firmware 1.48): one WebSocket connection (RFC 6455, port 81) that stays open, instead of a
// new HTTP connection for every request. On it the H4 sends its state by itself (the same JSON as /status, 10 times a
// second, to a connection that asked for it) and the app sends the gamepad's speeds and that it is still there. As
// FluidNC's web UI and grblHAL do. Over a weak WiFi a new connection per request takes 100-300 ms and sometimes fails;
// a lasting one only sends its few bytes. The HTTP API stays the same, and the app falls back to it when this link
// cannot be opened (an older firmware, a network that does not let it through).
// Messages from the app (text, JSON):
//   {"t":"sub"}                      send me the state (the page)
//   {"t":"alive"}                    I am still here (every 250 ms; for a program the app watches, as /status)
//   {"t":"jog","x":..,"z":..,"c":..} the gamepad's speeds, as /jog; answered {"t":"jog","ok":1} or {"t":"jog","ok":0,"e":"why"}
// A connection that sends nothing for WS_SILENT_MS is closed (the app reconnects); every jog stops by itself 0.3 s
// after the last one anyway (grbl.ino), as over HTTP.
// Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
// Copyright (c) 2026 Marioskiv
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ws_proto.h"

#define WS_CLIENTS 3           // the page, the app's gamepad, one more (a second tablet)
#define WS_PUSH_MS 100         // the state, 10 times a second
#define WS_SILENT_MS 3000      // nothing from the app for this long: closed
#define WS_HANDSHAKE_MS 2000   // the opening request must arrive within this
#define WS_REQUEST_MAX 1500

struct WsLink {
  WiFiClient c;
  bool used = false, open = false, sub = false;
  String req;
  unsigned long since = 0, lastRx = 0, lastPush = 0;
};
WsLink wsLinks[WS_CLIENTS];
WsDecoder wsDecoders[WS_CLIENTS];
WiFiServer wsServer(WS_PORT);
bool wsStarted = false;
static uint8_t wsOut[4096];

void wsClose(int i) {
  WsLink& l = wsLinks[i];
  if (l.open) {
    size_t n = ws_frame(wsOut, sizeof(wsOut), WS_OP_CLOSE, (const uint8_t*)"\x03\xe8", 2);  // 1000: normal
    if (n) l.c.write(wsOut, n);
  }
  l.c.stop();
  l.used = l.open = l.sub = false;
  l.req = "";
}

bool wsSend(int i, uint8_t op, const char* data, size_t len) {
  size_t n = ws_frame(wsOut, sizeof(wsOut), op, (const uint8_t*)data, len);
  if (n == 0 || wsLinks[i].c.write(wsOut, n) != n) {  // the network does not take it: closed, the app reconnects
    wsClose(i);
    return false;
  }
  return true;
}

// a number field of a message ("x":12.5), false when missing or not a finite number
static bool wsNumber(const char* m, const char* name, float* out) {
  char key[12];
  snprintf(key, sizeof(key), "\"%s\":", name);
  const char* p = strstr(m, key);
  if (p == NULL) return false;
  p += strlen(key);
  char* end;
  double v = strtod(p, &end);
  if (end == p || !isfinite(v)) return false;
  *out = (float)v;
  return true;
}

static bool wsType(const char* m, const char* type) {
  char key[24];
  snprintf(key, sizeof(key), "\"t\":\"%s\"", type);
  return strstr(m, key) != NULL;
}

void wsMessage(int i, const char* m) {
  WsLink& l = wsLinks[i];
  if (l.sub) cncAppSeenMs = millis();  // the page is watching (h4.ino cncLinkWatch), as a /status request
  if (wsType(m, "sub")) {
    l.sub = true;
    cncAppSeenMs = millis();
    l.lastPush = 0;  // the state at once
  } else if (wsType(m, "jog")) {
    float v[3];
    const char* refused;
    if (!wsNumber(m, "x", &v[0]) || !wsNumber(m, "z", &v[1]) || !wsNumber(m, "c", &v[2])) {
      refused = "Bad jog message";
      webJogRequest(0, 0, 0);  // whatever it meant: stop
    } else {
      refused = jogFromApp(v[0], v[1], v[2]);
    }
    String a = refused == NULL ? String("{\"t\":\"jog\",\"ok\":1}") : "{\"t\":\"jog\",\"ok\":0,\"e\":\"" + jsonEscape(String(refused)) + "\"}";
    wsSend(i, WS_OP_TEXT, a.c_str(), a.length());
  }
  // "alive" and anything unknown: only that the app is there
}

// The opening HTTP request of a connection: answered 101 with the accept key, or 400 and closed.
void wsHandshake(int i) {
  WsLink& l = wsLinks[i];
  while (l.c.available() > 0 && l.req.length() < WS_REQUEST_MAX) l.req += (char)l.c.read();
  if (l.req.indexOf("\r\n\r\n") < 0) {
    if (l.req.length() >= WS_REQUEST_MAX || millis() - l.since > WS_HANDSHAKE_MS) wsClose(i);
    return;
  }
  char key[80], accept[32], upgrade[32];
  bool ok = ws_header(l.req.c_str(), "Sec-WebSocket-Key", key, sizeof(key)) && ws_accept_key(key, accept) &&
            ws_header(l.req.c_str(), "Upgrade", upgrade, sizeof(upgrade)) && strcasecmp(upgrade, "websocket") == 0;
  if (!ok) {
    const char* bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    l.c.write((const uint8_t*)bad, strlen(bad));
    wsClose(i);
    return;
  }
  String r = String("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ") + accept + "\r\n\r\n";
  l.c.write((const uint8_t*)r.c_str(), r.length());
  l.req = "";
  l.open = true;
  l.lastRx = millis();
  wsDecoders[i].reset();
}

// In taskWifi, every pass.
void wsLoop() {
  if (!wsStarted) {
    wsServer.begin();
    wsServer.setNoDelay(true);
    wsStarted = true;
  }
  if (wsServer.hasClient()) {
    WiFiClient incoming = wsServer.accept();
    int slot = -1;
    unsigned long oldest = 0;
    for (int i = 0; i < WS_CLIENTS; i++) {
      if (!wsLinks[i].used) { slot = i; break; }
      if (slot < 0 || (long)(oldest - wsLinks[i].lastRx) > 0) { slot = i; oldest = wsLinks[i].lastRx; }
    }
    if (wsLinks[slot].used) wsClose(slot);  // all taken: the one quiet the longest makes room
    WsLink& l = wsLinks[slot];
    l.c = incoming;
    l.c.setNoDelay(true);
    l.used = true;
    l.open = l.sub = false;
    l.req = "";
    l.since = l.lastRx = millis();
  }
  String state;  // built once per pass for every connection that wants it
  for (int i = 0; i < WS_CLIENTS; i++) {
    WsLink& l = wsLinks[i];
    if (!l.used) continue;
    if (!l.c.connected()) {
      wsClose(i);
      continue;
    }
    if (!l.open) {
      wsHandshake(i);
      continue;
    }
    // what came in, up to 2 KB per pass so that the other connections and the HTTP server are not kept waiting
    for (int n = 0; n < 2048 && l.used && l.c.available() > 0; n++) {
      int r = wsDecoders[i].push((uint8_t)l.c.read());
      if (r == WS_ERROR) {
        wsClose(i);
        break;
      }
      if (r != WS_MSG) continue;
      l.lastRx = millis();
      uint8_t op = wsDecoders[i].opcode();
      if (op == WS_OP_TEXT) wsMessage(i, wsDecoders[i].data());
      else if (op == WS_OP_PING) wsSend(i, WS_OP_PONG, wsDecoders[i].data(), wsDecoders[i].length());
      else if (op == WS_OP_CLOSE) wsClose(i);
    }
    if (!l.used) continue;
    if (millis() - l.lastRx > WS_SILENT_MS) {
      wsClose(i);
      continue;
    }
    if (l.sub && millis() - l.lastPush >= WS_PUSH_MS) {
      l.lastPush = millis();
      if (state.length() == 0) state = statusJson();
      wsSend(i, WS_OP_TEXT, state.c_str(), state.length());
    }
  }
}
