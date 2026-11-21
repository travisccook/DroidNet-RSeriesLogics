// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// REVIEW FINDINGS, 2026-11-21: an independent review of the R2 changes (contract v1.4, core
// revision CA2: fe1c8ad..8174454). Every test here is a FINDING: it fails on the code as
// reviewed. It is not part of the authors' suite's claims, and it is meant to be kept (once the
// defect is fixed, it passes) or turned into a KNOWN_BUG entry by whoever owns the fix.
//
// THE MEASURE is the authors' own, from test/host/test_logic_regions.cpp (stage 1k) and v1.4
// section 9: the worst second (the most onsets inside any 1,000 ms) over the whole field and over
// every region of a quarter of it or more (bands of rows, bands of columns, the four quarters), by
// brightness (each LED's brightest channel) and by luminance, in the two readings of stage 1k:
//   gate  the gate's step on the ceiling;
//   fine  the gate's step on the look's own full scale (its brightest LED at the ceiling).
// The rule is stage 1k's: three or fewer, or no more than the display's own stock blink has by
// the same reading. The difference from stage 1k: here the LEDs are the REAL LAYER's
// (ContractLogics.h on mock_engine.h, whose walk stage 2g holds LED for LED to stage 1k's model),
// driven by contract lines, not by a pump formula.
//
// R2-F1 (safety). FX_LOGIC_PUMP_MIN_MS (400 ms) is enforced only inside one pump configuration on
// one timeline: fxLogicPumpFires() keeps every k-th GROUP of the current (am, an, ap, bpb) on the
// current clock. Anything that moves the group (a C re-seed, which starts beat 0 and so fires at
// once; a live A that changes ap=; a section seam) lets the logic level rise again as soon as the
// field gate's 340 ms allow. The same holds for B and b= on a logic look: their rises share the
// core's 340 ms window (contractHoldsDue) and the field gate's, never the 400. Stage 1k's own
// model says why 340 is not enough for this look (the walk takes a level at each LED's next step,
// 0 to ~130 ms late, a different amount each time): with rises 341 to 380 ms apart a region lights
// four times in a second. The layer's _logicFrame() asks the field gate, whose window is 340.
// Streams (a) to (c) and (f) below.
//
// R2-F2 (safety, and v1.4 N4 does not hold). native:0 and logic at one b= under 255 are not one
// light. native:0 at b<255 runs NORMAL with the engine's brightness written to the level
// (_engineBri: the palette baked at the level, then updateDisplay(fBri) scales it by the level
// again); logic bakes the palette at the CEILING and scales it by the level once (_logicApply,
// updateDisplay(level)). The gate is told the same level for both (contractNativeStarted at the
// level; _logicFrame asks and tells the level), and nothing clears or re-reads the field when
// logic takes the idle blink's LEDs over (gLedsWalk), so the rise from native:0 to logic is no
// onset to it. Streams (d) and (g): at the owner's 255, native:0 and a pumped logic by turns
// every 170 ms light the WHOLE field four times in a second by the gate's own count.
//
// R2-F3 (safety: the whole field, by the gate's own count). A change of colour on a logic look is
// a change of the walk's LUMINANCE that the gate never sees: N4 has the gates take logic "at the
// level it asks for, by both measures", so the gate holds luminance == level for the walk, and a
// recolour from a high-luminance hue to a low one and back (green, blue, green) is spaced only by
// the look gate's 170 ms. Every drawn look is measured by luminance (CA1.4) and that rise would
// wait the field gate's window. Stream (e).
//
// A sketch of a fix that clears (a) to (e) here (tried on a scratch copy of the layer, not
// committed; it cleared (a) to (e) and five of the six cases of (f)): in _logicFrame, ask the
// field gate with the window at FX_LOGIC_PUMP_MIN_MS for every onset of logic's level, whatever
// raised it; take a recolour of a logic look that is up as a rise (it waits FX_LOGIC_PUMP_MIN_MS
// from fieldOnsetMs and then sets it); and take the idle blink's LEDs as unknown light
// (contractNativeEnded(known=false)) when the engine's live brightness is not the ceiling as logic
// enters. R2-F2's first check (the two lights) stays a question of what native:0 at b<255 should
// look like.
//
// Run alone: clang++ -std=c++17 -O1 test_review_r2_findings.cpp && ./a.out   (about 5 s)
#include "logics_harness.h"
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

