// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Contract v1.3 on the RSeries layer: lifecycle, live hold, load verification, watchdog, query.
#include "logics_harness.h"
#include <cstring>

static void sendScored(const char* line, uint8_t& n, uint16_t& k) { send(line); n++; k ^= contractCrc16(line); }
static void sendV(const char* unit, uint8_t n, uint16_t k) {
  char v[40]; snprintf(v, sizeof v, "L%sV:n=%u,k=%04x", unit, (unsigned)n, (unsigned)k); send(v);
}

int main() {
  // L1: M:v=show, X and M:v=idle leave nothing behind, and stop the clock.
  resetAll(); send("L*A:i=solid,c=ff0000,m=200,am=2"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(200);
  send("**M:v=show");
  CHECK(gContract[0].inShow && gContract[0].base == CB_DARK && gContract[0].effect == CE_OFF);
  CHECK(gContract[0].color.r == 0 && gContract[0].beatMod == 0 && !gBeat.running);
  run(50); CHECK(FLD->_seq == 14 && RLD->_seq == 14);
  send("**X"); CHECK(gContract[0].inShow);                 // X stays in its mode
  send("**M:v=idle"); run(50);
  CHECK(!gContract[0].inShow && gContract[0].base == CB_NONE && FLD->_seq == 0 && RLD->_seq == 0);

  // S4: a second live look inside 170 ms is held, the newest one wins, and it lands on time.
  resetAll(); send("LFA:i=solid,c=ff0000"); run(50);
  send("LFA:i=solid,c=00ff00"); send("LFA:i=solid,c=0000ff"); run(100);
  CHECK(FLD->px(0, 0).r > 0 && FLD->px(0, 0).b == 0);      // 150 ms in: still the first look
  run(40);
  CHECK(FLD->px(0, 0).b > 0 && FLD->px(0, 0).r == 0 && FLD->px(0, 0).g == 0);   // blue, never green
  send("LFA:i=solid,c=00ff00"); send("**X"); run(400);
  CHECK(FLD->_seq == 14);                                  // a reset drops the held look

  // Score + V: a complete load is good; the first beat of the show is dark until Play.
  resetAll(); uint8_t n = 0; uint16_t k = 0;
  send("**M:v=show,wd=15");
  sendScored("LFA:i=solid,c=ff0000,b=80,at=0", n, k);
  sendScored("LFA:i=native:4,at=8,am=2,m=200,ae=flash", n, k);
  sendV("F", n, k);
  CHECK(gContract[0].loadOk == 1 && gContract[0].scoreLen == 2 && gContract[0].wdSec == 15);
  run(300); CHECK(FLD->_seq == 14);                        // C3: no clock yet, nothing plays
  send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(300);
  CHECK(FLD->_seq == 106 && FLD->px(0, 0).r == 80);        // b= is carried by the scored entry
  send("LFQ"); const char* q; uint8_t qn;
  CHECK(contractReplyPending(q, qn) && qn == 37 && memcmp(q, "!LFq:ver=1.3,cap=32,n=2,ok=1,bpm=120\r", qn) == 0);
  contractReplyDone(); send("L*Q"); CHECK(!contractReplyPending(q, qn));   // never answer a wildcard
  send("**Q");  CHECK(!contractReplyPending(q, qn));
  run(4000);                                               // into the native section (beat 8)
  CHECK(FLD->_seq == 4);
  run(4000);
  CHECK(FLD->_restarts[4] == 1 && !gContract[0].pulseActive);   // F5: no accent restarts it
  send("LFP:c=ffffff"); run(50); CHECK(FLD->_seq == 4);    // nor does a live pulse

  // V: a missing line sends the unit back to its native look and reports ok=2.
  resetAll(); n = 0; k = 0; send("**M:v=show");
  sendScored("LFA:i=solid,c=ff0000,at=0", n, k);
  k ^= contractCrc16("LFA:i=solid,c=0000ff,at=8"); n++;    // the host counted a line the board never got
  sendV("F", n, k); run(50);
  CHECK(gContract[0].loadOk == 2 && !gContract[0].inShow && gContract[0].scoreLen == 0 && FLD->_seq == 0);
  send("LFQ"); CHECK(contractReplyPending(q, qn) && memcmp(q, "!LFq:ver=1.3,cap=32,n=0,ok=2,bpm=0\r", qn) == 0);
  // V: an overflowed table is bad even when n and k match what arrived.
  resetAll(); n = 0; k = 0; send("**M:v=show");
  for (int i = 0; i < SCORE_MAX + 1; i++) {
    char l[48]; snprintf(l, sizeof l, "LFA:i=solid,c=ff0000,at=%d", i * 4); sendScored(l, n, k);
  }
  CHECK(gContract[0].scoreLen == SCORE_MAX && gContract[0].overflow);
  sendV("F", n, k); CHECK(gContract[0].loadOk == 2);
  // S7: a scored native code off the allow-list is not stored.
  resetAll(); send("**M:v=show"); send("LFA:i=native:25,at=0"); send("LFA:i=native:99,at=4");
  CHECK(gContract[0].scoreLen == 0);

  // L2: wd= seconds of silence in show mode is M:v=idle. Any addressed line is a keepalive.
  resetAll(); send("**M:v=show,wd=2"); send("LFA:i=solid,c=ff0000"); run(1900);
  CHECK(FLD->_seq == 106);
  send("**C:bpm=120,ph=0,beat=0"); run(1900); CHECK(FLD->_seq == 106);     // kept alive
  send("P*A:i=solid"); run(1900);                           // a line for another board is not
  CHECK(FLD->_seq == 0 && !gContract[0].inShow && !gBeat.running);
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000"); run(60000, 100);
  CHECK(FLD->_seq == 106);                                 // no wd=: no watchdog

  // S6 / F7: the ceiling is the owner's brightness, B reaches native looks, idle puts it back.
  resetAll(); LogicEngineFLDDefault.fBri = 90;             // the owner turned the front down
  send("**M:v=show"); send("LFA:i=solid,c=ffffff,b=255"); run(50);
  CHECK(gContract[0].ceil == 90 && FLD->px(0, 0).r == 90);
  send("LFB:b=40"); send("LFA:i=native:0"); run(400);
  CHECK(FLD->_liveBri == 40 && LogicEngineFLDDefault.fBri == 40);
  send("**M:v=idle"); run(50);
  CHECK(LogicEngineFLDDefault.fBri == 90 && FLD->_liveBri == 90);
  // ...and it does not undo a change someone else made during the show.
  resetAll(); send("**M:v=show"); send("LFB:b=40"); LogicEngineFLDDefault.fBri = 255; send("**M:v=idle");
  CHECK(LogicEngineFLDDefault.fBri == 255);

  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract v1.3 on the RSeries layer OK\n");
  return 0;
}
