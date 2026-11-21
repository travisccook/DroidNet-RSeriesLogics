// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Contract v1.3 on the RSeries layer, on the mock engine: what each spec item looks like on the
// two logic displays. (The rules themselves are unit-tested in test_contract_v13.cpp.)
#include "logics_harness.h"
#include <cstring>
#include <initializer_list>

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
  CHECK(gContract[0].base == CB_DARK && FLD->_fx == 106 && RLD->_fx == 106);   // effect 106 draws the dark baseline
  CHECK(fieldLevel(FLD) == 0 && fieldLevel(RLD) == 0);     // dark on the LEDs, which is what counts
  run(1100); CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);   // ...and the first section on its beat

  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); run(50);    // the show's first live A
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);
  CHECK(RLD->_seq == 0 && gContract[1].base == CB_NONE);   // only the display addressed has begun

  resetAll(); send("**M:v=show"); send("LFP:c=ffffff"); run(50);            // the show's first P
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r > 0);
  run(1000);
  // Back to the show's dark baseline, ON THE PIXELS. (The 2026-11-16 review's finding 1: the
  // sequence number said lights-out while the overlay's last frame stayed lit.)
  CHECK(fieldLevel(FLD) == 0 && FLD->_fx == 106 && !gContract[0].pulseActive);
  // the same in a show where one display has no look and the accents are wildcards
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=0000ff"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(500);
  CHECK(fieldLevel(RLD) == 0);
  send("L*P:c=ffffff,d=170"); run(100); CHECK(fieldLevel(RLD) > 100 && FLD->px(0, 0).r > 100);
  run(200); CHECK(fieldLevel(RLD) == 0 && FLD->px(0, 0).b > 0 && FLD->px(0, 0).r == 0);    // each back to its own base

  // every boundary leaves nothing of the last look behind
  resetAll(); send("L*A:i=solid,c=ff0000,m=200,am=2,an=2,ap=1"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(200);
  CHECK(gContract[0].accentEvery == 0x11 && gBeat.running);
  send("**M:v=show");
  CHECK(gContract[0].inShow && gContract[0].effect == CE_OFF && gContract[0].color.r == 0);
  CHECK(gContract[0].beatMod == 0 && gContract[0].accentMode == 0 && gContract[0].accentEvery == 0);
  CHECK(!gBeat.running);                                   // no display needs the clock any more
  run(50); CHECK(FLD->_seq == 0 && RLD->_seq == 0);        // the displays it held are given back to NORMAL
  send("L*A:i=solid,c=00ff00"); run(200); send("**X"); run(50);
  CHECK(gContract[0].inShow && gContract[0].base == CB_DARK && FLD->_fx == 106);   // X: dark, still in its mode
  CHECK(fieldLevel(FLD) == 0 && fieldLevel(RLD) == 0);
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
  CHECK(gContract[0].curSection == 1);                                    // S4 for every path: 170 ms from the last look change...
  run(100);
  CHECK(gContract[0].curSection == 0 && FLD->px(0, 0).r > 0);            // ...and then the section the beat is in
}

// ---- S6, S7: the owner's brightness is the ceiling; verb B; the native allow-list ----
static void test_brightness_and_native() {
  // the ceiling is the display's own brightness, read when the show arms
  resetAll(); LogicEngineFLDDefault.fBri = 90;             // the owner turned the front down
  send("**M:v=show"); send("L*A:i=solid,c=ffffff,b=255"); run(50);
  CHECK(gContract[0].ceil == 90 && FLD->px(0, 0).r == 90);                // full is the owner's 90, not 255
  CHECK(gContract[1].ceil == 140 && RLD->px(0, 0).r == 140);              // each display has its own
  run(200); send("LFA:b=128"); run(250);
  CHECK(FLD->px(0, 0).r == 45);                                           // half of the ceiling
  // ...and with no show at all (a live cue on a booted board)
  resetAll(); send("LFA:i=solid,c=ffffff"); run(50);
  CHECK(gContract[0].ceil == 160 && FLD->px(0, 0).r == 160);
  // the accent overlay rides the look's level, so it is under the ceiling too
  resetAll(); send("LFP:c=ffffff,d=200"); run(50);
  CHECK(FLD->px(0, 0).r == 160);

  // verb B reaches native looks and survives the engine's restoreSettings()
  resetAll(); send("**M:v=show"); send("LFB:b=64"); send("LFA:i=native:0"); run(50);
  CHECK(FLD->_seq == 0 && FLD->_liveBri == 40 && LogicEngineFLDDefault.fBri == 40);   // 160 * 64 / 255
  run(200); send("LFA:i=native:4"); run(250);
  CHECK(FLD->_seq == 4 && FLD->_liveBri == 40);                           // an effect change later, still 40
  CHECK(RLD->_liveBri == 140 && LogicEngineRLDDefault.fBri == 140);       // the rear was not addressed
  run(200); send("LFA:i=native:0,b=255"); run(250);
  CHECK(FLD->_liveBri == 160);                                            // b= on a native look does the same
  send("LFB:b=64"); CHECK(FLD->_liveBri == 40);
  // every boundary puts the owner's brightness back
  // (S9: the native look was dimmed to a quarter a moment ago, so its way back up is an onset and
  // waits for the window: 340 ms from the b=255 above. It is back well inside half a second.)
  send("**M:v=idle"); run(50);
  CHECK(FLD->_seq == 0 && FLD->_liveBri < 160);
  run(350);
  CHECK(LogicEngineFLDDefault.fBri == 160 && FLD->_liveBri == 160 && FLD->_seq == 0 && !gBriTouched[0]);
  resetAll(); send("**M:v=show"); send("LFB:b=64"); CHECK(LogicEngineFLDDefault.fBri == 40);
  send("LFX"); run(20);                             CHECK(LogicEngineFLDDefault.fBri == 160 && gContract[0].bright == 255);
  // (the engine's brightness goes back once effect 106 is drawing the dark display: nobody sees it move)
  send("LFB:b=64"); run(20);                        CHECK(gContract[0].bright == 64);           // X stays in show mode: B is still taken
  CHECK(LogicEngineFLDDefault.fBri == 160);         // (effect 106 holds the dark display: nothing to dim in the engine)
  send("**M:v=show");                               CHECK(LogicEngineFLDDefault.fBri == 160 && gContract[0].bright == 255);
  // S6: B is honoured only in show mode. Outside a show it changes nothing, on any look.
  resetAll(); send("LFB:b=64"); send("L*B:v=10");
  CHECK(gContract[0].bright == 255 && LogicEngineFLDDefault.fBri == 160 && FLD->_liveBri == 160 && FLD->_calcCalls == 0);
  send("LFA:i=solid,c=ffffff"); run(50); CHECK(FLD->px(0, 0).r == 160);
  run(200); send("LFB:b=64"); run(50);   CHECK(FLD->px(0, 0).r == 160);
  send("**M:v=show"); send("LFA:i=solid,c=ffffff"); send("LFB:b=64"); run(250);
  CHECK(FLD->px(0, 0).r == 40);                            // ...and in one, it dims a contract look too
  send("**M:v=idle"); send("LFB:b=64"); CHECK(gContract[0].bright == 255 && LogicEngineFLDDefault.fBri == 160);
  // ...but does not undo a change someone else made during the show (#LEBRI, the web UI, the trimpot)
  resetAll(); send("**M:v=show"); send("LFB:b=64"); LogicEngineFLDDefault.fBri = 255; send("**M:v=idle");
  CHECK(LogicEngineFLDDefault.fBri == 255);
  // the ceiling is re-read at each boundary, from the owner's value, not from what B wrote
  resetAll(); send("**M:v=show"); send("LFB:b=64"); run(50); send("**M:v=show"); run(400);
  CHECK(gContract[0].ceil == 160 && FLD->_liveBri == 160);
  send("LFA:i=solid,c=ffffff"); run(50);
  CHECK(FLD->px(0, 0).r == 160);

  // S7: a line naming a native code off the allow-list is dropped whole, live or scored
  const int off[] = { 25, 15, 18, 99, 100, 106, 300, -1, 3 };   // 3: Leia sets its own brightness, past the ceiling
  for (int code : off) {
    char l[40];
    resetAll(); send("LFA:i=solid,c=ff0000"); run(200);
    snprintf(l, sizeof l, "LFA:i=native:%d,c=00ff00", code); send(l); run(250);
    CHECK(FLD->_seq == 106 && gContract[0].color.r == 0xff && FLD->_oobCalls == 0);   // not even its colour was taken
    resetAll(); send("**M:v=show"); snprintf(l, sizeof l, "LFA:i=native:%d,at=0", code); send(l);
    CHECK(gContract[0].scoreLen == 0);
  }
  const int on[] = { 0, 4, 14, 19, 24, 101, 105 };
  for (int code : on) {
    char l[40];
    resetAll(); snprintf(l, sizeof l, "LFA:i=native:%d", code); send(l); run(50);
    CHECK(FLD->_seq == code && gContract[0].base == CB_NATIVE);
  }

  // no overlay on a native look (R1): neither the scored accent nor a live P restarts it
  resetAll(); send("**M:v=show"); send("LFA:i=native:4,at=0,am=2,m=200,ae=flash");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(5000);
  CHECK(FLD->_seq == 4 && FLD->_restarts[4] == 1 && !gContract[0].pulseActive);
  send("LFP:c=ffffff"); run(300);
  CHECK(FLD->_seq == 4 && FLD->_restarts[4] == 1 && !gContract[0].pulseActive);
  // ...while the same accent on a contract look still fires
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,at=0,am=2,m=200,ae=flash");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50);
  CHECK(gContract[0].pulseActive);
}