static const int FRAME_MS = 10;
// ---- the count: stage 1k's count(), on series in sixteenths of a level ----
struct Worst { int most = 0, gap = 1000000; };
static void countOnsets(const std::vector<int>& v, int step, Worst& w) {
  if (v.empty()) return;
  bool lit = false; int ref = v[0];
  std::vector<int> at;
  for (size_t i = 0; i < v.size(); i++) {
    const int x = v[i];
    if (lit) { if (x > ref) ref = x; else if (ref - x >= step) { lit = false; ref = x; } }
    else if (x < ref) ref = x;
    else if (x - ref >= step) {
      lit = true; ref = x;
      const int t = (int)i * FRAME_MS;
      if (!at.empty() && t - at.back() < w.gap) w.gap = t - at.back();
      at.push_back(t);
      int k = 0; for (size_t j = at.size(); j-- > 0 && t - at[j] < 1000;) k++;
      if (k > w.most) w.most = k;
    }
  }
}
// FastLED's rainbow conversion and scale8 as the mock has them, for the look's own full scale.
static void fullScale(const LogicEngineRenderer* r, int ceil, int& fullLv, int& fullLy) {
  fullLv = fullLy = 0;
  for (int k = 0; k < MOCK_COLORS; k++) {
    int R, G, B; mockHsvRainbow(r->_all[k][0] + r->_hue, r->_all[k][1], ceil == 255 ? r->_all[k][2] : mockScale8(ceil, r->_all[k][2]), R, G, B);
    const int lv = R > G ? (R > B ? R : B) : (G > B ? G : B), ly = contractLuminance((uint8_t)R, (uint8_t)G, (uint8_t)B);
    if (lv > fullLv) fullLv = lv;
    if (ly > fullLy) fullLy = ly;
  }
}
// Every frame of a display: its LEDs into region series (stage 1k's regions).
struct Meter {
  struct Reg { bool rows; int a, b, q; };
  int W = 0, H = 0;
  std::vector<Reg> regs;
  std::vector<std::vector<int>> ser[2];            // [measure][region + whole] -> frames
  void init(int w, int h) {
    W = w; H = h; regs.clear();
    for (int y0 = 0; y0 < H; y0++) for (int y1 = y0 + 1; y1 <= H; y1++) if ((y1 - y0) * 4 >= H && !(y0 == 0 && y1 == H)) regs.push_back({ true, y0, y1, -1 });
    for (int x0 = 0; x0 < W; x0++) for (int x1 = x0 + 1; x1 <= W; x1++) if ((x1 - x0) * 4 >= W && !(x0 == 0 && x1 == W)) regs.push_back({ false, x0, x1, -1 });
    for (int q = 0; q < 4; q++) regs.push_back({ false, 0, 0, q });
    for (int m = 0; m < 2; m++) ser[m].assign(regs.size() + 1, std::vector<int>());
  }
  void see(const LogicEngineRenderer* r) {
    for (int m = 0; m < 2; m++) {
      std::vector<int> row((size_t)H, 0), col((size_t)W, 0); int quad[4] = { 0, 0, 0, 0 }, all = 0;
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const CRGB c = r->px(x, y);
        const int v = m ? contractLuminance(c.r, c.g, c.b) : (c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b));
        row[(size_t)y] += v; col[(size_t)x] += v; all += v; quad[(x >= W / 2 ? 1 : 0) + (y >= H / 2 ? 2 : 0)] += v;
      }
      for (size_t k = 0; k < regs.size(); k++) {
        const Reg& g = regs[k]; int sum = 0, n = 0;
        if (g.q >= 0) { sum = quad[g.q]; n = ((g.q & 1) ? W - W / 2 : W / 2) * ((g.q & 2) ? H - H / 2 : H / 2); }
        else if (g.rows) { for (int y = g.a; y < g.b; y++) sum += row[(size_t)y]; n = (g.b - g.a) * W; }
        else { for (int x = g.a; x < g.b; x++) sum += col[(size_t)x]; n = (g.b - g.a) * H; }
        ser[m][k].push_back(sum * 16 / n);
      }
      ser[m][regs.size()].push_back(all * 16 / (W * H));
    }
  }
  struct Result { Worst gateWhole, gateRegion, fineWhole, fineRegion; };
  Result result(int ceil, int fullLv, int fullLy) const {
    Result res; const size_t R = regs.size();
    const int gateStep = contractFieldStep((uint8_t)ceil) * 16;
    for (int m = 0; m < 2; m++) {
      const int fineStep = contractFieldStep((uint8_t)(m ? fullLy : fullLv)) * 16;
      for (size_t k = 0; k <= R; k++) {
        countOnsets(ser[m][k], gateStep, k == R ? res.gateWhole : res.gateRegion);
        countOnsets(ser[m][k], fineStep, k == R ? res.fineWhole : res.fineRegion);
      }
    }
    return res;
  }
};

