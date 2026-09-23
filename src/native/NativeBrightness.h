// DroidNet native brightness: the "#LEBRI[F|R]<1..255>" Marcduino action. RAM only.
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// Included by src/main.cpp right after MarcduinoLogics.h, so FLD, RLD, FastLED, the
// LogicEngineFLDDefault / LogicEngineRLDDefault objects and MARCDUINO_ACTION are in scope.
// It is not part of the Driveable-Animation Contract (src/contract/) and does not touch it.
//
// How it dims (the "hybrid output dimmer"):
//   * setup() captures each display's FastLED controller: nativeBrightnessCapture() compares
//     FastLED.count() before and after FLD_init() / RLD_init(). Only a display whose init added
//     exactly ONE controller is kept, and that controller's temporal dither is switched off
//     (it can shimmer at low correction values). Any other count keeps nothing, and the
//     command then ignores that display completely.
//   * The first accepted command for a display raises its brightness baseline to 255: the
//     RAM default object's fBri (every effect change copies it back over the live settings,
//     LogicEngine.h restoreSettings), the live fBri, and the colour table. Once per display.
//   * Every accepted command sets that display's controller correction to (L, L, L). FastLED
//     applies it at output time, so it scales every look (palette, text, custom effects
//     100-105 and the contract looks) on the next frame, and 255 is the hardware maximum.
//   * Nothing is written to Preferences/NVS. After a power-on the look is stock again.
//
// The action runs deferred, on the player, in the same event task that renders and calls
// FastLED.show(). Marcduino::getCommand() points into the receiver's line buffer, so it is
// read here, when the action runs, and never cached.
#pragma once
#include "native_brightness_parse.h"

static constexpr uint8_t NATIVE_BRIGHTNESS_UNIT_FRONT = 0;
static constexpr uint8_t NATIVE_BRIGHTNESS_UNIT_REAR = 1;

static CLEDController* sNativeBrightnessCtl[2] = { nullptr, nullptr };
static bool sNativeBrightnessRaised[2] = { false, false };

// setup(): call right after a display's init, with FastLED.count() from just before it.
static inline void nativeBrightnessCapture(uint8_t unit, int countBefore)
{
    CLEDController* captured = nullptr;
    if (FastLED.count() - countBefore == 1)
    {
        captured = &FastLED[countBefore];
        captured->setDither(DISABLE_DITHER);
    }
    sNativeBrightnessCtl[unit] = captured;
}

static inline void nativeBrightnessApply(uint8_t unit, uint8_t level)
{
    CLEDController* controller = sNativeBrightnessCtl[unit];
    LogicEngineRenderer* renderer = (unit == NATIVE_BRIGHTNESS_UNIT_FRONT) ? FLD : RLD;
    if (controller == nullptr || renderer == nullptr)
    {
        return;
    }
    if (!sNativeBrightnessRaised[unit])
    {
        LogicEngineSettings& defaults =
            (unit == NATIVE_BRIGHTNESS_UNIT_FRONT) ? LogicEngineFLDDefault : LogicEngineRLDDefault;
        defaults.fBri = LogicEngineDefaults::MAX_BRIGHTNESS;
        renderer->setBrightness(LogicEngineDefaults::MAX_BRIGHTNESS);
        renderer->calculateAllColors();
        sNativeBrightnessRaised[unit] = true;
    }
    controller->setCorrection(CRGB(level, level, level));
}

// The whole command: parse the text after "#LEBRI" and apply it, or ignore it silently.
static inline void nativeBrightnessCommand(const char* tail)
{
    uint8_t mask = 0;
    uint8_t level = 0;
    if (!nativeBrightnessParse(tail, mask, level))
    {
        return;
    }
    if ((mask & NATIVE_BRIGHTNESS_FRONT) != 0)
    {
        nativeBrightnessApply(NATIVE_BRIGHTNESS_UNIT_FRONT, level);
    }
    if ((mask & NATIVE_BRIGHTNESS_REAR) != 0)
    {
        nativeBrightnessApply(NATIVE_BRIGHTNESS_UNIT_REAR, level);
    }
}

MARCDUINO_ACTION(LEBrightness, #LEBRI, ({
    nativeBrightnessCommand(Marcduino::getCommand());
}))