// ---- S4: live look changes are at least 170 ms apart ----
static void test_live_hold() {
  resetAll(); send("LFA:i=solid,c=ff0000"); run(50);
  send("LFA:i=solid,c=00ff00"); send("LFA:i=solid,c=0000ff"); run(100);
  CHECK(FLD->px(0, 0).r > 0 && FLD->px(0, 0).b == 0);      // 150 ms in: still the first look
  run(40);
  CHECK(FLD->px(0, 0).b > 0 && FLD->px(0, 0).r == 0 && FLD->px(0, 0).g == 0);   // blue; green never showed
  run(200); send("LFA:c=00ff00"); send("LFA:s=7"); run(200);                    // two held patches merge
  CHECK(FLD->px(0, 0).g > 0 && gContract[0].speed == 7);
  send("LFA:i=solid,c=ff0000"); send("**X"); run(400);
  CHECK(fieldLevel(FLD) == 0 && !gContract[0].holdPending); // a reset drops the held look
  // S9: an X and a new look cannot be used to flash a display. The second look is held by S4
  // until 170 ms after the first, and it would then light the display 170 ms after it last lit:
  // a dark-to-lit onset needs 340 ms from the one before, whichever lines caused the two. So the
  // field gate holds it down to under a quarter of the ceiling until then. (This test used to
  // assert the relight at 170 ms as correct.)
  resetAll(); send("LFX"); run(500);
  send("LFA:i=solid,c=ffffff"); run(20); CHECK(fieldLevel(FLD) == 160);    // the first onset, 10 ms in
  send("LFX"); send("LFA:i=solid,c=ffffff"); run(100);
  CHECK(fieldLevel(FLD) == 0 && gContract[0].holdPending);                 // S4: still held, 120 ms after the first look
  run(60);  CHECK(gContract[0].base == CB_LOOK && fieldLevel(FLD) <= 40);  // 180 ms: applied, and held down by the gate
  run(150); CHECK(fieldLevel(FLD) <= 40);                                  // 330 ms
  run(30);  CHECK(fieldLevel(FLD) == 160);                                 // 360 ms: 340 after the first onset
  // the two displays hold separately
  resetAll(); send("LFA:i=solid,c=ff0000"); run(50); send("LRA:i=solid,c=00ff00"); run(20);
  CHECK(RLD->_seq == 106 && RLD->px(0, 0).g > 0);
}

// ---- sections 6, 7, 10: the score, the load check, the query ----
static void sendScored(const char* line, uint8_t& n, uint16_t& k) { send(line); n++; k ^= contractCrc16(line); }
static void sendV(const char* unit, uint8_t n, uint16_t k) {
  char v[40]; snprintf(v, sizeof v, "L%sV:n=%u,k=%04x", unit, (unsigned)n, (unsigned)k); send(v);
}
static bool reply(const char* want) {
  const char* q; uint8_t qn;
  if (!contractReplyPending(q, qn)) return false;
  const bool same = qn == strlen(want) && memcmp(q, want, qn) == 0;
  if (!same) printf("     reply was: %.*s\n", (int)qn - 1, q);
  contractReplyDone();
  return same;
}
static void test_score_verify_query() {
  const char* q; uint8_t qn;
  // a complete load is good; nothing plays until Play sends the clock
  resetAll(); uint8_t n = 0; uint16_t k = 0;
  send("**M:v=show,wd=15");
  sendScored("LFA:i=solid,c=ff0000,b=128,at=0", n, k);
  sendScored("LFA:i=native:4,at=8,am=2,m=200,ae=flash", n, k);
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.4,rel=2,cap=32,n=2,ok=0,bpm=0\r"));     // loaded, not checked yet
  sendV("F", n, k);
  CHECK(gContract[0].loadOk == 1 && gContract[0].scoreLen == 2 && gContract[0].wdSec == 15);
  run(300); CHECK(FLD->_seq == 0);                         // armed: the native idle look, no clock yet (C3)
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50);
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r == 80);        // the scored b=128 under the 160 ceiling
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.4,rel=2,cap=32,n=2,ok=1,bpm=120\r"));
  send("LRQ:r=2"); CHECK(reply(";W2,;S0!LRq:ver=1.4,rel=2,cap=32,n=0,ok=0,bpm=120\r"));   // routed via WCB 2
  send("LFQ");     CHECK(!contractReplyPending(q, qn));    // no r=: not answered
  send("L*Q:r=0"); CHECK(!contractReplyPending(q, qn));    // a wildcard is never answered
  send("**Q:r=0"); CHECK(!contractReplyPending(q, qn));
  send("PFQ:r=0"); CHECK(!contractReplyPending(q, qn));    // another board's query
  run(4000); CHECK(FLD->_seq == 4);                        // into the native section, on beat 8

  // a line delivered twice (a mesh retry) changes neither n nor k
  resetAll(); n = 0; k = 0; send("**M:v=show");
  sendScored("LFA:i=solid,c=ff0000,at=0", n, k); sendScored("LFA:i=solid,c=0000ff,at=8", n, k);
  send("LFA:i=solid,c=0000ff,at=8"); send("LFA:i=solid,c=ff0000,at=0");
  sendV("F", n, k); CHECK(gContract[0].loadOk == 1 && gContract[0].scoreLen == 2);
  // a V that expects a line the board never got: back to the native look, and Q says so
  resetAll(); n = 0; k = 0; send("**M:v=show");
  sendScored("LFA:i=solid,c=ff0000,at=0", n, k);
  k ^= contractCrc16("LFA:i=solid,c=0000ff,at=8"); n++;    // lost on the way
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50); CHECK(FLD->_seq == 106);
  sendV("F", n, k); run(50);
  CHECK(gContract[0].loadOk == 2 && !gContract[0].inShow && gContract[0].scoreLen == 0);
  // (the display goes back to NORMAL when that is no onset: red to NORMAL's white is one in
  // luminance, and the red look lit 40 ms ago, so the hand-back waits out the 340 ms)
  CHECK(FLD->_seq == 106);
  run(350); CHECK(FLD->_seq == 0 && FLD->_fx == 0);
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.4,rel=2,cap=32,n=0,ok=2,bpm=0\r"));
  send("**M:v=idle"); send("**X");
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.4,rel=2,cap=32,n=0,ok=2,bpm=0\r"));     // ok=2 outlives idle and X...
  send("**M:v=show");
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.4,rel=2,cap=32,n=0,ok=0,bpm=0\r"));     // ...until the next M:v=show
  // one display failing its check does not stop the other, or the clock it plays from
  resetAll(); send("**M:v=show"); send("L*A:i=solid,c=ff0000,at=0"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50);
  send("LFV:n=9,k=0000"); run(400);
  CHECK(FLD->_seq == 0 && RLD->_seq == 106 && gBeat.running);
  // a line whose bytes changed on the way is a bad load too
  resetAll(); n = 0; k = 0; send("**M:v=show");
  k ^= contractCrc16("LFA:i=solid,c=ff0000,at=0"); n++; send("LFA:i=solid,c=ff0001,at=0");
  sendV("F", n, k); CHECK(gContract[0].loadOk == 2);
  // capacity is 32; the 33rd distinct entry marks the load bad even when the host's n and k match what arrived
  resetAll(); n = 0; k = 0; send("**M:v=show");
  for (int i = 0; i < SCORE_MAX; i++) {
    char l[48]; snprintf(l, sizeof l, "LFA:i=solid,c=ff0000,at=%d", i * 4); sendScored(l, n, k);
  }
  CHECK(SCORE_MAX == 32 && gContract[0].scoreLen == 32 && !gContract[0].overflow);
  { ContractUnitState copy = gContract[0]; CHECK(scoreVerify(copy, gScore[0], n, k) == 1); }
  send("LFA:i=solid,c=ff0000,at=4000");
  CHECK(gContract[0].scoreLen == 32 && gContract[0].overflow);
  sendV("F", n, k); CHECK(gContract[0].loadOk == 2);
  // a V without both numbers is not a V
  resetAll(); send("**M:v=show"); send("LFV:n=0"); send("LFV:k=0000"); CHECK(gContract[0].loadOk == 0 && gContract[0].inShow);

  // every-N accents from the score: front on even beats, rear on odd ones
  resetAll(); send("**M:v=show");
  send("LFA:i=solid,c=ff0000,at=0,am=2,m=200,an=2,ap=0,ae=flash");
  send("LRA:i=solid,c=0000ff,at=0,am=2,m=200,an=2,ap=1,ae=flash");
  send("**C:bpm=120,ph=0,bpb=4,beat=0");
  int both = 0, front = 0, rear = 0; uint32_t lf = 0, lr = 0;
  for (uint32_t t = 10; t < 8000; t += 10) {                // beats 0..15 (beat 16 starts at 8000 ms)
    _mock_millis += 10; frame();
    const bool f = gContract[0].pulseActive && gContract[0].pulseStartMs != lf;
    const bool r = gContract[1].pulseActive && gContract[1].pulseStartMs != lr;
    if (f) { lf = gContract[0].pulseStartMs; front++; }
    if (r) { lr = gContract[1].pulseStartMs; rear++; }
    if (f && r) both++;
  }
  if (front != 8 || rear != 8 || both != 0) printf("     front=%d rear=%d both=%d\n", front, rear, both);
  CHECK(front == 8 && rear == 8 && both == 0);             // 16 beats: eight each, never together
}

