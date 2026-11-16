// Part of the DroidNet Driveable-Animation Contract test harness.
// Copyright (c) 2026 Travis Cook. Shared byte-identically across the DroidNet
// RSeries/PSI/Flthy forks.  SPDX-License-Identifier: LGPL-2.1-only OR MIT
// Host unit tests for the contract v1.3 additions to contract_core.h — no external deps.
// -std=c++11 clean: the AVR forks build the core with gnu++11.
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static ContractParams P(const char* line) { ParsedContract p; contractParse(line, p); return p.params; }

// ---- S5: saturating parameters, exact mode words, the v1.3 keys ----
static void test_saturating_params() {
  CHECK(P("LFA:s=300").speed == 255);
  CHECK(P("LFA:b=300").bright == 255);
  CHECK(P("LFA:b=-1").bright == 0);
  CHECK(P("LFA:m=256").beatMod == 255);
  CHECK(P("L*L:v=99999999999").level == 255);
  CHECK(P("**C:bpb=999").bpb == 255);             // range-checked at the seed (S1), not wrapped here
  CHECK(P("**C:bpm=65656").bpm == 65535);         // was 120 after a 16-bit wrap
  CHECK(P("**C:bpm=128.5").bpm == 128);
  CHECK(P("**C:bpm=0").hasBpm && P("**C:bpm=0").bpm == 0);   // C6: hold is a real value, not a parse failure
  for (int am = 0; am <= 4; am++) {               // the five known modes pass through
    char l[16]; snprintf(l, sizeof l, "LFA:am=%d", am);
    CHECK(P(l).hasAm && P(l).accentMode == am);
  }
  CHECK(P("LFA:am=5").accentMode == 0);           // an unknown mode is no accent...
  CHECK(P("LFA:am=9").accentMode == 0);
  CHECK(P("LFA:am=258").accentMode == 0);         // ...and never wraps into a known one (was 2)
  CHECK(P("LFA:am=-1").accentMode == 0);
  CHECK(P("**M:v=show").mode == 's');
  CHECK(P("**M:v=idle").mode == 'i');
  CHECK(P("**M:v=stop").mode == 0);               // used to mean show
  CHECK(P("**M:v=i").mode == 0);
  CHECK(P("**M:v=showtime").mode == 0);
  CHECK(P("**M:v=show,wd=15").hasWd && P("**M:v=show,wd=15").wd == 15);
  CHECK(P("**M:v=show,wd=999").wd == 255);
  CHECK(!P("**M:v=show").hasWd);
  CHECK(P("LFA:an=4,ap=2").hasAn && P("LFA:an=4,ap=2").an == 4 && P("LFA:an=4,ap=2").ap == 2);
  CHECK(!P("LFA:am=2").hasAn && P("LFA:am=2").an == 1 && P("LFA:am=2").ap == 0);
  CHECK(P("LFQ:r=0").hasRoute && P("LFQ:r=0").route == 0);
  CHECK(P("LFQ:r=2").route == 2 && P("LFQ:r=250").route == 99);
  CHECK(!P("LFQ").hasRoute);
  CHECK(accentDurClamp(0) == 170 && accentDurClamp(180) == 180);
  CHECK(accentDurClamp(4000000000UL) == 2550 && accentDurClamp(_parseU32("-1")) == 2550);
  ParsedContract p;
  CHECK(contractParse("LFV:n=12,k=a1B2", p) && p.verb == CV_VERIFY);
  CHECK(p.params.hasN && p.params.n == 12 && p.params.hasK && p.params.k == 0xA1B2);
  CHECK(contractParse("LFV:n=12,k=a1b", p) && !p.params.hasK);       // k is exactly four hex digits
  CHECK(contractParse("LFV:n=12,k=a1b2c", p) && !p.params.hasK);
  CHECK(contractParse("LFV:n=12,k=a1g2", p) && !p.params.hasK);
  CHECK(contractParse("LFV:n=300,k=0000", p) && p.params.n == 255);
}

