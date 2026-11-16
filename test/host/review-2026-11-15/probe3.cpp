#include <initializer_list>
#include "mock2.h"
#include "../../../src/contract/ContractLogics.h"
#include <cmath>
static void send(const char* s){ ParsedContract p; if (contractParse(s,p)) applyContract(p); }
static void resetAll(){ for(int u=0;u<2;u++){ gContract[u]=ContractUnit{}; gScoreLen[u]=0; } gBeat=BeatClock{}; gShowMode=false; gLevel=0;
  _fld_obj = LogicEngineRenderer{}; _rld_obj = LogicEngineRenderer{}; _rld_obj._id = 2; }
static void frame(){ contractTick(); if (FLD->_seq==106) ContractRender(*FLD); if (RLD->_seq==106) ContractRender(*RLD); }
int main(){
  printf("P1: beatPosAt float vs exact (double), worst beat-time error in ms\n");
  for (int bpm : {97, 137, 174}) for (uint32_t span : {600000u, 3600000u, 86400000u, 1000000000u}) {
    BeatClock bc; bc.running=true; bc.bpm=bpm; bc.anchorMs=4000000000u; bc.bpb=4; bc.beatOffset=0;   // anchor near the millis() wrap
    double worst=0; long idxErr=0;
    for (uint32_t d = span>200000? span-200000:0; d < span; d += 37) {
      BeatPos bp = beatPosAt(bc, bc.anchorMs + d);
      double exact = (double)d * bpm / 60000.0; double got = bp.beatIndex + bp.phase;
      double e = fabs(got-exact) * 60000.0 / bpm; if (e>worst) worst=e; if ((long)floor(exact)!=bp.beatIndex) idxErr++;
    }
    printf("   bpm=%3d  %10u ms after anchor (across the 32-bit wrap): worst %.3f ms, wrong beat index on %ld samples\n", bpm, span, worst, idxErr);
  }
  { BeatClock bc; bc.running=true; bc.bpm=120; bc.anchorMs=1000; bc.bpb=4;
    BeatPos a = beatPosAt(bc, 1000u + 2147483000u), b = beatPosAt(bc, 1000u + 2147484000u);
    printf("   24.86 days after the last seed: beatIndex %ld -> %ld (int32 ms delta wraps; the clock is never stopped by X / M:v=idle)\n", (long)a.beatIndex, (long)b.beatIndex); }
  { BeatClock bc; ContractParams pr; pr.hasBpm=true; pr.bpm=65535; pr.hasBeat=true; pr.beatAnchor=0; beatClockSeed(bc,pr,0);
    BeatPos a = beatPosAt(bc, 10), b = beatPosAt(bc, 20); printf("   bpm=65535 accepted: beat index advances %ld per 10 ms frame\n", (long)(b.beatIndex-a.beatIndex)); }

  printf("P2: pump at the host default m=200 (am=2), white, b=200\n");
  for (int bpm : {150,180,200,240}) { resetAll(); _mock_millis=100000; char l[96]; send("LFA:i=solid,c=ffffff,b=200,m=200,am=2"); snprintf(l,sizeof l,"**C:bpm=%d,ph=0,bpb=4,beat=0",bpm); send(l);
    int mn=255,mx=0,rises=0; bool low=true; for (uint32_t t=0;t<10000;t+=10){ _mock_millis=100000+t; frame(); int v=FLD->_px[0][0].r; if(v<mn)mn=v; if(v>mx)mx=v; if(low&&v>=150){rises++;low=false;} else if(!low&&v<=70) low=true; }
    printf("   bpm=%3d: level %d..%d, %.1f swings/s\n", bpm, mn, mx, rises/10.0); }

  printf("P3: i=flash (s=255, red) + live P (white) every 340 ms, offset 170 ms: colour/luminance state changes on pixel(0,0)\n");
  { resetAll(); _mock_millis=100000; send("LFA:i=flash,c=ff0000,s=255"); int changes=0; CRGB last=FLD->_px[0][0];
    for (uint32_t t=0;t<10000;t+=10){ _mock_millis=100000+t; if (t%340==170) send("LFP:c=ffffff,d=170"); frame(); CRGB c=FLD->_px[0][0]; if (c.r!=last.r||c.g!=last.g||c.b!=last.b){changes++; last=c;} }
    printf("   %.1f state changes/s (a 2.94 Hz on/off flash is 5.9 changes/s)\n", changes/10.0); }

  printf("P4: accent on a scored NATIVE section restarts the native sequence every accent beat\n");
  { resetAll(); _mock_millis=100000; send("**M:v=show"); send("LFA:i=native:4,at=0,am=2,m=200,ae=flash"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); int s0=FLD->_selects;
    int toNative=0, last=FLD->_seq; for (uint32_t t=0;t<10000;t+=10){ _mock_millis=100000+t; frame(); if (FLD->_seq!=last){ if (FLD->_seq==4) toNative++; last=FLD->_seq; } }
    printf("   native:4 (re)selected %d times in 10 s (20 beats); selectSequence calls=%d\n", toNative, FLD->_selects-s0); }

  printf("P5: scoreInsert at cap keeps arrival order, not time order\n");
  { ScoreEntry t[4]; int n=0; for (int at : {40,30,20,10,0}) { ScoreEntry e; e.atBeat=at; int n2=scoreInsert(t,n,4,e); if (n2==n) printf("   at=%d dropped silently\n",at); n=n2; } printf("   table:"); for(int i=0;i<n;i++) printf(" %ld",(long)t[i].atBeat); printf("\n"); }
  return 0;
}