// ---- the 2026-11-16 review's findings that the core (CA1.2) closes, seen on the displays ----
static void test_review_r1_core() {
  // 5 (S1): a bpm= that is not a number does not hold the show. The clock keeps its tempo, and
  // the host's next bare correction still lands.
  for (const char* l : { "**C:bpm=", "**C:bpm=abc", "**C:bpm=-120", "**C:bpm=x20", "**C:bpm= ", "**C:bpm=12O" }) {
    resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,at=0"); send("LFA:i=solid,c=0000ff,at=8");
    send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(1000);
    const uint32_t anchor = gBeat.anchorMs;
    send(l); run(100);
    CHECK(gBeat.running && gBeat.bpm == 120 && gBeat.anchorMs == anchor);
    send("**C:ph=0,beat=2"); run(4000);
    CHECK(gBeat.running && gContract[0].curSection == 1 && FLD->px(0, 0).b > 0);     // it played on into the second section
  }
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(500);
  send("**C:bpm=0"); CHECK(!gBeat.running);                // the real Hold is untouched

  // 4 (section 7): a unit that failed its V stays on its native look. A scored line that arrives
  // late, and then Play, must not start half a show.
  resetAll(); send("**M:v=show,wd=15"); send("LFA:i=solid,c=ff0000,at=0"); send("LFV:n=2,k=1234"); run(50);
  CHECK(gContract[0].loadOk == 2 && !gContract[0].inShow && FLD->_fx == 0);
  send("LFA:i=solid,c=0000ff,at=8");                       // the mesh retry lands after the V
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(5000);
  CHECK(gContract[0].scoreLen == 0 && gContract[0].base == CB_NONE && FLD->_fx == 0);
  { const char* q; uint8_t qn; send("LFQ:r=0"); CHECK(contractReplyPending(q, qn) && strstr(q, ",n=0,ok=2,") != nullptr); contractReplyDone(); }
  send("LFM:v=show"); send("LFA:i=solid,c=0000ff,at=0");                             // the next show loads and plays
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(600);
  CHECK(gContract[0].scoreLen == 1 && gContract[0].loadOk == 0 && FLD->_fx == 106 && FLD->px(0, 0).b > 0);

  // 8: a scored line delivered again while it plays changes nothing: not the section's timeline,
  // and not a B the host sent since.
  resetAll(); send("**M:v=show"); send("LFA:i=wipe,c=ffffff,at=0"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(500);
  send("LFB:b=64"); run(300);
  const uint32_t started = gContract[0].startMs;
  send("LFA:i=wipe,c=ffffff,at=0"); run(300);
  CHECK(gContract[0].startMs == started && gContract[0].bright == 64 && gContract[0].curSection == 0);
  send("LFA:i=wipe,c=ff0000,at=0"); run(50);               // a DIFFERENT line for that beat does replace it:
  CHECK(gContract[0].startMs == started && gContract[0].color.g == 0);     // another colour recolours the wipe in place (CA1.7),
  send("LFA:i=chase,c=ff0000,at=0"); run(200);
  CHECK(gContract[0].startMs != started && gContract[0].effect == CE_CHASE);   // another look starts it again

  // 6: beat indices at the ends of the int32 range are clamped at parse (the sanitized fuzz stage
  // proves the arithmetic; this is the layer taking such lines without harm)
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,at=2147483647,am=3,m=200,ae=flash");
  send("LFA:i=solid,c=00ff00,at=-2147483648,am=2,an=16,ap=15,m=200,ae=flash");
  send("**C:bpm=300,beat=2147483647"); run(500); send("**C:bpm=300,beat=-2147483648"); run(500);
  CHECK(gScore[0][0].atBeat == -CONTRACT_BEAT_MAX && gScore[0][1].atBeat == CONTRACT_BEAT_MAX && gBeat.beatOffset == -CONTRACT_BEAT_MAX);

  // 7 (section 10): an r= that is not a number 0..99 gets no reply
  for (const char* l : { "LFQ:r=", "LFQ:r=zz", "LFQ:r=100", "LFQ:r=-1", "LFQ:r=1x" }) {
    const char* q; uint8_t qn;
    resetAll(); send(l); CHECK(!contractReplyPending(q, qn));
  }
}

// Full-field swings on the front display over `ms`: the level falls to <= lo and then rises to
// >= hi. Returns how many, and in minGapMs the shortest time between two of them (sampled every
// 10 ms, so a 340 ms cycle reads as 330 to 350). The gap is the safety number: a count over a
// window is off by one depending on where the window starts.
static int levelSwings(int lo, int hi, uint32_t ms, uint32_t& minGapMs) {
  int rises = 0; bool low = true; uint32_t last = 0; minGapMs = 0xFFFFFFFFu;
  for (uint32_t t = 0; t < ms; t += 10) {
    _mock_millis += 10; frame();
    const int v = FLD->px(0, 0).r;
    if (low && v >= hi) {
      if (rises > 0 && _mock_millis - last < minGapMs) minGapMs = _mock_millis - last;
      last = _mock_millis; rises++; low = false;
    } else if (!low && v <= lo) low = true;
  }
  return rises;
}
// ---- S9 on this board, path by path (stage 2e tests it as a property; these are the plain cases) ----
static void test_s9_paths() {
  // B on a contract look: dimming is at once, and the way back up is an onset like any other
  resetAll(); send("**M:v=show"); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(500);
  send("LFB:b=0"); run(20);   CHECK(fieldLevel(FLD) == 0);
  send("LFB:b=255"); run(20); CHECK(fieldLevel(FLD) == 160);               // 510 ms after the display last lit: at once
  send("LFB:b=0"); run(20);   CHECK(fieldLevel(FLD) == 0);
  send("LFB:b=255"); run(20); CHECK(fieldLevel(FLD) <= 40);                // 40 ms after it last lit: held down
  run(280); CHECK(fieldLevel(FLD) <= 40);
  run(40);  CHECK(fieldLevel(FLD) == 160);                                 // 340 ms after it last lit
  // a look that only dims a little is not a blackout, and its way back is not an onset
  // (the step is more than a tenth of the ceiling: 17 levels of the owner's 160)
  // (CA1.7: a higher B is taken at most once in 340 ms, whatever its size: contractBrightDue. So
  // each of these comes 400 ms after the B that last went up.)
  run(400);
  send("LFB:b=240"); run(20); CHECK(fieldLevel(FLD) == 150); send("LFB:b=255"); run(20); CHECK(fieldLevel(FLD) == 160);
  send("LFB:b=240"); run(20); CHECK(fieldLevel(FLD) == 150); send("LFB:b=255"); run(20); CHECK(fieldLevel(FLD) == 150);   // 40 ms after it: it waits
  run(300); CHECK(fieldLevel(FLD) == 160);
  run(400);
  send("LFB:b=220"); run(20); CHECK(fieldLevel(FLD) == 138);              // a dip of more than a step is a swing,
  send("LFB:b=255"); run(20); CHECK(fieldLevel(FLD) == 160);              // and the way back an onset (the first in a long time)
  send("LFB:b=220"); run(20); CHECK(fieldLevel(FLD) == 138);
  send("LFB:b=255"); run(20);                                             // the next one waits: the frame that is up stays up
  CHECK(fieldLevel(FLD) == 138);
  run(260); CHECK(fieldLevel(FLD) == 138);
  run(60);  CHECK(fieldLevel(FLD) == 160);

  // P's b= is the overlay's own: it never reaches the base look, and a P the cool-down refuses changes nothing
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ffffff"); run(500);
  send("LFP:c=ff0000,b=30,d=200"); run(60);
  CHECK(FLD->px(0, 0).r == 160 * 30 / 255 && FLD->px(0, 0).g == 0 && gContract[0].bright == 255);
  send("LFP:c=ff0000,b=255,d=200"); run(60);                               // 60 ms after the last accent: refused whole
  CHECK(FLD->px(0, 0).r == 160 * 30 / 255 && gContract[0].pulseBright == 30);
  run(200); CHECK(fieldLevel(FLD) == 160 && FLD->px(0, 0).g == 160);       // the white base look, as it was
  // a P with no b= rides the look's level
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ffffff,b=128"); run(500); send("LFP:c=ff0000"); run(60);
  CHECK(FLD->px(0, 0).r == 80 && FLD->px(0, 0).g == 0);

  // giving a dark display back to the engine lights it: that waits for the window too
  resetAll(); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20); CHECK(fieldLevel(FLD) == 160);
  send("LFX"); run(20); CHECK(fieldLevel(FLD) == 0);
  send("LFM:v=idle"); run(50);
  CHECK(fieldLevel(FLD) == 0 && FLD->_fx == 106 && gContract[0].base == CB_NONE);    // still dark, 80 ms after it last lit
  run(230); CHECK(fieldLevel(FLD) == 0);
  run(60);  CHECK(FLD->_fx == 0 && fieldLevel(FLD) == 160);                // NORMAL again, 340 ms after
  // (X and idle in ONE pass never show a dark frame. The hand-back waits all the same, S10: a
  // stock look's first light counts as an onset whatever the display shows, lit or dark, so it
  // keeps 340 ms from when the white look came up. The frame that is up stays up meanwhile.)
  resetAll(); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20); send("LFX"); send("LFM:v=idle"); run(20);
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 160);
  run(280); CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 160);
  run(40);  CHECK(FLD->_fx == 0 && fieldLevel(FLD) == 160);
  // ...and at once when there was no onset to wait out
  resetAll(); send("LFA:i=solid,c=ffffff"); run(500); send("LFX"); run(500); send("LFM:v=idle"); run(20);
  CHECK(FLD->_fx == 0 && fieldLevel(FLD) == 160);
  // X is never the one that waits
  resetAll(); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20); send("LFX"); run(10);
  CHECK(fieldLevel(FLD) == 0);

  // B on a native look (the idle look of an armed show): the same rule, on the engine's brightness
  resetAll(); send("**M:v=show"); run(500);
  send("LFB:b=0"); run(20);   CHECK(FLD->_fx == 0 && FLD->_liveBri == 0);  // dimmer: at once
  send("LFB:b=255"); run(20); CHECK(FLD->_liveBri == 160);                 // nothing lit lately: at once
  send("LFB:b=0"); run(20);   CHECK(FLD->_liveBri == 0);
  send("LFB:b=255"); run(100); CHECK(FLD->_liveBri == 0);                  // an onset 40 ms after the last: held
  run(240); CHECK(FLD->_liveBri == 160);                                   // by 340 ms it is back
  // a native section takes its brightness at the frame it starts, through the gate
  resetAll(); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20); send("LFX"); run(200);
  send("LFA:i=native:4"); run(50);
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 0);                          // 220 ms after the display last lit: it waits, dark
  run(120); CHECK(FLD->_fx == 4 && FLD->_liveBri == 160);                  // and starts when the window ends
  // native lights-out is dark, never held, and what follows it is an onset
  resetAll(); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20); send("LFX"); run(180);
  send("LFA:i=native:14"); run(20); CHECK(FLD->_fx == 14 && fieldLevel(FLD) == 0);
  send("LFM:v=idle"); run(50);      CHECK(FLD->_fx == 14);                 // 250 ms after the last onset
  run(100);                         CHECK(FLD->_fx == 0 && fieldLevel(FLD) == 160);
  // A selection that is replaced before the engine draws it never happened: it is not on the
  // LEDs, and the gate is not told it was. (Idle and then X, in one pass: the display stays
  // dark, and the next look is not held back for an onset that never lit anything.)
  resetAll(); send("LFX"); run(500); send("LFM:v=idle"); send("LFX"); run(20);
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 0 && FLD->_restarts[0] == 0);
  send("LFA:i=solid,c=ffffff"); run(20); CHECK(fieldLevel(FLD) == 160);

  // the pump across a seam: two scored sections a beat apart at 300 BPM, each accenting "its" beat
  resetAll(); send("**M:v=show");
  for (int b = 0; b < 16; b++) { char l[96]; snprintf(l, sizeof l, "LFA:i=solid,c=ffffff,at=%d,am=2,m=255,an=2,ap=%d", b, b % 2); send(l); }
  send("**C:bpm=300,ph=0,bpb=4,beat=0");
  uint32_t gap = 0; const int swings = levelSwings(16, 120, 3200, gap);
  CHECK(swings >= 6 && swings <= 9 && gap >= 340);                         // it was 5 a second, 200 ms apart

  // a scored A carries its look: without i= it is dropped, not stored as solid black
  resetAll(); send("**M:v=show"); send("LFA:i=wipe,c=ff0000,at=0"); send("LFA:b=128,at=4"); send("LFA:i=bogus,c=00ff00,at=8");
  CHECK(gContract[0].scoreLen == 1 && gScore[0][0].effect == CE_WIPE);

  // section 10: one reply per display can wait; the front's goes first
  { const char* q; uint8_t qn;
    resetAll(); send("LFQ:r=0"); send("LRQ:r=2"); send("LFQ:r=0");
    CHECK(contractReplyPending(q, qn) && memcmp(q, ";S0!LFq:", 8) == 0); contractReplyDone();
    CHECK(contractReplyPending(q, qn) && memcmp(q, ";W2,;S0!LRq:", 12) == 0); contractReplyDone();
    CHECK(!contractReplyPending(q, qn)); }
}

