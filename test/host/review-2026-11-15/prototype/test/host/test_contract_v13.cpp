// Part of the DroidNet Driveable-Animation Contract test harness.
// Copyright (c) 2026 Travis Cook. Shared byte-identically across the DroidNet
// RSeries/PSI/Flthy forks.  SPDX-License-Identifier: LGPL-2.1-only OR MIT
// Host unit tests for the v1.3 additions to contract_core.h.
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static ContractParams P(const char* line) { ParsedContract p; contractParse(line, p); return p.params; }

// ---- S5: saturating parameters, exact mode words ----
static void test_saturating_params() {
  CHECK(P("LFA:s=300").speed == 255);
  CHECK(P("LFA:b=300").bright == 255);
  CHECK(P("LFA:b=-1").bright == 0);
  CHECK(P("LFA:m=256").beatMod == 255);
  CHECK(P("LFA:am=258").accentMode == 4);
  CHECK(P("L*L:v=99999999999").level == 255);
  CHECK(P("**C:bpb=999").bpb == 255);             // range-checked at the seed (S1), not wrapped here
  CHECK(P("**C:bpm=65656").bpm == 65535);         // was 120 after a 16-bit wrap
  CHECK(P("**C:bpm=128.5").bpm == 128);
  CHECK(P("**M:v=show").mode == 's');
  CHECK(P("**M:v=idle").mode == 'i');
  CHECK(P("**M:v=stop").mode == 0);               // used to mean show
  CHECK(P("**M:v=i").mode == 0);
  CHECK(P("**M:v=show,wd=15").hasWd && P("**M:v=show,wd=15").wd == 15);
  CHECK(P("**M:v=show,wd=999").wd == 255);
  CHECK(accentDurClamp(0) == 170 && accentDurClamp(180) == 180);
  CHECK(accentDurClamp(4000000000UL) == 2550 && accentDurClamp(_parseU32("-1")) == 2550);
}

// ---- S1: BPM and bar ranges ----
static void test_bpm_range() {
  BeatClock bc;
  CHECK(beatClockSeed(bc, P("**C:bpm=120,ph=0,bpb=4,beat=0"), 1000) == CS_SEEK);
  CHECK(bc.running && bc.bpm == 120 && bc.anchorMs == 1000);
  const BeatClock before = bc;
  const char* bad[] = { "**C:bpm=0,ph=5,beat=9", "**C:bpm=29,ph=5", "**C:bpm=301,ph=5",
                        "**C:bpm=65535", "**C:bpm=-1", "**C:bpm=1200,beat=4" };
  for (const char* line : bad) {
    CHECK(beatClockSeed(bc, P(line), 5000) == CS_IGNORED);
    CHECK(bc.bpm == before.bpm && bc.anchorMs == before.anchorMs
          && bc.beatOffset == before.beatOffset && bc.running);     // ignored WHOLE
  }
  CHECK(beatClockSeed(bc, P("**C:bpm=30"), 1000) == CS_SEEK && bc.bpm == 30);
  CHECK(beatClockSeed(bc, P("**C:bpm=300"), 1000) == CS_SEEK && bc.bpm == 300);
  beatClockSeed(bc, P("**C:bpm=120,bpb=0"), 1000);  CHECK(bc.bpb == 4);
  beatClockSeed(bc, P("**C:bpm=120,bpb=13"), 1000); CHECK(bc.bpb == 4);
  beatClockSeed(bc, P("**C:bpm=120,bpb=12"), 1000); CHECK(bc.bpb == 12);
  BeatClock stopped;                                 // a bare re-anchor cannot start a clock
  CHECK(beatClockSeed(stopped, P("**C:ph=10,beat=8"), 1000) == CS_IGNORED && !stopped.running);
}

