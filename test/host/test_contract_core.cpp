// Host unit tests for contract_core.h — no external deps, tiny assert harness.
#include "../../src/contract/contract_core.h"
#include <cstdio>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static void test_scope_and_verb() {
  ParsedContract p;
  CHECK(contractParse("LFA:i=solid,c=0080FF,d=0", p));
  CHECK(p.valid && p.cls == 'L' && p.unit == 'F' && p.verb == CV_ANIMATE);
}

static void test_verbs_and_units() {
  ParsedContract p;
  CHECK(contractParse("**P:c=FFFFFF,d=120", p) && p.cls == '*' && p.unit == '*' && p.verb == CV_PULSE);
  CHECK(contractParse("LRA", p) && p.cls == 'L' && p.unit == 'R' && p.verb == CV_ANIMATE);
  CHECK(contractParse("**C:bpm=120,ph=0,bpb=4", p) && p.verb == CV_CLOCK);
  CHECK(contractParse("**X", p) && p.verb == CV_STOP);
  CHECK(contractParse("**M:v=show", p) && p.verb == CV_MODE);
  CHECK(!contractParse("L", p));        // too short
  CHECK(!contractParse("LFZ", p));      // unknown verb
  CHECK(!contractParse("", p));         // empty
  CHECK(!contractParse("QFA", p));      // bad class
  CHECK(!contractParse("LZA", p));      // bad unit
}

static void test_params() {
  ParsedContract p;
  CHECK(contractParse("LFA:i=solid,c=0080FF,d=0", p));
  CHECK(p.params.hasEffect && p.params.effect == CE_SOLID);
  CHECK(p.params.hasColor && p.params.color.r == 0x00 && p.params.color.g == 0x80 && p.params.color.b == 0xFF);
  CHECK(p.params.hasDur && p.params.durMs == 0);

  CHECK(contractParse("**P:c=FFFFFF,d=120,b=200", p));
  CHECK(p.params.color.r == 0xFF && p.params.hasDur && p.params.durMs == 120 && p.params.bright == 200);

  CHECK(contractParse("LFA:i=flash,c=FF00FF,s=200,d=500,m=255,at=44,am=1", p));
  CHECK(p.params.effect == CE_FLASH && p.params.speed == 200 && p.params.durMs == 500);
  CHECK(p.params.beatMod == 255 && p.params.hasAt && p.params.atBeat == 44 && p.params.accentMode == 1);

  CHECK(contractParse("LFA:i=native:105", p) && p.params.effect == CE_NATIVE && p.params.nativeCode == 105);
  CHECK(contractParse("P*L:v=190", p) && p.verb == CV_LEVEL && p.params.hasLevel && p.params.level == 190);
  CHECK(contractParse("**C:bpm=128,ph=40,bpb=4,beat=64", p));
  CHECK(p.params.bpm == 128 && p.params.phMs == 40 && p.params.bpb == 4 && p.params.hasBeat && p.params.beatAnchor == 64);
  CHECK(contractParse("**M:v=show", p) && p.params.mode == 's');
  CHECK(contractParse("**M:v=idle", p) && p.params.mode == 'i');
  // unknown param ignored, still valid:
  CHECK(contractParse("LFA:i=solid,zz=9", p) && p.params.effect == CE_SOLID);
  // color rejects bad hex (leaves hasColor false):
  CHECK(contractParse("LFA:c=XYZ", p) && !p.params.hasColor);
}

int main() {
  test_scope_and_verb();
  test_verbs_and_units();
  test_params();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
