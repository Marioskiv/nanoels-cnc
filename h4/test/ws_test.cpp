/*
  ws_test.cpp - the WebSocket protocol of the H4's lasting link to the app (ws_proto.cpp): SHA-1 and Base64 against
  their standard test vectors, the handshake key of RFC 6455's own example, header reading, and the frame reader
  with frames as a browser / the app send them (masked, in pieces, with a ping between pieces, 16-bit lengths) plus
  frames it must refuse; then 20000 random messages in random-sized chunks, read back exactly.
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "../ws_proto.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int checks = 0, failures = 0;
static void check(bool ok, const char* what) {
  checks++;
  if (!ok) {
    failures++;
    printf("FAIL: %s\n", what);
  }
}

static std::string hex(const uint8_t* h, int n) {
  std::string s;
  char b[3];
  for (int i = 0; i < n; i++) { snprintf(b, sizeof(b), "%02x", h[i]); s += b; }
  return s;
}
static std::string sha1(const std::string& s) {
  uint8_t h[20];
  ws_sha1((const uint8_t*)s.data(), s.size(), h);
  return hex(h, 20);
}
static std::string b64(const std::string& s) {
  char out[200];
  ws_base64((const uint8_t*)s.data(), s.size(), out);
  return out;
}

struct Msg { int op; std::string data; };
// feeds bytes, collects messages; false on a protocol error
static bool feed(WsDecoder& d, const std::vector<uint8_t>& bytes, std::vector<Msg>& out) {
  for (uint8_t b : bytes) {
    int r = d.push(b);
    if (r == WS_ERROR) return false;
    if (r == WS_MSG) out.push_back({d.opcode(), std::string(d.data(), d.length())});
  }
  return true;
}
static std::vector<uint8_t> masked(int op, const std::string& s, bool fin = true) {
  std::vector<uint8_t> f(s.size() + 20);
  uint8_t m[4] = {0x12, 0x34, 0x56, 0x78};
  size_t n = ws_frame_masked(f.data(), f.size(), (uint8_t)op, (const uint8_t*)s.data(), s.size(), m);
  f.resize(n);
  if (!fin) f[0] &= 0x7F;
  return f;
}

int main() {
  // SHA-1: FIPS 180 / RFC 3174 test vectors (also messages that end on and across the 56-byte boundary)
  check(sha1("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1 abc");
  check(sha1("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "sha1 empty");
  check(sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "sha1 448 bits");
  check(sha1(std::string(1000000, 'a')) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "sha1 a million a");
  check(sha1("The quick brown fox jumps over the lazy dog") == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12", "sha1 fox");
  // Base64: RFC 4648 section 10
  const char* bv[][2] = {{"", ""}, {"f", "Zg=="}, {"fo", "Zm8="}, {"foo", "Zm9v"}, {"foob", "Zm9vYg=="}, {"fooba", "Zm9vYmE="}, {"foobar", "Zm9vYmFy"}};
  for (auto& v : bv) check(b64(v[0]) == v[1], v[0]);
  // the handshake: RFC 6455 section 1.3
  char acc[32];
  check(ws_accept_key("dGhlIHNhbXBsZSBub25jZQ==", acc) && strcmp(acc, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0, "accept key of RFC 6455");
  check(!ws_accept_key("", acc), "empty key refused");
  const char* req = "GET / HTTP/1.1\r\nHost: 192.168.4.1:81\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                    "sec-websocket-key:   dGhlIHNhbXBsZSBub25jZQ==  \r\nSec-WebSocket-Version: 13\r\n\r\n";
  char v[80];
  check(ws_header(req, "Sec-WebSocket-Key", v, sizeof(v)) && strcmp(v, "dGhlIHNhbXBsZSBub25jZQ==") == 0, "header in any letter case, trimmed");
  check(ws_header(req, "upgrade", v, sizeof(v)) && strcmp(v, "websocket") == 0, "upgrade header");
  check(!ws_header(req, "Origin", v, sizeof(v)), "missing header");

  // frames: RFC 6455 section 5.7 examples (a masked "Hello" from a client, an unmasked one from a server)
  {
    WsDecoder d(true);
    std::vector<Msg> out;
    std::vector<uint8_t> f = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
    check(feed(d, f, out) && out.size() == 1 && out[0].op == WS_OP_TEXT && out[0].data == "Hello", "RFC masked Hello");
    WsDecoder s(false);
    out.clear();
    check(feed(s, {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f}, out) && out.size() == 1 && out[0].data == "Hello", "RFC unmasked Hello (server side reader)");
    uint8_t o[20];
    size_t n = ws_frame(o, sizeof(o), WS_OP_TEXT, (const uint8_t*)"Hello", 5);
    check(n == 7 && memcmp(o, "\x81\x05Hello", 7) == 0, "server frame Hello");
  }
  // in pieces with a ping between them; a 300-byte message (16-bit length)
  {
    WsDecoder d(true);
    std::vector<Msg> out;
    std::vector<uint8_t> all;
    auto a = masked(WS_OP_TEXT, "Hel", false), p = masked(WS_OP_PING, "x"), b = masked(WS_OP_CONT, "lo", true);
    all.insert(all.end(), a.begin(), a.end());
    all.insert(all.end(), p.begin(), p.end());
    all.insert(all.end(), b.begin(), b.end());
    std::string big(300, 'q');
    auto c = masked(WS_OP_TEXT, big);
    all.insert(all.end(), c.begin(), c.end());
    check(feed(d, all, out) && out.size() == 3 && out[0].op == WS_OP_PING && out[0].data == "x" && out[1].data == "Hello" &&
              out[2].data == big, "pieces with a ping between them, then a 300-byte message");
  }
  // refused: a client frame not masked, too long, a control frame in pieces, a continuation without a start
  {
    WsDecoder d(true);
    std::vector<Msg> out;
    check(!feed(d, {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'}, out), "unmasked client frame refused");
    d.reset();
    check(!feed(d, masked(WS_OP_TEXT, std::string(WS_MAX_MESSAGE + 1, 'a')), out), "too long refused");
    d.reset();
    auto ping = masked(WS_OP_PING, "x", false);
    check(!feed(d, ping, out), "control frame in pieces refused");
    d.reset();
    check(!feed(d, masked(WS_OP_CONT, "x"), out), "continuation without a start refused");
    d.reset();
    std::vector<uint8_t> rsv = masked(WS_OP_TEXT, "x");
    rsv[0] |= 0x40;
    check(!feed(d, rsv, out), "reserved bit refused");
    d.reset();
    out.clear();
    check(feed(d, masked(WS_OP_CLOSE, ""), out) && out.size() == 1 && out[0].op == WS_OP_CLOSE, "close");
  }
  // 20000 random messages (pieces, pings, lengths 0 ... 2048) in random chunks
  {
    unsigned s = 1;
    auto rnd = [&s]() { s = s * 1103515245 + 12345; return (s >> 16) & 0x7fff; };
    WsDecoder d(true);
    std::vector<uint8_t> stream;
    std::vector<Msg> want, got;
    for (int i = 0; i < 20000; i++) {
      size_t len = rnd() % 8 == 0 ? rnd() % (WS_MAX_MESSAGE + 1) : rnd() % 200;
      std::string m;
      for (size_t k = 0; k < len; k++) m += (char)(rnd() & 0xff);
      int parts = 1 + rnd() % 3;
      size_t at = 0;
      for (int p = 0; p < parts; p++) {
        size_t n = p == parts - 1 ? len - at : (len - at) * (rnd() % 100) / 100;
        auto f = masked(p == 0 ? WS_OP_TEXT : WS_OP_CONT, m.substr(at, n), p == parts - 1);
        stream.insert(stream.end(), f.begin(), f.end());
        at += n;
        if (p < parts - 1 && rnd() % 10 == 0) {  // a ping between the pieces comes out before the message
          auto pg = masked(WS_OP_PING, "p");
          stream.insert(stream.end(), pg.begin(), pg.end());
          want.push_back({WS_OP_PING, "p"});
        }
      }
      want.push_back({WS_OP_TEXT, m});
    }
    bool ok = true;
    for (size_t i = 0; i < stream.size() && ok;) {
      size_t n = 1 + rnd() % 3000;
      std::vector<uint8_t> chunk(stream.begin() + i, stream.begin() + std::min(stream.size(), i + n));
      ok = feed(d, chunk, got);
      i += n;
    }
    bool same = ok && got.size() == want.size();
    for (size_t i = 0; same && i < got.size(); i++) same = got[i].op == want[i].op && got[i].data == want[i].data;
    check(same, "20000 random messages in random chunks read back exactly");
  }
  printf("ws: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