// ---- S1 / C6: BPM and bar ranges, and hold ----
static void test_bpm_range_and_hold() {
  BeatClock bc;
  CHECK(beatClockSeed(bc, P("**C:bpm=120,ph=0,bpb=4,beat=0"), 1000) == CS_SEEK);
  CHECK(bc.running && bc.bpm == 120 && bc.anchorMs == 1000);
  const BeatClock before = bc;
  const char* bad[] = { "**C:bpm=1,ph=5,beat=9", "**C:bpm=29,ph=5", "**C:bpm=301,ph=5",
                        "**C:bpm=65535", "**C:bpm=1200,beat=4", "**C:bpm=99999999999" };
  for (const char* line : bad) {
    CHECK(beatClockSeed(bc, P(line), 5000) == CS_IGNORED);
    CHECK(bc.bpm == before.bpm && bc.anchorMs == before.anchorMs
          && bc.beatOffset == before.beatOffset && bc.running);     // ignored WHOLE
  }
  CHECK(beatClockSeed(bc, P("**C:bpm=30"), 1000) == CS_SEEK && bc.bpm == 30);
  CHECK(beatClockSeed(bc, P("**C:bpm=300"), 1000) == CS_SEEK && bc.bpm == 300);
  beatClockSeed(bc, P("**C:bpm=120,bpb=0"), 1000);  CHECK(bc.bpb == 4);
  beatClockSeed(bc, P("**C:bpm=120,bpb=13"), 1000); CHECK(bc.bpb == 4);
  beatClockSeed(bc, P("**C:bpm=120,bpb=999"), 1000); CHECK(bc.bpb == 4);   // saturates to 255, then S1
  beatClockSeed(bc, P("**C:bpm=120,bpb=12"), 1000); CHECK(bc.bpb == 12);
  beatClockSeed(bc, P("**C:bpm=120,bpb=1"), 1000);  CHECK(bc.bpb == 1);
  BeatClock stopped;                                 // a bare re-anchor cannot start a clock
  CHECK(beatClockSeed(stopped, P("**C:ph=10,beat=8"), 1000) == CS_IGNORED && !stopped.running);

  // C6: bpm=0 holds. Nothing but a C with a tempo resumes.
  beatClockSeed(bc, P("**C:bpm=120,ph=0,bpb=4,beat=0"), 10000);
  CHECK(beatPosAt(bc, 12000).beatIndex == 4);
  CHECK(beatClockSeed(bc, P("**C:bpm=0"), 12000) == CS_HOLD && !bc.running);
  CHECK(beatPosAt(bc, 20000).beatIndex == 0);                              // a stopped clock reads as zero
  CHECK(beatClockSeed(bc, P("**C:ph=100,beat=9"), 13000) == CS_IGNORED && !bc.running);   // the 4 s correction does not resume
  CHECK(beatClockSeed(bc, P("**C:bpm=0,ph=5,beat=3"), 13000) == CS_HOLD && !bc.running);  // hold twice is still hold
  CHECK(beatClockSeed(bc, P("**C:bpm=120,ph=0,bpb=4,beat=4"), 30000) == CS_SEEK && bc.running);
  CHECK(beatPosAt(bc, 30500).beatIndex == 5);                              // resumed where the host said
  BeatClock fresh;                                   // hold on a clock that never ran: still a hold, still stopped
  CHECK(beatClockSeed(fresh, P("**C:bpm=0"), 5) == CS_HOLD && !fresh.running);
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
  CHECK(accentEdgeAfterSeed(bc, 99900) == -1);           // before the anchor: 400 ms into beat -1
  // the host's bare correction, 400 ms into beat 8: the next beat is 9, 100 ms away
  CHECK(beatClockSeed(bc, P("**C:ph=100,beat=9"), 104400) == CS_NUDGE);
  CHECK(accentEdgeAfterSeed(bc, 104400) == 8);
  // a correction that pulls the position back across a beat boundary is still a nudge
  CHECK(beatClockSeed(bc, P("**C:ph=10,beat=9"), 104520) == CS_NUDGE);     // was 20 ms into beat 9; now 10 ms before it
  CHECK(accentEdgeAfterSeed(bc, 104520) == 8);
  // a bare line that jumps two bars is a seek, not a nudge
  CHECK(beatClockSeed(bc, P("**C:ph=0,beat=0"), 104600) == CS_SEEK);
  // a line with bpm= is always a seek, even when nothing moved
  CHECK(beatClockSeed(bc, P("**C:bpm=120,ph=0,beat=0"), 104600) == CS_SEEK);
  BeatClock off;
  CHECK(accentEdgeAfterSeed(off, 5) == BEAT_NONE);
  CHECK(scoreCursor(1, 0, true) == 1);               // C2: a nudge never steps back
  CHECK(scoreCursor(1, 0, false) == 0);              // a seek may
  CHECK(scoreCursor(1, 2, true) == 2 && scoreCursor(-1, 0, true) == 0 && scoreCursor(3, -1, true) == 3);
}

