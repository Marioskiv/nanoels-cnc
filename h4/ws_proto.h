/*
  ws_proto.h - the WebSocket protocol (RFC 6455) for the H4's one lasting link to the app: the handshake key, frames
  in and out. No Arduino dependency, so that it is tested on a PC (test/ws_test.cpp).
  Part of the NanoEls H4 CNC fork: https://github.com/Marioskiv/nanoels-cnc
  Copyright (c) 2026 Marioskiv
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include <stddef.h>
#include <stdint.h>

#define WS_MAX_MESSAGE 2048  // longest message taken from the app (the app sends short ones)

#define WS_OP_CONT 0x0
#define WS_OP_TEXT 0x1
#define WS_OP_BINARY 0x2
#define WS_OP_CLOSE 0x8
#define WS_OP_PING 0x9
#define WS_OP_PONG 0xA

// SHA-1 of data (20 bytes out).
void ws_sha1(const uint8_t* data, size_t len, uint8_t out[20]);
// Base64 of data; out must hold 4 * ((len + 2) / 3) + 1 characters. Returns the length written (without the 0).
size_t ws_base64(const uint8_t* data, size_t len, char* out);
// Sec-WebSocket-Accept for a Sec-WebSocket-Key (out: 29 characters + 0). False when the key is not 1-60 characters.
bool ws_accept_key(const char* key, char out[32]);
// The value of a header in an HTTP request (name without ':', any letter case), trimmed; false when absent.
bool ws_header(const char* request, const char* name, char* out, size_t cap);

// A frame from the server (never masked, always one piece). Returns its length, 0 when cap is too small.
size_t ws_frame(uint8_t* out, size_t cap, uint8_t opcode, const uint8_t* payload, size_t len);
// A frame from a client (masked with mask): for tests and a client.
size_t ws_frame_masked(uint8_t* out, size_t cap, uint8_t opcode, const uint8_t* payload, size_t len, const uint8_t mask[4]);

// Reads the frames of one connection a byte at a time. push() returns:
//   WS_NONE  the frame / message goes on;
//   WS_MSG   a whole message: opcode() (TEXT, BINARY, PING, PONG or CLOSE) and data() / length() (a text is 0-ended);
//   WS_ERROR a protocol error (a client's frame not masked, too long, a control frame in pieces, ...): close it.
#define WS_NONE 0
#define WS_MSG 1
#define WS_ERROR (-1)
class WsDecoder {
 public:
  WsDecoder(bool fromClient = true) : fromClient_(fromClient) { reset(); }
  void reset();
  int push(uint8_t b);
  uint8_t opcode() const { return outOp_; }
  const char* data() const { return (const char*)outData_; }
  size_t length() const { return outLen_; }

 private:
  bool fromClient_;
  int state_;
  uint8_t op_, msgOp_, mask_[4], lenBytes_;
  bool fin_, masked_, inPieces_;
  uint64_t len_, got_;
  // control frames (ping, close) may come between the pieces of a message: their own buffer
  uint8_t ctl_[126];
  uint8_t msg_[WS_MAX_MESSAGE + 1];
  size_t partLen_;
  uint8_t outOp_;
  const uint8_t* outData_;
  size_t outLen_;
  int frameDone();
};