// ---- L6: a native command during a show ends the show on that display ----
// "A native command" on this board is anything that starts a sequence the contract did not
// select: @nT... through the Marcduino actions, the engine's own command handler, the web page.
// They all end in the engine starting that sequence, which is what the layer is told
// (contractEngineLatched). The tests stand in for the button with a direct selectSequence().
static void test_native_takeover() {
  // a live look: the button takes the display, and the show is over on it (not on the other one)
  resetAll(); send("**M:v=show,wd=2"); send("L*A:i=solid,c=ff0000"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(1000);
  FLD->selectSequence(3); run(20);
  CHECK(FLD->_fx == 3 && !gContract[0].inShow && gContract[0].base == CB_NONE);
  CHECK(gContract[1].inShow && RLD->_fx == 106 && gBeat.running);          // the rear plays on, from the same clock
  const int restarts0 = FLD->_restarts[0];
  send("**Q"); run(1500); send("**Q"); run(1500); send("**L:v=1"); run(1500);
  CHECK(FLD->_fx == 3 && FLD->_restarts[0] == restarts0);                  // nothing of the show comes back to it
  run(2500);                                                               // the host goes away: the watchdog ends the REST
  CHECK(!gContract[1].inShow && RLD->_fx == 0 && FLD->_fx == 3 && FLD->_restarts[0] == restarts0);   // ...and leaves the button's sequence alone

  // a scored show with accents: the next accent, and the next section, do not take it back
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,at=0,am=2,m=200,ae=flash"); send("LFA:i=solid,c=00ff00,at=4");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(1100);
  FLD->selectSequence(3); run(20); CHECK(FLD->_fx == 3);
  run(4000); CHECK(FLD->_fx == 3 && gContract[0].scoreLen == 0 && !gContract[0].pulseActive);
  // until the host starts it again
  send("LFM:v=show"); send("LFA:i=solid,c=0000ff"); run(50); CHECK(FLD->_fx == 106 && FLD->px(0, 0).b > 0);

  // a dark display (X), a native section, an overlay outside a show: all of them are "held"
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); run(300); send("LFX"); run(300);
  FLD->selectSequence(5); run(20); CHECK(FLD->_fx == 5 && !gContract[0].inShow && gContract[0].base == CB_NONE);
  resetAll(); send("**M:v=show"); send("LFA:i=native:4,b=128"); run(300); CHECK(FLD->_fx == 4 && FLD->_liveBri == 80);
  FLD->selectSequence(5); run(20);
  CHECK(FLD->_fx == 5 && !gContract[0].inShow && gContract[0].base == CB_NONE);
  // ...and the owner's brightness comes back: brighter is an onset, so 340 ms after the button's sequence started (N2)
  run(400);
  CHECK(FLD->_liveBri == 160 && LogicEngineFLDDefault.fBri == 160 && !gBriTouched[0]);
  resetAll(); send("LFP:c=ffffff,d=2000"); run(100); CHECK(FLD->_fx == 106);
  FLD->selectSequence(3); run(20); CHECK(FLD->_fx == 3 && !gContract[0].pulseActive);
  const int normalStarts = FLD->_restarts[0];
  run(3000); send("**M:v=show"); run(50);
  CHECK(FLD->_fx == 3 && FLD->_restarts[0] == normalStarts);               // arming does not cut the button's sequence off

  // the owner's brightness comes back through the gate when the show had dimmed a native look to dark
  resetAll(); send("**M:v=show"); send("LFA:i=native:4,b=0"); run(500); CHECK(FLD->_liveBri == 0);
  FLD->selectSequence(5); run(20); CHECK(FLD->_fx == 5);
  run(400); CHECK(FLD->_liveBri == 160);

  // NOT a take-over: a timed native section that runs out (the engine falls back to NORMAL by itself)
  resetAll(); send("**M:v=show"); send("LFA:i=native:4,at=0"); send("LFA:i=solid,c=00ff00,at=8");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(500); CHECK(FLD->_fx == 4);
  FLD->selectSequence(0); run(20);
  CHECK(FLD->_fx == 0 && gContract[0].inShow && gContract[0].scoreLen == 2);
  run(4000); CHECK(FLD->_fx == 106 && FLD->px(0, 0).g > 0);                // the next section takes the display
  // NOT a take-over: the sequence the contract itself selected, and effect 106 coming back
  resetAll(); send("**M:v=show"); send("LFA:i=native:4"); run(300); send("LFA:i=solid,c=ff0000"); run(300);
  CHECK(gContract[0].inShow && FLD->_fx == 106);
  // NOT covered: an armed show that has not begun. The display is the engine's own, and the
  // engine starts sequences by itself there (RANDOM does it every few seconds), so a button
  // cannot be told from that. The button plays; the show's first C, A or P then takes the display.
  resetAll(); send("**M:v=show"); run(300); FLD->selectSequence(3); run(300);
  CHECK(FLD->_fx == 3 && gContract[0].inShow);
  send("LFA:i=solid,c=ff0000"); run(50); CHECK(FLD->_fx == 106);
  // someone selecting effect 106 by its number gets the default look, not a frozen display
  resetAll(); FLD->selectSequence(106); run(30); CHECK(FLD->_fx == 0 && FLD->_seq == 0);
}

