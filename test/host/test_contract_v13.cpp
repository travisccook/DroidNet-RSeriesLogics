// Part of the DroidNet Driveable-Animation Contract test harness.
// Copyright (c) 2026 Travis Cook. Shared byte-identically across the DroidNet
// RSeries/PSI/Flthy forks.  SPDX-License-Identifier: LGPL-2.1-only OR MIT
// Host unit tests for the contract v1.3 additions to contract_core.h — no external deps.
// -std=c++11 clean: the AVR forks build the core with gnu++11.
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static ContractParams P(const char* line) { ParsedContract p; contractParse(line, p); return p.params; }

// ---- S5: saturating parameters, exact mode words, the v1.3 keys ----
static void test_saturating_params() {
  CHECK(P("LFA:s=300").speed == 255);
  CHECK(P("LFA:b=300").bright == 255);
  CHECK(P("LFA:b=-1").bright == 0);
  CHECK(P("LFA:m=256").beatMod == 255);
  CHECK(P("L*L:v=99999999999").level == 255);
  CHECK(P("**C:bpb=999").bpb == 255);             // range-checked at the seed (S1), not wrapped here
  CHECK(P("**C:bpm=65656").bpm == 65535);         // was 120 after a 16-bit wrap
  CHECK(P("**C:bpm=128.5").bpm == 128);
  CHECK(P("**C:bpm=0").hasBpm && P("**C:bpm=0").bpm == 0);   // C6: hold is a real value, not a parse failure
  for (int am = 0; am <= 4; am++) {               // the five known modes pass through
    char l[16]; snprintf(l, sizeof l, "LFA:am=%d", am);
    CHECK(P(l).hasAm && P(l).accentMode == am);
  }
  CHECK(P("LFA:am=5").accentMode == 0);           // an unknown mode is no accent...
  CHECK(P("LFA:am=9").accentMode == 0);
  CHECK(P("LFA:am=258").accentMode == 0);         // ...and never wraps into a known one (was 2)
  CHECK(P("LFA:am=-1").accentMode == 0);
  CHECK(P("**M:v=show").mode == 's');
  CHECK(P("**M:v=idle").mode == 'i');
  CHECK(P("**M:v=stop").mode == 0);               // used to mean show
  CHECK(P("**M:v=i").mode == 0);
  CHECK(P("**M:v=showtime").mode == 0);
  CHECK(P("**M:v=show,wd=15").hasWd && P("**M:v=show,wd=15").wd == 15);
  CHECK(P("**M:v=show,wd=999").wd == 255);
  CHECK(!P("**M:v=show").hasWd);
  CHECK(P("LFA:an=4,ap=2").hasAn && P("LFA:an=4,ap=2").an == 4 && P("LFA:an=4,ap=2").ap == 2);
  CHECK(!P("LFA:am=2").hasAn && P("LFA:am=2").an == 1 && P("LFA:am=2").ap == 0);
  CHECK(P("LFQ:r=0").hasRoute && P("LFQ:r=0").route == 0);
  CHECK(P("LFQ:r=2").route == 2 && P("LFQ:r=250").route == 99);
  CHECK(!P("LFQ").hasRoute);
  CHECK(accentDurClamp(0) == 170 && accentDurClamp(180) == 180);
  CHECK(accentDurClamp(4000000000UL) == 2550 && accentDurClamp(_parseU32("-1")) == 2550);
  ParsedContract p;
  CHECK(contractParse("LFV:n=12,k=a1B2", p) && p.verb == CV_VERIFY);
  CHECK(p.params.hasN && p.params.n == 12 && p.params.hasK && p.params.k == 0xA1B2);
  CHECK(contractParse("LFV:n=12,k=a1b", p) && !p.params.hasK);       // k is exactly four hex digits
  CHECK(contractParse("LFV:n=12,k=a1b2c", p) && !p.params.hasK);
  CHECK(contractParse("LFV:n=12,k=a1g2", p) && !p.params.hasK);
  CHECK(contractParse("LFV:n=300,k=0000", p) && p.params.n == 255);
}

int main() {
  test_saturating_params();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
