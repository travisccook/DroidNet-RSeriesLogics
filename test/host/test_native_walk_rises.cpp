// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// THE IDLE BLINK'S BRIGHTNESS RISES (native:0, NORMAL), on the RSeries layer, on the mock engine with
// its walk drawn LED by LED (gMockWalkNormal). Fork-local. Backported 2026-11-24 from the R2 line,
// where an independent review found it (its R2-F4 and R2-F5).
//
// native:0 is the engine's walk: each LED takes a new brightness at its next step, and an LED steps
// once in P + 1 frames, P its pause (random8(delay) at a key colour, fade at a tween). Under a b= or
// B the engine's brightness is the level, which bakes the palette at it and scales it again, so the
// walk's light goes as about the fourth power of the level. Two consequences the gate's 340 ms rule
// could not see: a rise's light reaches some LEDs up to the walk's longest pause late, so rises 340
// ms apart light a part of the display four times a second; and a rise under the gate's step (a
// tenth of the ceiling) is a rise of the light by about a third. The layer now holds every rise of
// native:0's brightness, of any size, to the walk's window (ContractLogics.h, _walkWindowMs):
//     W = 2 * STROBE_MIN_STATE_MS + ENGINE_FRAME_MS * (max(delay - 1, fade) + 1)
// 440 ms on the front at its own pace (fade 1, delay 10), 740 ms on the rear (3, 40).
//
// THE MEASURE is the R2 line's (its stage 1k and its review's stage R2rev): the worst second (the
// most onsets inside any 1,000 ms) over the whole field and over every region of a quarter of it
// or more, by brightness and by luminance, counted with the gate's own rule (a rise of a step above
// the trough after a fall of a step) in two readings: the step of the ceiling, and the step of the
// walk's own full scale (its brightest LED at the ceiling). The rule: three or fewer, or no more
// than the display's own stock blink. A model of the stock code (mock_engine.h), not a display.
#include "logics_harness.h"
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

static const int FRAME_MS = 10;
// ---- the count, on series in sixteenths of a level (the R2 line's stage 1k count) ----
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
// The walk's own full scale (the fine reading): the palette the engine has (its number and hue)
// BAKED AT THE CEILING, its brightest LED by each measure. (native:0 under a b= bakes its palette at
// the level, so the engine's current table is not the full scale.)
static void fullScale(const LogicEngineRenderer* r, int ceil, int& fullLv, int& fullLy) {
  mockFullScale(r->_bakedPal < 0 ? r->_pal : r->_bakedPal, r->_hue, ceil, fullLv, fullLy);
}
// Every frame of a display: its LEDs into region series: the whole field, every band of rows and of
// columns a quarter of it or more, the four quarters.
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
  // measure, the LARGEST seen while the walk is up (a larger scale counts fewer onsets).
  fullLv = fullLy = 0;
  for (uint32_t t = 0; t < (uint32_t)seconds * 1000u; t += FRAME_MS) {
    if (script) script(t, u, ctx);
    _mock_millis += FRAME_MS; frame();
    if (script && t > 2000 && r->_fx == 0) {        // the walk: native:0
      int a, b; fullScale(r, ceil, a, b); if (a > fullLv) fullLv = a; if (b > fullLy) fullLy = b;
    }
    if (t >= 2000) mt.see(r);                       // (the first 2 s: the look comes up)
  }
  if (!script) fullScale(r, ceil, fullLv, fullLy);
  gMockWalkNormal = false;
  return mt.result(ceil, fullLv, fullLy);
}
// The rule: three or fewer in any second, or no more than the stock blink, by either reading.
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

