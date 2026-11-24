// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Contract v1.4 (core revision CA2) on the RSeries layer, on the mock engine: the two looks this
// fork draws itself, logic (spec N1 to N5) and vu (N10 to N12). Fork-local. Everything here is
// host-tested on a MOCK of the engine; nothing has run on a display.
//
//   * the mock's walk is the region model's walk (test_logic_regions.cpp, stage 1k): a copy of
//     the model's code, below, is held to the mock LED for LED;
//   * what the look sets on the engine (palette, hue, fade, delay, brightness) and what it never
//     touches (the RAM default object);
//   * the level every frame: updateDisplay(level), the level under the ceiling (N3; CA2 reading
//     3: not a fraction of 255), pumped by fxLogicPumpAmount() (CA2 reading 1: not
//     beatAccentAmount());
//   * the gates: asked with the level for both measures, a held frame drawn at the last level
//     shown, told what was drawn; logic's windows (400 ms, and 500 ms for a rise while up that is
//     not its pump's beat), a recolour, the 400 ms after the walk;
//   * the hand-over with the idle blink: a hold of 400 ms each way in which no LED moves, and no
//     part of a display lit more often in a second than by the idle blink alone, on the look's
//     own scale (the 2026-11-21 review, R2-F2: under b=255 the two are not one light);
//   * restoration: palette, hue, fade, delay and brightness are the owner's again after the look,
//     after X, after M:v=idle, after the watchdog and after a take-over;
//   * the pump reaches the LEDs as the model has it: the layer's LEDs are the model's LEDs, frame
//     by frame, at the tempos and paces the model runs;
//   * vu: not pumped it is meter; pumped, the level rides the beat through the level hold, the
//     brightness stays at b=, L is not shown, and no row lights more than three times a second.
#include "logics_harness.h"
#include <cstring>
#include <string>
#include <vector>

