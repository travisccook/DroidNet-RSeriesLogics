// src/contract/ContractLogics.h — RSeries Logic firmware layer for the
// Driveable-Animation Contract (Phase-1: live cues). ZERO edits to lib/Reeltwo.
//
// INCLUDE ORDER (see main.cpp ~line 441): this header is included AFTER ReelTwo /
// LogicEngine.h / FastLED and the global engine pointers `FLD`/`RLD` + the
// `FLD_/RLD_selectSequence` wrappers, and BEFORE `CustomLogicEffectSelector`.
// It relies on those being already declared: CRGB, millis(), byte,
// LogicEngineRenderer, FLD/RLD (LogicEngineRenderer*), FLD_/RLD_selectSequence.
//
// DESIGN (verified against LogicEngine.h on 2026-07-12): one fork effect fn,
// `ContractRender`, is appended to `sCustomLogicEffects[]` at id 106 and renders a
// per-unit contract state every ~10 ms frame (LogicEngine.h:818). It MUST return
// true or the engine reverts to NORMAL (LogicEngine.h:820-830); it is selected with
// numSeconds=0 so it holds. Arbitrary color is written raw via `setPixelRGB`
// (LogicEngine.h:1383), which bypasses the palette/`fBri` path — so brightness is
// applied here per-unit. Verb P is an OVERLAY flag on the same effect (no effect
// switching); rainbow/native looks pass through to native ids and a pulse over them
// restores the native id on expiry.
#pragma once
#include "contract_core.h"

static const uint8_t  SAFE_MAX_BRIGHTNESS = 200;   // heat/USB clamp (fork spec §11)
static const uint32_t STROBE_MIN_STATE_MS = 160;   // >=160 ms/state => <= ~3 Hz (fork spec §11)
static const byte     FX_CONTRACT   = 106;         // single fork effect id (appended in main.cpp)
static const byte     NATIVE_RAINBOW = 10;         // native RAINBOW seq
static const byte     NATIVE_LIGHTSOUT = 14;       // native blackout/de-energize
static const byte     NATIVE_NORMAL = 0;           // native idle shimmer

// Per-unit contract state. Index 0 = FLD (engine id 1), 1 = RLD (engine id 2).
struct ContractUnit {
  ContractEffect effect = CE_SOLID;
  RGB      color{0, 0, 0};
  uint8_t  speed = 128;
  uint8_t  bright = SAFE_MAX_BRIGHTNESS;
  uint8_t  beatMod = 0;                 // Plan 2 (beat-clock)
  uint32_t durMs = 0, startMs = 0;
  byte     baseSeq = FX_CONTRACT;       // seq shown as the base look (106, or a native id)
  bool     armed = false;
  // verb P (pulse) overlay
  bool     pulseActive = false;
  RGB      pulseColor{255, 255, 255};
  uint32_t pulseDurMs = 0, pulseStartMs = 0;
};
static ContractUnit gContract[2];
static bool gShowMode = false;

// ---------------------------------------------------------------- helpers ----
static inline uint8_t _clampBright(int v) {
  return (uint8_t)(v < 0 ? 0 : (v > SAFE_MAX_BRIGHTNESS ? SAFE_MAX_BRIGHTNESS : v));
}
static inline CRGB _scaled(const RGB& c, uint8_t bri) {   // apply brightness ourselves (setPixelRGB bypasses fBri)
  return CRGB((uint16_t)c.r * bri / 255, (uint16_t)c.g * bri / 255, (uint16_t)c.b * bri / 255);
}
static inline void _fill(LogicEngineRenderer& r, const CRGB& c) {
  for (int y = 0; y < r.height(); y++)
    for (int x = 0; x < r.width(); x++)
      r.setPixelRGB(x, y, c);
}
// verb-dispatch context (outside an effect fn): act on a specific engine by unit index.
static inline void _select(uint8_t u, byte seq) { if (u == 0) FLD_selectSequence(seq); else RLD_selectSequence(seq); }
static inline void _reset(uint8_t u)            { if (u == 0) { if (FLD) FLD->resetEffect(); } else { if (RLD) RLD->resetEffect(); } }
static inline void _bright(uint8_t u, byte b)   { if (u == 0) { if (FLD) FLD->setBrightness(b); } else { if (RLD) RLD->setBrightness(b); } }

// ---- the single fork effect fn: renders unit u's contract state each frame ----
inline bool ContractRender(LogicEngineRenderer& r) {
  uint8_t u = (r.getID() == 1) ? 0 : 1;
  ContractUnit& g = gContract[u];
  uint32_t now = millis();

  // verb-P overlay: layer a solid pulse over the base, auto-restore on expiry.
  if (g.pulseActive) {
    if (now - g.pulseStartMs < g.pulseDurMs) {
      _fill(r, _scaled(g.pulseColor, g.bright));
      return true;
    }
    g.pulseActive = false;                                  // pulse done -> restore base
    if (g.baseSeq != FX_CONTRACT) { r.selectSequence(g.baseSeq); return true; }  // hand back to a native base
  }

  switch (g.effect) {
    case CE_OFF:
      _fill(r, CRGB(0, 0, 0));
      break;
    case CE_FLASH: {
      uint32_t half = STROBE_MIN_STATE_MS + (uint32_t)(255 - g.speed);   // >=160 ms/state (strobe cap)
      bool on = ((now - g.startMs) % (2 * half)) < half;
      _fill(r, on ? _scaled(g.color, g.bright) : CRGB(0, 0, 0));
      break;
    }
    case CE_SOLID:
    default:
      _fill(r, _scaled(g.color, g.bright));
      break;
  }
  return true;   // MUST return true (LogicEngine.h:820 reverts to NORMAL on false)
}

