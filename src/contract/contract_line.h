// Part of the DroidNet Driveable-Animation Contract — an additive layer added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// src/contract/contract_line.h — the line assembler and the drain loop behind the fork's Serial1
// reader. Pure C++ (no Arduino), host-tested (test/host/test_contract_line.cpp). Fork-local: the
// PSI and Flthy forks have their own readers.
//
// WHY THE FORK HAS ITS OWN READER. The library's MarcduinoSerial reads ONE byte per loop pass
// (lib/Reeltwo/src/core/Marcduino.h). The loop passes at most once a millisecond and spends about
// 3 ms of every 10 in FastLED.show(), so it drains roughly 700-800 bytes a second, while 9600 baud
// delivers 960. Native Marcduino commands are short and rare, so that never mattered; a contract
// score is kilobytes back to back, and the UART's 256-byte receive buffer would overflow — silently,
// since nothing on this wire is acknowledged. It also made the beat-clock seed late by however
// much was queued in front of it. This reader drains everything that has arrived, every pass.
#pragma once
#include <stdint.h>
#include <string.h>

// One line, with its NUL. The longest line Studio can emit is 102 characters (a scored A with
// every key, an=/ap= included; test/host/run.sh stage 3 re-derives that), so 128 leaves room.
static const uint8_t CONTRACT_LINE_BUF = 128;
static const uint8_t CONTRACT_DRAIN_MAX = 255;      // bytes taken in one pass, at most

struct ContractLineBuf {
  char    buf[CONTRACT_LINE_BUF];
  uint8_t pos = 0;
  bool    over = false;
  // Feed one byte. true: buf holds a complete NUL-terminated line (the CR is not stored).
  //   * A line is framed on its terminator (CR) alone: no timeout, no gap (contract section 11).
  //   * An LF at the start of a line is skipped, so a host that sends CR LF works.
  //   * A '!' line that overflowed the buffer is DROPPED: its truncation would still parse, and
  //     would quietly play a different show. Any other line keeps the stock behaviour (it is
  //     processed as far as it fit), so no native command changes.
  bool feed(char ch) {
    if (ch == 0x0D) {
      buf[pos] = '\0';
      const bool keep = buf[0] != '\0' && !(over && buf[0] == '!');
      pos = 0; over = false;
      return keep;
    }
    if (ch == 0x0A && pos == 0) return false;
    if (pos < CONTRACT_LINE_BUF - 1) buf[pos++] = ch; else over = true;
    return false;
  }
};

// How many bytes one pass may take when every byte is echoed to a pass-through stream first.
// The echo's write() WAITS for room, and the reader runs on the task that renders the LEDs, so a
// pass never takes more than the pass-through can accept without waiting:
//   * `room` > 0 (its availableForWrite()): that many, at most CONTRACT_DRAIN_MAX;
//   * no room, on a stream that has reported room before (`sawRoom`): NONE this pass. The bytes
//     wait in the UART's receive buffer;
//   * a stream that never reports room (Print's default availableForWrite() is 0): the stock one
//     byte a pass, which may wait one byte time, as the library's reader always did.
// THE LIMIT THIS LEAVES. The reader can then take bytes no faster than the pass-through sends
// them. With the pass-through at the input's baud that is no limit. With a slower one (the
// stock Serial2 pass-through is 2400 baud against 9600 in), a back-to-back burst is absorbed by
// the receive buffer (1024 bytes, see main.cpp) and the pass-through's own FIFO, about 1.2 KB in
// all; beyond that, bytes are lost in the UART, and verb V reports the load bad. Section 11's
// 400-byte burst fits either way.
inline uint8_t contractDrainBudget(int room, bool& sawRoom) {
  if (room > 0) { sawRoom = true; return room > CONTRACT_DRAIN_MAX ? CONTRACT_DRAIN_MAX : (uint8_t)room; }
  return sawRoom ? 0 : 1;
}

// A line as the library's WiFi receiver hands it to its command handler. That receiver frames on
// CR alone, keeps an LF that follows it as the first byte of the NEXT line, and cuts a line off
// silently at its buffer (`bufSize` bytes with the NUL). Returns the line to process, or nullptr
// to drop it:
//   * one leading LF is skipped, so a host that sends CR LF works (without this every contract
//     line after the first began with LF, was not a '!' line, and was lost);
//   * a contract line that fills the buffer was cut off, or may have been: dropped, for the same
//     reason the Serial1 reader drops one (it would still parse, and play a different show). The
//     longest line Studio emits is 102 characters; the buffer holds 127.
// Any other line is returned as it came, cut off or not: no native command changes.
inline const char* contractWifiLine(const char* cmd, size_t bufSize) {
  if (cmd == nullptr) return nullptr;
  const bool full = strlen(cmd) + 1 >= bufSize;
  if (*cmd == 0x0A) cmd++;
  if (*cmd == '\0') return nullptr;
  if (full && *cmd == '!') return nullptr;
  return cmd;
}

// One pass of the reader: take up to `budget` bytes from `in` (anything with available() and
// read()), handing each to pass(uint8_t) first (the pass-through echo).
//   * A contract line ('!' first) goes to onContract(afterBang) AT ONCE, and the drain goes on:
//     the contract layer runs in this call, so several lines in one pass all run, in order.
//   * Any other line goes to onNative(line), and the drain STOPS for this pass: the Marcduino
//     dispatcher only queues the command for the animation player, which reads it from this very
//     buffer a moment later, so the buffer must not be touched again before then.
// Returns the bytes taken.
template <class In, class Pass, class OnContract, class OnNative>
inline uint8_t contractDrain(ContractLineBuf& lb, In& in, uint8_t budget, Pass pass,
                             OnContract onContract, OnNative onNative) {
  uint8_t n = 0;
  while (n < budget && in.available()) {
    const int ch = in.read();
    if (ch < 0) break;
    n++;
    pass((uint8_t)ch);
    if (!lb.feed((char)ch)) continue;
    if (lb.buf[0] == '!') { onContract(lb.buf + 1); continue; }
    onNative(lb.buf);
    break;
  }
  return n;
}
