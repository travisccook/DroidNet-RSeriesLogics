// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Host mock of the reeltwo engine API surface that ContractLogics.h uses. It type-checks the
// firmware layer AND models the three engine behaviours the contract depends on, each tied to
// lib/Reeltwo/src/dome/LogicEngine.h:
//   * which sequence is selected, and WHEN the engine notices a change (LE:721, :804, :842-849).
//     The order inside animate() matters and is reproduced exactly: the effect is latched only
//     when the selected value differs from the previous one, the effect RUNS, and only THEN is
//     previous := current (LE:818-819). So a selectSequence() made from INSIDE an effect is
//     never noticed: the next frame sees previous == current and keeps running the old effect.
//     (This mock used to set previous before running the effect, which hid exactly that bug.)
//   * restoreSettings() on every effect change: the live fBri goes back to the RAM default
//     object's fBri (LE:807, :1830);
//   * a custom effect that returns false hands the display to the default effect (LE:818-830);
//   * the effect selector is called exactly when the engine starts a newly selected sequence
//     (LE:804-813). The fork's selectors in main.cpp report that to contractEngineLatched(), and
//     so does this mock;
//   * LogicEffectDefaultSelector() indexes one past its 25-entry table for 25 (LE:2733).
//   * animate() draws at most every 10 ms (LE:792), however often the loop passes. A selection
//     made between two frames waits for the next one; the contract's tick does not;
//   * two stock effects that are NOT steady light, because the contract may select them and the
//     S9 gate has to live with what they do by themselves: FLASHCOLOR (6) alternates full and
//     50/255 every 250 ms (LE:2081-2091), and FAILURE (2) is dark from 8 s until it ends itself
//     at 18 s, when the engine falls back to its default look (LE:2014-2046);
//   * the stock effects that IGNORE the brightness setting: 19 to 24 write their pixels at a
//     value of their own (150 to 255: LE:2125-2190, 2404-2700), so do the fork's 101 to 105, and
//     Leia (3) sets its brightness itself every frame (LE:1945-1970).
//     Here they fill the display at 200 whatever setBrightness() was given.
//   * v1.4 (Task F2): THE WALK, which the logic look runs. The live settings (fade, hue, delay,
//     palette, brightness) and the RAM default object they are restored from (LE:1085, :1829);
//     the palette as calculateAllColors() bakes it (LE:1427-1486); every LED walking it
//     (updateMappedLED(), LE:1493-1519: a step when its pause runs out, `fade` frames at a tween,
//     random8(delay) at every fifth colour); FastLED's hsv2rgb_rainbow; the walk's start as
//     setup() draws it (LE:775-786); updateDisplay(bri) (LE:1149). It is the model of
//     test/host/test_logic_regions.cpp (stage 1k) and test_native_s10.cpp, line for line, and
//     test_contract_logics_r2.cpp holds the two to each other. FastLED's random8() has one seed
//     for both displays; here each display has its own (gMockWalkSeed), so a test can replay one.
//     The setters the look calls (setPaletteHue, setFade, setDelay, setBrightness, restoreSettings,
//     calculateAllColors, updateDisplay) record their calls.
// It is still a mock. A native look is not drawn pixel by pixel: every native sequence but
// LIGHTSOUT fills the display with its live brightness (a stand-in for "lit": the worst case for
// a full-field measurement), and LIGHTSOUT fills it black. The one exception is opt-in:
// with gMockWalkNormal set, NORMAL (0) is drawn by the walk, as LogicEffectNormal() draws it
// (LE:1922-1929), so a test can compare the logic look with the idle blink LED for LED.
#pragma once
#include <stdint.h>
typedef uint8_t byte;

static uint32_t _mock_millis = 0;
inline uint32_t millis() { return _mock_millis; }

struct CHSV {
  uint8_t h, s, v;
  CHSV(int hh, int ss, int vv) : h((uint8_t)hh), s((uint8_t)ss), v((uint8_t)vv) {}
};
struct CRGB {
  uint8_t r = 0, g = 0, b = 0;
  CRGB() {}
  CRGB(int rr, int gg, int bb) : r((uint8_t)rr), g((uint8_t)gg), b((uint8_t)bb) {}
  CRGB(const CHSV& c) : r(c.v), g(c.v), b(c.v) {}   // level only: enough to see brightness
};

