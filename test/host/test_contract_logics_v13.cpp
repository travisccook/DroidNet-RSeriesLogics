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
  send("**M:v=idle"); run(50);
  CHECK(LogicEngineFLDDefault.fBri == 160 && FLD->_liveBri == 160 && FLD->_seq == 0);
  resetAll(); send("**M:v=show"); send("LFB:b=64"); CHECK(LogicEngineFLDDefault.fBri == 40);
  send("LFX");                                      CHECK(LogicEngineFLDDefault.fBri == 160 && gContract[0].bright == 255);
  send("LFB:b=64");                                 CHECK(LogicEngineFLDDefault.fBri == 40);    // X stays in show mode
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
  resetAll(); send("**M:v=show"); send("LFB:b=64"); send("**M:v=show"); send("LFA:i=solid,c=ffffff"); run(50);
  CHECK(gContract[0].ceil == 160 && FLD->px(0, 0).r == 160);

  // S7: a line naming a native code off the allow-list is dropped whole, live or scored
  const int off[] = { 25, 15, 18, 99, 100, 106, 300, -1 };
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
  CHECK(FLD->_seq == 14 && !gContract[0].holdPending);     // a reset drops the held look
  // an X and a new look cannot be used to flash faster than the hold allows
  resetAll(); send("LFA:i=solid,c=ffffff"); run(20); send("LFX"); send("LFA:i=solid,c=ffffff"); run(100);
  CHECK(FLD->_seq == 14);                                  // still held: 120 ms after the first look
  run(60); CHECK(FLD->_seq == 106);
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
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.3,rel=1,cap=32,n=2,ok=0,bpm=0\r"));     // loaded, not checked yet
  sendV("F", n, k);
  CHECK(gContract[0].loadOk == 1 && gContract[0].scoreLen == 2 && gContract[0].wdSec == 15);
  run(300); CHECK(FLD->_seq == 0);                         // armed: the native idle look, no clock yet (C3)
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50);
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r == 80);        // the scored b=128 under the 160 ceiling
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.3,rel=1,cap=32,n=2,ok=1,bpm=120\r"));
  send("LRQ:r=2"); CHECK(reply(";W2,;S0!LRq:ver=1.3,rel=1,cap=32,n=0,ok=0,bpm=120\r"));   // routed via WCB 2
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
  CHECK(gContract[0].loadOk == 2 && !gContract[0].inShow && gContract[0].scoreLen == 0 && FLD->_seq == 0);
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.3,rel=1,cap=32,n=0,ok=2,bpm=0\r"));
  send("**M:v=idle"); send("**X");
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.3,rel=1,cap=32,n=0,ok=2,bpm=0\r"));     // ok=2 outlives idle and X...
  send("**M:v=show");
  send("LFQ:r=0"); CHECK(reply(";S0!LFq:ver=1.3,rel=1,cap=32,n=0,ok=0,bpm=0\r"));     // ...until the next M:v=show
  // one display failing its check does not stop the other, or the clock it plays from
  resetAll(); send("**M:v=show"); send("L*A:i=solid,c=ff0000,at=0"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(50);
  send("LFV:n=9,k=0000"); run(50);
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
  CHECK(FLD->_seq == 14 && gContract[0].wdSec == 2);
  run(1600); CHECK(FLD->_seq == 0 && !gContract[0].inShow);                // dark, then given back
  // no wd=: no watchdog, so a show stored in a WCB macro still plays
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); run(60000, 100);
  CHECK(FLD->_seq == 106 && gContract[0].inShow);
  // idle disarms it
  resetAll(); send("**M:v=show,wd=2"); send("**M:v=idle"); FLD->selectSequence(3); run(5000);
  CHECK(FLD->_seq == 3);
}

// ---- flash, pulse and scan on the core's shared timing (S8 for pulse) ----
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
static void test_shared_effect_timing() {
  // pulse: the shared triangle breathe. 2 s a cycle at s=0, dark at its start, full at half way.
  resetAll(); send("LFA:i=pulse,c=ffffff,s=0"); run(10);
  CHECK(FLD->px(0, 0).r <= 2);
  run(990); CHECK(FLD->px(0, 0).r >= 158 && FLD->px(0, 0).r <= 160);       // the peak is the ceiling, not above it
  run(1000); CHECK(FLD->px(0, 0).r <= 2);
  // S8: at the fastest speed the cycle is 340 ms, never shorter: under 2.95 swings a second
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255");
  uint32_t gap = 0;
  const int fast = levelSwings(30, 120, 10000, gap);
  CHECK(fast >= 29 && fast <= 30 && gap >= 330);                           // a 340 ms cycle: 2.94 a second
  // under the beat pump the fill is steady and the pump moves it (S2 keeps that under the same floor)
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255,m=255,am=2"); send("**C:bpm=300,ph=0,bpb=4,beat=0");
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 390);                     // 300 BPM: every second beat, 400 ms
  resetAll(); send("LFA:i=pulse,c=ffffff,s=255,m=255,am=2"); send("**C:bpm=60,ph=0,bpb=4,beat=0");
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 990 && gap <= 1010);      // one swing a beat: the breathe is not added on top

  // scan: one lit column, ping-pong, one step per fxStepMs (30 ms at s=255) on the 8-wide mock panel
  resetAll(); send("LFA:i=scan,c=ffffff,s=255");
  int col[16];
  for (int step = 0; step < 16; step++) {
    _mock_millis += (step == 0) ? 10 : 30; frame();
    int lit = -1, count = 0;
    for (int x = 0; x < 8; x++) if (FLD->px(x, 0).r > 0) { lit = x; count++; }
    CHECK(count == 1);
    col[step] = lit;
  }
  const int want[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1 };
  for (int step = 0; step < 16; step++) CHECK(col[step] == want[step]);
  for (int y = 0; y < 10; y++) CHECK(FLD->px(1, y).r > 0);                 // a whole column, top to bottom

  // flash: 170 ms a state at s=255 and 425 ms at s=0, the strobe cap at every speed
  resetAll(); send("LFA:i=flash,c=ffffff,s=255"); run(10);
  CHECK(FLD->px(0, 0).r == 160); run(150); CHECK(FLD->px(0, 0).r == 160);  // 160 ms: still lit
  run(20);  CHECK(FLD->px(0, 0).r == 0);                                   // 180 ms: dark
  run(160); CHECK(FLD->px(0, 0).r == 160);                                 // 340 ms: lit again
  resetAll(); send("LFA:i=flash,c=ffffff,s=255");
  levelSwings(30, 120, 10000, gap); CHECK(gap >= 330);
  resetAll(); send("LFA:i=flash,c=ffffff,s=0"); run(420);
  CHECK(FLD->px(0, 0).r == 160); run(20); CHECK(FLD->px(0, 0).r == 0);
}

int main() {
  test_lifecycle();
  test_clock();
  test_brightness_and_native();
  test_live_hold();
  test_score_verify_query();
  test_watchdog();
  test_shared_effect_timing();
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract v1.3 on the RSeries layer OK\n");
  return 0;
}