static int gCases = 0, gOver = 0;
// One case: `script` is called before every frame with the time since the first line (ms) and
// sends what it likes. The display's own stock blink (no line at all) is measured the same way.
typedef void (*Script)(uint32_t t, int u, void* ctx);
static int gOwnerBri = 0;                         // 0: the display's own brightness; else the owner's, set before the run
static Meter::Result measure(int u, int w, int h, int seconds, Script script, void* ctx, int& ceilOut, int& fullLv, int& fullLy) {
  mockEngineSize(0, u ? 8 : w, u ? 10 : h); mockEngineSize(1, u ? w : 16, u ? h : 4);
  gMockWalkNormal = true;
  resetAll();
  LogicEngineRenderer* r = u ? RLD : FLD;
  if (gOwnerBri) {                                 // the owner's slider: the default object and the live value
    (u ? LogicEngineRLDDefault : LogicEngineFLDDefault).fBri = (byte)gOwnerBri; r->setBrightness((byte)gOwnerBri); r->calculateAllColors();
  }
  const int ceil = gOwnerBri ? gOwnerBri : (u ? 140 : 160); ceilOut = ceil;
  run(1000);                                       // the idle blink a while
  Meter mt; mt.init(r->width(), r->height());
  // The look's own full scale (the fine reading's): its brightest LED at the ceiling, by each
  // measure, the LARGEST seen while logic is up (a larger scale counts fewer onsets).
  fullLv = fullLy = 0;
  for (uint32_t t = 0; t < (uint32_t)seconds * 1000u; t += FRAME_MS) {
    if (script) script(t, u, ctx);
    _mock_millis += FRAME_MS; frame();
    if (script && t > 2000 && r->_fx == 106 && gLogicUp[u]) {
      int a, b; fullScale(r, ceil, a, b); if (a > fullLv) fullLv = a; if (b > fullLy) fullLy = b;
    }
    if (t >= 2000) mt.see(r);                       // (the first 2 s: the look comes up)
  }
  if (!script) fullScale(r, ceil, fullLv, fullLy);
  gMockWalkNormal = false;
  return mt.result(ceil, fullLv, fullLy);
}
// The rule of stage 1k (v1.4 section 9): three or fewer, or no more than the stock blink.
static bool judge(const char* what, const Meter::Result& r, const Meter::Result& s) {
  const bool ok = (r.fineWhole.most <= 3 || r.fineWhole.most <= s.fineWhole.most) &&
                  (r.fineRegion.most <= 3 || r.fineRegion.most <= s.fineRegion.most) &&
                  (r.gateWhole.most <= 3 || r.gateWhole.most <= s.gateWhole.most) &&
                  (r.gateRegion.most <= 3 || r.gateRegion.most <= s.gateRegion.most);
  gCases++;
  if (!ok) {
    gOver++;
    printf("    OVER  %-58s fine %d/%d (closest %d/%d ms)  gate %d/%d (closest %d/%d ms)  stock fine %d/%d gate %d/%d\n", what,
           r.fineWhole.most, r.fineRegion.most, r.fineWhole.gap, r.fineRegion.gap, r.gateWhole.most, r.gateRegion.most,
           r.gateWhole.gap, r.gateRegion.gap, s.fineWhole.most, s.fineRegion.most, s.gateWhole.most, s.gateRegion.most);
  }
  return ok;
}

