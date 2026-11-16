// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// The findings of the 2026-11-15 lighting review, as tests of the RSeries layer on the mock
// engine. Promoted from test/host/review-2026-11-15/. Each KNOWN_BUG names the finding.
#include "logics_harness.h"
#include <cmath>

// Full-field swings per second on the front display over 10 s (a swing: <= lo, then >= hi).
static double swingsPerSec(int lo, int hi) {
  int rises = 0; bool low = true;
  for (uint32_t t = 0; t < 10000; t += 10) {
    _mock_millis += 10; frame();
    int v = FLD->px(0, 0).r;
    if (low && v >= hi) { rises++; low = false; } else if (!low && v <= lo) low = true;
  }
  return rises / 10.0;
}

// A scored am=<am> section with ae=flash, music exactly on the seeded tempo, and the host's bare
// re-anchor every 4 s arriving `off` ms after a multiple of 4 s. Counts accents that start more
// than 15 ms from a beat, and accent beats that got none.
static void reanchorRun(int bpm, int am, uint32_t off, int& offBeat, int& missed) {
  resetAll();
  const uint32_t T0 = _mock_millis; const double per = 60000.0 / bpm;
  char line[96];
  send("**M:v=show");
  snprintf(line, sizeof line, "LFA:i=solid,c=3b82f6,at=0,am=%d,m=200,ae=flash", am); send(line);
  snprintf(line, sizeof line, "**C:bpm=%d,ph=0,bpb=4,beat=0", bpm); send(line);
  offBeat = missed = 0;
  uint32_t lastArm = 0; static char hit[512]; for (char& h : hit) h = 0;
  for (uint32_t t = 0; t < 60000; t++) {
    _mock_millis = T0 + t;
    if (t >= 4000 && (t % 4000) == off) {
      long nb = (long)ceil(t / per - 1e-9); long ph = lround(nb * per - t);
      snprintf(line, sizeof line, "**C:ph=%ld,beat=%ld", ph < 0 ? 0 : ph, nb); send(line);
    }
    frame();
    if (gContract[0].pulseActive && gContract[0].pulseStartMs != lastArm) {
      lastArm = gContract[0].pulseStartMs;
      double ph = fmod((double)t, per), d = ph < per / 2 ? ph : per - ph;
      if (d > 15.0) offBeat++; else hit[lround(t / per)] = 1;
    }
  }
  for (long k = 0; k <= (long)floor(59999 / per); k++)
    if ((am == 2 || k % 4 == 0) && !hit[k]) missed++;
}

int main() {
  // F3 (S7): native:25 reaches LogicEffectDefaultSelector's off-by-one (LE:2733).
  resetAll(); send("LFA:i=native:25"); run(50);
  CHECK(FLD->_oobCalls == 0);
  resetAll(); send("LFA:i=native:4"); run(50);
  CHECK(FLD->_seq == 4);                                   // an allowed code still runs

  // F4 (L1, L3): stale state at show boundaries.
  resetAll(); send("L*P:c=ffffff,d=120"); run(50);
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);          // the pulse shows
  run(1000);
  CHECK(FLD->_seq == 0);
  resetAll(); send("L*A:i=solid,c=ff0000"); run(200); send("**X"); run(200); send("L*P:c=ffffff"); run(1000);
  CHECK(FLD->_seq == 14);
  resetAll(); send("L*A:i=solid,c=00ff00"); run(200);
  send("**M:v=show"); send("L*A:i=solid,c=0000ff,at=8"); send("**C:bpm=120,ph=2000,bpb=4,beat=0"); run(500);
  CHECK(FLD->_seq == 14);

  // F5: an accent on a scored native section restarts the native sequence every beat.
  resetAll(); send("**M:v=show"); send("LFA:i=native:4,at=0,am=2,m=200,ae=flash");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(10000);
  CHECK(FLD->_restarts[4] == 1);

  // F7 (S6): brightness. B must survive restoreSettings(); the ceiling is the owner's fBri (160).
  resetAll(); send("L*B:b=100"); send("LFA:i=native:0"); run(50); send("LFA:i=native:4"); run(300);
  CHECK(FLD->_liveBri == 100);
  resetAll(); send("LFA:i=solid,c=ffffff,b=255"); run(200);
  CHECK(FLD->px(0, 0).r <= 160);

  // F1 (C1): the 4 s re-anchor must not move or swallow accents.
  int ob = 0, mi = 0;
  reanchorRun(120, 2, 0, ob, mi);   CHECK(ob == 0 && mi == 0);          // on the beat: always clean
  reanchorRun(120, 2, 400, ob, mi);
  CHECK(ob == 0 && mi == 0);
  reanchorRun(120, 1, 400, ob, mi);
  CHECK(ob == 0 && mi == 0);

  // F2 (S1, S2): the pump is under the same rate floor as the overlay.
  resetAll(); send("LFA:i=solid,c=ffffff,m=255,am=2"); send("**C:bpm=120,ph=0,bpb=4,beat=0");
  CHECK(fabs(swingsPerSec(30, 120) - 2.0) < 0.11);                      // unchanged at 120 BPM
  resetAll(); send("LFA:i=solid,c=ffffff,m=255,am=2"); send("**C:bpm=240,ph=0,bpb=4,beat=0");
  CHECK(swingsPerSec(30, 120) <= 2.95);
  resetAll(); send("LFA:i=solid,c=ffffff,m=255,am=2"); send("**C:bpm=600,ph=0,bpb=4,beat=0");
  CHECK(!gBeat.running);

  // C2: a nudge just after a section boundary must not flip the section back.
  resetAll(); send("**M:v=show"); send("LFA:i=wipe,c=ff0000,at=0"); send("LFA:i=wipe,c=0000ff,at=8");
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  int switches = 0, last = -2;
  for (uint32_t t = 0; t < 6000; t++) {
    _mock_millis++; if (t == 4010) send("**C:ph=30,beat=8");
    frame();
    if (gContract[0].curSection != last) { last = gContract[0].curSection; switches++; }
  }
  CHECK(switches == 2);

  // S5: P's d= is bounded.
  resetAll(); send("L*A:i=solid,c=ff0000"); run(200); send("L*P:c=ffffff,d=-1"); run(3000);
  CHECK(!gContract[0].pulseActive);

  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("review regressions OK (%d known defect(s) still open)\n", gKnown);
  return 0;
}
