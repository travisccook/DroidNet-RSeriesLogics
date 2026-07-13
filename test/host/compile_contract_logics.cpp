// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// Distributed as part of that LGPL-2.1 work; you may redistribute and/or modify it
// under the terms of the GNU Lesser General Public License, version 2.1. No warranty;
// see the LICENSE file at the repository root.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Host check of the firmware layer ContractLogics.h against mock_engine.h.
// Part TYPE-CHECK (proves the firmware C++ compiles with the verified engine API
// signatures — catches typos / wrong arity / type errors before the bench), part
// BEHAVIORAL GUARD over the mock's recorded pixels:
//   * a scored NATIVE section threads its native code into the ScoreEntry (the cross-fork
//     drift the 2026-07-12 fork audit fixed on PSI/Flthy);
//   * v1.2: the board fires its OWN effect-swap accent once per accent beat, never on a
//     non-accent beat, never per frame, and NEVER for a v1.1 entry (no ae=);
//   * SAFETY (photosensitivity): the accent strobe cap is >= 340 ms between STARTS and is
//     enforced UNCONDITIONALLY — a re-arm while an overlay is still on screen is REJECTED;
//   * v1.2: CE_FLASH's lit state is beat-pumped (it used to render at the raw ceiling).
#include "mock_engine.h"
#include "../../src/contract/ContractLogics.h"
#include <cstdio>

static int gFails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL :%d  %s\n", __LINE__, #cond); gFails++; } } while (0)

static void send(const char* afterBang) {
  ParsedContract p;
  if (contractParse(afterBang, p)) applyContract(p);
}
// Wipe every fork-owned global so each scenario starts from power-on state.
static void resetAll() {
  for (uint8_t u = 0; u < 2; u++) { gContract[u] = ContractUnit{}; gScoreLen[u] = 0; }
  gBeat = BeatClock{};
  gShowMode = false;
  gLevel = 0;
}
// Advance the board one tick and report whether an accent was armed ON THIS TICK.
// (pulseStartMs == now is only true for an overlay armed at this instant; a still-running
// overlay from an earlier beat was armed at an earlier millis.)
static bool tickFired(uint8_t u) {
  contractTick();
  return gContract[u].pulseActive && gContract[u].pulseStartMs == _mock_millis;
}

