// Part of the DroidNet Driveable-Animation Contract — an additive layer added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
// src/contract/contract_line.h — the line assembler behind the fork's Serial1 reader. Pure C++
// (no Arduino), host-tested. Fork-local: the PSI and Flthy forks keep their own readers.
#pragma once
#include <stdint.h>

static const uint8_t CONTRACT_LINE_BUF = 96;   // the longest Studio line is 90 chars (run.sh stage 3)

struct ContractLineBuf {
  char    buf[CONTRACT_LINE_BUF];
  uint8_t pos = 0;
  bool    over = false;
  // Feed one byte. true: buf holds a complete NUL-terminated line (the CR is not stored).
  // A leading LF is skipped (contract spec 4.3). A '!' line that overflowed is DROPPED: its
  // truncation would still parse and play a different show. Any other line keeps the stock
  // behaviour (processed as far as it fit).
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
