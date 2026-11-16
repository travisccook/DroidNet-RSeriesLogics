// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Shared helpers for the RSeries behaviour tests: send a line, run engine frames, and mark a
// defect the suite knows about. Fork-local (not shared with the PSI and Flthy forks).
#pragma once
#include "mock_engine.h"
#include "../../src/contract/ContractLogics.h"
#include <cstdio>

static int gFails = 0, gKnown = 0;   // gKnown: defects still marked KNOWN_BUG
#define CHECK(cond) do { if (!(cond)) { printf("FAIL :%d  %s\n", __LINE__, #cond); gFails++; } } while (0)
// A defect from the 2026-11-15 review that is still in the code. `cond` is what CORRECT firmware
// does. While the defect is there the line prints XFAIL and the suite stays green. The task that
// fixes it turns the line into CHECK(cond); if it starts passing before that, the suite fails.
#define KNOWN_BUG(tag, cond) do { if (cond) { printf("XPASS %s :%d  fixed: make it a CHECK\n", tag, __LINE__); gFails++; } \
                                  else { printf("XFAIL %s\n", tag); gKnown++; } } while (0)

static void send(const char* afterBang) { contractHandleLine(afterBang); }
// Power-on state for every fork-owned global and for the mock engine.
static void resetAll() {
  for (uint8_t u = 0; u < 2; u++) { gContract[u] = ContractUnitState(); gBriTouched[u] = false; }
  gBeat = BeatClock();
  gLevel = 0;
  gLastLineMs = 0; gQueryLen = 0;
  mockEngineReset();
  _mock_millis = 100000;
}
// The full-field level of a display, 0..255: the mean over its pixels of each pixel's brightest
// channel (so a red field counts like a white one). This is what the eye gets from the whole
// panel, and what the S9 property is measured on. Assert on THIS for "dark" and "lit": the
// selected sequence number says what was asked for, not what is on the LEDs.
static int fieldLevel(const LogicEngineRenderer* r) {
  long sum = 0;
  for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) {
    const CRGB c = r->px(x, y);
    sum += c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
  }
  return (int)(sum / 80);
}
// One mainLoop() pass at the current time: the engine renders, then the contract ticks.
static void frame() { mockEngineFrame(*FLD); mockEngineFrame(*RLD); contractTick(); }
static inline void run(uint32_t ms, uint32_t step = 10) {
  for (uint32_t end = _mock_millis + ms; _mock_millis != end; ) { _mock_millis += step; frame(); }
}
