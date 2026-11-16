#include "mock2.h"
#include "../../../src/contract/ContractLogics.h"
#include <cmath>
#include <vector>
static void send(const char* s){ ParsedContract p; if (contractParse(s,p)) applyContract(p); }
static void resetAll(){ for(int u=0;u<2;u++){ gContract[u]=ContractUnit{}; gScoreLen[u]=0; } gBeat=BeatClock{}; gShowMode=false; gLevel=0;
  _fld_obj = LogicEngineRenderer{}; _rld_obj = LogicEngineRenderer{}; _rld_obj._id = 2; }
static void frame(){ contractTick(); if (FLD->_seq==106) ContractRender(*FLD); if (RLD->_seq==106) ContractRender(*RLD); }

// Scored am=<am> section with an effect accent, music EXACTLY at the seeded tempo (so a
// re-anchor carries zero phase correction). Host re-anchors every 4000 ms, arriving `off` ms
// into a beat. Report accents that start off the beat grid and beats that lost their accent.
static void reanchorRun(int bpm, int am, uint32_t off, int& offBeat, int& missed, int& total, bool reanchor){
  resetAll(); const uint32_t T0 = 100000; _mock_millis = T0; const double per = 60000.0 / bpm;
  char line[96];
  send("**M:v=show");
  snprintf(line,sizeof line,"LFA:i=solid,c=3b82f6,at=0,am=%d,m=200,ae=flash",am); send(line);
  snprintf(line,sizeof line,"**C:bpm=%d,ph=0,bpb=4,beat=0",bpm); send(line);
  offBeat = missed = total = 0;
  uint32_t lastArm = 0; std::vector<char> hit(400,0);
  for (uint32_t t = 0; t < 60000; t++) {
    _mock_millis = T0 + t;
    if (reanchor && t >= 4000 && (t % 4000) == off % 4000 ) {       // every 4 s, `off` ms after a multiple of 4 s
      double beatsNow = t / per; long nb = (long)ceil(beatsNow - 1e-9);
      long ph = lround(nb * per - t); if (ph < 0) ph = 0;
      snprintf(line,sizeof line,"**C:ph=%ld,beat=%ld",ph,nb); send(line);
    }
    frame();
    if (gContract[0].pulseActive && gContract[0].pulseStartMs != lastArm) {
      lastArm = gContract[0].pulseStartMs; total++;
      double ph = fmod((double)t, per); double d = ph < per/2 ? ph : per - ph;   // ms from the nearest beat
      long k = lround(t / per);
      if (d > 15.0) offBeat++; else if (k >= 0 && k < 400) hit[k] = 1;
    }
  }
  long beats = (long)floor(59999 / per) + 1;
  for (long k = 0; k < beats; k++) { bool want = (am==2) || (k % 4 == 0); if (want && !hit[k]) missed++; }
}
int main(){
  int ob, mi, to;
  printf("T8: scored ae= accent vs the host's 4 s bare re-anchor (music exactly on the seeded tempo, 60 s)\n");
  reanchorRun(120,2,0,ob,mi,to,false); printf("   120 BPM am=2, no re-anchor          : accents=%d offbeat=%d missed=%d\n",to,ob,mi);
  for (uint32_t off : {10u,100u,200u,300u,360u,400u,450u,490u}) {
    reanchorRun(120,2,off,ob,mi,to,true); printf("   120 BPM am=2, re-anchor at beat+%3ums : accents=%d offbeat=%d missed=%d\n",off,to,ob,mi);
  }
  for (uint32_t off : {400u,1000u,1900u}) { reanchorRun(120,1,off,ob,mi,to,true); printf("   120 BPM am=1, re-anchor at bar +%4ums : accents=%d offbeat=%d missed=%d\n",off,to,ob,mi); }
  for (uint32_t off : {360u,500u,650u}) { reanchorRun(90,2,off,ob,mi,to,true); printf("    90 BPM am=2, re-anchor at beat+%3ums : accents=%d offbeat=%d missed=%d\n",off,to,ob,mi); }

  printf("T9: beat-pump luminance (i=solid white, m=255, am=2) — full dark->bright swings per second, no accent overlay involved\n");
  for (int bpm : {120,180,200,240,300,600}) {
    resetAll(); _mock_millis = 100000; char line[96];
    send("LFA:i=solid,c=ffffff,b=200,m=255,am=2"); snprintf(line,sizeof line,"**C:bpm=%d,ph=0,bpb=4,beat=0",bpm); send(line);
    int rises = 0; bool low = true; int mn=255, mx=0;
    for (uint32_t t = 0; t < 10000; t += 10) { _mock_millis = 100000 + t; frame(); int v = FLD->_px[0][0].r; if (v<mn) mn=v; if (v>mx) mx=v;
      if (low && v >= 160) { rises++; low = false; } else if (!low && v <= 40) low = true; }
    printf("   bpm=%3d : %4.1f flashes/s (level swings %d..%d of 255; flash = <=40 -> >=160)\n", bpm, rises/10.0, mn, mx);
  }
  printf("T9b: live verb A alternation is not rate-limited (host sends solid white / off every 100 ms)\n");
  { resetAll(); _mock_millis=100000; int rises=0; bool low=true;
    for (uint32_t t=0;t<10000;t+=10){ _mock_millis=100000+t; if (t%200==0) send("LFA:i=solid,c=ffffff"); else if (t%200==100) send("LFA:i=off"); frame(); int v=FLD->_px[0][0].r; if (low&&v>=160){rises++;low=false;} else if(!low&&v<=40) low=true; }
    printf("   %4.1f flashes/s\n", rises/10.0); }
  printf("T9c: chase at s=255: per-pixel on/off rate\n");
  { resetAll(); _mock_millis=100000; send("LFA:i=chase,c=ffffff,s=255"); int rises=0; bool low=true;
    for (uint32_t t=0;t<10000;t+=10){ _mock_millis=100000+t; frame(); int v=FLD->_px[0][0].r; if (low&&v>=160){rises++;low=false;} else if(!low&&v<=40) low=true; }
    printf("   pixel(0,0) %4.1f flashes/s, 1/3 of the columns lit at any time\n", rises/10.0); }

  printf("T10: section flip-flop when a re-anchor lands just after a boundary and pulls the index back\n");
  { resetAll(); _mock_millis=100000; send("**M:v=show"); send("LFA:i=wipe,c=ff0000,at=0"); send("LFA:i=wipe,c=0000ff,at=8"); send("**C:bpm=120,ph=0,bpb=4,beat=0");
    int switches=0, last=-2; for (uint32_t t=0;t<6000;t++){ _mock_millis=100000+t; if (t==4010) send("**C:ph=30,beat=8"); frame(); if (gContract[0].curSection!=last){ printf("   t=%4u ms section -> %d (startMs reset)\n", t, gContract[0].curSection); last=gContract[0].curSection; switches++; } } }
  return 0;
}