// ---- S2: the rate floor covers the pump and the overlay ----
static void test_rate_divisor() {
  CHECK(beatRateDivisor(30) == 1 && beatRateDivisor(120) == 1 && beatRateDivisor(176) == 1);
  CHECK(beatRateDivisor(177) == 2 && beatRateDivisor(300) == 2);
  CHECK(barRateDivisor(300, 4) == 1 && barRateDivisor(300, 1) == 2 && barRateDivisor(176, 1) == 1);
  for (int bpm = 30; bpm <= 300; bpm++) {            // the property, for every legal tempo
    uint32_t period = 60000u / (uint32_t)bpm;        // floor: the worst case
    CHECK((uint32_t)beatRateDivisor((uint16_t)bpm) * (period + 1) >= 340u);
  }
  BeatClock bc; bc.running = true; bc.bpm = 240; bc.bpb = 4;
  int fired = 0;
  for (int32_t beat = -8; beat < 8; beat++) {
    BeatPos bp; bp.beatIndex = beat; bp.barPos = (uint8_t)(((beat % 4) + 4) % 4);
    bool f = beatAccentFires(2, bp, bc);
    CHECK(f == (beat % 2 == 0));                     // every second beat at 240 BPM
    if (f) fired++;
    CHECK((beatAccentAmount(2, bp, 255, 0, bc) > 0) == f);          // pump and overlay agree
    CHECK(beatAccentFires(1, bp, bc) == (bp.barPos == 0));          // a 1 s bar needs no divisor
  }
  CHECK(fired == 8);
  bc.bpm = 120;
  BeatPos odd; odd.beatIndex = 3; odd.barPos = 3;
  CHECK(beatAccentFires(2, odd, bc));                // unchanged at ordinary tempos
  CHECK(!beatAccentFires(9, odd, bc));               // unknown mode: silent
}

// ---- C1 / C2: what a re-anchor may do ----
static void test_seed_kinds_and_edge() {
  BeatClock bc;
  beatClockSeed(bc, P("**C:bpm=120,ph=0,bpb=4,beat=0"), 100000);
  CHECK(accentEdgeAfterSeed(bc, 100000) == BEAT_NONE);   // on the beat: it may accent
  CHECK(accentEdgeAfterSeed(bc, 100060) == BEAT_NONE);   // 60 ms in: still may
  CHECK(accentEdgeAfterSeed(bc, 100061) == 0);           // 61 ms in: beat 0 counts as done
  CHECK(accentEdgeAfterSeed(bc, 100400) == 0);
  CHECK(accentEdgeAfterSeed(bc, 100520) == BEAT_NONE);   // 20 ms into beat 1
  // the host's bare correction, 400 ms into beat 8: next beat is 9, 100 ms away
  CHECK(beatClockSeed(bc, P("**C:ph=100,beat=9"), 104400) == CS_NUDGE);
  CHECK(accentEdgeAfterSeed(bc, 104400) == 8);
  // a bare line that jumps two bars is a seek, not a nudge
  CHECK(beatClockSeed(bc, P("**C:ph=0,beat=0"), 104500) == CS_SEEK);
  // a line with bpm= is always a seek
  CHECK(beatClockSeed(bc, P("**C:bpm=120,ph=0,beat=0"), 104500) == CS_SEEK);
  BeatClock off;
  CHECK(accentEdgeAfterSeed(off, 5) == BEAT_NONE);
  CHECK(scoreCursor(1, 0, true) == 1);               // C2: a nudge never steps back
  CHECK(scoreCursor(1, 0, false) == 0);              // a seek may
  CHECK(scoreCursor(1, 2, true) == 2 && scoreCursor(-1, 0, true) == 0);
}