// The RAM default object (LE:176-201), the fields the layer reads. FRONT/REAR fade, hue, delay,
// palette and brightness: LE:90-105.
struct LogicEngineSettings { byte fFade; byte fHue; byte fDelay; byte fPalNum; byte fBri; };
static LogicEngineSettings LogicEngineFLDDefault = { 1, 0, 10, 0, 160 };
static LogicEngineSettings LogicEngineRLDDefault = { 3, 0, 40, 1, 140 };

// ---- the walk's arithmetic: FastLED, as test_logic_regions.cpp models it (scale8 without
// FASTLED_SCALE8_FIXED, the model's default) ----
inline int mockScale8(int i, int s) { return (i * s) >> 8; }
inline int mockScale8v(int i, int s) { return ((i * s) >> 8) + ((i && s) ? 1 : 0); }
inline int mockMap8(int i, int lo, int hi) { return lo + mockScale8(i, hi - lo); }
inline void mockHsvRainbow(int h, int s, int v, int& r, int& g, int& b) {      // hsv2rgb_rainbow
  h &= 255;
  const int off8 = (h & 31) << 3, third = mockScale8(off8, 85), two = mockScale8(off8, 170);
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
    else { int d = 255 - s; d = mockScale8v(d, d); const int sc = 255 - d; r = mockScale8(r, sc) + d; g = mockScale8(g, sc) + d; b = mockScale8(b, sc) + d; }
  }
  if (v != 255) {
    v = mockScale8v(v, v);
    if (v == 0) r = g = b = 0; else { r = mockScale8(r, v); g = mockScale8(g, v); b = mockScale8(b, v); }
  }
}
// The engine's six palettes (LE:1432-1439), HSV key colours.
static const uint8_t kMockKeys[6][4][3] = {
  { {170,255,0}, {170,255,85}, {170,255,170}, {170,0,170} },     // front
  { {90,235,0}, {75,255,250}, {30,255,184}, {0,255,250} },       // rear
  { {0,255,0}, {0,255,0}, {0,255,100}, {0,255,250} },            // monotone
  { {0,255,0}, {0,255,250}, {40,255,0}, {40,255,250} },          // red and yellow
  { {165,50,248}, {166,181,226}, {165,223,89}, {255,255,214} },  // blue and red
  { {87,206,105}, {79,255,214}, {43,255,250}, {25,255,214} } };  // yellow and green
static const int MOCK_TWEENS = 14, MOCK_COLORS = 4 + MOCK_TWEENS * 3, MOCK_WALK = 2 * MOCK_COLORS - 2;   // 46 colours, 90 steps
// A palette's full scale at a brightness: its brightest LED by each measure (the brightest
// channel; Rec.709 luminance in whole numbers) over the 46 colours baked at `bri`, with `hue`
// added. Stage 1k's "the look's own full scale", for a palette the engine may have baked at
// another level (logic bakes at the level it draws).
inline void mockFullScale(int palNum, int hue, int bri, int& lv, int& ly) {
  lv = ly = 0;
  const int p = palNum < 6 ? palNum : 0;
  for (int k = 0; k < 4; k++) {
    const int t = k + MOCK_TWEENS * k;
    for (int j = 0; j <= (t + 1 != MOCK_COLORS ? MOCK_TWEENS : 0); j++) {
      int c[3];
      for (int el = 0; el < 3; el++) {
        const int per = (t + 1 != MOCK_COLORS) ? (kMockKeys[p][k + 1][el] - kMockKeys[p][k][el]) / (MOCK_TWEENS + 1) : 0;
        const int val = (j && per) ? ((kMockKeys[p][k][el] + j * per) & 255) : kMockKeys[p][k][el];
        c[el] = (el == 2) ? mockMap8(val, 1, bri) : val;
      }
      const int v = bri == 255 ? c[2] : mockScale8(bri, c[2]);
      int R, G, B; mockHsvRainbow(c[0] + hue, c[1], v, R, G, B);
      const int a = R > G ? (R > B ? R : B) : (G > B ? G : B), y = (54 * R + 183 * G + 19 * B) >> 8;
      if (a > lv) lv = a; if (y > ly) ly = y;
    }
  }
}
// The walk's seed for each display at mockEngineReset(), and whether NORMAL is drawn by the walk.
static uint16_t gMockWalkSeed[2] = { 0x51F1u, 0x2D5Bu };
static bool gMockWalkNormal = false;

