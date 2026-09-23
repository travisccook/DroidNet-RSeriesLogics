// Host behaviour test for src/native/NativeBrightness.h, run against mock_native_brightness.h.
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// What it pins (spec 2026-09-22, section 2, "the hybrid output dimmer"):
//   * capture keeps a display's controller only when its init added EXACTLY one, and turns
//     that controller's dither off; zero or two added -> nothing kept;
//   * the first accepted command for a display raises the RAM default fBri and the live fBri
//     to 255 and recalculates the colours, ONCE per display;
//   * every accepted command sets that display's controller correction to (L, L, L);
//   * a display with no captured controller is ignored completely (not raised either);
//   * a rejected tail changes nothing;
//   * the Marcduino action text is exactly "#LEBRI" and its body parses getCommand().
#include "mock_native_brightness.h"
#include "../../src/native/NativeBrightness.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static LogicEngineRenderer gFront, gRear;

// Power-on state: both displays constructed, one controller each (the C2B5 default types).
static void powerOn(bool haveRear) {
  FastLED = CFastLED();
  gFront = LogicEngineRenderer();
  gRear = LogicEngineRenderer();
  LogicEngineFLDDefault.fBri = LogicEngineDefaults::FRONT_BRI;
  LogicEngineRLDDefault.fBri = LogicEngineDefaults::REAR_BRI;
  sNativeBrightnessCtl[0] = sNativeBrightnessCtl[1] = nullptr;
  sNativeBrightnessRaised[0] = sNativeBrightnessRaised[1] = false;

  const int beforeFront = FastLED.count();
  FLD = &gFront; FastLED.mockAdd();                 // FLD_init(): one addLeds()
  nativeBrightnessCapture(NATIVE_BRIGHTNESS_UNIT_FRONT, beforeFront);

  const int beforeRear = FastLED.count();
  if (haveRear) { RLD = &gRear; FastLED.mockAdd(); } // RLD_init(): one addLeds()
  else { RLD = nullptr; }                           // kLogicEngineNoRLD: none
  nativeBrightnessCapture(NATIVE_BRIGHTNESS_UNIT_REAR, beforeRear);
}

static bool corrected(const CLEDController& c, uint8_t l) {
  return c.correction.r == l && c.correction.g == l && c.correction.b == l;
}

static void test_capture_keeps_one_controller_and_disables_dither() {
  powerOn(true);
  CHECK(sNativeBrightnessCtl[0] == &FastLED[0]);
  CHECK(sNativeBrightnessCtl[1] == &FastLED[1]);
  CHECK(FastLED[0].ditherMode == DISABLE_DITHER);
  CHECK(FastLED[1].ditherMode == DISABLE_DITHER);
  CHECK(FastLED[0].correctionCalls == 0 && FastLED[1].correctionCalls == 0);  // power-on = stock
}

static void test_capture_ignores_zero_or_two_controllers() {
  powerOn(false);
  CHECK(sNativeBrightnessCtl[1] == nullptr);                   // no RLD: nothing kept
  const int before = FastLED.count();
  FastLED.mockAdd(); FastLED.mockAdd();                        // two added: ambiguous
  nativeBrightnessCapture(NATIVE_BRIGHTNESS_UNIT_REAR, before);
  CHECK(sNativeBrightnessCtl[1] == nullptr);
  CHECK(FastLED[before].ditherMode == BINARY_DITHER);          // untouched
}

static void test_first_command_raises_once_then_only_corrects() {
  powerOn(true);
  nativeBrightnessCommand("128");
  CHECK(LogicEngineFLDDefault.fBri == 255 && LogicEngineRLDDefault.fBri == 255);
  CHECK(gFront.liveBri == 255 && gRear.liveBri == 255);
  CHECK(gFront.calculateCalls == 1 && gRear.calculateCalls == 1);
  CHECK(corrected(FastLED[0], 128) && corrected(FastLED[1], 128));

  // An effect change copies the defaults back (restoreSettings); simulate a live change too.
  gFront.liveBri = 90;
  nativeBrightnessCommand("F10");
  CHECK(corrected(FastLED[0], 10));
  CHECK(corrected(FastLED[1], 128));                           // rear untouched
  CHECK(gFront.calculateCalls == 1);                           // not raised again
  CHECK(gFront.liveBri == 90);

  nativeBrightnessCommand("R1");
  CHECK(corrected(FastLED[1], 1) && corrected(FastLED[0], 10));
  CHECK(gRear.calculateCalls == 1);
}

static void test_front_only_first_command_leaves_rear_stock() {
  powerOn(true);
  nativeBrightnessCommand("F255");
  CHECK(LogicEngineFLDDefault.fBri == 255);
  CHECK(LogicEngineRLDDefault.fBri == LogicEngineDefaults::REAR_BRI);
  CHECK(gRear.calculateCalls == 0 && FastLED[1].correctionCalls == 0);
  CHECK(corrected(FastLED[0], 255));
}

static void test_display_without_controller_is_ignored() {
  powerOn(false);
  nativeBrightnessCommand("200");
  CHECK(corrected(FastLED[0], 200));
  CHECK(LogicEngineRLDDefault.fBri == LogicEngineDefaults::REAR_BRI);   // rear not raised
  CHECK(gRear.calculateCalls == 0);
  nativeBrightnessCommand("R50");                                        // rear only: no-op
  CHECK(corrected(FastLED[0], 200) && FastLED[0].correctionCalls == 1);
}

static void test_rejected_tails_change_nothing() {
  powerOn(true);
  const char* const bad[] = {"", "0", "256", "f50", "F", " 1", "1000", "12a"};
  for (const char* tail : bad) nativeBrightnessCommand(tail);
  CHECK(FastLED[0].correctionCalls == 0 && FastLED[1].correctionCalls == 0);
  CHECK(LogicEngineFLDDefault.fBri == LogicEngineDefaults::FRONT_BRI);
  CHECK(LogicEngineRLDDefault.fBri == LogicEngineDefaults::REAR_BRI);
  CHECK(gFront.calculateCalls == 0 && gRear.calculateCalls == 0);
}

static void test_marcduino_action_text_and_body() {
  CHECK(strcmp(mockActionText_LEBrightness, "#LEBRI") == 0);
  powerOn(true);
  Marcduino::mockCommand() = "R77";             // what processCommand leaves after "#LEBRI"
  mockAction_LEBrightness();
  CHECK(corrected(FastLED[1], 77));
  CHECK(FastLED[0].correctionCalls == 0);
}

int main() {
  test_capture_keeps_one_controller_and_disables_dither();
  test_capture_ignores_zero_or_two_controllers();
  test_first_command_raises_once_then_only_corrects();
  test_front_only_first_command_leaves_rear_stock();
  test_display_without_controller_is_ignored();
  test_rejected_tails_change_nothing();
  test_marcduino_action_text_and_body();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