// ---- L1 / L2: one reset, and what survives it ----
static void test_show_reset_and_watchdog() {
  ContractUnitState s;
  s.effect = CE_SOLID; s.color = ContractRGB{9, 9, 9}; s.base = CB_LOOK; s.pulseActive = true;
  s.holdPending = true; s.scoreLen = 5; s.curSection = 3; s.loadN = 5; s.loadK = 0xBEEF;
  s.loadOk = 1; s.overflow = true; s.lastAccentBeat = 42; s.hasDeadline = true; s.wdSec = 15;
  s.pinForward = true; s.pulseLastMs = 7777; s.lastApplyMs = 8888;
  contractOnShow(s, 160, 15);
  CHECK(s.inShow && s.wdSec == 15 && s.base == CB_DARK && s.ceil == 160 && s.bright == 160);
  CHECK(s.effect == CE_OFF && s.color.r == 0 && !s.pulseActive && !s.holdPending);
  CHECK(s.scoreLen == 0 && s.curSection == -1 && s.loadN == 0 && s.loadK == 0 && s.loadOk == 0);
  CHECK(!s.overflow && s.lastAccentBeat == BEAT_NONE && !s.hasDeadline && !s.pinForward);
  CHECK(s.pulseLastMs == 7777 && s.lastApplyMs == 8888);   // the safety anchors are NOT reset
  s.scoreLen = 3;
  contractOnStop(s, 140);                                  // X keeps the mode and the watchdog
  CHECK(s.inShow && s.wdSec == 15 && s.base == CB_DARK && s.scoreLen == 0 && s.ceil == 140);
  CHECK(!contractWatchdogExpired(s, 1000, 15999));
  CHECK(contractWatchdogExpired(s, 1000, 16000));
  CHECK(contractWatchdogExpired(s, 0xFFFFF000u, 0xFFFFF000u + 15000u));   // across the millis wrap
  contractOnIdle(s, 140);
  CHECK(!s.inShow && s.wdSec == 0 && s.base == CB_NONE);
  CHECK(!contractWatchdogExpired(s, 0, 4000000000UL));     // idle: never
  ContractUnitState noWd; contractOnShow(noWd, 200, 0);
  CHECK(!contractWatchdogExpired(noWd, 0, 4000000000UL));  // no wd= on the wire: never
}

// ---- S4: the live-look hold ----
static void test_live_hold() {
  ContractUnitState s; ContractLookPatch a, b, out;
  a.has = LP_COLOR; a.color = ContractRGB{1, 0, 0};
  b.has = LP_COLOR; b.color = ContractRGB{2, 0, 0};
  CHECK(liveLookOffer(s, a, 1000));                  // first look: now
  CHECK(!liveLookOffer(s, a, 1100));                 // 100 ms later: held
  CHECK(!liveLookOffer(s, b, 1150));                 // the newest held look wins
  CHECK(!liveLookDue(s, 1169, out));
  CHECK(liveLookDue(s, 1170, out) && out.color.r == 2);
  CHECK(!liveLookDue(s, 1171, out));                 // delivered once
  CHECK(!liveLookOffer(s, a, 1200));                 // the window restarted at 1170
  CHECK(liveLookOffer(s, a, 1340));
  ContractParams pr = P("LFA:i=solid,c=0a0b0c,s=9,m=7");
  ContractLookPatch lp = lookPatchFrom(pr);
  CHECK(lp.has == (LP_EFFECT | LP_COLOR | LP_SPEED | LP_BEATMOD) && lp.effect == CE_SOLID);
  CHECK(lp.color.g == 0x0b && lp.speed == 9 && lp.beatMod == 7);
}

