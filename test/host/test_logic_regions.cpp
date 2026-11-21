// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// v1.4 SECTION 9 FOR logic: A MODEL OF THE WALK, NOT A MEASUREMENT. Nothing here has been seen
// on a display, and there is no engine and no layer in it. Fork-local (the logics are the only
// board that draws logic). It runs before the core is copied to the other forks, because it
// decides two of the core's constants: FX_LOGIC_FADE_MIN and FX_LOGIC_DELAY_MIN.
//
// What is modelled, from lib/Reeltwo/src/dome/LogicEngine.h, as test/host/test_native_s10.cpp
// models it:
//   * the palette as calculateAllColors() bakes it at the display's own brightness (LE:1427-1486):
//     the display's own (front 0, rear 1), or, for a colour with a hue, the front palette's shape
//     at that hue (setPaletteHue(0, fxLogicHue(c) - FX_LOGIC_HUE_BASE), v1.4 N1);
//   * every LED walking it (updateMappedLED(), LE:1493-1519): one step when its pause runs out,
//     a pause of `fade` frames at a tween and of random8(delay) at every fifth colour, the pauses
//     drawn from FastLED's random8() in LED order, the walk's start as setup() draws it (LE:775);
//   * FastLED's hsv2rgb_rainbow (the value is squared);
//   * the engine's frame: one every 10 ms;
//   * the pace from fxLogicPace() on the display's own fade and delay (v1.4 N2), and the level
//     from the core's own clock: b=255, the pump am=2 with m=255, envBright() under the ceiling
//     (the display's own brightness), reaching each LED at its next step as updateDisplay(level)
//     gives it (v1.4 N3; the stock blink is updateDisplay() = updateDisplay(fBri), so at the
//     ceiling the look is the light of native:0 exactly).
// Display sizes: every one the engine knows, front 8 x 10 with the front's settings, rear 16 x 4,
// 24 x 4, 28 x 4 and 32 x 8 with the rear's (LE:90-105: front fade 1, delay 10, brightness 160;
// rear fade 3, delay 40, brightness 140).
//
// The measure, v1.4 section 9: the worst second (the most onsets inside any 1,000 ms) over the
// whole field and over every region of a quarter of it or more (every band of rows, every band
// of columns, the four quarters), by brightness (each LED's brightest channel) and by luminance
// (contractLuminance), steady and under the pump at 30, 120, 176, 177 and 300 BPM. An onset is
// counted the way the gate counts one (contractFieldStep: a rise of a step or more above the last
// trough, a fall of a step below the last peak ends it), in two readings of "full scale":
//   gate     the CEILING, the display's own brightness: what the S9 gate itself counts;
//   fine     the LOOK'S OWN full scale, its brightest LED by each measure at the ceiling. The
//            stricter, and the one the rule is held to: the walk is drawn far under the ceiling
//            (on the front at brightness 160 no channel passes 17: the palette's value is scaled
//            by the brightness when it is baked and again by updateDisplay(fBri), and the
//            rainbow conversion squares it), so a tenth of the ceiling is more than the look's
//            whole light. (A tenth of each series' own peak, stage 1i's reading, was tried and
//            dropped: on a region whose peak is under 2 levels it counts quarter-level noise.)
// A pace is on offer for a display when, by the fine reading, its worst second has three onsets
// or fewer, or no more than that display's own stock blink (native:0: its own palette and pace,
// steady) has in the same measure. The test fails when a pace on offer breaks that rule, which is
// the evidence on which the floors of v1.4 N2 move (DROIDNET_FORK.md has the table this prints).
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int g_checks = 0, g_fail = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- FastLED, as test_native_s10.cpp models it (scale8 without FASTLED_SCALE8_FIXED; build with
// -DLOGIC_SCALE8_FIXED for the vendored FastLED 3.5's own setting, which moves a value by 1) ----
#ifdef LOGIC_SCALE8_FIXED
static int scale8(int i, int s) { return (i * (1 + s)) >> 8; }
#else
static int scale8(int i, int s) { return (i * s) >> 8; }
#endif
static int scale8v(int i, int s) { return ((i * s) >> 8) + ((i && s) ? 1 : 0); }
static int map8(int i, int lo, int hi) { return lo + scale8(i, hi - lo); }
static void hsv(int h, int s, int v, int& r, int& g, int& b) {
  h &= 255;
  const int off8 = (h & 31) << 3, third = scale8(off8, 85), two = scale8(off8, 170);
  switch (h >> 5) {
    case 0: r = 255 - third; g = third; b = 0; break;
    case 1: r = 171; g = 85 + third; b = 0; break;
    case 2: r = 171 - two; g = 170 + third; b = 0; break;
    case 3: r = 0; g = 255 - third; b = third; break;
    case 4: r = 0; g = 171 - two; b = 85 + two; break;
    case 5: r = third; g = 0; b = 255 - third; break;
    case 6: r = 85 + third; g = 0; b = 171 - third; break;
    default: r = 170 + third; g = 0; b = 85 - third; break;
  }
  if (s != 255) {
    if (s == 0) r = g = b = 255;
    else { int d = 255 - s; d = scale8v(d, d); const int sc = 255 - d; r = scale8(r, sc) + d; g = scale8(g, sc) + d; b = scale8(b, sc) + d; }
  }
  if (v != 255) {
    v = scale8v(v, v);
    if (v == 0) r = g = b = 0; else { r = scale8(r, v); g = scale8(g, v); b = scale8(b, v); }
  }
}
struct Rand8 {                                   // FastLED's random8(), on a seed of its own
  uint16_t seed;
  uint8_t next() { seed = (uint16_t)(seed * 2053u + 13849u); return (uint8_t)((seed & 0xFFu) + (seed >> 8)); }
  uint8_t next(uint8_t lim) { return (uint8_t)((next() * (unsigned)lim) >> 8); }
};

// ---- the engine's palettes and settings (LE:90-105, LE:1432-1438) ----
static const int kKeys[2][4][3] = {
  { {170,255,0}, {170,255,85}, {170,255,170}, {170,0,170} },   // front
  { {90,235,0}, {75,255,250}, {30,255,184}, {0,255,250} } };   // rear
struct Disp { const char* name; int pal, bri, fade, delay; };
static const Disp kFront = { "front", 0, 160, 1, 10 }, kRear = { "rear", 1, 140, 3, 40 };
static const int TWEENS = 14, COLORS = 4 + TWEENS * 3, WALK = 2 * COLORS - 2;   // 46 colours, 90 steps there and back

struct Walk {
  int n = 0, hue = 0, fade = 1, delay = 10;
  int pal[COLORS][3];
  std::vector<uint8_t> num, pause, lv, ly;
  Rand8 rnd{ 1 };
  void bake(int palNum, int bri) {                 // calculateAllColors(palNum, bri)
    for (int k = 0; k < 4; k++) {
      const int t = k + TWEENS * k;
      pal[t][0] = kKeys[palNum][k][0]; pal[t][1] = kKeys[palNum][k][1]; pal[t][2] = map8(kKeys[palNum][k][2], 1, bri);
      if (t + 1 != COLORS)
        for (int j = 1; j <= TWEENS; j++)
          for (int el = 0; el < 3; el++) {
            const int per = (kKeys[palNum][k + 1][el] - kKeys[palNum][k][el]) / (TWEENS + 1);
            const int val = per ? ((kKeys[palNum][k][el] + j * per) & 255) : kKeys[palNum][k][el];
            pal[t + j][el] = (el == 2) ? map8(val, 1, bri) : val;
          }
    }
  }
  void led(int i, int bri) {                       // updateMappedLED(i, hue, bri)
    if (pause[i]) { pause[i]--; return; }
    if (++num[i] >= WALK) num[i] = 0;
    const int real = num[i] >= COLORS ? (COLORS - 2) - (num[i] - COLORS) : num[i];
    pause[i] = (uint8_t)((num[i] % 5 == 0) ? rnd.next((uint8_t)delay) : fade);
    const int* c = pal[real];
    int r, g, b; hsv(c[0] + hue, c[1], bri == 255 ? c[2] : scale8(bri, c[2]), r, g, b);
    lv[i] = (uint8_t)(r > g ? (r > b ? r : b) : (g > b ? g : b));
    ly[i] = contractLuminance((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }
  void setup(int count, uint16_t seed) {           // LogicEngineRenderer::setup() (LE:775-786)
    n = count; rnd.seed = seed;
    num.assign((size_t)n, 0); pause.assign((size_t)n, 0); lv.assign((size_t)n, 0); ly.assign((size_t)n, 0);
    for (int i = 0; i < n; i++) { num[i] = rnd.next((uint8_t)WALK); pause[i] = rnd.next(); led(i, 255); }
  }
};

// ---- the count -------------------------------------------------------------------------------
// One series' worst second: the most onsets inside any 1,000 ms, and the closest two. `step` in
// the series' units; an onset is a rise of at least `step` above the trough (the gate's rule).
static const int FRAME_MS = 10;
struct Worst { int most = 0, gap = 1000000; };
static void count(const std::vector<int>& v, int step, Worst& w) {
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
// What one run gives: the worst second for the whole field and for any region, by each reading.
struct Result { Worst gateWhole, gateRegion, fineWhole, fineRegion; int fullLv = 0, fullLy = 0; };

// One run: a display of W x H with settings d, at speed s, in colour c (own palette when it has no
// hue), at bpm (0: steady), for `seconds`. The walk runs 5 s at the look's settings first. The
// pump is logic's own (fxLogicPumpAmount), or with `s2Only` S2's floor alone (beatAccentAmount),
// which is what v1.4 N3 first said and what this model found short.
static Result run(const Disp& d, int W, int H, uint8_t s, uint32_t c, int bpm, int seconds, uint16_t seed, bool s2Only = false) {
  Walk wk;
  wk.bake(d.pal, d.bri); wk.fade = d.fade; wk.delay = d.delay; wk.hue = 0;
  wk.setup(W * H, seed);
  const ContractRGB col = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
  if (!fxLogicOwn(col)) { wk.bake(0, d.bri); wk.hue = (uint8_t)(fxLogicHue(col) - FX_LOGIC_HUE_BASE); }   // N1
  wk.fade = fxLogicPace(s, (uint8_t)d.fade, FX_LOGIC_FADE_MIN);                                          // N2
  wk.delay = fxLogicPace(s, (uint8_t)d.delay, FX_LOGIC_DELAY_MIN);
  const uint8_t ceil = (uint8_t)d.bri;
  for (int f = 0; f < 500; f++) for (int i = 0; i < wk.n; i++) wk.led(i, ceil);
  BeatClock bc;
  const uint32_t t0 = 4294960000u;                  // across the millis() wrap
  if (bpm) {
    ContractParams pr; pr.hasBpm = true; pr.bpm = (uint16_t)bpm; pr.hasPh = true; pr.phMs = 0;
    pr.hasBpb = true; pr.bpb = 4; pr.hasBeat = true; pr.beatAnchor = 0;
    beatClockSeed(bc, pr, t0);
  }
  // the regions: bands of rows and of columns a quarter of the field or more, and the quarters
  struct Reg { bool rows; int a, b, q; };
  std::vector<Reg> regs;
  for (int y0 = 0; y0 < H; y0++) for (int y1 = y0 + 1; y1 <= H; y1++) if ((y1 - y0) * 4 >= H && !(y0 == 0 && y1 == H)) regs.push_back({ true, y0, y1, -1 });
  for (int x0 = 0; x0 < W; x0++) for (int x1 = x0 + 1; x1 <= W; x1++) if ((x1 - x0) * 4 >= W && !(x0 == 0 && x1 == W)) regs.push_back({ false, x0, x1, -1 });
  for (int q = 0; q < 4; q++) regs.push_back({ false, 0, 0, q });
  const int F = seconds * 1000 / FRAME_MS, R = (int)regs.size();
  // series in sixteenths of a level: [measure][region + whole][frame]
  std::vector<std::vector<int>> ser[2];
  for (int m = 0; m < 2; m++) ser[m].assign((size_t)R + 1, std::vector<int>((size_t)F));
  std::vector<int> row((size_t)H), colS((size_t)W);
  for (int f = 0; f < F; f++) {
    uint8_t level = ceil;                           // b=255: the ceiling
    if (bpm) {
      const BeatPos bp = beatPosAt(bc, t0 + (uint32_t)f * FRAME_MS);
      level = envBright(ceil, 255, s2Only ? beatAccentAmount(2, bp, 255, 0, bc, 0) : fxLogicPumpAmount(2, bp, 255, 0, bc, 0));
    }
    for (int i = 0; i < wk.n; i++) wk.led(i, level);
    for (int m = 0; m < 2; m++) {
      const std::vector<uint8_t>& px = m ? wk.ly : wk.lv;
      std::fill(row.begin(), row.end(), 0); std::fill(colS.begin(), colS.end(), 0);
      int quad[4] = { 0, 0, 0, 0 }, all = 0;
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const int v = px[(size_t)(y * W + x)];
        row[(size_t)y] += v; colS[(size_t)x] += v; all += v;
        quad[(x >= W / 2 ? 1 : 0) + (y >= H / 2 ? 2 : 0)] += v;
      }
      for (int k = 0; k < R; k++) {
        const Reg& g = regs[(size_t)k];
        int sum = 0, n = 0;
        if (g.q >= 0) { sum = quad[g.q]; n = ((g.q & 1) ? W - W / 2 : W / 2) * ((g.q & 2) ? H - H / 2 : H / 2); }
        else if (g.rows) { for (int y = g.a; y < g.b; y++) sum += row[(size_t)y]; n = (g.b - g.a) * W; }
        else { for (int x = g.a; x < g.b; x++) sum += colS[(size_t)x]; n = (g.b - g.a) * H; }
        ser[m][(size_t)k][(size_t)f] = sum * 16 / n;
      }
      ser[m][(size_t)R][(size_t)f] = all * 16 / (W * H);
    }
  }
  Result res;
  // the look's own full scale: its brightest LED at the ceiling, by each measure, over the palette
  for (int k = 0; k < COLORS; k++) {
    int r, g, b; hsv(wk.pal[k][0] + wk.hue, wk.pal[k][1], ceil == 255 ? wk.pal[k][2] : scale8(ceil, wk.pal[k][2]), r, g, b);
    const int lv = r > g ? (r > b ? r : b) : (g > b ? g : b), ly = contractLuminance((uint8_t)r, (uint8_t)g, (uint8_t)b);
    if (lv > res.fullLv) res.fullLv = lv;
    if (ly > res.fullLy) res.fullLy = ly;
  }
  const int gateStep = contractFieldStep(ceil) * 16;
  for (int m = 0; m < 2; m++) {
    const int fineStep = contractFieldStep((uint8_t)(m ? res.fullLy : res.fullLv)) * 16;
    for (int k = 0; k <= R; k++) {
      count(ser[m][(size_t)k], gateStep, k == R ? res.gateWhole : res.gateRegion);
      count(ser[m][(size_t)k], fineStep, k == R ? res.fineWhole : res.fineRegion);
    }
  }
  return res;
}

int main(int argc, char** argv) {
  const bool verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
  static const struct { const Disp* d; int w, h; } kSizes[] = {
    { &kFront, 8, 10 }, { &kRear, 16, 4 }, { &kRear, 24, 4 }, { &kRear, 28, 4 }, { &kRear, 32, 8 } };
  static const int kBpm[6] = { 0, 30, 120, 176, 177, 300 };
  static const uint8_t kPace[3] = { 128, 0, 255 };            // the own pace, the slowest, the fastest
  static const struct { const char* name; uint32_t c; } kColour[3] = { { "own", 0x000000u }, { "116530", 0x116530u }, { "ff0000", 0xff0000u } };
  const int SECONDS = 60;
  printf("logic, v1.4 section 9: a MODEL of the walk (no engine, no layer). The worst second (onsets in any 1,000 ms)\n");
  printf("by the fine reading (the gate's step on the look's own full scale), whole field / any region of a quarter or more,\n");
  printf("steady and pumped (am=2, m=255, fxLogicPumpAmount); the stock blink (native:0) beside it. A ! is a pace that\n");
  printf("breaks the rule. FX_LOGIC_FADE_MIN %u, FX_LOGIC_DELAY_MIN %u, FX_LOGIC_PUMP_MIN_MS %u.\n",
         (unsigned)FX_LOGIC_FADE_MIN, (unsigned)FX_LOGIC_DELAY_MIN, (unsigned)FX_LOGIC_PUMP_MIN_MS);
  printf("%-6s %-6s %-4s %-4s %-7s %-8s %-7s %-7s %-7s %-7s %-7s %-7s  %s\n", "disp", "size", "bri", "s", "colour", "fade/del", "steady", "30", "120", "176", "177", "300", "stock");
  for (const auto& sz : kSizes) for (int bri : { 0, 255 }) {
    Disp d = *sz.d; if (bri) d.bri = bri;          // the display's own brightness, and the owner's highest
    // the stock blink of this display: its own palette and pace, steady
    const Result stock = run(d, sz.w, sz.h, 128, 0, 0, SECONDS, 0x51F1u);
    for (uint8_t s : kPace) for (const auto& col : kColour) {
      if (bri && col.c) continue;                   // colours at the display's own brightness
      char cell[6][16]; bool ok = true;
      for (int k = 0; k < 6; k++) {
        const Result r = run(d, sz.w, sz.h, s, col.c, kBpm[k], SECONDS, (uint16_t)(0x51F1u + (unsigned)k * 7919u));
        const int w = r.fineWhole.most, g = r.fineRegion.most;
        // v1.4 section 9: three or fewer, or no more than the display's own stock blink
        const bool wOk = w <= 3 || w <= stock.fineWhole.most;
        const bool gOk = g <= 3 || g <= stock.fineRegion.most;
        if (!wOk || !gOk) ok = false;
        // and by the gate's own count (its ceiling's tenth)
        CHECK(r.gateWhole.most <= 3 || r.gateWhole.most <= stock.gateWhole.most);
        CHECK(r.gateRegion.most <= 3 || r.gateRegion.most <= stock.gateRegion.most);
        snprintf(cell[k], sizeof cell[k], "%d/%d%s", w, g, (wOk && gOk) ? "" : "!");
        if (verbose) printf("     %s %dx%d bri %d s=%d %s %d BPM: gate %d/%d (closest %d/%d ms), fine %d/%d (closest %d/%d ms; full scale %d/%d)\n",
                            d.name, sz.w, sz.h, d.bri, s, col.name, kBpm[k], r.gateWhole.most, r.gateRegion.most, r.gateWhole.gap, r.gateRegion.gap,
                            w, g, r.fineWhole.gap, r.fineRegion.gap, r.fullLv, r.fullLy);
      }
      char size[12]; snprintf(size, sizeof size, "%dx%d", sz.w, sz.h);
      char pace[12]; snprintf(pace, sizeof pace, "%u/%u", (unsigned)fxLogicPace(s, (uint8_t)d.fade, FX_LOGIC_FADE_MIN), (unsigned)fxLogicPace(s, (uint8_t)d.delay, FX_LOGIC_DELAY_MIN));
      printf("%-6s %-6s %-4d %-4d %-7s %-8s %-7s %-7s %-7s %-7s %-7s %-7s  %d/%d%s\n", d.name, size, d.bri, s, col.name, pace,
             cell[0], cell[1], cell[2], cell[3], cell[4], cell[5], stock.fineWhole.most, stock.fineRegion.most, ok ? "" : "   a pace that breaks section 9");
      CHECK(ok);
    }
  }
  // WHY logic's pump has a floor of its own. Every tempo from 146 to 180 BPM (where one beat is
  // 411 to 333 ms), on the front and the smallest rear display, at the three paces, at the
  // display's own brightness and at 255, pumped with S2's floor alone and with
  // FX_LOGIC_PUMP_MIN_MS: how many of those cases have a worst second over three by either
  // reading, and the closest two onsets of any region.
  printf("The pump's floor, 146 to 180 BPM every 2, front 8x10 and rear 16x4, s = 0, 128, 255, own and 255 brightness (30 s each):\n");
  for (int rule = 0; rule < 2; rule++) {
    int cases = 0, over = 0, closest = 1000000, at = 0;
    for (int which = 0; which < 2; which++) for (int bri : { 0, 255 }) for (uint8_t s : kPace) for (int bpm = 146; bpm <= 180; bpm += 2) {
      Disp d = which ? kRear : kFront; if (bri) d.bri = bri;
      const Result r = run(d, which ? 16 : 8, which ? 4 : 10, s, 0, bpm, 30, (uint16_t)(0x2D5Bu + (unsigned)bpm * 7919u), rule == 0);
      cases++;
      if (r.fineWhole.most > 3 || r.fineRegion.most > 3 || r.gateWhole.most > 3 || r.gateRegion.most > 3) over++;
      if (r.fineRegion.gap < closest) { closest = r.fineRegion.gap; at = bpm; }
    }
    printf("  %-34s %3d of %d cases over three in a second; closest two onsets of a region %d ms (%d BPM)\n",
           rule ? "logic's own, FX_LOGIC_PUMP_MIN_MS:" : "S2's floor alone (340 ms):", over, cases, closest, at);
    if (rule) CHECK(over == 0);
  }
  // the constants are what the tables above were run with, so that a change to one is a change here
  CHECK(FX_LOGIC_FADE_MIN == 1 && FX_LOGIC_DELAY_MIN == 10 && FX_LOGIC_PUMP_MIN_MS == 400u);
  if (g_fail) { printf("FAILURES  %d/%d checks passed\n", g_checks - g_fail, g_checks); return 1; }
  printf("OK  %d/%d checks passed\n", g_checks, g_checks);
  return 0;
}
