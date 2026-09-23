// DroidNet native brightness: the pure parser for "#LEBRI[F|R]<level>".
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// No Arduino, FastLED or Reeltwo includes: the host suite compiles this file with clang
// (test/host/test_native_brightness.cpp, fuzz_native_brightness.cpp, prefix_guard.cpp).
// The firmware action in NativeBrightness.h hands it the text AFTER "#LEBRI".
//
// Grammar:
//   tail  := [unit] level END
//   unit  := 'F' (front) | 'R' (rear)            uppercase only; no unit = both displays
//   level := 1 to 3 ASCII digits worth 1..255    leading zeros allowed ("007" is 7)
//   END   := the NUL. MarcduinoSerial ends a line at CR and does not trim spaces.
//
// Anything else returns false and leaves unitMask and level untouched. There is no clamp
// and no floor: 0, 256-999, four or more digits, lowercase units, signs, spaces and
// trailing text are all ignored, so 0 can never reach the LEDs.
#pragma once
#include <stdint.h>

static constexpr uint8_t NATIVE_BRIGHTNESS_FRONT = 0x01;
static constexpr uint8_t NATIVE_BRIGHTNESS_REAR = 0x02;
static constexpr uint8_t NATIVE_BRIGHTNESS_BOTH = NATIVE_BRIGHTNESS_FRONT | NATIVE_BRIGHTNESS_REAR;
static constexpr uint8_t NATIVE_BRIGHTNESS_MAX_DIGITS = 3;

static inline bool nativeBrightnessParse(const char* tail, uint8_t& unitMask, uint8_t& level)
{
    if (tail == nullptr)
    {
        return false;
    }
    const char* cur = tail;
    uint8_t mask = NATIVE_BRIGHTNESS_BOTH;
    if (*cur == 'F')
    {
        mask = NATIVE_BRIGHTNESS_FRONT;
        ++cur;
    }
    else if (*cur == 'R')
    {
        mask = NATIVE_BRIGHTNESS_REAR;
        ++cur;
    }
    uint16_t value = 0;     // at most 3 digits, so at most 999
    uint8_t digits = 0;
    while (*cur >= '0' && *cur <= '9')
    {
        if (digits == NATIVE_BRIGHTNESS_MAX_DIGITS)
        {
            return false;
        }
        value = static_cast<uint16_t>((value * 10U) + static_cast<uint16_t>(*cur - '0'));
        ++digits;
        ++cur;
    }
    if (digits == 0 || *cur != '\0' || value < 1U || value > 255U)
    {
        return false;
    }
    unitMask = mask;
    level = static_cast<uint8_t>(value);
    return true;
}
