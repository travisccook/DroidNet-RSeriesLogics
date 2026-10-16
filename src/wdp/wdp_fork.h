// src/wdp/wdp_fork.h — the RSeries fork's WDP device announce: this board's identity and the
// two calls src/main.cpp makes. wdp_announce_core.h (shared with the PSI and Flthy forks) builds
// and paces the line; this file only says what the board is and where the line goes:
//
//     @WDP1 {"type":"R-Series Logics","fw":"<commit>","caps":["droidnet.contract","led.logic"]}
//
// The port is Serial1, the Marcduino serial port the WCB is wired to. Nothing else in the
// firmware writes to Serial1: MarcduinoSerial only reads it (its pass-through echoes to Serial2
// or Serial), and the console and core log output are UART0. So no other writer has to finish
// a line first with wdpRest(). Both calls run on the task that reads Serial1: wdpForkInit() in
// setup() before the event task starts, wdpForkTick() in mainLoop() on that task.
//
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#include "wdp_announce_core.h"

// The build's commit: scripts/pre_extra.py defines it from git rev-parse --short=7 HEAD, with
// "-dirty" for a tree with changes. A build without that script says "unknown".
#ifndef WDP_FW
#define WDP_FW "unknown"
#endif
#define WDP_TYPE "R-Series Logics"   // the WCB Wizard's name for it (Wizard/device-labels.js)
#define WDP_CAPS "droidnet.contract led.logic"

// "" X compiles only when X is a string literal, so a WDP_FW define that lost its quotes fails
// the build here instead of announcing garbage.
static_assert(sizeof("" WDP_FW) - 1 <= WDP_FW_MAX, "WDP_FW must be a string of at most WDP_FW_MAX chars");
static_assert(sizeof("" WDP_TYPE) - 1 <= WDP_TYPE_MAX, "WDP_TYPE is longer than the WCB keeps");
static_assert(sizeof("" WDP_CAPS) - 1 <= WDP_CAPS_MAX, "WDP_CAPS is longer than the WCB keeps");

static WdpAnnouncer sWdpAnnouncer;
static bool sWdpOn = false;   // false: the Marcduino serial port is off and Serial1 never began

// The seed for the announce jitter: the hardware RNG mixed with the chip's factory MAC, so two
// boards differ even when the RNG has no entropy yet (WiFi is not up this early in setup()).
inline uint32_t wdpForkSeed(uint32_t rng, uint64_t mac) {
    return rng ^ (uint32_t)mac ^ (uint32_t)(mac >> 32);
}

// Once, right after Serial1.begin().
inline void wdpForkInit(uint32_t seed, uint32_t nowMs) {
    wdpInit(sWdpAnnouncer, seed, nowMs);
    sWdpOn = true;
}

// Every mainLoop() pass. Writes only what fits in the port's free TX room, so it never blocks.
template <class Port>
inline void wdpForkTick(Port& port, uint32_t nowMs) {
    if (!sWdpOn) return;
    const char* chunk = 0;
    uint8_t n = wdpTick(sWdpAnnouncer, nowMs, WDP_TYPE, WDP_FW, WDP_CAPS, port.availableForWrite(), &chunk);
    if (n) port.write((const uint8_t*)chunk, n);
}