// ---- the 2026-11-16 re-review of ab9addd: N2, N3 and N5 (N1 and the gate's step are stage 2e's) ----
// The owner moves the display's brightness during a show: the web slider or the trimpot writes
// the default object and the live value (LogicEngine's changeDefaultSettings()).
static void ownerSets(int v) { LogicEngineFLDDefault.fBri = (byte)v; FLD->setBrightness((byte)v); }
static int brightestPixel(const LogicEngineRenderer* r) {
  int m = 0;
  for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) { const CRGB c = r->px(x, y); const int v = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b); if (v > m) m = v; }
  return m;
}
static void test_rereview() {
  // N3: the owner turns the display DOWN while a contract-dimmed native look is up, and the host
  // arms again (Studio's Resend). The owner's 80 is the display's brightness from then on: the
  // re-arm must not put the old 160 back, the next look's ceiling is 80, and idle leaves 80.
  resetAll(); send("**M:v=show"); send("LFA:i=native:0,b=64"); run(500);
  CHECK(LogicEngineFLDDefault.fBri == 40);
  ownerSets(80); run(200);
  send("**M:v=show"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 80 && FLD->_liveBri == 80);          // (it used to go back to 160, and stay there)
  send("LFA:i=solid,c=ffffff"); run(600);
  CHECK(gContract[0].ceil == 80 && brightestPixel(FLD) == 80);
  send("**M:v=idle"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 80 && FLD->_liveBri == 80);
  // ...the same when the next line is a native look at b=255: 255 is the ceiling, and the ceiling is the owner's 80
  resetAll(); send("**M:v=show"); send("LFA:i=native:0,b=64"); run(500); ownerSets(80); run(200);
  send("LFA:i=native:1,b=255"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 80 && FLD->_liveBri == 80);
  send("**M:v=idle"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 80);
  // (while a contract look is up the engine's brightness is the owner's anyway: this always worked)
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ffffff"); run(500); ownerSets(80); run(200); send("**M:v=show"); run(600);
  send("LFA:i=solid,c=ffffff"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 80 && gContract[0].ceil == 80 && brightestPixel(FLD) == 80);

  // N5: after the owner's change the host's NEXT B is honoured again, as a fraction of the new brightness
  resetAll(); send("**M:v=show"); send("LFA:i=native:0"); send("LFB:b=64"); run(500); ownerSets(80); run(200);
  CHECK(LogicEngineFLDDefault.fBri == 80);                                  // theirs stands...
  send("LFB:b=255"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 80);                                  // ...b=255 of 80 is 80
  send("LFB:b=128"); run(600);
  CHECK(LogicEngineFLDDefault.fBri == 40 && FLD->_liveBri == 40);

  // S10, three things a second fork's property stage found, checked here too.
  // (1) A native look's first light counts as an onset whatever the display shows: it may clear
  //     the display and light it at once. So it takes a LIT display only 340 ms after the display
  //     last lit, as it would a dark one, and the look that is up stays up until then.
  resetAll(); send("**M:v=show"); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20);
  CHECK(fieldLevel(FLD) == 160);
  send("LFA:i=native:4"); run(170);                       // (held as a live look for 170 ms: S4)
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 160);
  run(140); CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 160);
  run(40);  CHECK(FLD->_fx == 4 && fieldLevel(FLD) == 160);
  // (2) ...and so does the hand-back to the display's own look, from a lit display
  resetAll(); send("**M:v=show"); send("LFX"); run(500); send("LFA:i=solid,c=ffffff"); run(20);
  send("**M:v=idle"); run(300);
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 160);
  run(40); CHECK(FLD->_fx == 0 && fieldLevel(FLD) == 160);
  // (3) Verb L feeds the contract's own meter look and nothing else: a native sequence that is
  //     up, a mic effect included, is not touched by it.
  resetAll(); send("**M:v=show"); send("LFA:i=native:12"); run(600);
  CHECK(FLD->_fx == 12 && fieldLevel(FLD) == 160);
  { const int calls = FLD->_calcCalls, selects = FLD->_selects;
    send("L*L:v=0"); run(100); send("L*L:v=255"); run(100); send("L*L:v=3"); run(100);
    CHECK(FLD->_fx == 12 && fieldLevel(FLD) == 160 && FLD->_liveBri == 160 && FLD->_calcCalls == calls && FLD->_selects == selects); }

  // An X always lands. Two of them inside one engine frame, with a button between them: the first
  // selected effect 106, the button replaced that selection before the frame, the second is the
  // later word. (Found by the property stage at 1 to 4 ms a pass.)
  resetAll(); run(500);
  send("**X"); FLD->selectSequence(4); send("**X"); run(50);
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 0);
  // ...and so does one X after a button, with effect 106 running
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ffffff"); run(500);
  FLD->selectSequence(4); send("LFX"); run(50);
  CHECK(FLD->_fx == 106 && fieldLevel(FLD) == 0);

  // N2 / S10: the gate does not see inside a native look, and a native look is not always steady
  // light. So the frame a contract look takes over from one counts as the field's last
  // brightening: an X and a relight straight after a native look wait the 340 ms out.
  resetAll(); send("**M:v=show"); send("LFA:i=native:24"); run(2000);
  CHECK(FLD->_fx == 24);
  send("LFX"); run(10); send("LFA:i=solid,c=ffffff"); run(20);
  CHECK(fieldLevel(FLD) <= 16);
  run(300); CHECK(fieldLevel(FLD) <= 16);
  run(40);  CHECK(fieldLevel(FLD) == 160);
  // ...NORMAL is the exception: an armed show begins from it at once
  resetAll(); run(1000); send("**M:v=show"); send("LFA:i=solid,c=ffffff"); run(20);
  CHECK(fieldLevel(FLD) == 160);
  // FAILURE (2) is dark from its eighth second and ends itself at 18 s: the engine relights
  // NORMAL then, by itself, and the show goes on. The contract's next light keeps 340 ms from that.
  resetAll(); send("**M:v=show"); send("LFA:i=native:2"); run(17990);
  CHECK(FLD->_fx == 2 && fieldLevel(FLD) == 0);                             // (the mock has FAILURE's timing)
  run(30); CHECK(FLD->_fx == 0 && fieldLevel(FLD) == 160 && gContract[0].inShow);   // it ran out: NORMAL again
  send("LFX"); run(10); send("LFA:i=solid,c=ffffff"); run(20);
  CHECK(fieldLevel(FLD) <= 16);
  // FLASHCOLOR (6) is full and 50/255 by turns, every 250 ms: its whole field moves by itself,
  // so nothing is known of it when it ends. A contract look that takes the display from it, in
  // its dim half or just after its own flash, waits the window out.
  resetAll(); send("**M:v=show"); send("LFA:i=native:6"); run(20);
  CHECK(FLD->_fx == 6 && fieldLevel(FLD) == 160);
  run(250); CHECK(fieldLevel(FLD) == 31);
  run(250); CHECK(fieldLevel(FLD) == 160);                                  // its own ON edge, 510 ms in
  send("LFA:i=solid,c=ffffff"); run(20);
  CHECK(FLD->_fx == 106 && FLD->_writes == 0);                              // effect 106 has the display and has drawn nothing:
  { const long w = FLD->_writes; run(300); CHECK(FLD->_writes == w); }      // the sequence's last frame stays up, for the window
  run(40);  CHECK(fieldLevel(FLD) == 160 && FLD->_writes != 0);
  // ...and a brighter B reaches such a sequence only when it next starts; a dimmer one at once
  resetAll(); send("**M:v=show"); send("LFA:i=native:6,b=128"); run(600);
  CHECK(FLD->_fx == 6 && LogicEngineFLDDefault.fBri == 80);
  send("LFB:b=64"); run(50);  CHECK(LogicEngineFLDDefault.fBri == 40);
  send("LFB:b=255"); run(900); CHECK(LogicEngineFLDDefault.fBri == 40);     // not on top of its own flashes
  send("LFA:i=native:1"); run(900); CHECK(FLD->_fx == 1 && LogicEngineFLDDefault.fBri == 160);   // the next look has it
  // (a steady sequence takes it while it runs, through the gate, as before)
  resetAll(); send("**M:v=show"); send("LFA:i=native:1,b=128"); run(600); send("LFB:b=255"); run(600);
  CHECK(FLD->_fx == 1 && LogicEngineFLDDefault.fBri == 160);

  // S10: where a stock sequence's rate follows the engine's speed digit, the contract starts it
  // at the fastest speed that passes the test (the core's nativeSpeedLogics)
  { static const int kSeq[][2] = { { 4, 1 }, { 19, 1 }, { 20, 2 }, { 22, 1 }, { 6, 0 }, { 7, 0 }, { 24, 0 }, { 1, 0 } };
    for (const auto& q : kSeq) {
      char l[40]; snprintf(l, sizeof l, "LFA:i=native:%d", q[0]);
      resetAll(); send("**M:v=show"); send(l); run(100);
      CHECK(FLD->_fx == q[0] && FLD->_speed == q[1]);
    }
    resetAll(); send("**M:v=show"); send("LFA:i=native:4"); run(100); send("LFA:i=solid,c=ffffff"); run(600);
    CHECK(FLD->_fx == 106 && FLD->_speed == 0); }

  // S11: re-applying the same look does not restart it; a look that differs in anything does
  resetAll(); send("**M:v=show"); send("LFA:i=gradient,c=ffffff,s=90"); run(500);
  { const uint32_t t0 = gContract[0].startMs;
    send("LFA:i=gradient,c=ffffff,s=90"); run(300); CHECK(gContract[0].startMs == t0);
    send("LFA:b=200"); run(300);                    CHECK(gContract[0].startMs == t0 && gContract[0].bright == 200);
    send("LFA:c=ffffff"); run(300);                 CHECK(gContract[0].startMs == t0);
    send("LFA:s=91"); run(300);                     CHECK(gContract[0].startMs != t0);
    const uint32_t t1 = gContract[0].startMs;
    send("LFA:c=ff0000"); run(300);                 CHECK(gContract[0].startMs == t1 && gContract[0].color.g == 0);   // CA1.7: recoloured in place
    send("LFA:i=scan"); run(300);                   CHECK(gContract[0].startMs != t1); }
  // CA1.7: a scan in another colour goes on from where it is. The lit column does not jump back
  // to the first one, and the new colour comes when the look gate lets it (170 ms after the last).
  { resetAll(); send("**M:v=show"); send("LFA:i=scan,c=ff0000,s=255"); run(400);
    int before = -1; for (int x = 0; x < 8; x++) if (FLD->px(x, 0).r > 0) before = x;
    CHECK(before >= 4);                                                    // 72 ms a column: five columns in
    send("LFA:i=scan,c=0000ff,s=255"); run(80);
    int after = -1; for (int x = 0; x < 8; x++) if (FLD->px(x, 0).b > 0) after = x;
    CHECK(after >= before && FLD->px(after < 0 ? 0 : after, 0).r == 0); }  // on its way still, in blue
  // ...and neither does a section that carries on the look of the one before it
  resetAll(); send("**M:v=show");
  send("LFA:i=gradient,c=ffffff,s=90,at=0"); send("LFA:i=gradient,c=ffffff,s=90,b=128,at=4"); send("LFA:i=solid,c=ffffff,at=8");
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(1000);
  { const uint32_t t0 = gContract[0].startMs;
    run(1500); CHECK(gContract[0].curSection == 1 && gContract[0].startMs == t0 && gContract[0].bright == 128);
    run(2000); CHECK(gContract[0].curSection == 2 && gContract[0].startMs != t0); }
}

