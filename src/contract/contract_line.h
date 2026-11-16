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