// ------------------------------------------------------------ verb dispatch ----
inline void applyContractToUnit(uint8_t u, const ParsedContract& p) {
  ContractUnit& g = gContract[u];
  const ContractParams& pr = p.params;
  uint32_t now = millis();
  switch (p.verb) {
    case CV_ANIMATE: {
      if (pr.hasAt) return;                                 // Plan 2 schedules; Phase-1 ignores at=
      if (pr.hasColor)   g.color   = pr.color;
      if (pr.hasSpeed)   g.speed   = pr.speed;
      if (pr.hasBright)  g.bright  = _clampBright(pr.bright);
      if (pr.hasBeatMod) g.beatMod = pr.beatMod;
      if (pr.hasDur)     g.durMs   = pr.durMs;
      g.startMs = now; g.armed = true; g.pulseActive = false;
      if (pr.hasEffect) {
        g.effect = pr.effect;
        if (pr.effect == CE_SOLID || pr.effect == CE_FLASH || pr.effect == CE_OFF) {
          g.baseSeq = FX_CONTRACT; _select(u, FX_CONTRACT); _reset(u);
        } else if (pr.effect == CE_RAINBOW) {
          g.baseSeq = NATIVE_RAINBOW; _select(u, NATIVE_RAINBOW);        // native rainbow (ignores color, per contract)
        } else if (pr.effect == CE_NATIVE && pr.nativeCode >= 0) {
          g.baseSeq = (byte)pr.nativeCode; _select(u, (byte)pr.nativeCode);
        } else {
          // scan/sparkle/meter/pulse core ids arrive in Plan 2 — render as solid for now
          g.effect = CE_SOLID; g.baseSeq = FX_CONTRACT; _select(u, FX_CONTRACT); _reset(u);
        }
      } else if (g.baseSeq == FX_CONTRACT) {
        _reset(u);                                          // i omitted: patch params, re-render current look
      }
      break;
    }
    case CV_PULSE: {
      // strobe cool-down: drop pulses arriving faster than ~3 Hz
      if (!g.pulseActive && g.pulseStartMs != 0 && (now - g.pulseStartMs) < 2 * STROBE_MIN_STATE_MS) return;
      g.pulseColor  = pr.hasColor ? pr.color : RGB{255, 255, 255};
      g.pulseDurMs  = pr.hasDur ? pr.durMs : 120;
      if (pr.hasBright) g.bright = _clampBright(pr.bright);
      g.pulseStartMs = now;
      g.pulseActive  = true;
      _select(u, FX_CONTRACT); _reset(u);                   // ensure ContractRender active + retrigger
      break;
    }
    case CV_BRIGHT: {
      uint8_t b = _clampBright(pr.hasBright ? pr.bright : (pr.hasLevel ? pr.level : g.bright));
      g.bright = b; _bright(u, b);                          // volatile fBri too (native effects); no NVS
      break;
    }
    case CV_STOP:
      g.armed = false; g.pulseActive = false;
      _select(u, NATIVE_LIGHTSOUT);                         // blackout + de-energize
      break;
    case CV_MODE:
      if (pr.mode == 's')      { gShowMode = true;  g.armed = false; g.pulseActive = false; _select(u, NATIVE_LIGHTSOUT); }
      else if (pr.mode == 'i') { gShowMode = false; g.armed = false; _select(u, NATIVE_NORMAL); }
      break;
    default: break;                                         // C / L / Q -> Plan 2
  }
}

// Fan a parsed command to the addressed unit(s) per class/unit (fork spec §4).
inline void applyContract(const ParsedContract& p) {
  if (p.cls != 'L' && p.cls != '*') return;                 // not a Logic board
  if (p.unit == 'F' || p.unit == '*') applyContractToUnit(0, p);
  if (p.unit == 'R' || p.unit == '*') applyContractToUnit(1, p);
  // unit 'T' (HP) => neither
}

// The single fork command entry point. '!' is collision-free vs every native prefix
// (~RT / @LE / @AP / @<digit> / #LE). Marcduino::getCommand() = bytes after '!'.
MARCDUINO_ACTION(Contract, !, ({
  ParsedContract p;
  if (contractParse(Marcduino::getCommand(), p)) applyContract(p);
}))

// Boot wiring. Phase-1: nothing. Plan 2 wires FLD_/RLD_setPeakValueProvider for verb L.
inline void contractSetup() { }