// ---- score: capacity, overflow, b=, and verb V ----
static void test_score_verify() {
  CHECK(contractCrc16("123456789") == 0x29B1);       // the CRC-16/CCITT-FALSE check value
  CHECK(contractCrc16("") == 0xFFFF);
  const char* l1 = "LFA:i=solid,c=ff0000,at=0";
  const char* l2 = "LFA:i=wipe,c=0000ff,at=16,am=2,m=200";
  ScoreEntry t[2]; ContractUnitState s; ScoreEntry e;
  e.atBeat = 16; CHECK(scoreAccept(s, t, 2, e, contractCrc16(l2)));
  e.atBeat = 0;  CHECK(scoreAccept(s, t, 2, e, contractCrc16(l1)));
  CHECK(s.scoreLen == 2 && s.loadN == 2 && t[0].atBeat == 0 && t[1].atBeat == 16);
  const uint16_t k = (uint16_t)(contractCrc16(l1) ^ contractCrc16(l2));
  CHECK(s.loadK == k);                               // order-independent
  ContractUnitState ok = s;  CHECK(scoreVerify(ok, 2, k) == 1 && ok.loadOk == 1);
  ContractUnitState c1 = s;  CHECK(scoreVerify(c1, 3, k) == 2);                    // a line is missing
  ContractUnitState c2 = s;  CHECK(scoreVerify(c2, 2, (uint16_t)(k ^ 1)) == 2);    // a line differs
  e.atBeat = 32; CHECK(!scoreAccept(s, t, 2, e, 0x1234));                          // full
  CHECK(s.overflow && s.scoreLen == 2 && s.loadN == 2 && s.loadK == k);
  CHECK(scoreVerify(s, 2, k) == 2);                  // an overflowed load is bad even if n and k match
  ParsedContract p;
  CHECK(contractParse("LFV:n=12,k=a1B2", p) && p.verb == CV_VERIFY);
  CHECK(p.params.hasN && p.params.n == 12 && p.params.hasK && p.params.k == 0xA1B2);
  CHECK(contractParse("LFV:n=12,k=a1b", p) && !p.params.hasK);       // k is exactly four hex digits
  CHECK(contractParse("LFV:n=12,k=a1b2c", p) && !p.params.hasK);
  ScoreEntry d; CHECK(d.bright == 255);              // b= absent: the ceiling
}

// ---- section 10: the query reply ----
static void test_query_reply() {
  char buf[CONTRACT_QUERY_MAX];
  uint8_t n = contractFormatQuery(buf, sizeof buf, 'L', 'F', 32, 5, 1, 120);
  CHECK(n == 37 && memcmp(buf, "!LFq:ver=1.3,cap=32,n=5,ok=1,bpm=120\r", n) == 0);
  n = contractFormatQuery(buf, sizeof buf, 'H', 'T', 255, 255, 2, 300);
  CHECK(n == 40 && n <= CONTRACT_QUERY_MAX && buf[n - 1] == '\r');
  n = contractFormatQuery(buf, sizeof buf, 'P', 'R', 12, 0, 0, 0);
  CHECK(memcmp(buf, "!PRq:ver=1.3,cap=12,n=0,ok=0,bpm=0\r", n) == 0);
  CHECK(contractFormatQuery(buf, 47, 'L', 'F', 32, 5, 1, 120) == 0);  // too small: nothing written
  ParsedContract p;
  CHECK(!contractParse("LFq:ver=1.3,cap=32,n=5,ok=1,bpm=120", p));    // a reply is never a command
}

// ---- F6: one timing definition for flash / pulse / scan ----
static void test_v1_effect_timing() {
  CHECK(fxFlashHalfMs(255) == 170 && fxFlashHalfMs(0) == 425 && fxFlashHalfMs(128) == 297);
  CHECK(fxFlashOn(0, 255) && fxFlashOn(169, 255) && !fxFlashOn(170, 255) && fxFlashOn(340, 255));
  CHECK(fxPulseBright(0, 0) == 0 && fxPulseBright(1000, 0) == 255 && fxPulseBright(500, 0) == 127);
  CHECK(fxPulseBright(170, 255) == 255 && fxPulseBright(340, 255) == 0);   // 340 ms period at s=255
  for (uint32_t t = 0; t < 4000; t++) CHECK(fxPulseBright(t, 200) <= 255);
  const int want[] = { 0, 1, 2, 3, 2, 1, 0, 1 };     // ping-pong on N=4, one step per 30 ms
  for (int i = 0; i < 8; i++) CHECK(fxScanPos(30u * (uint32_t)i, 255, 4, false) == want[i]);
  CHECK(fxScanPos(30u * 7u, 255, 6, true) == 1);     // a ring wraps
  CHECK(fxScanPos(999, 255, 1, false) == 0 && fxScanPos(999, 255, 0, true) == 0);
}

int main() {
  test_saturating_params();
  test_bpm_range();
  test_rate_divisor();
  test_seed_kinds_and_edge();
  test_show_reset_and_watchdog();
  test_live_hold();
  test_score_verify();
  test_query_reply();
  test_v1_effect_timing();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
