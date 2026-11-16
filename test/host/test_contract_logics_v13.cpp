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

int main() {
  test_lifecycle();
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract v1.3 on the RSeries layer OK\n");
  return 0;
}