// ---- L2: the host-silence watchdog ----
static void test_watchdog() {
  resetAll(); send("**M:v=show,wd=2"); send("LFA:i=solid,c=ff0000"); run(1900);
  CHECK(FLD->_seq == 106);
  send("**C:bpm=120,ph=0,beat=0"); run(1900); CHECK(FLD->_seq == 106);     // any addressed line keeps it alive
  send("**Q"); run(1900);           CHECK(FLD->_seq == 106);               // a wildcard Q too, though it gets no reply
  send("LRL:v=10"); run(1900);      CHECK(FLD->_seq == 106);               // a line for the other display: same board
  send("P*A:i=solid"); send("HTX"); send("LFA:i=sol"); run(1900);          // another board's lines are not...
  CHECK(FLD->_seq == 106);                                                 // ...but LFA:i=sol is well-formed and ours
  send("xx"); send("L"); send("LFz"); run(2100);                           // malformed lines are not
  CHECK(FLD->_seq == 0 && !gContract[0].inShow && !gContract[1].inShow && !gBeat.running);
  // X leaves the watchdog armed
  resetAll(); send("**M:v=show,wd=2"); send("LFA:i=solid,c=ff0000"); run(200); send("**X"); run(500);
  CHECK(fieldLevel(FLD) == 0 && FLD->_fx == 106 && gContract[0].wdSec == 2);
  run(1600); CHECK(FLD->_seq == 0 && !gContract[0].inShow);                // dark, then given back
  // no wd=: no watchdog, so a show stored in a WCB macro still plays
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); run(60000, 100);
  CHECK(FLD->_seq == 106 && gContract[0].inShow);
  // idle disarms it
  resetAll(); send("**M:v=show,wd=2"); send("**M:v=idle"); FLD->selectSequence(3); run(5000);
  CHECK(FLD->_seq == 3);
}

