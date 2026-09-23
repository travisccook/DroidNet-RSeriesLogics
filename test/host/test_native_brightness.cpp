// Host unit tests for src/native/native_brightness_parse.h: the "#LEBRI" tail parser.
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// The parser sees the text AFTER "#LEBRI" (Marcduino::getCommand()). The approved rule is
// strict: an optional uppercase F/R, then 1-3 digits worth 1..255, then the end of the string.
// Everything else is IGNORED -- no clamp, no floor, and the outputs are left untouched.
#include "../../src/native/native_brightness_parse.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static constexpr uint8_t SENTINEL = 0xA5;

struct Accept { const char* tail; uint8_t mask; uint8_t level; };

// Every accepted boundary. F = 0x01 (front), R = 0x02 (rear), no unit = 0x03 (both).
static const Accept kAccept[] = {
  {"1",    0x03,   1}, {"128",  0x03, 128}, {"255",  0x03, 255},
  {"F1",   0x01,   1}, {"F255", 0x01, 255}, {"F128", 0x01, 128},
  {"R1",   0x02,   1}, {"R255", 0x02, 255}, {"R128", 0x02, 128},
  {"9",    0x03,   9}, {"99",   0x03,  99}, {"100",  0x03, 100},
  {"007",  0x03,   7}, {"01",   0x03,   1}, {"001",  0x03,   1},
  {"F007", 0x01,   7}, {"R09",  0x02,   9}, {"254",  0x03, 254},
};

// Every rejected shape the spec names, plus the near misses around each boundary.
static const char* const kReject[] = {
  "",                                   // bare "#LEBRI"
  "0", "00", "000", "F0", "R000",       // zero is never applied
  "256", "999", "F256", "R300",         // above 255: ignored, not clamped
  "1000", "0001", "0128", "F1000",      // four or more digits
  "f128", "r128", "f", "r",             // lowercase units
  "X128", "B128", "A1", "FR1", "RF1",   // other letters, two units
  "F", "R",                             // unit with no level
  " 128", "128 ", "F 128", " F128",     // spaces (MarcduinoSerial does not trim)
  "-5", "+5", "F-1", "F+1",             // signs
  "12a", "128\r", "128\n", "128|5",     // trailing text, a stray terminator, a duration
  "1.5", "12,8", "F12x",
};

static void test_accepted_table() {
  for (const Accept& a : kAccept) {
    uint8_t mask = SENTINEL, level = SENTINEL;
    const bool ok = nativeBrightnessParse(a.tail, mask, level);
    CHECK(ok);
    CHECK(mask == a.mask);
    CHECK(level == a.level);
    if (!ok || mask != a.mask || level != a.level)
      printf("      tail \"%s\": ok=%d mask=0x%02x level=%u\n", a.tail, ok, mask, level);
  }
}

static void test_rejected_table_leaves_outputs_untouched() {
  for (const char* tail : kReject) {
    uint8_t mask = SENTINEL, level = SENTINEL;
    const bool ok = nativeBrightnessParse(tail, mask, level);
    CHECK(!ok);
    CHECK(mask == SENTINEL && level == SENTINEL);
    if (ok) printf("      tail \"%s\" was accepted (mask=0x%02x level=%u)\n", tail, mask, level);
  }
}

static void test_null_tail_is_rejected() {
  uint8_t mask = SENTINEL, level = SENTINEL;
  CHECK(!nativeBrightnessParse(nullptr, mask, level));
  CHECK(mask == SENTINEL && level == SENTINEL);
}

// Every value 0..999 with every unit prefix: exactly 1..255 is accepted, at that value.
static void test_every_three_digit_value() {
  const char* const units[] = {"", "F", "R"};
  const uint8_t masks[] = {0x03, 0x01, 0x02};
  for (int u = 0; u < 3; u++) {
    for (int v = 0; v <= 999; v++) {
      char tail[8];
      snprintf(tail, sizeof tail, "%s%d", units[u], v);
      uint8_t mask = SENTINEL, level = SENTINEL;
      const bool ok = nativeBrightnessParse(tail, mask, level);
      const bool want = (v >= 1 && v <= 255);
      CHECK(ok == want);
      if (want) CHECK(mask == masks[u] && level == v);
    }
  }
}

// The three examples the library ships for rseries.cfg.brightness, minus the "#LEBRI" prefix.
static void test_library_examples() {
  uint8_t mask = 0, level = 0;
  CHECK(nativeBrightnessParse("128", mask, level) && mask == 0x03 && level == 128);   // #LEBRI128
  CHECK(nativeBrightnessParse("F255", mask, level) && mask == 0x01 && level == 255);  // #LEBRIF255
  CHECK(nativeBrightnessParse("R1", mask, level) && mask == 0x02 && level == 1);      // #LEBRIR1
}

static void test_mask_constants() {
  CHECK(NATIVE_BRIGHTNESS_FRONT == 0x01);
  CHECK(NATIVE_BRIGHTNESS_REAR == 0x02);
  CHECK(NATIVE_BRIGHTNESS_BOTH == 0x03);
}

int main() {
  test_accepted_table();
  test_rejected_table_leaves_outputs_untouched();
  test_null_tail_is_rejected();
  test_every_three_digit_value();
  test_library_examples();
  test_mask_constants();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