// ---- S2: the rate floor covers the pump and the overlay; every-N accents ----
static BeatPos posAt(int32_t beat, uint8_t bpb) {
  BeatPos bp; bp.beatIndex = beat; bp.barPos = (uint8_t)(((beat % bpb) + bpb) % bpb); return bp;
}
static void test_rate_floor_and_every_n() {
  CHECK(beatRateDivisor(30) == 1 && beatRateDivisor(120) == 1 && beatRateDivisor(176) == 1);
  CHECK(beatRateDivisor(177) == 2 && beatRateDivisor(300) == 2);
  CHECK(barRateDivisor(300, 4) == 1 && barRateDivisor(300, 1) == 2 && barRateDivisor(176, 1) == 1);
  CHECK(barRateDivisor(300, 2) == 1 && barRateDivisor(120, 0) == 1);     // bpb 0 reads as 4
  for (int bpm = 30; bpm <= 300; bpm++) {            // the property, for every legal tempo and group
    for (int grp = 1; grp <= 16; grp++) {
      const uint32_t k = barRateDivisor((uint16_t)bpm, (uint8_t)grp);
      CHECK(k * (uint32_t)grp * 60000u >= 340u * (uint32_t)bpm);                 // k groups last >= 340 ms
      CHECK(k == 1 || (k - 1) * (uint32_t)grp * 60000u < 340u * (uint32_t)bpm);  // and k is the smallest
    }
  }
  BeatClock bc; bc.running = true; bc.bpm = 240; bc.bpb = 4;
  int fired = 0;
  for (int32_t beat = -8; beat < 8; beat++) {
    const BeatPos bp = posAt(beat, 4);
    const bool f = beatAccentFires(2, bp, bc);
    CHECK(f == (beat % 2 == 0));                     // every second beat at 240 BPM
    if (f) fired++;
    CHECK((beatAccentAmount(2, bp, 255, 0, bc) > 0) == f);          // pump and overlay agree
    CHECK(beatAccentFires(3, bp, bc) == f);                         // build is every beat too, same floor
    CHECK(beatAccentFires(1, bp, bc) == (bp.barPos == 0));          // a 1 s bar needs no divisor
    CHECK(beatAccentFires(4, bp, bc) == (bp.barPos == 0));
    CHECK(!beatAccentFires(0, bp, bc) && !beatAccentFires(9, bp, bc));   // none, and an unknown mode: silent
  }
  CHECK(fired == 8);
  bc.bpm = 300; bc.bpb = 1;                          // a one-beat bar at 300 BPM is a 200 ms bar
  for (int32_t beat = -4; beat < 4; beat++) CHECK(beatAccentFires(1, posAt(beat, 1), bc) == (beat % 2 == 0));
  bc.bpm = 120; bc.bpb = 4;
  for (int32_t beat = -9; beat < 9; beat++) {        // ordinary tempos: the v1.2 predicate, unchanged
    const BeatPos bp = posAt(beat, 4);
    CHECK(beatAccentFires(2, bp, bc) && beatAccentFires(3, bp, bc));
    CHECK(beatAccentFires(1, bp, bc) == (bp.barPos == 0));
  }

  // every-N: one byte, n 1..16 in the high nibble (as n-1), the phase in the low one
  CHECK(accentEveryPack(1, 0) == 0 && accentEveryN(0) == 1 && accentEveryPhase(0) == 0);
  CHECK(accentEveryPack(4, 2) == 0x32 && accentEveryN(0x32) == 4 && accentEveryPhase(0x32) == 2);
  CHECK(accentEveryPack(16, 15) == 0xFF && accentEveryN(0xFF) == 16 && accentEveryPhase(0xFF) == 15);
  CHECK(accentEveryPack(0, 0) == 0 && accentEveryPack(200, 3) == 0xF3);     // n clamps to 1..16
  CHECK(accentEveryPack(4, 4) == 0x30 && accentEveryPack(4, 250) == 0x30);  // a phase >= n is 0
  CHECK(accentEveryPhase(0x1F) == 0);                                       // even from a byte nobody packed
  CHECK(accentEveryFrom(P("LFA:am=2")) == 0 && accentEveryFrom(P("LFA:am=2,an=2,ap=1")) == 0x11);
  CHECK(accentEveryFrom(P("LFA:am=2,an=0")) == 0 && accentEveryFrom(P("LFA:am=2,an=99,ap=99")) == 0xF0);
  for (int32_t beat = -9; beat < 17; beat++) {
    const BeatPos bp = posAt(beat, 4);
    CHECK(beatAccentFires(2, bp, bc, accentEveryPack(2, 0)) == (((beat % 2) + 2) % 2 == 0));   // front
    CHECK(beatAccentFires(2, bp, bc, accentEveryPack(2, 1)) == (((beat % 2) + 2) % 2 == 1));   // rear: alternate
    CHECK(beatAccentFires(2, bp, bc, accentEveryPack(4, 3)) == (((beat % 4) + 4) % 4 == 3));
    CHECK(beatAccentFires(1, bp, bc, accentEveryPack(4, 3)) == (bp.barPos == 0));              // am=1 ignores it
    CHECK((beatAccentAmount(2, bp, 200, 0, bc, accentEveryPack(4, 3)) > 0)
          == beatAccentFires(2, bp, bc, accentEveryPack(4, 3)));
  }
  bc.bpm = 300;                                      // the floor applies to the RESULT of every-N
  for (int32_t beat = -8; beat < 16; beat++) {
    const BeatPos bp = posAt(beat, 4);
    CHECK(beatAccentFires(2, bp, bc, accentEveryPack(2, 1)) == (((beat % 2) + 2) % 2 == 1));   // 400 ms apart: all kept
    CHECK(beatAccentFires(2, bp, bc, accentEveryPack(1, 0)) == (beat % 2 == 0));               // 200 ms apart: every other
  }
  CHECK(beatSectionProgress(8, 0, 16) == 0.5f && beatSectionProgress(8, 16, 16) == 0.0f);
  CHECK(beatSectionProgress(4, 8, 4) == 0.0f);
}

int main() {
  test_saturating_params();
  test_bpm_range_and_hold();
  test_seed_kinds_and_edge();
  test_rate_floor_and_every_n();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
