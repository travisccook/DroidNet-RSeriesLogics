#include "mock2.h"
#include "../../../src/contract/ContractLogics.h"
#include <cmath>
static void send(const char* s){ ParsedContract p; if (contractParse(s,p)) applyContract(p); else printf("   (rejected: %s)\n", s); }
static void resetAll(){ for(int u=0;u<2;u++){ gContract[u]=ContractUnit{}; gScoreLen[u]=0; } gBeat=BeatClock{}; gShowMode=false; gLevel=0;
  _fld_obj = LogicEngineRenderer{}; _rld_obj = LogicEngineRenderer{}; _rld_obj._id = 2; _fld_obj._id=1; }
// one "engine frame": tick + (if 106 selected) render, like mainLoop
static void frame(){ contractTick(); if (FLD->_seq==106) ContractRender(*FLD); if (RLD->_seq==106) ContractRender(*RLD); }
static void run(uint32_t ms, uint32_t step=10){ uint32_t end=_mock_millis+ms; while(_mock_millis<end){ _mock_millis+=step; frame(); } }
static void show(const char* tag){ CRGB c=FLD->_px[0][0]; printf("   %-44s FLD seq=%3d px00=(%3d,%3d,%3d) effect=%d\n", tag, FLD->_seq, c.r,c.g,c.b,(int)gContract[0].effect); }
int main(){
  printf("T1: pulse on a freshly booted board (native NORMAL=0 on screen)\n");
  resetAll(); _mock_millis=5000; send("L*P:c=ffffff,d=120"); run(50); show("during pulse"); run(1000); show("1 s after pulse (expected: native 0)");

  printf("T2: look, stop (X), then a stray pulse\n");
  resetAll(); _mock_millis=5000; send("L*A:i=solid,c=ff0000"); run(100); show("red look"); send("**X"); run(100); show("after X (LIGHTSOUT=14)");
  send("L*P:c=ffffff"); run(1000); show("1 s after pulse (expected: 14/dark)");

  printf("T2b: M:v=idle then a pulse\n");
  resetAll(); _mock_millis=5000; send("L*A:i=solid,c=ff0000"); run(100); send("**M:v=idle"); run(100); show("after idle (NORMAL=0)");
  send("L*P:c=ffffff"); run(1000); show("1 s after pulse (expected: 0)");

  printf("T3: second show's pre-roll shows the first show's last look\n");
  resetAll(); _mock_millis=5000; send("L*A:i=solid,c=00ff00"); run(100);
  send("**M:v=show"); run(20); show("after M:v=show");
  send("L*A:i=solid,c=0000ff,at=8"); send("**C:bpm=120,ph=2000,bpb=4,beat=0"); run(500); show("score loaded, before beat 8 (expected dark)");

  printf("T4: A with d= (spec: auto-revert after N ms)\n");
  resetAll(); _mock_millis=5000; send("LFA:i=solid,c=ff0000,d=300"); run(2000); show("2 s after d=300");

  printf("T5: scored b= is dropped; numeric wrap instead of clamp\n");
  resetAll(); _mock_millis=5000; send("**C:bpm=120,ph=0,bpb=4,beat=0"); send("LFA:i=solid,c=ffffff,b=40,at=0"); run(100); show("scored b=40 (white at bright 40 => 40)");
  { ParsedContract p; contractParse("LFA:s=300,b=300,m=256,am=258",p); printf("   s=300->%d b=300->%d m=256->%d am=258->%d\n",p.params.speed,p.params.bright,p.params.beatMod,p.params.accentMode); }
  { ParsedContract p; contractParse("**C:bpm=128.5,ph=10",p); printf("   bpm=128.5->%u",p.params.bpm); contractParse("**C:bpm=-1",p); printf("  bpm=-1->%u",p.params.bpm); contractParse("**C:bpm=70000",p); printf("  bpm=70000->%u (host int=32bit; AVR atoi differs)\n",p.params.bpm); }

  printf("T6: verb P d= is unbounded\n");
  resetAll(); _mock_millis=5000; send("L*A:i=solid,c=ff0000"); send("L*P:c=ffffff,d=4000000000"); run(60000,100); show("60 s after P d=4e9 (expected red)");
  resetAll(); _mock_millis=5000; send("L*A:i=solid,c=ff0000"); send("L*P:c=ffffff,d=-1"); run(60000,100); show("60 s after P d=-1");

  printf("T7: native codes pass straight to selectSequence\n");
  resetAll(); send("LFA:i=native:25"); printf("   native:25  -> FLD seq=%d (LogicEffectDefaultSelector: 25 entries, guard is '>' not '>=')\n", FLD->_seq);
  resetAll(); send("LFA:i=native:300"); printf("   native:300 -> FLD seq=%d (byte cast)\n", FLD->_seq);
  resetAll(); send("LFA:i=native:99"); printf("   native:99  -> FLD seq=%d (RANDOM)\n", FLD->_seq);
  return 0;
}