// ---- the region model's walk, COPIED from test_logic_regions.cpp (Walk, hsv, Rand8, the
// palettes), with each LED's colour kept as well as its two measures. It is the model the core's
// floors were decided on; the mock (mock_engine.h) is a second writing of the same engine code.
namespace model {
static int scale8(int i, int s) { return (i * s) >> 8; }
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
struct Rand8 {
  uint16_t seed;
  uint8_t next() { seed = (uint16_t)(seed * 2053u + 13849u); return (uint8_t)((seed & 0xFFu) + (seed >> 8)); }
  uint8_t next(uint8_t lim) { return (uint8_t)((next() * (unsigned)lim) >> 8); }
};
static const int kKeys[2][4][3] = {
  { {170,255,0}, {170,255,85}, {170,255,170}, {170,0,170} },   // front
  { {90,235,0}, {75,255,250}, {30,255,184}, {0,255,250} } };   // rear
static const int TWEENS = 14, COLORS = 4 + TWEENS * 3, WALK = 2 * COLORS - 2;
struct Walk {
  int n = 0, hue = 0, fade = 1, delay = 10;
  int pal[COLORS][3];
  std::vector<uint8_t> num, pause;
  std::vector<CRGB> px;
  Rand8 rnd{ 1 };
  void bake(int palNum, int bri) {
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
  void led(int i, int bri) {
    if (pause[i]) { pause[i]--; return; }
    if (++num[i] >= WALK) num[i] = 0;
    const int real = num[i] >= COLORS ? (COLORS - 2) - (num[i] - COLORS) : num[i];
    pause[i] = (uint8_t)((num[i] % 5 == 0) ? rnd.next((uint8_t)delay) : fade);
    const int* c = pal[real];
    int r, g, b; hsv(c[0] + hue, c[1], bri == 255 ? c[2] : scale8(bri, c[2]), r, g, b);
    px[(size_t)i] = CRGB(r, g, b);
  }
  void setup(int count, uint16_t seed) {
    n = count; rnd.seed = seed;
    num.assign((size_t)n, 0); pause.assign((size_t)n, 0); px.assign((size_t)n, CRGB());
    for (int i = 0; i < n; i++) { num[i] = rnd.next((uint8_t)WALK); pause[i] = rnd.next(); led(i, 255); }
  }
  void frame(int bri) { for (int i = 0; i < n; i++) led(i, bri); }
};
}  // namespace model

// A logic look entered over the idle blink comes up FX_LOGIC_PUMP_MIN_MS after the frame at which
// effect 106 starts (the next engine frame), and its first frame is then drawn: 400 + 2 frames.
static const uint32_t ENTER_MS = FX_LOGIC_PUMP_MIN_MS + 50;

static bool sameLeds(const LogicEngineRenderer* r, const model::Walk& w) {
  for (int i = 0; i < w.n; i++) {
    const CRGB a = r->px(i % r->width(), i / r->width()), b = w.px[(size_t)i];
    if (a.r != b.r || a.g != b.g || a.b != b.b) return false;
  }
  return true;
}
static std::string leds(const LogicEngineRenderer* r) {
  std::string s;
  for (int y = 0; y < r->height(); y++) for (int x = 0; x < r->width(); x++) { const CRGB c = r->px(x, y); s += (char)c.r; s += (char)c.g; s += (char)c.b; }
  return s;
}
static ContractRGB rgb(uint32_t c) { return ContractRGB{ (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }; }
static void sizes(int rw, int rh) { mockEngineSize(0, 8, 10); mockEngineSize(1, rw, rh); }
// The owner sets a display's brightness (the web slider): the default object and the live value.
static void ownerSets(int u, int v) {
  (u == 0 ? LogicEngineFLDDefault : LogicEngineRLDDefault).fBri = (byte)v;
  LogicEngineRenderer* r = u == 0 ? FLD : RLD; r->setBrightness((byte)v); r->calculateAllColors();
}
// The live settings are the RAM default object's, every field.
static bool ownersLive(const LogicEngineRenderer* r) {
  const LogicEngineSettings& d = r->_id == 1 ? LogicEngineFLDDefault : LogicEngineRLDDefault;
  return r->_fade == d.fFade && r->_hue == d.fHue && r->_delay == d.fDelay && r->_pal == d.fPalNum && r->_liveBri == d.fBri &&
         r->_bakedPal == d.fPalNum && r->_bakedBri == d.fBri;
}
static bool defaultsAre(const LogicEngineSettings& d, int fade, int hue, int delay, int pal, int bri) {
  return d.fFade == fade && d.fHue == hue && d.fDelay == delay && d.fPalNum == pal && d.fBri == bri;
}
// The level the layer must give the engine at `now` for a logic look (N3): b= under the ceiling,
// pumped by logic's own pump, from a clock seeded the way the layer's is.
static uint8_t expectedLevel(const BeatClock& bc, uint8_t ceil, uint8_t b, uint8_t am, uint8_t m, uint8_t every, uint32_t now) {
  const uint8_t level = contractLevel(ceil, b);
  if (!am || !m || !bc.running) return level;
  return envBright(level, m, fxLogicPumpAmount(am, beatPosAt(bc, now), m, 0, bc, every));
}
static BeatClock seeded(int bpm, uint32_t now) {
  BeatClock bc; ContractParams pr;
  pr.hasBpm = true; pr.bpm = (uint16_t)bpm; pr.hasPh = true; pr.phMs = 0; pr.hasBpb = true; pr.bpb = 4; pr.hasBeat = true; pr.beatAnchor = 0;
  beatClockSeed(bc, pr, now);
  return bc;
}

// ---- 1. the mock's walk is the model's walk ----
static void test_mock_walk_is_the_model() {
  gMockWalkNormal = true;
  static const struct { int w, h, pal, bri, fade, delay; } kD[] = { { 8, 10, 0, 160, 1, 10 }, { 16, 4, 1, 140, 3, 40 }, { 32, 8, 1, 255, 3, 40 } };
  int compared = 0;
  for (const auto& d : kD) {
    sizes(d.w, d.h); resetAll();
    LogicEngineRenderer* r = (d.pal == 0) ? FLD : RLD;
    if (d.bri != (d.pal == 0 ? 160 : 140)) { r->setBrightness((byte)d.bri); r->_bake(r->_pal, (byte)d.bri); }
    model::Walk w; w.bake(d.pal, d.bri); w.fade = d.fade; w.delay = d.delay;
    w.setup(d.w * d.h, gMockWalkSeed[d.pal == 0 ? 0 : 1]);
    if (d.bri == (d.pal == 0 ? 160 : 140)) CHECK(sameLeds(r, w));          // the start, as setup() draws it
    else w.px.assign(w.px.size(), CRGB());
    if (d.bri != (d.pal == 0 ? 160 : 140)) for (int y = 0; y < r->height(); y++) for (int x = 0; x < r->width(); x++) r->_px[y][x] = CRGB();
    bool same = true;
    for (int f = 0; f < 4000; f++) {
      const int level = (f / 7) % 3 == 0 ? d.bri : (f * 37) % 256;
      if (f == 1500) { r->setFade(7); r->setDelay(90); w.fade = 7; w.delay = 90; }          // a pace
      if (f == 2500) { r->setPaletteHue(0, 86); w.bake(0, d.bri); w.hue = 86; }             // a hue
      r->updateDisplay((byte)level); w.frame(level);
      if (!sameLeds(r, w)) same = false;
      compared++;
    }
    CHECK(same);
  }
  // mockFullScale() (the look's own full scale, for a palette baked at another level) is the
  // brightest LED of the palette the engine bakes at that brightness
  for (int pal = 0; pal < 6; pal++) for (int hue : { 0, 86, 246 }) for (int bri : { 1, 40, 140, 160, 255 }) {
    LogicEngineRenderer t; t._bake((byte)pal, (byte)bri);
    int lv = 0, ly = 0;
    for (int k = 0; k < MOCK_COLORS; k++) {
      int R, G, B; mockHsvRainbow(t._all[k][0] + hue, t._all[k][1], bri == 255 ? t._all[k][2] : mockScale8(bri, t._all[k][2]), R, G, B);
      const int a = R > G ? (R > B ? R : B) : (G > B ? G : B), y = contractLuminance((uint8_t)R, (uint8_t)G, (uint8_t)B);
      if (a > lv) lv = a; if (y > ly) ly = y;
    }
    int fl, fy; mockFullScale(pal, hue, bri, fl, fy);
    CHECK(fl == lv && fy == ly);
  }
  gMockWalkNormal = false; sizes(8, 10);
  printf("  the mock's walk is the model's: %d frames compared LED for LED\n", compared);
}

// ---- 2. what the look sets on the engine ----
static void test_logic_sets() {
  // Entering over the idle blink: its light is not known to the gate, so the look waits 400 ms, the
  // idle blink's last frame up meanwhile (rule 1 of _logicFrame; R2-F2)
  resetAll(); send("LFA:i=logic"); run(ENTER_MS);
  CHECK(FLD->_fx == 106 && RLD->_fx == 0);
  CHECK(FLD->_pal == 0 && FLD->_hue == 0 && FLD->_fade == 1 && FLD->_delay == 10 && FLD->_liveBri == 160);
  CHECK(FLD->_palHueCalls == 0 && FLD->_fadeSets == 0 && FLD->_delaySets == 0);   // the own look, own pace: nothing to change
  CHECK(FLD->_updCalls >= 4 && FLD->_updLast == 160);                            // updateDisplay(level) every frame; b=255: the ceiling
  CHECK(defaultsAre(LogicEngineFLDDefault, 1, 0, 10, 0, 160));
  // N1: a colour with a hue is the front palette's shape at that hue
  static const uint32_t kCol[] = { 0xff0000u, 0x0000ffu, 0x116530u, 0xffe500u, 0xff00ffu, 0x22c55eu };
  for (uint32_t c : kCol) for (int u = 0; u < 2; u++) {
    resetAll(); char l[48]; snprintf(l, sizeof l, "L%cA:i=logic,c=%06x", u ? 'R' : 'F', (unsigned)c); send(l); run(ENTER_MS);
    LogicEngineRenderer* r = u ? RLD : FLD;
    const uint8_t hue = (uint8_t)(fxLogicHue(rgb(c)) - FX_LOGIC_HUE_BASE);
    CHECK(r->_fx == 106 && r->_pal == 0 && r->_hue == hue && r->_lastPalArg == 0 && r->_lastHueArg == hue);
    CHECK(r->_bakedPal == 0 && r->_bakedBri == (u ? 140 : 160));                 // baked at the ceiling
    CHECK(r->_fade == (u ? 3 : 1) && r->_delay == (u ? 40 : 10));                 // the display's own pace
    CHECK(defaultsAre(LogicEngineRLDDefault, 3, 0, 40, 1, 140) && defaultsAre(LogicEngineFLDDefault, 1, 0, 10, 0, 160));
  }
  CHECK((uint8_t)(fxLogicHue(rgb(0xff0000u)) - FX_LOGIC_HUE_BASE) == 86);
  // a colour with no hue: the display's own palette and hue
  for (uint32_t c : { 0xffffffu, 0x101010u, 0x000000u, 0x7f8f9eu }) {
    resetAll(); char l[48]; snprintf(l, sizeof l, "L*A:i=logic,c=%06x", (unsigned)c); send(l); run(ENTER_MS);
    CHECK(FLD->_pal == 0 && FLD->_hue == 0 && RLD->_pal == 1 && RLD->_hue == 0 && FLD->_palHueCalls == 0 && RLD->_palHueCalls == 0);
  }
  // N2: the pace from fxLogicPace() on the display's own values
  for (int s : { 0, 64, 127, 128, 129, 200, 255 }) {
    resetAll(); char l[48]; snprintf(l, sizeof l, "L*A:i=logic,s=%d", s); send(l); run(ENTER_MS);
    CHECK(FLD->_fade == fxLogicPace((uint8_t)s, 1, FX_LOGIC_FADE_MIN) && FLD->_delay == fxLogicPace((uint8_t)s, 10, FX_LOGIC_DELAY_MIN));
    CHECK(RLD->_fade == fxLogicPace((uint8_t)s, 3, FX_LOGIC_FADE_MIN) && RLD->_delay == fxLogicPace((uint8_t)s, 40, FX_LOGIC_DELAY_MIN));
  }
  resetAll(); send("L*A:i=logic,s=0"); run(ENTER_MS);
  CHECK(FLD->_fade == 4 && FLD->_delay == 40 && RLD->_fade == 12 && RLD->_delay == 160);
  resetAll(); send("L*A:i=logic,s=255"); run(ENTER_MS);
  CHECK(FLD->_fade == 1 && FLD->_delay == 10 && RLD->_fade == 1 && RLD->_delay == 20);
  // the owner's own pace is the base: an owner who set the rear slower gets it slower again
  resetAll(); LogicEngineRLDDefault.fFade = 6; LogicEngineRLDDefault.fDelay = 80; send("LRA:i=logic,s=0"); run(ENTER_MS);
  CHECK(RLD->_fade == 24 && RLD->_delay == 255);
  // b= is a fraction of the ceiling; the palette is baked at the level, as native:0 at that b= is (N3, N4)
  // (the owner's change on the idle look is a change of look the gate is told of: 170 ms first)
  resetAll(); ownerSets(0, 90); run(300); send("LFA:i=logic,b=128"); run(ENTER_MS);
  CHECK(FLD->_updLast == contractLevel(90, 128) && FLD->_liveBri == contractLevel(90, 128) && FLD->_bakedBri == contractLevel(90, 128) &&
        LogicEngineFLDDefault.fBri == 90);
  // the owner moves the brightness during the look: the ceiling, and the palette with it
  ownerSets(0, 200); run(LOGIC_RISE_MS + 10);                           // (a rise while up, not a beat: 500 ms)
  CHECK(gContract[0].ceil == 200 && FLD->_bakedBri == contractLevel(200, 128) && FLD->_liveBri == contractLevel(200, 128) &&
        FLD->_updLast == contractLevel(200, 128));
}

// ---- 3. the level every frame (N3, and CA2's two readings) ----
static void test_logic_level_every_frame() {
  int frames = 0, wrong = 0, missed = 0;
  static const struct { int bpm, am, m, an, b; } kCase[] = {
    { 30, 2, 255, 1, 255 }, { 120, 2, 255, 1, 255 }, { 160, 2, 255, 1, 255 }, { 176, 2, 255, 1, 255 }, { 177, 2, 255, 1, 255 },
    { 300, 2, 255, 1, 255 }, { 120, 1, 200, 1, 255 }, { 176, 3, 255, 1, 180 }, { 168, 2, 255, 2, 255 }, { 240, 2, 128, 1, 90 } };
  for (const auto& c : kCase) for (int u = 0; u < 2; u++) {
    resetAll();
    LogicEngineRenderer* r = u ? RLD : FLD;
    char l[96]; snprintf(l, sizeof l, "L%cA:i=logic,b=%d,am=%d,m=%d,an=%d", u ? 'R' : 'F', c.b, c.am, c.m, c.an); send(l);
    run(ENTER_MS + 200);                                                // the look up (its entry hold over), no clock yet: not pumped
    snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", c.bpm); send(l);
    const BeatClock bc = seeded(c.bpm, _mock_millis);
    const uint8_t every = accentEveryPack((uint8_t)c.an, 0), ceil = u ? 140 : 160;
    int rises = 0, pumpBeats = 0, prev = 0; int32_t lastBeat = BEAT_NONE;
    for (int f = 0; f < 1500; f++) {
      const int before = r->_updCalls; const long fr = r->_frames;
      _mock_millis += 10; frame();
      if (r->_frames == fr) continue;
      frames++;
      if (r->_updCalls != before + 1) missed++;                       // exactly one updateDisplay a frame
      if (r->_updLast != expectedLevel(bc, ceil, (uint8_t)c.b, (uint8_t)c.am, (uint8_t)c.m, every, _mock_millis)) wrong++;
      const BeatPos bp = beatPosAt(bc, _mock_millis);
      if (bp.beatIndex != lastBeat) { lastBeat = bp.beatIndex; if (fxLogicPumpFires((uint8_t)c.am, bp, bc, every)) pumpBeats++; }
      if (r->_updLast > prev + 20) rises++;
      prev = r->_updLast;
    }
    // 160 BPM, every beat: S2's floor fires on every beat, logic's own on every other one
    if (c.bpm == 160 && c.am == 2 && c.an == 1) {
      int s2 = 0; int32_t lb = BEAT_NONE;
      for (uint32_t t = 0; t < 15000; t += 10) { const BeatPos bp = beatPosAt(bc, bc.anchorMs + t); if (bp.beatIndex != lb) { lb = bp.beatIndex; if (beatAccentFires(2, bp, bc, 0)) s2++; } }
      CHECK(s2 >= 39 && pumpBeats >= 19 && pumpBeats <= s2 / 2 + 1);
      CHECK(rises >= 19 && rises <= s2 / 2 + 1);                         // the light the engine is given rises on those alone
    }
  }
  CHECK(wrong == 0 && missed == 0);
  printf("  the level every frame: %d frames, %d off the expected level, %d without exactly one updateDisplay\n", frames, wrong, missed);
}

// ---- 4. the gates: asked with the level, a held frame at the last level shown, told what was drawn ----
static void test_logic_gates() {
  // S9: from dark, inside the window. A solid lights the display; X; logic 200 ms after the onset
  resetAll(); send("**M:v=show"); send("LFX"); run(20); send("LFA:i=solid,c=ffffff"); run(10);
  const uint32_t onset = gContract[0].fieldOnsetMs;
  run(190); send("LFX"); run(10); CHECK(fieldLevel(FLD) == 0);
  send("LFA:i=logic");
  bool held = true, after = true; int heldFrames = 0;
  for (int f = 0; f < 40; f++) {
    _mock_millis += 10; frame();
    if (_mock_millis - onset < FX_LOGIC_PUMP_MIN_MS) { heldFrames++; if (FLD->_updCalls != 0 || fieldLevel(FLD) != 0) held = false; }
    else if (_mock_millis - onset >= FX_LOGIC_PUMP_MIN_MS + 10 && FLD->_updLast != 160) after = false;
  }
  CHECK(held && after && heldFrames >= 15);                              // held as it enters: the frame that is up (X's dark) stays up
  CHECK(gContract[0].fieldLit && gContract[0].fieldRef == 160 && gContract[0].fieldOnsetMs >= onset + FX_LOGIC_PUMP_MIN_MS);   // 400 ms: logic's window
  // S9 while the look is up: a frame the field gate holds is drawn at the last level shown
  resetAll(); send("**M:v=show"); send("LFX"); run(400); send("LFA:i=logic"); run(10);
  const uint32_t g0 = gContract[0].fieldOnsetMs;
  CHECK(FLD->_updLast == 160 && g0 == _mock_millis);                     // the look lit the display: an onset
  run(90); send("LFB:b=0"); run(10); CHECK(FLD->_updLast == 0);          // down: at once
  run(40); send("LFB:b=255");                                            // up again 150 ms after the onset
  bool lastShown = true; int heldUp = 0, upAfter = 0;
  for (int f = 0; f < 50; f++) {
    const int c0 = FLD->_updCalls; _mock_millis += 10; frame();
    if (FLD->_updCalls != c0 + 1) lastShown = false;                     // the walk goes on every frame...
    if (_mock_millis - g0 < LOGIC_RISE_MS) { heldUp++; if (FLD->_updLast != 0) lastShown = false; }   // ...at the level last shown (a rise
    else if (FLD->_updLast != 160) lastShown = false; else upAfter++;   //    while up, not a beat: 500 ms)
  }
  CHECK(lastShown && heldUp >= 30 && upAfter >= 10);
  // S4: the field changed a moment ago (a red look got brighter: B). The logic frame waits for the
  // look gate; the frame that is up stays up, and the walk is not drawn
  resetAll(); send("**M:v=show"); send("LFA:i=solid,c=ff0000,b=100"); run(400);
  send("LFB:b=255"); run(10);
  const uint32_t look = gContract[0].lookMs;
  CHECK(look == _mock_millis && FLD->px(0, 0).r == 160);
  send("LFA:i=logic,c=000000");                                         // (a live A keeps the colour it does not name)
  bool red = true; int heldLook = 0;
  for (int f = 0; f < 30; f++) {
    _mock_millis += 10; frame();
    if (_mock_millis - look < STROBE_MIN_STATE_MS) { heldLook++; if (FLD->_updCalls != 0 || FLD->px(0, 0).r != 160) red = false; }
  }
  CHECK(red && heldLook >= 15 && FLD->_updCalls > 0 && gContract[0].lookSig == contractLookSig(CE_LOGIC, ContractRGB{0, 0, 0}));
  // S9: B raised every 50 ms. The level the engine gets goes up at most once in 500 ms (LOGIC_RISE_MS)
  resetAll(); send("**M:v=show"); send("LFA:i=logic"); run(ENTER_MS);
  uint32_t lastUp = 0; int ups = 0, prev = FLD->_updLast; bool spaced = true;
  for (uint32_t t = 0; t < 6000; t += 10) {
    if (t % 50 == 0) send((t / 50) % 2 ? "LFB:b=255" : "LFB:b=0");
    _mock_millis += 10; frame();
    if (FLD->_updLast > prev) { if (ups && _mock_millis - lastUp < LOGIC_RISE_MS) spaced = false; lastUp = _mock_millis; ups++; }
    prev = FLD->_updLast;
  }
  CHECK(spaced && ups >= 6);                                             // (B's own hold, 340 ms, and this one are not in step)
  // a look that is up asks for nothing: logic steady at b=255 is never held by the field gate
  resetAll(); send("LFA:i=logic"); run(ENTER_MS); int lows = 0;
  for (int f = 0; f < 300; f++) { _mock_millis += 10; frame(); if (FLD->_fx == 106 && FLD->_updLast != 160) lows++; }
  CHECK(lows == 0);
}

// ---- 5. the hand-over between the idle blink and logic ----
// N4 as written: native:0 and logic at one b= are one light (the palette baked at the level and
// scaled by it again, both: the owner's decision of 2026-11-24 on the review's R2-F2). So when
// the hand-over changes nothing (logic not pumped, in the display's own colours, at native:0's
// level) logic takes the idle blink's walk over as it is, and the idle blink takes logic's back.
// (A) The same light: at b=255 (own brightness, 255, 90) and at b=128 and b=200, the frames of a
//     run that goes between native:0 (the idle blink, or native:0 at that b=) and logic, by every
//     path, are the frames of a run that stays on native:0 at that b=, LED for LED, and every
//     frame of either display draws once: there is no hold.
// (B) A change at the hand-over (a pumped logic, a coloured one, by turns with native:0 every 300
//     and 600 ms at 255): unseen light, held; no LED moves while a display draws nothing, and no
//     part of a display is lit more often in a second than three or the idle blink alone, by the
//     gate's step on the look's own scale (stage 1k's fine reading) and on the ceiling. (Without
//     the hold, the walk going on while the idle blink waited for a pumped logic, the pumped case
//     every 300 ms lit the rear 16 x 4 four times a second by the gate's own count.)
struct FineCount {
  int W = 0, H = 0; std::vector<std::vector<int>> ser[2];           // [measure][whole, quarters, bands]
  struct Reg { int x0, x1, y0, y1; }; std::vector<Reg> regs;
  void init(int w, int h) {
    W = w; H = h; regs.clear(); regs.push_back({ 0, w, 0, h });
    for (int q = 0; q < 4; q++) regs.push_back({ (q & 1) ? w / 2 : 0, (q & 1) ? w : w / 2, (q & 2) ? h / 2 : 0, (q & 2) ? h : h / 2 });
    for (int y0 = 0; y0 < h; y0++) for (int y1 = y0 + 1; y1 <= h; y1++) if ((y1 - y0) * 4 >= h && !(y0 == 0 && y1 == h)) regs.push_back({ 0, w, y0, y1 });
    for (int x0 = 0; x0 < w; x0++) for (int x1 = x0 + 1; x1 <= w; x1++) if ((x1 - x0) * 4 >= w && !(x0 == 0 && x1 == w)) regs.push_back({ x0, x1, 0, h });
    for (int m = 0; m < 2; m++) ser[m].assign(regs.size(), std::vector<int>());
  }
  void see(const LogicEngineRenderer* r) {
    for (int m = 0; m < 2; m++) for (size_t k = 0; k < regs.size(); k++) {
      const Reg& g = regs[k]; long sum = 0;
      for (int y = g.y0; y < g.y1; y++) for (int x = g.x0; x < g.x1; x++) {
        const CRGB c = r->px(x, y);
        sum += m ? contractLuminance(c.r, c.g, c.b) : (c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b));
      }
      ser[m][k].push_back((int)(sum * 16 / ((g.x1 - g.x0) * (g.y1 - g.y0))));
    }
  }
  // the worst second over every series, the gate's rule with the step of `full` (by each measure)
  int worst(int fullLv, int fullLy) const {
    int most = 0;
    for (int m = 0; m < 2; m++) {
      const int step = contractFieldStep((uint8_t)(m ? fullLy : fullLv)) * 16;
      for (const auto& v : ser[m]) {
        bool lit = false; int ref = v.empty() ? 0 : v[0]; std::vector<int> at;
        for (size_t i = 0; i < v.size(); i++) {
          const int x = v[i];
          if (lit) { if (x > ref) ref = x; else if (ref - x >= step) { lit = false; ref = x; } }
          else if (x < ref) ref = x;
          else if (x - ref >= step) {
            lit = true; ref = x; at.push_back((int)i * 10);
            int k = 0; for (size_t j = at.size(); j-- > 0 && at.back() - at[j] < 1000;) k++;
            if (k > most) most = k;
          }
        }
      }
    }
    return most;
  }
};
// The look's own full scale at the ceiling: its brightest LED by each measure over the palette.
static void ownFull(const LogicEngineRenderer* r, int ceil, int& lv, int& ly) { mockFullScale(r->_pal, r->_hue, ceil, lv, ly); }
static int gHoldMoved = 0;
static int handoverRun(const std::vector<std::pair<uint32_t, const char*>>& script, int ownerBri, FineCount* fc, std::vector<std::string>& out,
                       int& on106, int& notOne) {
  resetAll();
  if (ownerBri) { ownerSets(0, ownerBri); ownerSets(1, ownerBri); }
  for (int u = 0; u < 2; u++) fc[u].init((u ? RLD : FLD)->width(), (u ? RLD : FLD)->height());
  size_t next = 0; on106 = 0; notOne = 0; out.clear(); gHoldMoved = 0; int& moved = gHoldMoved;
  std::string prevLeds[2] = { leds(FLD), leds(RLD) };
  for (uint32_t t = 0; t < 9000; t += 10) {
    while (next < script.size() && script[next].first <= t) send(script[next++].second);
    const int c0[2] = { FLD->_updCalls, RLD->_updCalls };
    _mock_millis += 10; frame();
    if (FLD->_fx == 106) on106++;
    for (int u = 0; u < 2; u++) {
      const LogicEngineRenderer* r = u ? RLD : FLD;
      if (r->_updCalls != c0[u] + 1) {                                  // a hold (or a frame drawn twice)
        notOne++;
        if (r->_updCalls != c0[u] || leds(r) != prevLeds[u]) moved++;   // a hold in which an LED moved
      }
      prevLeds[u] = leds(r);
      if (t >= 1000) fc[u].see(r);
    }
    out.push_back(leds(FLD) + leds(RLD));
  }
  return 0;
}
static void test_logic_handover() {
  gMockWalkNormal = true;
  int worstRun = 0, worstStock = 0, runs = 0;
  // (A) one light
  for (int b : { 255, 128, 200 }) for (int bri : { 0, 255, 90 }) for (int size = 0; size < 2; size++) {
    if (b != 255 && bri == 90) continue;
    sizes(size ? 24 : 16, 4);
    char n0[48], lg[48], lgF[48], lgR[48];
    snprintf(n0, sizeof n0, "L*A:i=native:0,b=%d", b); snprintf(lg, sizeof lg, "L*A:i=logic,b=%d", b);
    snprintf(lgF, sizeof lgF, "LFA:i=logic,b=%d", b); snprintf(lgR, sizeof lgR, "LRA:i=logic,c=ffffff,s=128,b=%d", b);
    std::vector<std::pair<uint32_t, const char*>> stock, script;
    if (b == 255)
      script = { { 500, "L*A:i=logic" },          // live, no show: the idle blink to logic
                 { 1500, "L*M:v=idle" },          // and back
                 { 2500, "**M:v=show,wd=2" }, { 2600, "LFA:i=logic" }, { 2700, "LRA:i=logic,c=ffffff,s=128,b=255" },   // a show's first A; a grey is own colours
                 { 3700, "**M:v=idle" },
                 { 4500, "**M:v=show,wd=1" }, { 4600, "L*A:i=logic" },   // ...and the watchdog gives it back (no line for a second)
                 { 7000, "L*A:i=logic,m=0,am=0" }, { 7900, "**M:v=idle" } };
    else {
      stock  = { { 400, "**M:v=show" }, { 500, n0 } };
      script = { { 400, "**M:v=show" }, { 500, n0 }, { 1500, lg }, { 2500, n0 }, { 3000, lgF }, { 3200, lgR }, { 4300, n0 },
                 { 5000, lg }, { 6000, n0 }, { 6400, lg }, { 7900, n0 } };
    }
    FineCount fa[2], fb[2]; std::vector<std::string> a, bb; int o106a = 0, o106b = 0, na = 0, nb = 0;
    handoverRun(stock, bri, fa, a, o106a, na);
    handoverRun(script, bri, fb, bb, o106b, nb);
    int differ = 0; for (size_t i = 0; i < a.size(); i++) if (a[i] != bb[i]) differ++;
    CHECK(differ == 0 && na == 0 && nb == 0);                           // the frames of native:0, and no hold
    CHECK(o106b > 250);                                                  // the look was up, for much of the run
    CHECK(FLD->_fx == 0 && RLD->_fx == 0);
    if (b == 255) CHECK(ownersLive(FLD) && ownersLive(RLD) && FLD->_palHueCalls == 0 && FLD->_fadeSets == 0 && FLD->_delaySets == 0);
    runs++;
  }
  // (B) a change at the hand-over
  int notOneB = 0, heldB = 0;
  for (int period : { 300, 600 }) for (int kind = 0; kind < 2; kind++) for (int size = 0; size < 2; size++) {
    sizes(size ? 24 : 16, 4);
    static char lines[2][48]; snprintf(lines[0], sizeof lines[0], "%s", kind ? "L*A:i=logic,c=00ff00" : "L*A:i=logic,am=2,m=255");
    std::vector<std::pair<uint32_t, const char*>> stock, script = { { 400, "**M:v=show" } };
    for (uint32_t t = 500; t < 8800; t += (uint32_t)period) {
      script.push_back({ t, ((t - 500) / (uint32_t)period) % 2 ? "L*A:i=native:0" : lines[0] });
      if (t == 500) script.push_back({ 510, "**C:bpm=120,ph=0,bpb=4,beat=0" });   // (after the show's first A: a C first would be its dark start)
    }
    FineCount fa[2], fb[2]; std::vector<std::string> a, bb; int o106a = 0, o106b = 0, na = 0, nb = 0;
    handoverRun(stock, 255, fa, a, o106a, na);
    handoverRun(script, 255, fb, bb, o106b, nb);
    notOneB += gHoldMoved; heldB += nb;
    for (int u = 0; u < 2; u++) {
      int lv, ly; mockFullScale(u ? 1 : 0, 0, 255, lv, ly);              // the idle blink's own full scale at 255
      if (kind) { int gl, gy; mockFullScale(0, (uint8_t)(fxLogicHue(rgb(0x00ff00u)) - FX_LOGIC_HUE_BASE), 255, gl, gy); if (gl > lv) lv = gl; if (gy > ly) ly = gy; }
      const int wa = fa[u].worst(lv, ly), wb = fb[u].worst(lv, ly), ga = fa[u].worst(255, 255), gb = fb[u].worst(255, 255);
      if (!((wb <= 3 || wb <= wa) && (gb <= 3 || gb <= ga))) printf("  OVER period %d kind %d size %d display %d: own scale %d (stock %d), gate %d (stock %d)\n", period, kind, size, u, wb, wa, gb, ga);
      CHECK((wb <= 3 || wb <= wa) && (gb <= 3 || gb <= ga));
      if (wb > worstRun) worstRun = wb;
      if (wa > worstStock) worstStock = wa;
    }
  }
  CHECK(notOneB == 0 && heldB > 0);                                     // held, and nothing moves while held
  // the seamless hand-over back waits logic's window: a steady logic that lit 100 ms ago (from X's
  // dark) is taken back by the idle blink 400 ms after it lit, the walk going on meanwhile
  sizes(16, 4); resetAll(); send("**M:v=show"); send("LFX"); run(500); send("LFA:i=logic"); run(10);
  const uint32_t lit = gContract[0].fieldOnsetMs; CHECK(lit == _mock_millis);
  run(90); send("LFM:v=idle");
  uint32_t normal = 0; bool walking = true;
  for (int f = 0; f < 60 && !normal; f++) { const int c0 = FLD->_updCalls; _mock_millis += 10; frame(); if (FLD->_updCalls != c0 + 1) walking = false; if (FLD->_fx == 0) normal = _mock_millis; }
  CHECK(walking && normal && normal >= lit + FX_LOGIC_PUMP_MIN_MS && normal <= lit + FX_LOGIC_PUMP_MIN_MS + 20);
  // a pumped logic is no seamless hand-over: the idle blink takes it back as unseen light, the
  // walk's last frame held 400 ms
  resetAll(); send("**M:v=show"); send("LFA:i=logic,am=2,m=255"); run(500); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(1000);
  const uint32_t left = _mock_millis; send("LFM:v=idle"); normal = 0; bool still = true; const std::string up0 = leds(FLD);
  for (int f = 0; f < 60 && !normal; f++) { _mock_millis += 10; frame(); if (FLD->_fx == 0) normal = _mock_millis; else if (leds(FLD) != up0) still = false; }
  CHECK(still && normal >= left + FX_LOGIC_PUMP_MIN_MS && normal <= left + FX_LOGIC_PUMP_MIN_MS + 20);
  gMockWalkNormal = false; sizes(8, 10);
  printf("  the hand-over: %d seamless runs frame for frame the idle blink's, none with a hold; with a change, %d frames held, none moving,"
         " the worst second on the look's own scale %d (the idle blink alone %d)\n", runs, heldB, worstRun, worstStock);
}

// ---- 6. restoration: the owner's settings after the look, X, idle, the watchdog, a take-over ----
static void test_logic_restoration() {
  enum { LOOK, STOP, IDLE, WATCHDOG, BUTTON, OVERLAY };
  for (int how = LOOK; how <= OVERLAY; how++) for (int u = 0; u < 2; u++) {
    resetAll();
    LogicEngineRenderer* r = u ? RLD : FLD;
    const char U = u ? 'R' : 'F';
    char l[96];
    snprintf(l, sizeof l, "**M:v=show%s", how == WATCHDOG ? ",wd=1" : ""); send(l);
    snprintf(l, sizeof l, "L%cA:i=logic,c=ff0000,s=0,b=128", U); send(l); run(ENTER_MS + 100);
    CHECK(r->_fx == 106 && r->_pal == 0 && r->_hue == 86 && r->_fade == (u ? 12 : 4) && r->_delay == (u ? 160 : 40));
    CHECK(!ownersLive(r) && r->_updLast == contractLevel(u ? 140 : 160, 128));
    switch (how) {
      case LOOK:     snprintf(l, sizeof l, "L%cA:i=solid,c=00ff00", U); send(l); break;
      case STOP:     snprintf(l, sizeof l, "L%cX", U); send(l); break;
      case IDLE:     snprintf(l, sizeof l, "L%cM:v=idle", U); send(l); break;
      case WATCHDOG: break;
      case BUTTON:   r->selectSequence(4); break;                         // @nT4 from the controller
      case OVERLAY:  snprintf(l, sizeof l, "L%cP:c=ffffff,d=600", U); send(l); break;
    }
    if (how == OVERLAY) {
      // the accent waits 400 ms after the walk (_logicLeave: the walk's light is not known to the
      // gate), the walk's last frame up meanwhile; the owner's settings are the engine's from the start
      run(100); CHECK(r->_fx == 106 && ownersLive(r) && r->px(0, 0).r == r->px(0, 0).b);
      run(350); CHECK(r->px(0, 0).r == contractLevel(u ? 140 : 160, 128) && r->px(0, 0).g == r->px(0, 0).r);   // the accent
      run(600); CHECK(r->_hue == 86 && r->_fade == (u ? 12 : 4) && r->_updLast == contractLevel(u ? 140 : 160, 128));   // the look again, as it was
      continue;
    }
    run(how == WATCHDOG ? 1600 : 600);
    CHECK(ownersLive(r));
    CHECK(defaultsAre(LogicEngineFLDDefault, 1, 0, 10, 0, 160) && defaultsAre(LogicEngineRLDDefault, 3, 0, 40, 1, 140));
    if (how == IDLE || how == WATCHDOG) CHECK(r->_fx == 0 && !gContract[u].inShow);
    if (how == BUTTON) CHECK(r->_fx == 4 && !gContract[u].inShow);
    if (how == STOP) CHECK(r->_fx == 106 && fieldLevel(r) == 0);
    if (how == LOOK) CHECK(r->_fx == 106 && r->px(0, 0).g == contractLevel(u ? 140 : 160, 128));   // (b=128 stays: a live A is a patch)
  }
  // the owner turned the display down during the look: theirs is the brightness afterwards
  resetAll(); send("LFA:i=logic,c=ff0000"); run(ENTER_MS); ownerSets(0, 70); run(300);
  CHECK(FLD->_liveBri == 70 && FLD->_bakedBri == 70 && FLD->_updLast == 70);
  send("LFM:v=idle"); run(500);
  CHECK(FLD->_fx == 0 && ownersLive(FLD) && FLD->_liveBri == 70 && LogicEngineFLDDefault.fBri == 70);
  // a native look with B in a show, then logic: the walk is baked at its level, as the native look
  // was, and the owner's brightness goes back to the default object.
  resetAll(); send("**M:v=show"); send("LFA:i=native:0,b=64"); run(400); CHECK(LogicEngineFLDDefault.fBri == 40);
  send("LFA:i=logic"); run(ENTER_MS + 100);
  CHECK(FLD->_fx == 106 && FLD->_bakedBri == 40 && FLD->_liveBri == 40 && FLD->_updLast == 40 && LogicEngineFLDDefault.fBri == 160);
}

// ---- 6b. after the walk: its light is not known to the gate (_logicLeave) ----
// The gate takes logic at its level (N4); the walk's own light is far under it. So the first lit
// frame after it is taken as an onset from dark: it waits 340 ms, the walk's last frame up.
static void test_logic_leave() {
  // logic, a white look, X, the white look again: two onsets 340 ms apart at the least
  resetAll(); send("**M:v=show"); send("LFA:i=logic"); run(ENTER_MS + 100);
  const int walkLevel = fieldLevel(FLD);
  CHECK(walkLevel < 16);                                                 // the walk at 160: dim (no channel above 17)
  send("LFA:i=solid,c=ffffff");
  uint32_t first = 0, second = 0; bool frozen = true; std::string up = leds(FLD);
  bool dark = false;
  for (uint32_t t = 0; t < 1200; t += 10) {
    if (first && _mock_millis == first + 10) send("LFX");               // X 10 ms after the white lit, and the white again
    if (first && _mock_millis == first + 20) send("LFA:i=solid,c=ffffff");
    _mock_millis += 10; frame();
    const int lv = fieldLevel(FLD);
    if (first && lv == 0) dark = true;
    if (!first && lv > 100) first = _mock_millis;
    if (!first && leds(FLD) != up) frozen = false;                       // the walk's last frame stays up while the white waits
    if (first && dark && !second && lv > 100) second = _mock_millis;
  }
  CHECK(first && second && frozen);
  CHECK(second - first >= 2 * STROBE_MIN_STATE_MS);
  // the white came 400 ms after the walk stopped (gWalkLeft, FX_LOGIC_PUMP_MIN_MS), not at once
  resetAll(); send("**M:v=show"); send("LFA:i=logic"); run(ENTER_MS + 100);
  const uint32_t asked = _mock_millis; send("LFA:i=solid,c=ffffff");
  uint32_t lit = 0; for (int f = 0; f < 60 && !lit; f++) { _mock_millis += 10; frame(); if (fieldLevel(FLD) > 100) lit = _mock_millis; }
  CHECK(lit >= asked + FX_LOGIC_PUMP_MIN_MS && lit <= asked + FX_LOGIC_PUMP_MIN_MS + 20);
  // an accent shorter than that is not shown; the walk carries on after it
  resetAll(); send("**M:v=show"); send("LFA:i=logic"); run(ENTER_MS + 100);
  send("LFP:c=ffffff,d=200"); int whiteFrames = 0;
  for (int f = 0; f < 80; f++) { _mock_millis += 10; frame(); if (fieldLevel(FLD) > 100) whiteFrames++; }
  CHECK(whiteFrames == 0 && FLD->_updLast == 160 && FLD->_fx == 106);
  // going to native:0 is no leave: the walk goes on into the idle blink (the hand-over test above)
}

// ---- 7. recoloured in place; the pace changed in place ----
static void test_logic_in_place() {
  // R2-F3: a recolour is a rise of the walk's luminance that the gate cannot measure. It waits 400
  // ms after the field last lit or got brighter, the walk going on in its old colours, and the
  // window starts again from it. Not a restart of the look (S11: recoloured in place).
  resetAll(); send("**M:v=show"); send("LFA:i=logic,c=ff0000");
  for (int f = 0; f < 100 && FLD->_hue != 86; f++) run(10);              // the red is up (a recolour of the idle blink)
  const uint32_t lit0 = gContract[0].fieldOnsetMs, start = gContract[0].startMs;
  CHECK(FLD->_hue == 86 && lit0 == _mock_millis);                        // ...and it started the window
  run(180);                                                               // 180 ms on: the look gate allows a colour
  send("LFA:i=logic,c=0000ff");
  bool walking = true, oldHue = true; uint32_t blue = 0;
  for (int f = 0; f < 40; f++) {
    const int c0 = FLD->_updCalls; _mock_millis += 10; frame();
    if (FLD->_updCalls != c0 + 1) walking = false;
    if (!blue && FLD->_hue == 246) blue = _mock_millis;
    if (_mock_millis - lit0 < FX_LOGIC_PUMP_MIN_MS && FLD->_hue != 86) oldHue = false;
  }
  CHECK(walking && oldHue && blue == lit0 + FX_LOGIC_PUMP_MIN_MS);       // the walk went on, red, until the window; then blue
  CHECK(gContract[0].fieldOnsetMs == blue && gContract[0].startMs == start);   // the window from the recolour; not restarted
  CHECK((uint8_t)(fxLogicHue(rgb(0x0000ffu)) - FX_LOGIC_HUE_BASE) == 246);
  send("LFA:i=logic,c=0000ff,s=255"); run(400);                          // a pace alone is no recolour
  CHECK(FLD->_fade == 1 && FLD->_delay == 10 && FLD->_hue == 246 && gContract[0].fieldOnsetMs == blue);
  // a scored logic section, then a solid one, reached by the clock
  resetAll(); send("**M:v=show"); send("LFA:i=logic,c=116530,at=0"); send("LFA:i=solid,c=ff0000,at=8"); send("**C:bpm=120,ph=0,bpb=4,beat=0");
  run(1000); CHECK(FLD->_fx == 106 && FLD->_hue == (uint8_t)(fxLogicHue(rgb(0x116530u)) - FX_LOGIC_HUE_BASE) && FLD->_updLast == 160);
  run(3500); CHECK(FLD->px(0, 0).r == 160 && ownersLive(FLD));
}

// ---- 8. the pump reaches the LEDs as the region model has it ----
// The same seed: the model's walk run by the model's own rule (each frame, the palette baked at
// envBright(level, m, fxLogicPumpAmount()) from the clock and every LED stepped at it), and the
// mock's walk run by the layer.
// Every LED of every frame is compared. If the layer delivered the level any other way (a
// fraction of 255, beatAccentAmount(), a frame skipped or doubled, a level held by the gate)
// the two would part.
static void test_logic_pump_is_the_model() {
  gMockWalkNormal = true;
  int cases = 0, apart = 0; long compared = 0;
  static const struct { int disp, w, h, bri; } kD[] = { { 0, 8, 10, 0 }, { 1, 16, 4, 0 }, { 1, 32, 8, 0 }, { 0, 8, 10, 255 }, { 1, 16, 4, 255 } };
  for (const auto& d : kD) for (int bpm : { 30, 120, 176, 177, 300 }) for (int s : { 128, 0, 255 }) for (uint32_t c : { 0u, 0xff0000u }) {
    if (c && s != 128) continue;
    sizes(d.disp ? d.w : 16, d.disp ? d.h : 4);
    resetAll();
    const int ownBri = d.disp ? 140 : 160, bri = d.bri ? d.bri : ownBri;
    if (d.bri) ownerSets(d.disp, d.bri);
    LogicEngineRenderer* r = d.disp ? RLD : FLD;
    model::Walk w; w.bake(d.disp, ownBri); w.fade = d.disp ? 3 : 1; w.delay = d.disp ? 40 : 10;
    w.setup(r->width() * r->height(), gMockWalkSeed[d.disp]);
    if (d.bri) w.bake(d.disp, bri);
    // the idle blink, a while
    for (int f = 0; f < 120; f++) { _mock_millis += 10; frame(); w.frame(bri); }
    bool same = sameLeds(r, w);
    char l[96]; snprintf(l, sizeof l, "L%cA:i=logic,c=%06x,s=%d,am=2,m=255", d.disp ? 'R' : 'F', (unsigned)c, s); send(l);
    // The entry over the idle blink: in its own colours (and not pumped: no clock yet) the same walk
    // goes on; in a colour it is a change, unseen light, and the idle blink's last frame is held
    // 400 ms, the model's walk waiting with it. The model takes the look's palette and pace at the
    // frame the layer first draws.
    const ContractRGB col = rgb(c);
    bool applied = false; int heldEntry = 0, mpal = d.disp, mbaked = bri;
    for (int f = 0; f < (int)(ENTER_MS / 10) + 20; f++) {
      const int c0 = r->_updCalls; _mock_millis += 10; frame();
      if (r->_updCalls == c0) { heldEntry++; continue; }
      if (!applied) {
        applied = true;
        if (!fxLogicOwn(col)) { mpal = 0; w.bake(0, bri); w.hue = (uint8_t)(fxLogicHue(col) - FX_LOGIC_HUE_BASE); }   // N1, as the model does
        w.fade = fxLogicPace((uint8_t)s, (uint8_t)(d.disp ? 3 : 1), FX_LOGIC_FADE_MIN);                       // N2
        w.delay = fxLogicPace((uint8_t)s, (uint8_t)(d.disp ? 40 : 10), FX_LOGIC_DELAY_MIN);
      }
      w.frame(bri);
      if (!sameLeds(r, w)) same = false;
    }
    // own colours: the idle blink taken over as it is, no frame held; a colour: a change, held 400 ms
    if (!applied || (c ? heldEntry < (int)(FX_LOGIC_PUMP_MIN_MS / 10) - 1 : heldEntry != 0)) same = false;
    snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", bpm); send(l);
    const BeatClock bc = seeded(bpm, _mock_millis);
    for (int f = 0; f < 2000; f++) {
      const int c0 = r->_updCalls; _mock_millis += 10; frame();
      if (r->_updCalls != c0 + 1) same = false;                          // under its own pump the look is never held
      const BeatPos bp = beatPosAt(bc, _mock_millis);
      const int level = envBright((uint8_t)bri, 255, fxLogicPumpAmount(2, bp, 255, 0, bc, 0));   // the model's rule (test_logic_regions.cpp):
      if (level != mbaked) { w.bake(mpal, level); mbaked = level; }                            // the palette at the level,
      w.frame(level);                                                                          // each LED at its next step
      if (!sameLeds(r, w)) same = false;
      compared++;
    }
    cases++; if (!same) apart++;
  }
  CHECK(apart == 0);
  gMockWalkNormal = false; sizes(8, 10);
  printf("  the pump: %d cases, %ld frames, the layer's LEDs and the model's apart in %d cases\n", cases, compared, apart);
}

// ---- 9. vu (N10 to N12) ----
// Each row of the front display: lit when its mean is half the ceiling or more, dark at a tenth or
// less. The most onsets of one row in any second, and the closest two.
struct Rows {
  bool lit[10] = {}; std::vector<uint32_t> at[10]; int most = 0; uint32_t gap = 0xFFFFFFFFu;
  void see(const LogicEngineRenderer* r, int ceil, uint32_t now) {
    for (int y = 0; y < r->height(); y++) {
      long s = 0; for (int x = 0; x < r->width(); x++) { const CRGB c = r->px(x, y); s += c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b); }
      const int v = (int)(s / r->width());
      if (!lit[y] && v * 2 >= ceil) {
        lit[y] = true;
        if (!at[y].empty() && now - at[y].back() < gap) gap = now - at[y].back();
        at[y].push_back(now);
        int k = 0; for (size_t j = at[y].size(); j-- > 0 && now - at[y][j] < 1000;) k++;
        if (k > most) most = k;
      } else if (lit[y] && v * 10 <= ceil) lit[y] = false;
    }
  }
  int onsets() const { int n = 0; for (const auto& a : at) n += (int)a.size(); return n; }
};
static void test_vu() {
  // not pumped, vu is meter: the same frames from the same lines
  for (int pass = 0; pass < 2; pass++) {
    std::vector<std::string> fr[2];
    for (int k = 0; k < 2; k++) {
      resetAll(); send("**M:v=show"); send(k ? "LFA:i=vu,c=00ff00" : "LFA:i=meter,c=00ff00");
      if (pass) send("**C:bpm=120,ph=0,bpb=4,beat=0");                   // a clock, and no pump (no am=, no m=)
      for (uint32_t t = 0; t < 5000; t += 10) {
        if (t % 130 == 0) { char l[32]; snprintf(l, sizeof l, "LFL:v=%u", (unsigned)((t * 37) % 256)); send(l); }
        _mock_millis += 10; frame(); fr[k].push_back(leds(FLD));
      }
    }
    CHECK(fr[0] == fr[1]);
  }
  // pumped: the level rides the beat, at most once in 340 ms; the brightness stays at b=
  for (int bpm : { 30, 120, 176, 177, 300 }) for (int seek = 0; seek < 2; seek++) for (int m : { 255, 128 }) {
    resetAll(); send("**M:v=show");
    char l[96]; snprintf(l, sizeof l, "LFA:i=vu,c=ffffff,am=2,m=%d", m); send(l);
    snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=0", bpm); send(l);
    Rows rows; bool bright = true; int lowest = 255, highest = 0;
    for (uint32_t t = 0; t < 20000; t += 10) {
      if (seek && t % 50 == 0) { snprintf(l, sizeof l, "**C:bpm=%d,ph=0,bpb=4,beat=%u", bpm, (unsigned)((t / 50) % 7)); send(l); }   // a seek every 50 ms
      if (t % 70 == 0) send((t / 70) % 2 ? "LFL:v=255" : "LFL:v=0");      // N12: L is not shown while the pump runs
      _mock_millis += 10; frame();
      rows.see(FLD, 160, _mock_millis);
      for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) { const CRGB c = FLD->px(x, y); if (c.r && c.r != 160) bright = false; }
      if (t > 2000) { if (gContract[0].level < lowest) lowest = gContract[0].level; if (gContract[0].level > highest) highest = gContract[0].level; }
    }
    CHECK(rows.most <= 3 && rows.gap >= 2 * STROBE_MIN_STATE_MS);
    CHECK(bright);                                                          // lit LEDs at the look's level, never pumped
    const int top = 255 - m + (m * m) / 255;                               // envBright(255, m, m): the top of the ride
    // (under a seek every 50 ms the raise a window lets through is whatever the newest offer is then)
    CHECK(seek ? (highest > 255 - m + 20 && rows.onsets() > 4) : (highest >= top - 6 && lowest <= 255 - m + 1 && rows.onsets() > 4));
    if (bpm == 120 && !seek && m == 255) printf("  vu at 120 BPM, m=255: rows lit at most %d times in a second, the closest two %u ms apart\n", rows.most, rows.gap);
    if (seek && bpm == 120 && m == 255) printf("  vu under a seek every 50 ms: at most %d, the closest two %u ms apart\n", rows.most, rows.gap);
  }
  // N12: L during the pump changes nothing, frame for frame
  {
    std::vector<std::string> fr[2];
    for (int k = 0; k < 2; k++) {
      resetAll(); send("**M:v=show"); send("LFA:i=vu,c=ffffff,am=1,m=200"); send("**C:bpm=120,ph=0,bpb=4,beat=0");
      for (uint32_t t = 0; t < 6000; t += 10) {
        if (k && t % 40 == 0) send((t / 40) % 3 ? "L*L:v=0" : "L*L:v=255");
        _mock_millis += 10; frame(); fr[k].push_back(leds(FLD));
      }
    }
    CHECK(fr[0] == fr[1]);
  }
  // the pump stops (C:bpm=0): L reaches the meter again
  resetAll(); send("**M:v=show"); send("LFA:i=vu,c=ffffff,am=2,m=255"); send("**C:bpm=120,ph=0,bpb=4,beat=0"); run(2000);
  send("**C:bpm=0"); run(400); send("LFL:v=128"); run(400);
  CHECK(gContract[0].level == 128 && FLD->px(0, 9).r == 160 && FLD->px(0, 0).r == 0);
  // B alternating 0 and 200 under the pump: the rows still light at most three times a second
  resetAll(); send("**M:v=show"); send("LFA:i=vu,c=ffffff,am=2,m=255"); send("**C:bpm=300,ph=0,bpb=4,beat=0");
  { Rows rows; for (uint32_t t = 0; t < 10000; t += 10) { if (t % 30 == 0) send((t / 30) % 2 ? "LFB:b=200" : "LFB:b=0"); _mock_millis += 10; frame(); rows.see(FLD, 125, _mock_millis); }
    CHECK(rows.most <= 3 && rows.gap >= 2 * STROBE_MIN_STATE_MS); }
}