// ---- the streams ----
struct Ctx { int bpm = 0, period = 0, high = 60; uint8_t s = 128; };
static void lineFor(int u, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void lineFor(int u, const char* fmt, ...) {
  char body[96], l[100]; va_list ap; va_start(ap, fmt); vsnprintf(body, sizeof body, fmt, ap); va_end(ap);
  snprintf(l, sizeof l, "L%c%s", u ? 'R' : 'F', body); send(l);
}
// (a) a logic look pumped on every other beat (am=2, an=2), and a live A each beat that moves the
// phase (ap=) onto the next beat: S2's floor fires every beat of a 2-beat group up to 352 BPM, and
// logic's own floor never sees a group fire twice. The pump now rises on every beat.
static void scriptPhase(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=logic,s=%u,am=2,m=255,an=2,ap=0", c.s); }
  if (t == 10) { char l[64]; snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", c.bpm); send(l); }
  // half way into beat k (k = 0, 1, ...): ap = (k + 1) % 2, so beat k + 1 fires
  const uint32_t beatMs1000 = 60000000u / (uint32_t)c.bpm;              // a beat in microseconds
  if (t >= 10) {
    const uint32_t us = (t - 10) * 1000u, k = us / beatMs1000, into = us % beatMs1000;
    if (into >= beatMs1000 / 2 && into < beatMs1000 / 2 + FRAME_MS * 1000u) lineFor(u, "A:i=logic,s=%u,am=2,m=255,an=2,ap=%u", c.s, (unsigned)((k + 1) % 2));
  }
}
// (b) the same clock re-seeded every `period` ms (C:bpm=,beat=0: a seek, a fresh beat 0, which a
// pumped look fires on at once). logic's pump floor does not see it; the field gate allows a rise
// 340 ms after the last.
static void scriptReseed(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=logic,s=%u,am=2,m=255", c.s); }
  if (t % (uint32_t)c.period == 10) { char l[64]; snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", c.bpm); send(l); }
}
// (c) B on a logic look with no pump: up every `period` ms, down `high` ms later. B's rises share the
// core's 340 ms window with L and b=, never logic's 400.
static void scriptBright(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=logic,s=%u", c.s); }
  if (t >= 1000 && (t - 1000) % (uint32_t)c.period == 0) lineFor(u, "B:b=255");
  if (t >= 1000 && (t - 1000) % (uint32_t)c.period == (uint32_t)c.high) lineFor(u, "B:b=0");
}

// (d) R2-F2: native:0 and logic by turns at one b= under 255, every 170 ms (S4's spacing). v1.4 N4
// says the two are one light ("a change of look (170 ms) and not an onset"); at b=255 they are.
// Under 255 they are not: native:0 draws the palette baked at the level and then scaled by it
// (_engineBri: setBrightness(level), calculateAllColors(); NORMAL: updateDisplay(fBri)), logic the
// palette baked at the CEILING scaled by the level (_logicApply, updateDisplay(level)). The gate
// is told the same level for both, so it never sees the rise from native:0 to logic.
static void scriptNative0(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=native:0,b=%d", c.period); }
  if (t >= 500 && t % 170 == 0) lineFor(u, (t / 170) % 2 ? "A:i=logic,b=%d" : "A:i=native:0,b=%d", c.period);
}
// (g) R2-F2, the hand-over both ways: native:0 and a logic look that is pumped (or in a colour)
// by turns every 170 ms, at the owner's brightness 255. The gate is told each hand-over's level
// at the frame it asks (contractNativeStarted; _logicFrame's ask and tell); the light comes in
// as each LED steps, on the same LEDs, from the other walk's state.
static void scriptHandover(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); }
  if (t % 170 == 0) lineFor(u, (t / 170) % 2 ? "A:i=native:0" : (c.high ? "A:i=logic,c=00ff00" : "A:i=logic,am=2,m=255"));
}
// (e) R2-F3: a logic look recoloured every 170 ms (S4's spacing), between two hues of very
// different luminance. The look gate spaces the colour changes; nothing measures the luminance of
// the walk (N4: the gates take the look at its level, by both measures), so a luminance onset
// every 340 ms is not seen by the field gate, which for every drawn look holds it to the window.
static void scriptHue(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  const unsigned a = (unsigned)c.period, b = (unsigned)c.high;     // (the two colours, reusing the fields)
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=logic,c=%06x", a); }
  if (t >= 500 && t % 170 == 0) lineFor(u, "A:i=logic,c=%06x", (t / 170) % 2 ? b : a);
}
// (f) the clock held and resumed by turns (C:bpm=0, then C:bpm=): a pumped logic look goes to its
// full level when the clock stops (not pumped) and fires at once on the resumed beat 0.
static void scriptHold(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=logic,am=2,m=255"); }
  if (t % (uint32_t)c.period == 10) {
    char l[64];
    if ((t / (uint32_t)c.period) % 2) snprintf(l, sizeof l, "**C:bpm=0");
    else snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", c.bpm);
    send(l);
  }
}

// CONTROL: a logic look pumped by its own rule alone (am=2, no other line), the case stage 1k
// passes. The measure here has to agree.
static void scriptPlain(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=logic,s=%u,am=2,m=255", c.s); }
  if (t == 10) { char l[64]; snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", c.bpm); send(l); }
}

int main() {
  static const struct { int u, w, h; } kD[] = { { 0, 8, 10 }, { 1, 16, 4 } };
  const int SECONDS = 30;
  int ceil = 0, fl = 0, fy = 0;
  // CONTROLS, first: what stage 1k passes passes here too, and the streams below at 400 ms and
  // more apart pass (the floor logic's own pump has). These must pass; they are not findings.
  {
    int over = 0, n = 0;
    for (int bri : { 0, 255 }) for (const auto& d : kD) {
      gOwnerBri = bri;
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (int bpm : { 120, 150, 160, 176, 177, 300 }) {
        Ctx c; c.bpm = bpm; char what[96]; snprintf(what, sizeof what, "CONTROL %s bri %d: am=2 alone, %d BPM", d.u ? "rear" : "front", ceil, bpm);
        const int o = gOver; judge(what, measure(d.u, d.w, d.h, SECONDS, scriptPlain, &c, ceil, fl, fy), stock); over += gOver - o; n++;
      }
      for (int period : { 400, 420 }) {
        Ctx c; c.bpm = 120; c.period = period; char what[96];
        snprintf(what, sizeof what, "CONTROL %s bri %d: re-seeded every %d ms", d.u ? "rear" : "front", ceil, period);
        int o = gOver; judge(what, measure(d.u, d.w, d.h, SECONDS, scriptReseed, &c, ceil, fl, fy), stock); over += gOver - o; n++;
        c.high = 170; snprintf(what, sizeof what, "CONTROL %s bri %d: B:b=255 every %d ms, b=0 170 ms on", d.u ? "rear" : "front", ceil, period);
        o = gOver; judge(what, measure(d.u, d.w, d.h, SECONDS, scriptBright, &c, ceil, fl, fy), stock); over += gOver - o; n++;
      }
    }
    gOwnerBri = 0;
    printf("CONTROLS: %d of %d over (stage 1k's own case, and the streams at 400 ms or more apart)\n", over, n);
    CHECK(over == 0);
    gOver -= over; gCases -= n;
  }
  printf("R2-F1: logic's level rising faster than FX_LOGIC_PUMP_MIN_MS, by paths its floor does not see\n");
  printf("       (the real layer on the mock engine; stage 1k's measure and rule; OVER = breaks v1.4 section 9)\n");
  for (int bri : { 0, 255 }) for (const auto& d : kD) {
    gOwnerBri = bri;
    const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
    printf("  %s %dx%d at brightness %d: stock blink fine %d/%d gate %d/%d\n", d.u ? "rear" : "front", d.w, d.h, ceil,
           stock.fineWhole.most, stock.fineRegion.most, stock.gateWhole.most, stock.gateRegion.most);
    for (uint8_t s : { (uint8_t)128, (uint8_t)0, (uint8_t)255 }) {
      int overA = 0, overB = 0, overC = 0, nA = 0, nB = 0, nC = 0;
      for (int bpm = 146; bpm <= 176; bpm += 2) {
        Ctx c; c.bpm = bpm; c.s = s; char what[96];
        snprintf(what, sizeof what, "%s bri %d s=%u: am=2,an=2, ap moved each beat, %d BPM", d.u ? "rear" : "front", ceil, s, bpm);
        const int o = gOver; judge(what, measure(d.u, d.w, d.h, SECONDS, scriptPhase, &c, ceil, fl, fy), stock); nA++; overA += gOver - o;
      }
      for (int period : { 345, 350, 355, 360, 370, 380 }) {
        Ctx c; c.bpm = 120; c.period = period; c.s = s; char what[96];
        snprintf(what, sizeof what, "%s bri %d s=%u: am=2 at 120 BPM, re-seeded every %d ms", d.u ? "rear" : "front", ceil, s, period);
        int o = gOver; judge(what, measure(d.u, d.w, d.h, SECONDS, scriptReseed, &c, ceil, fl, fy), stock); nB++; overB += gOver - o;
        for (int high : { 60, 170 }) {
          c.high = high;
          snprintf(what, sizeof what, "%s bri %d s=%u: no pump, B:b=255 every %d ms, b=0 %d ms on", d.u ? "rear" : "front", ceil, s, period, high);
          o = gOver; judge(what, measure(d.u, d.w, d.h, SECONDS, scriptBright, &c, ceil, fl, fy), stock); nC++; overC += gOver - o;
        }
      }
      printf("  %s bri %d s=%3u: ap moved each beat %d of %d over; C re-seed %d of %d over; B %d of %d over\n", d.u ? "rear " : "front", ceil, s,
             overA, nA, overB, nB, overC, nC);
    }
  }
  gOwnerBri = 0;
  printf("R2-F1: %d of %d cases break v1.4 section 9 by stage 1k's rule\n", gOver, gCases);
  CHECK(gOver == 0);
  // R2-F1, (f): the stopped clock
  {
    const int o = gOver, n = gCases;
    for (const auto& d : kD) {
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (int period : { 175, 180, 190 }) {
        Ctx c; c.bpm = 120; c.period = period; char what[96];
        snprintf(what, sizeof what, "%s: am=2, the clock held and resumed every %d ms", d.u ? "rear" : "front", period);
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptHold, &c, ceil, fl, fy), stock);
      }
    }
    printf("R2-F1 (f), the clock held and resumed by turns: %d of %d cases break it\n", gOver - o, gCases - n);
    CHECK(gOver == o);
  }
  // R2-F2: native:0 and logic at one b= are not one light under 255
  {
    // The light of each, steady, at b=128 and b=200 under the front's own 160: the sum over the
    // LEDs of each LED's brightest channel, averaged over 3 s of frames. N4: the same light.
    for (int b : { 128, 200 }) {
      long light[2] = { 0, 0 };
      for (int k = 0; k < 2; k++) {
        gMockWalkNormal = true; mockEngineSize(0, 8, 10); mockEngineSize(1, 16, 4); resetAll();
        char l[48]; send("**M:v=show"); snprintf(l, sizeof l, k ? "LFA:i=logic,b=%d" : "LFA:i=native:0,b=%d", b); send(l); run(3000);
        long sum = 0;
        for (int i = 0; i < 300; i++) {
          run(10);
          for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) { const CRGB c = FLD->px(x, y); sum += c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b); }
        }
        light[k] = sum / 300; gMockWalkNormal = false;
      }
      printf("R2-F2: at b=%d (level %d under 160) the front's LEDs sum to %ld as native:0 and %ld as logic (v1.4 N4: one light)\n",
             b, contractLevel(160, (uint8_t)b), light[0], light[1]);
      CHECK(light[1] * 10 <= light[0] * 11);                   // within a tenth
    }
    const int o = gOver, n = gCases;
    for (int bri : { 0, 255 }) for (const auto& d : kD) {
      gOwnerBri = bri;
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (int b : { 64, 128, 200 }) {
        Ctx c; c.period = b; char what[96];
        snprintf(what, sizeof what, "%s bri %d: native:0 and logic by turns every 170 ms, b=%d", d.u ? "rear" : "front", ceil, b);
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptNative0, &c, ceil, fl, fy), stock);
      }
    }
    // (g): the hand-over both ways, at the owner's 255
    gOwnerBri = 255;
    for (const auto& d : kD) {
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (int col = 0; col < 2; col++) {
        Ctx c; c.high = col; char what[96];
        snprintf(what, sizeof what, "%s bri 255: native:0 and logic,%s by turns every 170 ms", d.u ? "rear" : "front", col ? "c=00ff00" : "am=2,m=255");
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptHandover, &c, ceil, fl, fy), stock);
      }
    }
    gOwnerBri = 0;
    printf("R2-F2, native:0 and logic by turns: %d of %d cases break v1.4 section 9\n", gOver - o, gCases - n);
    CHECK(gOver == o);
  }
  // R2-F3: a logic look recoloured by turns
  {
    const int o = gOver, n = gCases;
    static const struct { uint32_t a, b; } kPair[] = { { 0x0000ffu, 0x00ff00u }, { 0xff0000u, 0x00ff00u }, { 0x0000ffu, 0xffff00u } };
    for (int bri : { 0, 255 }) for (const auto& d : kD) {
      gOwnerBri = bri;
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (const auto& pr : kPair) {
        Ctx c; c.period = (int)pr.a; c.high = (int)pr.b; char what[96];
        snprintf(what, sizeof what, "%s bri %d: logic, c=%06x and c=%06x by turns every 170 ms", d.u ? "rear" : "front", ceil, (unsigned)pr.a, (unsigned)pr.b);
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptHue, &c, ceil, fl, fy), stock);
      }
    }
    gOwnerBri = 0;
    printf("R2-F3, logic recoloured by turns: %d of %d cases break v1.4 section 9\n", gOver - o, gCases - n);
    CHECK(gOver == o);
  }
  if (gFails) { printf("REVIEW FINDINGS: %d CHECK(s) FAILED (expected while the defects are in the code)\n", gFails); return 1; }
  printf("review findings: all pass\n");
  return 0;
}