struct Ctx { int period = 0, high = 170; int low = 0; };
static void lineFor(int u, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void lineFor(int u, const char* fmt, ...) {
  char body[96], l[100]; va_list ap; va_start(ap, fmt); vsnprintf(body, sizeof body, fmt, ap); va_end(ap);
  snprintf(l, sizeof l, "L%c%s", u ? 'R' : 'F', body); send(l);
}
// B on native:0 (an armed show's idle look): up to 255 every `period` ms, down to `low` `high` ms on
static void scriptBright(uint32_t t, int u, void* vc) {
  const Ctx& c = *(Ctx*)vc;
  if (t == 0) { send("**M:v=show"); lineFor(u, "A:i=native:0"); }
  if (t >= 1000 && (t - 1000) % (uint32_t)c.period == 0) lineFor(u, "B:b=255");
  if (t >= 1000 && (t - 1000) % (uint32_t)c.period == (uint32_t)c.high) lineFor(u, "B:b=%d", c.low);
}

int main() {
  // ---- the walk's window ----
  CHECK(_walkWindowMs(1, 10) == 440 && _walkWindowMs(3, 40) == 740 && _walkWindowMs(12, 160) == 1940 && _walkWindowMs(1, 20) == 540);
  CHECK(_walkWindowMs(4, 1) == 390 && _walkWindowMs(0, 0) == 350);   // (a tween pause longer than the key's; no pause at all)
  CHECK(_walkLagMs(1, 10) == 100 && _walkLagMs(3, 40) == 400);

  // ---- B every 50 ms, up and down, on native:0: its brightness rises at most once in the window,
  // to 0 and to a level under the gate's step (b=238 on the front: 149 of 160; b=240 on the rear) ----
  {
    struct { const char* low; int u; } kCase[] = { { "0", 0 }, { "238", 0 }, { "0", 1 }, { "240", 1 } };
    int rises = 0; bool spaced = true;
    for (const auto& c : kCase) {
      resetAll(); char l[48]; send("**M:v=show"); snprintf(l, sizeof l, "L%cA:i=native:0", c.u ? 'R' : 'F'); send(l); run(1500);
      LogicEngineRenderer* r = c.u ? RLD : FLD;
      const uint32_t w = c.u ? _walkWindowMs(3, 40) : _walkWindowMs(1, 10);
      // The time each rise was decided (gWalkUpMs) is held to the window exactly. The engine's
      // brightness is read once a pass, after the frame: a rise decided in a line's handler shows
      // there one frame later than one decided in the tick, so on it the window is read to 10 ms.
      int prev = r->_liveBri; uint32_t last = 0, lastDecided = gWalkUpMs[c.u]; int n = 0, decided = 0;
      for (uint32_t t = 0; t < 8000; t += 10) {
        if (t % 50 == 0) {
          snprintf(l, sizeof l, "L%cB:b=%s", c.u ? 'R' : 'F', (t / 50) % 2 ? "255" : c.low); send(l);
          if (gWalkUpMs[c.u] != lastDecided) { if (decided && gWalkUpMs[c.u] - lastDecided < w) spaced = false; lastDecided = gWalkUpMs[c.u]; decided++; }
        }
        _mock_millis += 10; frame();
        if (gWalkUpMs[c.u] != lastDecided) { if (decided && gWalkUpMs[c.u] - lastDecided < w) spaced = false; lastDecided = gWalkUpMs[c.u]; decided++; }
        if (r->_liveBri > prev) { if (n && _mock_millis - last + ENGINE_FRAME_MS < w) spaced = false; last = _mock_millis; n++; }
        prev = r->_liveBri;
      }
      CHECK(decided == n);
      CHECK(n >= 5 && r->_fx == 0); rises += n;
    }
    CHECK(spaced);
    printf("  native:0 under B every 50 ms: %d rises of its brightness, each the walk's window apart\n", rises);
  }

  static const struct { int u, w, h; } kAll[] = { { 0, 8, 10 }, { 1, 16, 4 }, { 1, 24, 4 }, { 1, 28, 4 }, { 1, 32, 8 } };
  const int SECONDS = 30;
  int ceil = 0, fl = 0, fy = 0;
  // ---- the light: B up every 345 to 380 ms and down 170 ms later (the R2 review's R2-F4) ----
  {
    const int o = gOver, n = gCases;
    for (int bri : { 0, 255 }) for (const auto& d : kAll) {
      gOwnerBri = bri;
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (int period : { 345, 350, 360, 370, 380 }) {
        Ctx c; c.period = period; char what[96];
        snprintf(what, sizeof what, "%s %dx%d bri %d: B:b=255 every %d ms, b=0 170 ms on", d.u ? "rear" : "front", d.w, d.h, ceil, period);
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptBright, &c, ceil, fl, fy), stock);
      }
    }
    gOwnerBri = 0;
    printf("  native:0 under B every 345 to 380 ms: %d of %d cases over the rule\n", gOver - o, gCases - n);
    CHECK(gOver == o);
  }
  // ---- the light: B between 255 and a b= under the gate's step below it (the R2 review's R2-F5) ----
  {
    const int o = gOver, n = gCases;
    for (int bri : { 0, 255 }) for (const auto& d : { kAll[0], kAll[1] }) {
      gOwnerBri = bri;
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      const int step = contractFieldStep((uint8_t)ceil);
      int bsub = 255; while (bsub > 0 && ceil - contractLevel((uint8_t)ceil, (uint8_t)(bsub - 1)) < step) bsub--;
      for (int lo : { bsub, bsub + 8 }) for (int period : { 345, 360, 380 }) {
        Ctx c; c.period = period; c.low = lo; char what[112];
        snprintf(what, sizeof what, "%s bri %d: B:b=255 every %d ms, b=%d (level %d, under the step) 170 ms on", d.u ? "rear" : "front", ceil, period, lo,
                 contractLevel((uint8_t)ceil, (uint8_t)lo));
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptBright, &c, ceil, fl, fy), stock);
      }
    }
    gOwnerBri = 0;
    printf("  native:0 under B by less than the gate's step: %d of %d cases over the rule\n", gOver - o, gCases - n);
    CHECK(gOver == o);
  }
  // ---- controls: B at the window and slower, on the front and the rear ----
  {
    const int o = gOver, n = gCases;
    for (int bri : { 0, 255 }) for (const auto& d : { kAll[0], kAll[1] }) {
      gOwnerBri = bri;
      const Meter::Result stock = measure(d.u, d.w, d.h, SECONDS, nullptr, nullptr, ceil, fl, fy);
      for (int period : { 760, 1000 }) {
        Ctx c; c.period = period; char what[96];
        snprintf(what, sizeof what, "CONTROL %s bri %d: B:b=255 every %d ms, b=0 170 ms on", d.u ? "rear" : "front", ceil, period);
        judge(what, measure(d.u, d.w, d.h, SECONDS, scriptBright, &c, ceil, fl, fy), stock);
      }
    }
    gOwnerBri = 0;
    CHECK(gOver == o && gCases - n == 8);
  }
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("the idle blink's brightness rises keep the walk's window OK\n");
  return 0;
}