// ---- flash, pulse and scan on the core's shared timing (S8 for pulse) ----
static void test_shared_effect_timing() {
  // pulse: the shared triangle breathe. 2 s a cycle at s=0, dark at its start, full at half way.
  resetAll(); send("LFA:i=pulse,c=ffffff,s=0"); run(10);
  CHECK(FLD->px(0, 0).r <= 2);
  run(990); CHECK(FLD->px(0, 0).r >= 158 && FLD->px(0, 0).r <= 160);       // the peak is the ceiling, not above it
  run(1000); CHECK(FLD->px(0, 0).r <= 2);
  // S8, CA1.5: at the fastest speed the cycle is 680 ms, never shorter: 1.47 swings a second,
  // and the S9 gate leaves the triangle as drawn (at 340 ms it squared the rise off)
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255");
  uint32_t gap = 0;
  const int fast = levelSwings(30, 120, 10000, gap);
  CHECK(fast >= 14 && fast <= 15 && gap >= 670);
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255"); run(1020 + 170);        // a quarter of the way up its second rise
  CHECK(FLD->px(0, 0).r >= 76 && FLD->px(0, 0).r <= 84);                   // half the ceiling, as the triangle says: not held
  // chase lights the same number of columns at every step (3 of 8), so the field mean stands still
  resetAll(); send("LFA:i=chase,c=ffffff,s=255"); run(400);
  { const int level = fieldLevel(FLD); int moved = 0;
    for (int i = 0; i < 100; i++) { run(10); if (fieldLevel(FLD) != level) moved++; }
    CHECK(level == 160 * 3 / 8 && moved == 0); }
  // a wipe on 8 columns at the top speed takes 680 ms a cycle and is not held either
  resetAll(); send("LFA:i=wipe,c=ffffff,s=255"); run(10);
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 670 && gap <= 700);
  // under the beat pump the fill is steady and the pump moves it (S2 keeps that under the same floor)
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255,m=255,am=2"); send("**C:bpm=300,ph=0,bpb=4,beat=0");
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 390);                     // 300 BPM: every second beat, 400 ms
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255,m=255,am=2"); send("**C:bpm=60,ph=0,bpb=4,beat=0");
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 990 && gap <= 1010);      // one swing a beat: the breathe is not added on top

  // scan: one lit column, ping-pong, on the 8-wide mock panel. A lap there and back is never
  // under a second (CA1.7): 72 ms a step at s=255 (it was 30, and a quarter of the display went
  // from dark to half lit six times a second).
  resetAll(); send("LFA:i=scan,c=ffffff,s=255");
  int col[16];
  for (int step = 0; step < 16; step++) {
    if (step == 0) { _mock_millis += 10; frame(); }
    else for (int k = 0; k < 9; k++) { _mock_millis += 8; frame(); }
    int lit = -1, count = 0;
    for (int x = 0; x < 8; x++) if (FLD->px(x, 0).r > 0) { lit = x; count++; }
    CHECK(count == 1);
    col[step] = lit;
  }
  const int want[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1 };
  for (int step = 0; step < 16; step++) CHECK(col[step] == want[step]);
  for (int y = 0; y < 10; y++) CHECK(FLD->px(1, y).r > 0);                 // a whole column, top to bottom

  // flash: 190 ms a state at s=255 and 445 ms at s=0: over the strobe cap at every speed, by
  // the 20 ms that keep a late frame from putting the next ON edge inside the S9 window (CA1.5)
  resetAll(); send("LFA:i=flash,c=ffffff,s=255"); run(10);
  CHECK(FLD->px(0, 0).r == 160); run(170); CHECK(FLD->px(0, 0).r == 160);  // 180 ms: still lit
  run(20);  CHECK(FLD->px(0, 0).r == 0);                                   // 200 ms: dark
  run(180); CHECK(FLD->px(0, 0).r == 160);                                 // 380 ms: lit again
  resetAll(); send("LFA:i=flash,c=ffffff,s=255");
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 370 && gap <= 390);
  resetAll(); send("LFA:i=flash,c=ffffff,s=0"); run(440);
  CHECK(FLD->px(0, 0).r == 160); run(20); CHECK(FLD->px(0, 0).r == 0);
}

