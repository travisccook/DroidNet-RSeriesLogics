// Host mock of the engine and FastLED surface that src/native/NativeBrightness.h uses.
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// Signatures mirror the vendored sources as checked on 2026-09-22:
//   lib/FastLED/src/FastLED.h:562/:566   int CFastLED::count(); CLEDController& operator[](int)
//   lib/FastLED/src/controller.h:24      #define DISABLE_DITHER 0x00
//   lib/FastLED/src/controller.h:127/132 CLEDController::setDither(uint8_t), setCorrection(CRGB)
//   lib/Reeltwo/src/dome/LogicEngine.h   LogicEngineDefaults::MAX_BRIGHTNESS (255),
//     FRONT_BRI 160 / REAR_BRI 140, LogicEngineSettings::fBri, the static
//     LogicEngineFLDDefault / LogicEngineRLDDefault objects, setBrightness(byte),
//     calculateAllColors()
//   lib/Reeltwo/src/core/Marcduino.h:92  static const char* Marcduino::getCommand()
// The real compile is test/host/run.sh's cross-compile stage; this file only lets the host
// suite run the action's logic. It RECORDS what the action does so the test can assert on it.
#pragma once
#include <stdint.h>
typedef uint8_t byte;

#define DISABLE_DITHER 0x00
#define BINARY_DITHER 0x01

struct CRGB {
  uint8_t r = 0, g = 0, b = 0;
  CRGB() {}
  CRGB(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}
};

struct CLEDController {
  uint8_t ditherMode = BINARY_DITHER;
  CRGB correction = CRGB(255, 255, 255);
  int correctionCalls = 0;
  CLEDController& setDither(uint8_t mode) { ditherMode = mode; return *this; }
  CLEDController& setCorrection(CRGB c) { correction = c; correctionCalls++; return *this; }
};

// FastLED keeps a list of controllers in creation order; a display's init() appends one.
struct CFastLED {
  CLEDController controllers[8];
  int n = 0;
  int count() { return n; }
  CLEDController& operator[](int x) { return controllers[x]; }
  void mockAdd() { controllers[n] = CLEDController(); n++; }   // what one addLeds() does
};
static CFastLED FastLED;

namespace LogicEngineDefaults {
  static constexpr byte FRONT_BRI = 160;
  static constexpr byte REAR_BRI = 140;
  static constexpr byte MAX_BRIGHTNESS = 255;
}

struct LogicEngineSettings { byte fBri; };
static LogicEngineSettings LogicEngineFLDDefault = { LogicEngineDefaults::FRONT_BRI };
static LogicEngineSettings LogicEngineRLDDefault = { LogicEngineDefaults::REAR_BRI };

struct LogicEngineRenderer {
  byte liveBri = 0;
  int calculateCalls = 0;
  void setBrightness(byte bri) { liveBri = bri; }
  void calculateAllColors() { calculateCalls++; }
};
static LogicEngineRenderer* FLD = nullptr;
static LogicEngineRenderer* RLD = nullptr;

struct Marcduino {
  static const char*& mockCommand() { static const char* s = ""; return s; }
  static const char* getCommand() { return mockCommand(); }
};

// The real macro (Marcduino.h:14-20) registers a global action and runs the body later on the
// player. Here it records the action text and turns the body into a function the test calls.
#define MARCDUINO_ACTION(name, marc, p) \
  static const char* const mockActionText_##name = #marc; \
  static void mockAction_##name() { p; }