struct LogicEngineRenderer {
  unsigned _id = 1;
  int  _seq = 0, _prevSeq = -1;       // fDisplayEffectVal's sequence, and fPreviousEffectVal's
  int  _fx = 0;                       // the LATCHED effect: what animate() actually runs
  uint32_t _lastFrame = 0, _fxStart = 0; bool _drawn = false;   // the 10 ms frame gate; when the effect started
  long _frames = 0;                   // frames drawn
  int  _selects = 0;                  // selectSequence() calls
  int  _restarts[256] = {0};          // effect changes into each sequence
  int  _nativeFrames = 0;             // frames a native look ran
  int  _oobCalls = 0;                 // frames that would have called sLogicEffects[25]
  byte _liveBri = 160;                // fSettings.fBri
  int  _calcCalls = 0;
  unsigned getID() const { return _id; }
  // 8 by 10 unless a test asks for another size (mockEngineSize(): the rear display is 16, 24 or
  // 28 by 4, or 32 by 8, on a real board, and a look that lights columns or rows covers another
  // share of such a field).
  int _w = 8, _h = 10;
  int width() const { return _w; }
  int height() const { return _h; }
  CRGB _px[10][32];
  long _writes = 0;                   // setPixelRGB() calls: a frame effect 106 only HELD wrote nothing
  void setPixelRGB(int x, int y, const CRGB& c) {
    _writes++;
    if (x >= 0 && x < _w && y >= 0 && y < _h) _px[y][x] = c;
  }
  CRGB px(int x, int y) const { return _px[y][x]; }
  enum ColorVal { kDefault = 0 };
  int  _speed = 0;                    // the speed digit of the last selection (LE:160)
  void selectSequence(byte s, int = 0, int speed = 0, int = 0) { _seq = s; _speed = speed; _selects++; }
  void resetEffect() { _prevSeq = -1; }
  bool hasEffectChanged() const { return _seq != _prevSeq; }    // LE:842: a selection is waiting for the next frame
  // fFlipFlop (LE:795-799, 941). The engine toggles it every effect delay, whatever runs; here
  // it is FLASHCOLOR's only, and FLASHCOLOR starts on its bright half.
  bool getEffectFlip() const { return ((uint32_t)(millis() - _fxStart) / 250u) % 2u == 0u; }
  void setBrightness(byte b) { _liveBri = b; }
  byte getBrightness() const { return _liveBri; }
  // ---- the live settings and the walk (v1.4) ----
  byte _fade = 1, _hue = 0, _delay = 10, _pal = 0;
  int  _palHueCalls = 0, _fadeSets = 0, _delaySets = 0, _restoreCalls = 0, _updCalls = 0;
  int  _updLast = -1;                 // the argument of the last updateDisplay()
  int  _lastPalArg = -1, _lastHueArg = -1;
  int  _bakedPal = -1, _bakedBri = -1;   // what the palette was last baked from
  uint8_t _all[MOCK_COLORS][3] = {};  // fAllColors: HSV
  uint8_t _num[320] = {}, _pause[320] = {};   // fLEDStatus: the colour number and the pause of each LED
  uint16_t _rnd = 1;                  // random8()'s seed (this display's own)
  uint8_t random8() { _rnd = (uint16_t)(_rnd * 2053u + 13849u); return (uint8_t)((_rnd & 0xFFu) + (_rnd >> 8)); }
  uint8_t random8(uint8_t lim) { return (uint8_t)((random8() * (unsigned)lim) >> 8); }
  const LogicEngineSettings& _defaultsObj() const { return _id == 1 ? LogicEngineFLDDefault : LogicEngineRLDDefault; }
  // calculateAllColors(palNum, bri) (LE:1427-1486), as the engine runs it; _bake is the engine's
  // own (animate(), setPaletteHue()) and is not counted as a call from the layer.
  void _bake(byte palNum, byte bri) {
    _bakedPal = palNum; _bakedBri = bri;
    const int p = palNum < 6 ? palNum : 0;
    for (int k = 0; k < 4; k++) {
      const int t = k + MOCK_TWEENS * k;
      _all[t][0] = kMockKeys[p][k][0]; _all[t][1] = kMockKeys[p][k][1]; _all[t][2] = (uint8_t)mockMap8(kMockKeys[p][k][2], 1, bri);
      if (t + 1 != MOCK_COLORS)
        for (int j = 1; j <= MOCK_TWEENS; j++)
          for (int el = 0; el < 3; el++) {
            const int per = (kMockKeys[p][k + 1][el] - kMockKeys[p][k][el]) / (MOCK_TWEENS + 1);
            const int val = per ? ((kMockKeys[p][k][el] + j * per) & 255) : kMockKeys[p][k][el];
            _all[t + j][el] = (uint8_t)((el == 2) ? mockMap8(val, 1, bri) : val);
          }
    }
  }
  void calculateAllColors() { _calcCalls++; _bake(_pal, _liveBri); }
  void calculateAllColors(byte palNum, byte bri) { _calcCalls++; _bake(palNum, bri); }
  void setPaletteHue(byte palNum, byte hue) { _palHueCalls++; _lastPalArg = palNum; _lastHueArg = hue; _pal = palNum; _hue = hue; _bake(_pal, _liveBri); }
  unsigned getCurrentPalette() const { return _pal; }
  byte getHue() const { return _hue; }
  void setHue(byte h) { _hue = h; }
  byte getFade() const { return _fade; }
  void setFade(byte f) { _fadeSets++; _fade = f; }
  byte getDelay() const { return _delay; }
  void setDelay(byte d) { _delaySets++; _delay = d; }
  void _restore() { const LogicEngineSettings& d = _defaultsObj(); _fade = d.fFade; _hue = d.fHue; _delay = d.fDelay; _pal = d.fPalNum; _liveBri = d.fBri; }
  void restoreSettings() { _restoreCalls++; _restore(); }
  // updateMappedLED(index, hue, bri) (LE:1493-1519); the LEDs are row-major here.
  void _stepLED(int i, byte hueVal, byte briVal) {
    if (_pause[i] != 0) { _pause[i]--; return; }
    if (++_num[i] >= MOCK_WALK) _num[i] = 0;
    const int real = _num[i] >= MOCK_COLORS ? (MOCK_COLORS - 2) - (_num[i] - MOCK_COLORS) : _num[i];
    _pause[i] = (uint8_t)((_num[i] % 5 == 0) ? random8(_delay) : _fade);
    const uint8_t* c = _all[real];
    int r, g, b; mockHsvRainbow(c[0] + hueVal, c[1], briVal == 255 ? c[2] : mockScale8(briVal, c[2]), r, g, b);
    _px[i / _w][i % _w] = CRGB(r, g, b);
  }
  void updateDisplay(byte bri) { _updCalls++; _updLast = bri; _writes++; for (int i = 0; i < _w * _h; i++) _stepLED(i, _hue, bri); }
  void updateDisplay() { updateDisplay(_liveBri); }
  // setup() (LE:775-786): restoreSettings, the palette, and each LED's start on the walk. The
  // LEDs it lights are written only when NORMAL is drawn by the walk (the R1 tests start dark).
  void _walkSetup(uint16_t seed) {
    _restore(); _bake(_pal, _liveBri); _rnd = seed;
    CRGB keep[10][32]; for (int y = 0; y < 10; y++) for (int x = 0; x < 32; x++) keep[y][x] = _px[y][x];
    for (int i = 0; i < _w * _h; i++) { _num[i] = random8((uint8_t)MOCK_WALK); _pause[i] = random8(); _stepLED(i, _hue, 255); }
    if (!gMockWalkNormal) for (int y = 0; y < 10; y++) for (int x = 0; x < 32; x++) _px[y][x] = keep[y][x];
  }
};

