// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Host mock of the reeltwo engine API surface that ContractLogics.h uses. It type-checks the
// firmware layer AND models the three engine behaviours the contract depends on, each tied to
// lib/Reeltwo/src/dome/LogicEngine.h:
//   * which sequence is selected, and when it changes (LE:721, :804, :842-849);
//   * restoreSettings() on every effect change: the live fBri goes back to the RAM default
//     object's fBri (LE:807, :1830);
//   * a custom effect that returns false hands the display to the default effect (LE:818-830);
//   * LogicEffectDefaultSelector() indexes one past its 25-entry table for 25 (LE:2733).
// It is still a mock: native looks draw nothing, they only count frames and restarts.
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

struct LogicEngineSettings { byte fBri; };
static LogicEngineSettings LogicEngineFLDDefault = { 160 };   // FRONT_BRI (LE:104)
static LogicEngineSettings LogicEngineRLDDefault = { 140 };   // REAR_BRI  (LE:105)

struct LogicEngineRenderer {
  unsigned _id = 1;
  int  _seq = 0, _prevSeq = -1;       // selected sequence, and the one the last frame ran
  int  _selects = 0;                  // selectSequence() calls
  int  _restarts[256] = {0};          // effect changes into each sequence
  int  _nativeFrames = 0;             // frames a native look ran
  int  _oobCalls = 0;                 // frames that would have called sLogicEffects[25]
  byte _liveBri = 160;                // fSettings.fBri
  int  _calcCalls = 0;
  unsigned getID() const { return _id; }
  int width() const { return 8; }
  int height() const { return 10; }
  CRGB _px[10][8];
  void setPixelRGB(int x, int y, const CRGB& c) {
    if (x >= 0 && x < 8 && y >= 0 && y < 10) _px[y][x] = c;
  }
  CRGB px(int x, int y) const { return _px[y][x]; }
  void selectSequence(byte s, int = 0, int = 0, int = 0) { _seq = s; _selects++; }
  void resetEffect() { _prevSeq = -1; }
  void setBrightness(byte b) { _liveBri = b; }
  byte getBrightness() const { return _liveBri; }
  void calculateAllColors() { _calcCalls++; }
};

static LogicEngineRenderer _fld_obj, _rld_obj;
static LogicEngineRenderer* FLD = &_fld_obj;
static LogicEngineRenderer* RLD = &_rld_obj;
inline void FLD_selectSequence(byte s, int = 0, int = 0, int = 0) { FLD->selectSequence(s); }
inline void RLD_selectSequence(byte s, int = 0, int = 0, int = 0) { RLD->selectSequence(s); }

inline bool ContractRender(LogicEngineRenderer& r);   // the fork effect, id 106

// One engine frame for one display: LogicEngineRenderer::animate() (LE:789-831), reduced.
inline void mockEngineFrame(LogicEngineRenderer& r) {
  if (r._seq != r._prevSeq) {                                   // hasEffectChanged()
    r._liveBri = (r._id == 1 ? LogicEngineFLDDefault : LogicEngineRLDDefault).fBri;   // restoreSettings()
    r._restarts[r._seq & 255]++;
    r._prevSeq = r._seq;
  }
  bool keep = true;
  if (r._seq == 106) keep = ContractRender(r);
  else if (r._seq == 25) r._oobCalls++;                         // LE:2733: '>' lets 25 through
  else {
    r._nativeFrames++;
    if (r._seq == 14) {                                         // LIGHTSOUT: dark
      r._liveBri = 0;
      for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) r._px[y][x] = CRGB(0, 0, 0);
    }
  }
  if (!keep) r._seq = 0;                                        // back to fDefaultEffect (NORMAL)
}
inline void mockEngineReset() {
  _fld_obj = LogicEngineRenderer(); _rld_obj = LogicEngineRenderer(); _rld_obj._id = 2;
  _rld_obj._liveBri = 140;
  LogicEngineFLDDefault.fBri = 160; LogicEngineRLDDefault.fBri = 140;
}

struct Marcduino { static const char* getCommand() { return ""; } };

// The real macro registers a global Marcduino action; for a host type-check we skip
// it and replicate the action body inline in the test (see compile_contract_logics.cpp).
#define MARCDUINO_ACTION(name, marc, p)
