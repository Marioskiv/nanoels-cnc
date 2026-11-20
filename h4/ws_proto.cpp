/*
  ws_proto.cpp - the WebSocket protocol (RFC 6455) for the H4's one lasting link to the app (see ws_proto.h).
  SHA-1 as in FIPS 180-4, Base64 as in RFC 4648, written here for this use.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "ws_proto.h"
#include <ctype.h>
#include <string.h>

// ---------------------------------------------------------------- SHA-1

static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1Block(uint32_t h[5], const uint8_t* p) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
  for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
    else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
    else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
    else { f = b ^ c ^ d; k = 0xCA62C1D6; }
    uint32_t t = rol(a, 5) + f + e + k + w[i];
    e = d; d = c; c = rol(b, 30); b = a; a = t;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void ws_sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  size_t i = 0;
  for (; i + 64 <= len; i += 64) sha1Block(h, data + i);
  uint8_t last[128];
  size_t rest = len - i;
  memcpy(last, data + i, rest);
  last[rest] = 0x80;
  size_t total = rest + 1 + 8 <= 64 ? 64 : 128;
  memset(last + rest + 1, 0, total - rest - 1);
  uint64_t bits = (uint64_t)len * 8;
  for (int k = 0; k < 8; k++) last[total - 1 - k] = (uint8_t)(bits >> (8 * k));
  sha1Block(h, last);
  if (total == 128) sha1Block(h, last + 64);
  for (int k = 0; k < 5; k++) {
    out[4 * k] = (uint8_t)(h[k] >> 24);
    out[4 * k + 1] = (uint8_t)(h[k] >> 16);
    out[4 * k + 2] = (uint8_t)(h[k] >> 8);
    out[4 * k + 3] = (uint8_t)h[k];
  }
}

// ---------------------------------------------------------------- Base64

size_t ws_base64(const uint8_t* data, size_t len, char* out) {
  static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)data[i] << 16 | (i + 1 < len ? (uint32_t)data[i + 1] << 8 : 0) | (i + 2 < len ? data[i + 2] : 0);
    out[o++] = A[(v >> 18) & 63];
    out[o++] = A[(v >> 12) & 63];
    out[o++] = i + 1 < len ? A[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < len ? A[v & 63] : '=';
  }
  out[o] = '\0';
  return o;
}

bool ws_accept_key(const char* key, char out[32]) {
  static const char* GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  size_t n = strlen(key);
  if (n == 0 || n > 60) return false;
  char buf[100];
  memcpy(buf, key, n);
  memcpy(buf + n, GUID, 36);
  uint8_t h[20];
  ws_sha1((const uint8_t*)buf, n + 36, h);
  ws_base64(h, 20, out);
  return true;
}

bool ws_header(const char* request, const char* name, char* out, size_t cap) {
  size_t nl = strlen(name);
  const char* p = request;
  while (p && *p) {
    const char* eol = strstr(p, "\r\n");
    size_t lineLen = eol ? (size_t)(eol - p) : strlen(p);
    if (lineLen > nl && p[nl] == ':') {
      bool same = true;
      for (size_t i = 0; i < nl && same; i++) same = tolower((unsigned char)p[i]) == tolower((unsigned char)name[i]);
      if (same) {
        const char* v = p + nl + 1;
        const char* end = p + lineLen;
        while (v < end && (*v == ' ' || *v == '\t')) v++;
        while (end > v && (end[-1] == ' ' || end[-1] == '\t')) end--;
        size_t vl = (size_t)(end - v);
        if (vl + 1 > cap) return false;
        memcpy(out, v, vl);
        out[vl] = '\0';
        return true;
      }
    }
    if (!eol) break;
    p = eol + 2;
  }
  return false;
}

// ---------------------------------------------------------------- frames out

static size_t frameHeader(uint8_t* out, size_t cap, uint8_t opcode, size_t len, bool masked) {
  size_t h = len < 126 ? 2 : (len < 65536 ? 4 : 10);
  if (cap < h) return 0;
  out[0] = (uint8_t)(0x80 | (opcode & 0x0F));
  uint8_t m = masked ? 0x80 : 0;
  if (len < 126) {
    out[1] = (uint8_t)(m | len);
  } else if (len < 65536) {
    out[1] = (uint8_t)(m | 126);
    out[2] = (uint8_t)(len >> 8);
    out[3] = (uint8_t)len;
  } else {
    out[1] = (uint8_t)(m | 127);
    for (int i = 0; i < 8; i++) out[2 + i] = (uint8_t)((uint64_t)len >> (8 * (7 - i)));
  }
  return h;
}

size_t ws_frame(uint8_t* out, size_t cap, uint8_t opcode, const uint8_t* payload, size_t len) {
  size_t h = frameHeader(out, cap, opcode, len, false);
  if (h == 0 || h + len > cap) return 0;
  if (len) memcpy(out + h, payload, len);
  return h + len;
}

size_t ws_frame_masked(uint8_t* out, size_t cap, uint8_t opcode, const uint8_t* payload, size_t len, const uint8_t mask[4]) {
  size_t h = frameHeader(out, cap, opcode, len, true);
  if (h == 0 || h + 4 + len > cap) return 0;
  memcpy(out + h, mask, 4);
  for (size_t i = 0; i < len; i++) out[h + 4 + i] = payload[i] ^ mask[i & 3];
  return h + 4 + len;
}

// ---------------------------------------------------------------- frames in

enum { S_HEAD0, S_HEAD1, S_LEN, S_MASK, S_DATA };

void WsDecoder::reset() {
  state_ = S_HEAD0;
  inPieces_ = false;
  partLen_ = 0;
  msgOp_ = 0;
  outOp_ = 0;
  outData_ = msg_;
  outLen_ = 0;
}

int WsDecoder::push(uint8_t b) {
  switch (state_) {
    case S_HEAD0:
      if (b & 0x70) return WS_ERROR;  // reserved bits: no extension was agreed
      fin_ = (b & 0x80) != 0;
      op_ = b & 0x0F;
      if (op_ >= 0x8) {
        if (!fin_ || (op_ != WS_OP_CLOSE && op_ != WS_OP_PING && op_ != WS_OP_PONG)) return WS_ERROR;
      } else if (op_ == WS_OP_CONT) {
        if (!inPieces_) return WS_ERROR;
      } else if (op_ == WS_OP_TEXT || op_ == WS_OP_BINARY) {
        if (inPieces_) return WS_ERROR;  // a new message before the last one ended
        msgOp_ = op_;
        partLen_ = 0;
      } else {
        return WS_ERROR;
      }
      state_ = S_HEAD1;
      return WS_NONE;
    case S_HEAD1:
      masked_ = (b & 0x80) != 0;
      if (masked_ != fromClient_) return WS_ERROR;  // a client masks every frame, a server none
      len_ = b & 0x7F;
      got_ = 0;
      if (len_ == 126) { lenBytes_ = 2; len_ = 0; state_ = S_LEN; return WS_NONE; }
      if (len_ == 127) { lenBytes_ = 8; len_ = 0; state_ = S_LEN; return WS_NONE; }
      if (op_ >= 0x8 && len_ > 125) return WS_ERROR;
      state_ = masked_ ? S_MASK : S_DATA;
      if (state_ == S_DATA && len_ == 0) return frameDone();
      return WS_NONE;
    case S_LEN:
      len_ = (len_ << 8) | b;
      if (--lenBytes_ > 0) return WS_NONE;
      if (op_ >= 0x8) return WS_ERROR;  // control frames are at most 125 bytes
      if (len_ > WS_MAX_MESSAGE || partLen_ + len_ > WS_MAX_MESSAGE) return WS_ERROR;
      state_ = masked_ ? S_MASK : S_DATA;
      if (state_ == S_DATA && len_ == 0) return frameDone();
      return WS_NONE;
    case S_MASK:
      mask_[got_++] = b;
      if (got_ < 4) return WS_NONE;
      got_ = 0;
      if (op_ < 0x8 && partLen_ + len_ > WS_MAX_MESSAGE) return WS_ERROR;
      state_ = S_DATA;
      if (len_ == 0) return frameDone();
      return WS_NONE;
    default: {  // S_DATA
      uint8_t v = masked_ ? (uint8_t)(b ^ mask_[got_ & 3]) : b;
      if (op_ >= 0x8) ctl_[got_] = v;
      else msg_[partLen_ + got_] = v;
      got_++;
      if (got_ < len_) return WS_NONE;
      return frameDone();
    }
  }
}

int WsDecoder::frameDone() {
  state_ = S_HEAD0;
  if (op_ >= 0x8) {
    // a control frame (ping, pong, close) is a message of its own, also between the pieces of a message, which go on
    ctl_[len_] = 0;
    outOp_ = op_;
    outData_ = ctl_;
    outLen_ = (size_t)len_;
    return WS_MSG;
  }
  partLen_ += (size_t)len_;
  if (!fin_) {
    inPieces_ = true;
    return WS_NONE;
  }
  inPieces_ = false;
  msg_[partLen_] = 0;
  outOp_ = msgOp_;
  outData_ = msg_;
  outLen_ = partLen_;
  partLen_ = 0;
  return WS_MSG;
}