int main() {
  // exercise every fork entry point / verb / effect so the compiler type-checks them
  const char* cmds[] = {
    "LFA:i=solid,c=0080FF,d=0", "**P:c=FFFFFF,d=120", "**C:bpm=120,ph=0,bpb=4,beat=0",
    "LFB:v=120", "P*L:v=190", "L*A:i=rainbow", "LFA:i=scan,c=FF0000",
    "LRA:i=sparkle,c=00FF00,s=200", "LFA:i=meter", "LFA:i=solid,c=3B82F6,at=0,am=1",
    "LFA:i=fire,c=FF2A00,at=32,am=3", "**X", "**M:v=show", "**M:v=idle", "LFA:i=native:105",
    "L*A:i=comet,c=3B82F6,s=200",
    "L*A:i=chase,c=3B82F6,s=200", "L*A:i=wipe,c=3B82F6,s=200", "L*A:i=gradient,s=200",
    "L*A:i=colorcycle,s=200", "L*A:i=twinkle,c=3B82F6,s=200",
  };
  for (const char* s : cmds) { ParsedContract p; if (contractParse(s, p)) applyContract(p); }

  LogicEngineRenderer r;
  for (unsigned id = 1; id <= 2; id++) {
    r._id = id;
    for (uint32_t t = 0; t < 5000; t += 250) { _mock_millis = t; contractTick(); ContractRender(r); }
  }
  contractSetup();

  // replicate the MARCDUINO_ACTION(Contract, !, ...) body verbatim to type-check it
  { ParsedContract q; if (contractParse(Marcduino::getCommand(), q)) applyContract(q); }

  // behavioral guard: a scored NATIVE section must thread its native code into the
  // ScoreEntry (regression guard for the PSI/Flthy drift where nativeCode stayed -1).
  { ParsedContract p; if (contractParse("LFA:i=native:104,at=64", p)) applyContract(p); }
  if (gScoreLen[0] == 0 || gScore[0][gScoreLen[0] - 1].nativeCode != 104
      || gScore[0][gScoreLen[0] - 1].effect != CE_NATIVE) {
    printf("FAIL: RSeries scored native section did not thread nativeCode\n");
    return 1;
  }

  // =====================================================================================
  // v1.2 GUARD 1 — the SCORED accent self-fires on the board, edge-triggered.
  // A scored entry carries ae=/ac=/ad=; on every beat the SAME predicate the brightness
  // pump uses (beatAccentFires) says is an accent beat, the board must arm the overlay
  // itself — exactly once, without the Pi sending anything.
  // 120 BPM => a beat every 500 ms; 4 ticks per beat prove it is an EDGE, not a level.
  // =====================================================================================
  // 1a. am=1 (downbeat only, bpb=4): beats 0,4,8 accent; 1,2,3,5,... must not.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=3b82f6,at=0,am=1,m=200,ae=flash,ac=ffffff,ad=200");
  CHECK(gScoreLen[0] == 1);
  CHECK(gScore[0][0].accentFx == CE_FLASH);
  CHECK(gScore[0][0].accentColor.r == 0xff && gScore[0][0].accentColor.g == 0xff
        && gScore[0][0].accentColor.b == 0xff);
  CHECK(gScore[0][0].accentDur10 == 20);            // 200 ms / 10
  {
    LogicEngineRenderer rr; rr._id = 1;
    int fires[12] = {0};
    for (int k = 0; k < 12; k++)
      for (int sub = 0; sub < 4; sub++) {           // FOUR ticks inside every beat
        _mock_millis = 100000u + 500u * (uint32_t)k + 20u + 100u * (uint32_t)sub;
        if (tickFired(0)) fires[k]++;
        ContractRender(rr);                         // as mainLoop does — this retires an expired overlay
      }
    for (int k = 0; k < 12; k++) {
      int want = (k % 4 == 0) ? 1 : 0;              // am=1 => barPos 0 only
      if (fires[k] != want) {
        printf("FAIL :%d  am=1 beat %d fired %d time(s), expected %d\n", __LINE__, k, fires[k], want);
        gFails++;
      }
    }
    CHECK(gContract[0].pulseFx == CE_FLASH);        // the EFFECT crossed the wire, not just a colour
    CHECK(gContract[0].pulseColor.r == 0xff && gContract[0].pulseColor.b == 0xff);
    CHECK(gContract[0].pulseDurMs == 200);
    CHECK(!gContract[1].pulseActive);               // LFA addressed unit F only
  }

  // 1b. am=2 (every beat): all 12 beats accent, still exactly once each.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=3b82f6,at=0,am=2,m=200,ae=flash,ad=200");
  CHECK(gScore[0][0].accentColor.r == 0x3b && gScore[0][0].accentColor.g == 0x82
        && gScore[0][0].accentColor.b == 0xf6);     // no ac= => RESOLVED to the entry's c= at insert
  {
    LogicEngineRenderer rr; rr._id = 1;
    int total = 0;
    for (int k = 0; k < 12; k++) {
      int fires = 0;
      for (int sub = 0; sub < 4; sub++) {
        _mock_millis = 100000u + 500u * (uint32_t)k + 20u + 100u * (uint32_t)sub;
        if (tickFired(0)) fires++;
        ContractRender(rr);
      }
      if (fires != 1) {
        printf("FAIL :%d  am=2 beat %d fired %d time(s), expected 1\n", __LINE__, k, fires);
        gFails++;
      }
      total += fires;
    }
    CHECK(total == 12);
  }

  // 1c. BACKWARD COMPAT: a v1.1 scored entry (no ae=) must NEVER arm the overlay.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=3b82f6,at=0,am=2,m=200");
  CHECK(gScore[0][0].accentFx == CE_NONE);
  {
    LogicEngineRenderer rr; rr._id = 1;
    int fires = 0; bool everActive = false;
    for (int k = 0; k < 12; k++)
      for (int sub = 0; sub < 4; sub++) {
        _mock_millis = 100000u + 500u * (uint32_t)k + 20u + 100u * (uint32_t)sub;
        if (tickFired(0)) fires++;
        if (gContract[0].pulseActive) everActive = true;
        ContractRender(rr);
      }
    CHECK(fires == 0);
    CHECK(!everActive);
    CHECK(gContract[0].pulseStartMs == 0);
  }

  // 1d. The accent is an EFFECT SWAP, not the v1.1 solid fill. ae=off over a RED base with
  // no ac= (so the accent colour resolves to the base's own red): the v1.1 overlay would
  // fill RED, the v1.2 overlay renders CE_OFF => BLACK. Then it must restore the base.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=FF0000,b=200,at=0,am=2,ae=off,ad=200");
  {
    LogicEngineRenderer rr; rr._id = 1;
    _mock_millis = 100020;
    contractTick();
    CHECK(gContract[0].pulseActive);
    CHECK(gContract[0].pulseFx == CE_OFF);
    CHECK(gContract[0].pulseColor.r == 0xFF);       // a solid fill of pulseColor would be RED
    ContractRender(rr);
    CRGB during = rr.px(0, 0);
    CHECK(during.r == 0 && during.g == 0 && during.b == 0);   // the swapped-in CE_OFF renders black
    _mock_millis = 100020 + 210;                    // past ad=200 => the overlay expires
    contractTick();
    ContractRender(rr);
    CRGB after = rr.px(0, 0);
    CHECK(after.r > 0 && after.g == 0 && after.b == 0);       // base look restored (red)
  }

  // 1e. The beat edge must be dropped at EVERY show boundary, or a stale edge from the
  // previous show swallows the next show's first accent.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  gContract[0].lastAccentBeat = 42; send("**X");
  CHECK(gContract[0].lastAccentBeat == BEAT_NONE);                      // stop
  gContract[0].lastAccentBeat = 42; send("**M:v=idle");
  CHECK(gContract[0].lastAccentBeat == BEAT_NONE);                      // mode idle
  gContract[0].lastAccentBeat = 42; send("**M:v=show");
  CHECK(gContract[0].lastAccentBeat == BEAT_NONE);                      // mode show
  gContract[0].lastAccentBeat = 42; gContract[1].lastAccentBeat = 42;
  send("**C:bpm=128,ph=0,bpb=4,beat=0");
  CHECK(gContract[0].lastAccentBeat == BEAT_NONE);                      // clock re-seed
  CHECK(gContract[1].lastAccentBeat == BEAT_NONE);                      // (the clock is shared)
  gContract[0].lastAccentBeat = 42;
  send("LFA:i=solid,c=3b82f6,at=0,am=2,m=200,ae=flash,ad=200");
  CHECK(gContract[0].lastAccentBeat == BEAT_NONE);                      // score load

  // 1f. …and that reset has to MATTER: after a re-anchor (Studio re-seeds the clock on every
  // Play/seek) the new timeline's beat 0 must accent again, even though beat 0 of the old
  // timeline already did.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=3b82f6,at=0,am=2,m=200,ae=flash,ad=200");
  _mock_millis = 100020;
  CHECK(tickFired(0));
  CHECK(gContract[0].lastAccentBeat == 0);
  _mock_millis = 101000;                            // well past the strobe cool-down
  send("**C:bpm=120,ph=0,bpb=4,beat=0");            // re-anchor: this instant is beat 0 again
  _mock_millis = 101020;
  CHECK(tickFired(0));                              // beat 0 of the NEW timeline accents

  // 1g. The strobe cap still governs the SELF-FIRED accent. 300 BPM is a beat every 200 ms —
  // faster than the 2 * STROBE_MIN_STATE_MS (340 ms) re-arm floor — so the board must
  // DROP the beats it cannot safely honour instead of strobing. Run a realistic ~10 ms frame
  // loop (contractTick + ContractRender, as mainLoop does): the render pass is what retires an
  // expired overlay, and the cool-down gate only bites once it has.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=300,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=3b82f6,at=0,am=2,m=200,ae=flash");   // ad omitted => the 180 ms default
  CHECK(gScore[0][0].accentDur10 == 18);
  {
    LogicEngineRenderer rr; rr._id = 1;
    int fires = 0;
    uint32_t prevFire = 0, minGap = 0xFFFFFFFFu;
    for (uint32_t t = 100000; t < 102400; t += 10) {        // 2400 ms == 12 beats @ 300 BPM
      _mock_millis = t;
      if (tickFired(0)) {
        if (fires) { uint32_t gap = t - prevFire; if (gap < minGap) minGap = gap; }
        prevFire = t; fires++;
      }
      ContractRender(rr);                                    // the engine's frame: expires the overlay
    }
    CHECK(fires > 0);                                        // not vacuous: it DOES accent…
    CHECK(fires < 12);                                       // …but not on every 200 ms beat
    CHECK(minGap >= 2 * STROBE_MIN_STATE_MS);                // never faster than the strobe cap
    if (minGap < 2 * STROBE_MIN_STATE_MS)
      printf("       (accents re-armed %u ms apart; the cap is %u ms)\n",
             (unsigned)minGap, (unsigned)(2 * STROBE_MIN_STATE_MS));
  }

  // =====================================================================================
  // v1.2 GUARD 1S — THE STROBE CAP IS A SAFETY LIMIT (photosensitivity), so pin it as an
  // ABSOLUTE number of milliseconds rather than trusting whatever the constant happens to
  // say. 1g above asserts `minGap >= 2 * STROBE_MIN_STATE_MS`, which is true for ANY value
  // of the constant — it cannot notice the constant itself being lowered.
  // =====================================================================================
  {
    const uint32_t coolDownMs = 2u * STROBE_MIN_STATE_MS;    // shortest allowed gap between STARTS
    if (coolDownMs < 340u) {
      printf("FAIL :%d  the accent cool-down is %u ms => up to %.2f flashes/sec, above the "
             "<= 3 flashes/sec photosensitivity guidance.\n", __LINE__,
             (unsigned)coolDownMs, 1000.0 / (double)coolDownMs);
      printf("          STROBE_MIN_STATE_MS must be >= 170 (it is %u).\n",
             (unsigned)STROBE_MIN_STATE_MS);
      gFails++;
    }
    if (STROBE_MIN_STATE_MS != 170u) {
      printf("FAIL :%d  STROBE_MIN_STATE_MS is %u; PSI and Flthy both use 170. The three boards "
             "flash the SAME show at the SAME instant, so the cap has to be the same number on "
             "all three or the fastest board sets the real flash rate.\n", __LINE__,
             (unsigned)STROBE_MIN_STATE_MS);
      gFails++;
    }
  }

  // =====================================================================================
  // v1.2 GUARD 1H — SAFETY: the cool-down must be anchored UNCONDITIONALLY. The gate used to
  // be `!pulseActive && pulseStartMs != 0 && ...`; that `!pulseActive` short-circuit meant an
  // overlay that was STILL ON SCREEN skipped the cap entirely, so every re-arm was accepted.
  //
  // 1h-1 — the LIVE path (verb P). A 1000 ms accent is armed, then re-armed 200 ms later
  // (5 Hz) while it is demonstrably still up. It must be DROPPED. Then 400 ms after the
  // accepted one (2.5 Hz) must still PASS, mid-overlay — the cap must not be over-tight
  // either, or a Studio show stutters on alternate beats and just looks broken.
  // =====================================================================================
  resetAll();
  {
    _mock_millis = 500000;
    const uint32_t t = _mock_millis;
    send("L*A:i=solid,c=FF0000,b=200");                  // a base look on both units
    send("LFP:c=FFFFFF,d=1000");                         // accent #1: white, 1000 ms
    CHECK(gContract[0].pulseActive);
    CHECK(gContract[0].pulseStartMs == t);
    CHECK(gContract[0].pulseDurMs == 1000);
    CHECK(gContract[0].pulseColor.r == 0xFF && gContract[0].pulseColor.g == 0xFF);

    _mock_millis = t + 200;                              // 200 ms later == 5 flashes/sec
    // NOT VACUOUS: the overlay really is still up at this instant, which is the whole point —
    // this is precisely the state the old gate short-circuited on.
    CHECK(gContract[0].pulseActive);
    CHECK(_mock_millis - gContract[0].pulseStartMs < gContract[0].pulseDurMs);
    send("LFP:c=00FF00,d=1000");                         // re-arm attempt: green
    if (gContract[0].pulseStartMs != t || gContract[0].pulseColor.g != 0xFF
        || gContract[0].pulseColor.r != 0xFF) {
      printf("FAIL :%d  an accent only 200 ms after the previous one was ACCEPTED because an "
             "overlay was still active — 5 flashes/sec, over the <= 3 flashes/sec cap. The "
             "cool-down must not be gated on !pulseActive.\n", __LINE__);
      gFails++;
    }

    _mock_millis = t + 400;                              // 400 ms after the accepted start == 2.5 Hz
    CHECK(gContract[0].pulseActive);                     // still mid-overlay (1000 ms accent)
    send("LFP:c=00FF00,d=1000");                         // must PASS: 400 >= 340
    if (gContract[0].pulseStartMs != t + 400 || gContract[0].pulseColor.r != 0x00
        || gContract[0].pulseColor.g != 0xFF) {
      printf("FAIL :%d  the cool-down DROPPED a legal 2.5 Hz accent (400 ms gap) — over-tight, "
             "a Studio-authored show would stutter on alternate beats.\n", __LINE__);
      gFails++;
    }
  }

  // 1h-2 — the SCORED path (the board's own beat edge), in the shape Studio can actually emit:
  // an accent LONGER than the beat. ae=flash,ad=2550 with am=2 at 300 BPM means the overlay is
  // ALWAYS still active when the next beat lands, so ContractRender never retires it and the old
  // `!pulseActive` gate was bypassed on every single beat — the logics re-armed every 200 ms
  // (5 Hz). 1g cannot see this: its ad defaults to 180 ms, which expires before the next beat.
  resetAll();
  _mock_millis = 100000;
  send("**C:bpm=300,ph=0,bpb=4,beat=0");
  send("LFA:i=solid,c=3b82f6,at=0,am=2,m=200,ae=flash,ac=ffffff,ad=2550");
  CHECK(gScore[0][0].accentDur10 == 255);                 // 2550 ms / 10
  {
    LogicEngineRenderer rr; rr._id = 1;
    int fires = 0, firesMidOverlay = 0;
    uint32_t prevFire = 0, minGap = 0xFFFFFFFFu;
    for (uint32_t t = 100000; t < 102400; t += 10) {      // 2400 ms == 12 beats @ 300 BPM
      _mock_millis = t;
      const bool overlayUp = gContract[0].pulseActive
                          && (t - gContract[0].pulseStartMs) < gContract[0].pulseDurMs;
      if (tickFired(0)) {
        if (fires) {
          uint32_t gap = t - prevFire; if (gap < minGap) minGap = gap;
          if (overlayUp) firesMidOverlay++;               // re-armed while one was on screen
        }
        prevFire = t; fires++;
      }
      ContractRender(rr);                                  // the engine's frame
    }
    CHECK(fires > 1);                                      // not vacuous: it DOES re-accent…
    CHECK(fires < 12);                                     // …but not on all 12 of the 200 ms beats
    // …and every re-accent happened while the 2550 ms overlay was STILL UP, so this scenario
    // really does exercise the path the old gate short-circuited (if this is 0 the guard is
    // vacuous and proves nothing).
    CHECK(firesMidOverlay == fires - 1);
    if (minGap < 340u) {
      printf("FAIL :%d  a scored ad=2550 accent at 300 BPM re-armed every %u ms (%.2f flashes/sec) "
             "— the overlay was still active, so the cool-down was bypassed. The cap is 340 ms.\n",
             __LINE__, (unsigned)minGap, 1000.0 / (double)minGap);
      gFails++;
    }
  }

  // =====================================================================================
  // v1.2 GUARD 2 — CE_FLASH is beat-pumped. It rendered its lit state at the raw ceiling
  // (g.bright), the only sustained look on this board that ignored the shared envelope —
  // so an i=flash section pumped on the HPs/PSIs and sat flat on the Logics.
  // s=255 => half-period 170 ms (period 340). At 120 BPM (beat 500 ms) t=10 and t=460 are
  // BOTH inside a lit half, but sit at opposite ends of the beat envelope.
  // =====================================================================================
  resetAll();
  _mock_millis = 0;
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  send("L*A:i=flash,c=FF0000,s=255,b=200,m=200,am=2");
  {
    LogicEngineRenderer rf; rf._id = 1;
    _mock_millis = 10;                    // 10 % 340 < 170 => LIT; beat phase ~0.02 => accent peak
    ContractRender(rf);
    int onEarly = rf.px(0, 0).r;
    _mock_millis = 460;                   // 460 % 340 = 120 < 170 => LIT; beat phase 0.92 => floor
    ContractRender(rf);
    int onLate = rf.px(0, 0).r;
    _mock_millis = 200;                   // 200 % 340 >= 170 => the DARK half
    ContractRender(rf);
    int off = rf.px(0, 0).r;
    CHECK(!gContract[0].pulseActive);     // no overlay in play — this is the BASE look
    CHECK(onEarly > 0);                   // non-vacuous: both samples really are lit…
    CHECK(onLate > 0);
    CHECK(onEarly > onLate + 40);         // …and the lit level tracks the beat envelope
    CHECK(off == 0);                      // the dark half is still fully dark
    if (onEarly <= onLate + 40)
      printf("       (CE_FLASH lit level: early=%d late=%d — expected early to ride the accent)\n",
             onEarly, onLate);
  }

  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("ContractLogics.h type-check + score-native / score-accent / strobe-cap / "
         "flash-pump guards OK\n");
  return 0;
}