// ---- 10. logic and a native look by turns (the leave to native:4 and native:5), at the owner's 255 ----
// As well as the mock allows: it draws a native look as a fill at its brightness, the worst case
// for the whole field. Every 200 ms a live A asks for the other one. Counted by the gate's step on
// the ceiling (the S9 rule for light the gate can see) over the whole field and every region of a
// quarter or more; and, reported only, on the walk's own scale (which counts every fill as a flash).
static void test_logic_native_turns() {
  gMockWalkNormal = true;
  int worstGate = 0, worstOwn = 0, natives[2] = { 0, 0 };
  for (int period : { 200, 600 }) for (int seq : { 4, 5 }) for (int u = 0; u < 2; u++) {
    sizes(16, 4); resetAll(); ownerSets(0, 255); ownerSets(1, 255); run(1000);
    LogicEngineRenderer* r = u ? RLD : FLD;
    FineCount fc; fc.init(r->width(), r->height());
    send("**M:v=show");
    int starts = 0, lastFx = r->_fx, fullLv = 0, fullLy = 0;
    for (uint32_t t = 0; t < 20000; t += 10) {
      if (t % (uint32_t)period == 0) { char l[48]; snprintf(l, sizeof l, "L%cA:i=%s", u ? 'R' : 'F', (t / (uint32_t)period) % 2 ? "logic" : (seq == 4 ? "native:4" : "native:5")); send(l); }
      _mock_millis += 10; frame();
      if (r->_fx != lastFx && r->_fx == seq) starts++;
      lastFx = r->_fx;
      if (gLogicUp[u]) { int a, b; ownFull(r, 255, a, b); if (a > fullLv) fullLv = a; if (b > fullLy) fullLy = b; }
      if (t >= 1000) fc.see(r);
    }
    const int wg = fc.worst(255, 255), wo = fc.worst(fullLv, fullLy);
    CHECK(wg <= 3 && (period == 200 || starts >= 12));                 // (every 200 ms each hand-over is still waiting when the next line comes)
    if (wg > worstGate) worstGate = wg;
    if (wo > worstOwn) worstOwn = wo;
    natives[period == 600] += starts;
  }
  gMockWalkNormal = false; sizes(8, 10);
  printf("  logic and native:4 / native:5 by turns at 255: %d native starts in 80 s every 200 ms, %d every 600 ms; the worst second by the gate's step %d,"
         " on the walk's own scale %d (not asserted)\n", natives[0], natives[1], worstGate, worstOwn);
}

// ---- 11. projector's stand-in here (sparkle) is not pumped (N8) ----
static void test_projector_unpumped() {
  std::vector<std::string> fr[2];
  for (int k = 0; k < 2; k++) {
    resetAll(); send("**M:v=show"); send(k ? "LFA:i=projector,c=ff0000,am=2,m=255" : "LFA:i=projector,c=ff0000");
    send("**C:bpm=120,ph=0,bpb=4,beat=0");
    for (int f = 0; f < 400; f++) { _mock_millis += 10; frame(); fr[k].push_back(leds(FLD)); }
  }
  CHECK(fr[0] == fr[1]);
  bool full = true; for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) { const CRGB c = FLD->px(x, y); if (c.r && c.r != 160) full = false; }
  CHECK(full);
}

int main() {
  test_mock_walk_is_the_model();
  test_logic_sets();
  test_logic_level_every_frame();
  test_logic_gates();
  test_logic_handover();
  test_logic_restoration();
  test_logic_leave();
  test_logic_in_place();
  test_logic_pump_is_the_model();
  test_vu();
  test_logic_native_turns();
  test_projector_unpumped();
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract v1.4 logic and vu on the RSeries layer OK (%d known defect(s) still open)\n", gKnown);
  return 0;
}
