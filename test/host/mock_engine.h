// Minimal host mock of the reeltwo engine API surface that ContractLogics.h uses,
// to TYPE-CHECK the firmware layer on the host (NOT a behavioral sim). Signatures
// mirror LogicEngine.h / main.cpp as verified 2026-07-12:
//   LogicEngineRenderer::{getID,width,height,setPixelRGB,selectSequence,resetEffect,setBrightness}
//   globals FLD/RLD (LogicEngineRenderer*), FLD_/RLD_selectSequence, Marcduino::getCommand.
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
  CRGB(const CHSV&) {}   // FastLED provides CRGB(const CHSV&) rainbow conversion
};

struct LogicEngineRenderer {
  unsigned _id = 1;
  unsigned getID() const { return _id; }
  int width() const { return 8; }
  int height() const { return 10; }
  void setPixelRGB(int, int, const CRGB&) {}
  void selectSequence(byte, int = 0, int = 0, int = 0) {}
  void resetEffect() {}
  void setBrightness(byte) {}
};

static LogicEngineRenderer _fld_obj, _rld_obj;
static LogicEngineRenderer* FLD = &_fld_obj;
static LogicEngineRenderer* RLD = &_rld_obj;
inline void FLD_selectSequence(byte, int = 0, int = 0, int = 0) {}
inline void RLD_selectSequence(byte, int = 0, int = 0, int = 0) {}

struct Marcduino { static const char* getCommand() { return ""; } };

// The real macro registers a global Marcduino action; for a host type-check we skip
// it and replicate the action body inline in the test (see compile_contract_logics.cpp).
#define MARCDUINO_ACTION(name, marc, p)