// ---- CA1.6: coloured looks through the gate, and what the property stage found on the way ----
static void test_ca16() {
  // A COLOURED LOOK THAT FADES IN IS NOT HELD (the defect: its brightest channel crossed the
  // gate's step before its luminance did, the second crossing was taken for a second onset, and
  // the fade stalled for 340 ms and then jumped). Green, as Studio's frame parity found it.
  for (const char* line : { "LFA:i=pulse,c=22c55e,s=0", "LFA:i=pulse,c=116530,s=0", "LFA:i=pulse,c=ff0000,s=0", "LFA:i=wipe,c=a855f7,s=128" }) {
    resetAll();
    LogicEngineFLDDefault.fBri = 255; FLD->_liveBri = 255;
    run(1000);                                              // the owner's brightness has been up for a while
    send("**M:v=show"); send(line);
    int last = -1, longest = 0, same = 0, jump = 0;
    for (int i = 0; i < 100; i++) {                         // the first second: a rise from black
      _mock_millis += 10; frame();
      const int level = fieldLevel(FLD);
      if (level == last && level != 0) { if (++same > longest) longest = same; } else same = 0;
      if (last >= 0 && level - last > jump) jump = level - last;
      last = level;
    }
    CHECK(longest < 12);                                    // no frame stands for 120 ms in a rise (it stood for 340)
    CHECK(jump < 40);                                       // and nothing jumps (it jumped by half the look's level)
  }
  // THE HAND-OVER THAT MAY HAVE LIT (found by the property stage, in the code since CA1.4). The
  // owner's brightness goes back on the idle look (a rise the layer writes), the engine draws
  // it, and a contract look takes the display before the layer has told the gate. It is told the
  // safe half: the field may have lit just now. That is a change of look too, and an onset: the
  // contract's colour does not follow the white frame 10 ms later (it did, dimmed to a tenth).
  // The frame that is up stays until the contract's look may light the display, 340 ms on.
  resetAll(); run(500);
  send("LFM:v=show"); run(400);
  send("LFB:b=0"); run(400);                                // the armed display's idle look, dimmed to nothing
  CHECK(fieldLevel(FLD) == 0 && FLD->_fx == 0);
  _mock_millis += 5;
  send("LFM:v=idle");                                       // the owner's brightness goes back: written now
  CHECK(gRise[0]);
  _mock_millis += 5; frame();                               // the engine draws it. The gate is not told yet.
  CHECK(fieldLevel(FLD) == 160 && gRise[0]);
  const uint32_t whiteAt = _mock_millis;
  send("LFA:i=solid,c=ff0000");
  bool redEarly = false; uint32_t redAt = 0, fullAt = 0;
  for (int i = 0; i < 60; i++) {
    _mock_millis += 10; frame();
    const CRGB c = FLD->px(3, 4);
    const bool red = c.r != 0 && c.g == 0;
    if (red && !redAt) redAt = _mock_millis;
    if (red && _mock_millis - whiteAt < 170) redEarly = true;
    if (red && c.r == 160 && !fullAt) fullAt = _mock_millis;
    if (!redAt) CHECK(c.r == 160 && c.g == 160 && c.b == 160);          // the white frame stays up meanwhile
  }
  CHECK(!redEarly && redAt != 0 && redAt == fullAt);                          // never dimmed: a held frame is not shown
  CHECK(fullAt != 0 && fullAt - whiteAt >= 340 && fullAt - whiteAt <= 370);   // and it lights 340 ms after the white did

  // VERB B MOVES PART OF THE FIELD TOO (the PSI's re-verification, CA1.7): a meter with one row
  // lit is a tenth of the display, under the gate's step, and B:b=0 / B:b=200 every 30 ms lit
  // that row as often as the lines came. A higher B, or a higher b= on the look that is up, is
  // taken at most once in 340 ms; a lower one at once.
  resetAll(); run(500);
  send("**M:v=show"); send("LFA:i=meter,c=ffffff"); send("LFL:v=40"); run(600);
  CHECK(FLD->px(0, 9).r == 160 && FLD->px(0, 8).r == 0);                    // one row of ten
  { bool on = true; uint32_t lastOn = 0; int ons = 0; uint32_t closest = 0xFFFFFFFFu;
    for (uint32_t t = 0; t < 5000; t += 10) {
      if (t % 30 == 0) send((t / 30) % 2 ? "LFB:b=200" : "LFB:b=0");
      _mock_millis += 10; frame();
      const bool now = FLD->px(3, 9).r != 0;
      if (now && !on) { if (ons && _mock_millis - lastOn < closest) closest = _mock_millis - lastOn; lastOn = _mock_millis; ons++; }
      on = now;
    }
    CHECK(ons >= 10 && ons <= 15 && closest >= 340); }                      // (it was 80 in these 5 s)
  resetAll(); run(500);
  send("**M:v=show"); send("LFA:i=meter,c=ffffff"); send("LFL:v=40"); run(600);
  { bool on = true; uint32_t lastOn = 0; int ons = 0; uint32_t closest = 0xFFFFFFFFu;
    for (uint32_t t = 0; t < 5000; t += 10) {
      if (t % 30 == 0) send((t / 30) % 2 ? "LFA:b=200" : "LFA:b=0");        // the same through b= on the look that is up
      _mock_millis += 10; frame();
      const bool now = FLD->px(3, 9).r != 0;
      if (now && !on) { if (ons && _mock_millis - lastOn < closest) closest = _mock_millis - lastOn; lastOn = _mock_millis; ons++; }
      on = now;
    }
    CHECK(ons >= 5 && ons <= 15 && closest >= 340); }
  // a lower B is at once, the newest raise is the one that lands, and X is not a way round it
  resetAll(); run(500);
  send("**M:v=show"); send("LFA:i=solid,c=ffffff"); run(600);
  send("LFB:b=128"); run(20); CHECK(fieldLevel(FLD) == 80 && gContract[0].bright == 128);
  send("LFB:b=255"); run(20); CHECK(gContract[0].bright == 255);            // the first raise in a long time
  send("LFB:b=64"); run(20);  CHECK(gContract[0].bright == 64);
  send("LFB:b=255"); run(10); send("LFB:b=200"); run(100);
  CHECK(gContract[0].bright == 64 && gContract[0].brightWant == 200);       // waits, and the newer one replaces it
  run(250); CHECK(gContract[0].bright == 200);
  send("LFX"); CHECK(gContract[0].bright == 255 && gContract[0].brightWant == 255);   // the reset puts it back at the ceiling

  // VERB L MOVES PART OF THE FIELD (the PSI review's finding, the same on this board): a meter
  // with L:v=255 and L:v=128 by turns every 50 ms blinked its top half ten times a second, and
  // the whole-field gate saw a mean that stayed legal. A higher level is now taken at most once
  // in 340 ms per display (contractLevelOffer); a lower one at once.
  resetAll(); run(500);
  send("**M:v=show"); send("L*A:i=meter,c=ffffff"); send("L*L:v=128"); run(600);
  CHECK(FLD->px(0, 9).r == 160 && FLD->px(0, 5).r == 160 && FLD->px(0, 4).r == 0 && FLD->px(0, 0).r == 0);   // half: the bottom five rows
  { bool top = false; uint32_t lastOn = 0; int ons = 0; uint32_t closest = 0xFFFFFFFFu;
    for (uint32_t t = 0; t < 5000; t += 10) {
      if (t % 50 == 0) send((t / 50) % 2 ? "L*L:v=128" : "L*L:v=255");
      _mock_millis += 10; frame();
      const bool now = FLD->px(3, 0).r != 0;                // the top row
      if (now && !top) { if (ons && _mock_millis - lastOn < closest) closest = _mock_millis - lastOn; lastOn = _mock_millis; ons++; }
      top = now;
      CHECK(FLD->px(3, 9).r == 160);                        // the bottom half never moves
    }
    CHECK(ons >= 10 && ons <= 15 && closest >= 340); }      // it still follows, 2 to 3 times a second (it was 10)
  // a fall is at once, and a raise that had to wait lands when the window ends (the newest wins)
  resetAll(); run(500);
  send("**M:v=show"); send("LFA:i=meter,c=ffffff"); run(400);
  send("LFL:v=255"); run(100);                                              // up, at T
  CHECK(FLD->px(0, 0).r == 160 && gContract[0].level == 255 && gContract[1].level == 0);   // each display has its own level
  send("LFL:v=0"); run(10); CHECK(fieldLevel(FLD) == 0);                    // down: at once
  send("LFL:v=255"); run(10); send("LFL:v=128"); run(100);                  // up again 110 ms after T: it waits. Then a newer raise.
  CHECK(FLD->px(0, 0).r == 0 && FLD->px(0, 9).r == 0 && gContract[0].level == 0);
  run(100); CHECK(fieldLevel(FLD) == 0);                                    // T + 320
  run(30); CHECK(FLD->px(0, 9).r == 160 && FLD->px(0, 0).r == 0 && gContract[0].level == 128);   // T + 350: the newest one, 128
  // X clears the level, and is not a way round the window
  send("LFL:v=255"); run(400); CHECK(gContract[0].level == 255);            // (up at T + 350 + 340)
  send("LFL:v=0"); send("LFL:v=255"); run(350); CHECK(gContract[0].level == 255);   // up again: the last rise, at U
  send("LFX"); send("LFA:i=meter,c=ffffff"); send("LFL:v=255"); run(100);
  CHECK(gContract[0].level == 0 && fieldLevel(FLD) == 0);                   // U + 100: the reset cleared the level; the raise waits
  run(300); CHECK(gContract[0].level == 255 && FLD->px(0, 0).r == 160);     // U + 400
}

int main() {
  test_ca16();
  test_lifecycle();
  test_clock();
  test_brightness_and_native();
  test_live_hold();
  test_score_verify_query();
  test_review_r1_core();
  test_s9_paths();
  test_native_takeover();
  test_rereview();
  test_watchdog();
  test_shared_effect_timing();
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract v1.3 on the RSeries layer OK (%d known defect(s) still open)\n", gKnown);
  return 0;
}
