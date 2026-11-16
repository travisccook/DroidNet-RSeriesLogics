// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Contract v1.3 on the RSeries layer, on the mock engine: what each spec item looks like on the
// two logic displays. (The rules themselves are unit-tested in test_contract_v13.cpp.)
#include "logics_harness.h"
#include <cstring>

// ---- L1, L3, L5: the show boundaries ----
static void test_lifecycle() {
  // M:v=show arms without taking the display: the native idle look runs until the show's
  // first C, live A or P, so the dome is not dark between Send and Play.
  resetAll(); send("**M:v=show"); run(50);
  CHECK(gContract[0].inShow && gContract[0].base == CB_NONE);
  CHECK(FLD->_seq == 0 && FLD->_selects == 0);             // not even re-selected
  send("LFA:i=solid,c=ff0000,at=0"); run(200);             // a scored A is the load, not the start
  CHECK(gContract[0].base == CB_NONE && FLD->_seq == 0);
  send("**C:bpm=120,ph=1000,bpb=4,beat=0"); run(50);       // the show's first C, a second before beat 0
  CHECK(gContract[0].base == CB_DARK && FLD->_seq == 14 && RLD->_seq == 14);
  run(1100); CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);   // ...and the first section on its beat

  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); run(50);    // the show's first live A
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);
  CHECK(RLD->_seq == 0 && gContract[1].base == CB_NONE);   // only the display addressed has begun

  resetAll(); send("**M:v=show"); send("LFP:c=ffffff"); run(50);            // the show's first P
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);
  run(1000); CHECK(FLD->_seq == 14);                       // back to the show's dark baseline

  // every boundary leaves nothing of the last look behind
  resetAll(); send("L*A:i=solid,c=ff0000,m=200,am=2,an=2,ap=1"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(200);
  CHECK(gContract[0].accentEvery == 0x11 && gBeat.running);
  send("**M:v=show");
  CHECK(gContract[0].inShow && gContract[0].effect == CE_OFF && gContract[0].color.r == 0);
  CHECK(gContract[0].beatMod == 0 && gContract[0].accentMode == 0 && gContract[0].accentEvery == 0);
  CHECK(!gBeat.running);                                   // no display needs the clock any more
  run(50); CHECK(FLD->_seq == 0 && RLD->_seq == 0);        // the displays it held are given back to NORMAL
  send("L*A:i=solid,c=00ff00"); run(200); send("**X"); run(50);
  CHECK(gContract[0].inShow && gContract[0].base == CB_DARK && FLD->_seq == 14);   // X: dark, still in its mode
  send("**M:v=idle"); run(50);
  CHECK(!gContract[0].inShow && gContract[0].base == CB_NONE && FLD->_seq == 0 && RLD->_seq == 0);

  // the two displays share one clock: a reset stops it only when neither needs it
  resetAll(); send("L*A:i=solid,c=ff0000"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(200);
  send("LFX"); CHECK(gBeat.running);                       // the rear still has its look
  send("LRX"); CHECK(!gBeat.running);

  // a display the contract never took is left alone, whatever the engine is running on it
  resetAll(); FLD->selectSequence(3); run(50);
  const int selects = FLD->_selects;
  send("**M:v=idle"); send("**M:v=show"); run(50);
  CHECK(FLD->_seq == 3 && FLD->_selects == selects);

  // L3: a pulse with no show armed overlays the native look and returns to it by itself
  resetAll(); send("LFP:c=ffffff,d=200"); run(50);
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0 && gContract[0].base == CB_NONE);
  run(400); CHECK(FLD->_seq == 0 && !gContract[0].pulseActive);

  // L5: an A with no i= on a display with no look is i=solid; after that it is a patch
  resetAll(); send("LFA:c=00ff00"); run(50);
  CHECK(FLD->_seq == 106 && gContract[0].effect == CE_SOLID && FLD->px(0, 0).g > 0);
  run(200); send("LFA:c=0000ff"); run(250);
  CHECK(gContract[0].effect == CE_SOLID && FLD->px(0, 0).b > 0 && FLD->px(0, 0).g == 0);
  resetAll(); send("**M:v=show"); send("LFA:i=wipe,c=ff0000"); run(250); send("LFA:s=255"); run(250);
  CHECK(gContract[0].effect == CE_WIPE && gContract[0].speed == 255);       // a patch does not replace a look

  // S6: b= is a fraction of the ceiling, live and in a scored entry
  resetAll(); send("LFA:i=solid,c=ffffff"); run(50);
  const int full = FLD->px(0, 0).r;
  run(200); send("LFA:b=128"); run(250);
  CHECK(full > 0 && FLD->px(0, 0).r == full * 128 / 255);
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ffffff,b=128,at=0"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50);
  CHECK(FLD->px(0, 0).r == full * 128 / 255);
}

// ---- S1, C1, C2, C6: the one clock both displays share ----
static void test_clock() {
  // S1: a tempo out of range changes nothing, and does not start a show
  resetAll(); send("**M:v=show"); send("**C:bpm=600,ph=0,bpb=4,beat=0"); run(50);
  CHECK(!gBeat.running && gContract[0].base == CB_NONE && FLD->_seq == 0);

  // C6: bpm=0 holds. The score, the look and the section cursor stay; a C with a tempo resumes.
  resetAll(); send("**M:v=show");
  send("LFA:i=solid,c=ff0000,at=0"); send("LFA:i=solid,c=0000ff,at=8");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(2000);                       // beat 4, the red section
  CHECK(gContract[0].curSection == 0 && FLD->px(0, 0).r > 0);
  send("**C:bpm=0"); run(10000);                                          // Pause, for ten seconds
  CHECK(!gBeat.running && gContract[0].scoreLen == 2 && gContract[0].curSection == 0);
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0 && gContract[0].inShow);  // still the red look, still the show
  send("**C:ph=100,beat=30"); run(100);                                   // a bare correction does not resume it
  CHECK(!gBeat.running && gContract[0].curSection == 0);
  send("**C:bpm=120,ph=0,bpb=4,beat=4"); run(1900);                       // Resume where it stopped: beat 4
  CHECK(gBeat.running && gContract[0].curSection == 0);
  run(200); CHECK(gContract[0].curSection == 1 && FLD->px(0, 0).b > 0);   // beat 8 arrives 2 s after the resume

  // C1 for a scored A that arrives while the clock runs: mid-beat it must not fire that beat's accent
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,at=0,am=2,m=200,ae=flash");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(900);                        // 400 ms into beat 1
  const uint32_t armed = gContract[0].pulseStartMs;
  send("LFA:i=solid,c=00ff00,at=64,am=2,m=200,ae=flash"); run(50);        // a late line for a later section
  CHECK(gContract[0].pulseStartMs == armed);                              // no accent 400 ms off the beat
  run(100); CHECK(gContract[0].pulseStartMs != armed);                    // beat 2 accents on time

  // C2: a seek (bpm=) may move the cursor back; the regression file covers the nudge
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,at=0"); send("LFA:i=solid,c=0000ff,at=8");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(4100);
  CHECK(gContract[0].curSection == 1);
  send("**C:bpm=120,ph=0,bpb=4,beat=2"); run(50);                         // the user dragged the playhead back
  CHECK(gContract[0].curSection == 0 && FLD->px(0, 0).r > 0);
}

int main() {
  test_lifecycle();
  test_clock();
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract v1.3 on the RSeries layer OK\n");
  return 0;
}