static LogicEngineRenderer _fld_obj, _rld_obj;
static LogicEngineRenderer* FLD = &_fld_obj;
static LogicEngineRenderer* RLD = &_rld_obj;
inline void FLD_selectSequence(byte s, int = 0, int speed = 0, int = 0) { FLD->selectSequence(s, 0, speed); }
inline void RLD_selectSequence(byte s, int = 0, int speed = 0, int = 0) { RLD->selectSequence(s, 0, speed); }

inline bool ContractRender(LogicEngineRenderer& r);   // the fork effect, id 106
inline void contractEngineLatched(uint8_t u, unsigned seq);   // main.cpp's effect selectors call this

// One engine frame for one display: LogicEngineRenderer::animate() (LE:789-831), reduced, in the
// engine's own order.
inline void mockEngineFrame(LogicEngineRenderer& r) {
  const uint32_t now = millis();
  if (r._drawn && (uint32_t)(now - r._lastFrame) < 10u) return;   // not yet: a frame every 10 ms at most  LE:792
  r._drawn = true; r._lastFrame = now; r._frames++;
  if (r._seq != r._prevSeq) {                                   // hasEffectChanged()              LE:804
    r._fxStart = now;
    r._restore(); r._bake(r._pal, r._liveBri);                  // restoreSettings(), calculateAllColors() LE:807-808
    r._restarts[r._seq & 255]++;
    r._fx = r._seq;                                             // fLogicEffect = fEffectSelector() LE:813
    contractEngineLatched(r._id == 1 ? 0 : 1, (unsigned)r._seq);   // ...which is the fork's selector (main.cpp): it reports the start
  }
  bool keep = true;
  if (r._fx == 106) keep = ContractRender(r);                   // the effect runs                  LE:818
  else if (r._fx == 25) r._oobCalls++;                          // LE:2733: '>' lets 25 through
  else if (r._fx == 0 && gMockWalkNormal) { r._nativeFrames++; r.updateDisplay(); }   // LogicEffectNormal (LE:1922)
  else {
    r._nativeFrames++;
    const uint32_t ran = now - r._fxStart;
    byte level = (r._fx == 14) ? 0 : r._liveBri;                // LIGHTSOUT is dark; any other native look is "lit"
    if (r._fx == 14) r._liveBri = 0;
    if (r._fx == 6 && !r.getEffectFlip()) level = (byte)((unsigned)r._liveBri * 50u / 255u);   // FLASHCOLOR's low half
    if (r._fx == 2 && ran >= 8000u) level = 0;                  // FAILURE has faded out...
    if (r._fx == 2 && ran >= 18000u) keep = false;              // ...and ends itself: back to the default look
    if (r._fx == 3 || (r._fx >= 19 && r._fx <= 24) || (r._fx >= 101 && r._fx <= 105)) level = 200;   // their own level
    for (int y = 0; y < r._h; y++) for (int x = 0; x < r._w; x++) r._px[y][x] = CRGB(level, level, level);
  }
  r._prevSeq = r._seq;                                          // fPreviousEffectVal = fDisplayEffectVal  LE:819 (AFTER the effect)
  if (!keep) r._seq = 0;                                        // selectEffect(fDefaultEffect)     LE:829
}
// The size of each display from the next mockEngineReset() on: {front, rear} columns and rows.
static int gMockW[2] = { 8, 8 }, gMockH[2] = { 10, 10 };
inline void mockEngineSize(int u, int w, int h) { gMockW[u] = w > 32 ? 32 : w; gMockH[u] = h > 10 ? 10 : h; }
inline void mockEngineReset() {
  _fld_obj = LogicEngineRenderer(); _rld_obj = LogicEngineRenderer(); _rld_obj._id = 2;
  _fld_obj._w = gMockW[0]; _fld_obj._h = gMockH[0]; _rld_obj._w = gMockW[1]; _rld_obj._h = gMockH[1];
  LogicEngineFLDDefault = { 1, 0, 10, 0, 160 }; LogicEngineRLDDefault = { 3, 0, 40, 1, 140 };
  _fld_obj._walkSetup(gMockWalkSeed[0]); _rld_obj._walkSetup(gMockWalkSeed[1]);
}

struct Marcduino { static const char* getCommand() { return ""; } };

// The real macro registers a global Marcduino action; for a host type-check we skip
// it and replicate the action body inline in the test (see compile_contract_logics.cpp).
#define MARCDUINO_ACTION(name, marc, p)
