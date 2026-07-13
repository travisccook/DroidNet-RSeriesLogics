// src/contract/ContractLogics.h — RSeries Logic firmware layer for the
// Driveable-Animation Contract. Phase-1 (live cues) + Phase-2 (beat-clock +
// autonomous board-side score). ZERO edits to lib/Reeltwo.
//
// INCLUDE ORDER (main.cpp ~line 441): included AFTER ReelTwo / LogicEngine.h /
// FastLED and the global engine pointers FLD/RLD + FLD_/RLD_selectSequence, and
// BEFORE CustomLogicEffectSelector. Relies on: CRGB, CHSV, millis(), byte,
// LogicEngineRenderer, FLD/RLD (LogicEngineRenderer*), FLD_/RLD_selectSequence.
//
// DESIGN (verified vs LogicEngine.h 2026-07-12): ONE fork effect `ContractRender`
// at id 106 renders a per-unit contract state every ~10 ms frame (LogicEngine.h:818)
// and MUST return true (LogicEngine.h:820-830 reverts to NORMAL on false); selected
// numSeconds=0 to hold. setPixelRGB (:1383) bypasses fBri, so brightness + the
// beat-pump envelope are applied here. Verb P is a layered overlay (no effect
// switch). The Phase-2 score advances INSIDE ContractRender off the shared beat-clock.
#pragma once
#include "contract_core.h"

static const uint8_t  SAFE_MAX_BRIGHTNESS = 200;   // heat/USB clamp (fork spec §11)
static const uint32_t STROBE_MIN_STATE_MS = 160;   // >=160 ms/state => <= ~3 Hz (fork spec §11)
static const byte     FX_CONTRACT   = 106;         // single fork effect id (appended in main.cpp)
static const byte     NATIVE_LIGHTSOUT = 14;       // native blackout / de-energize
static const byte     NATIVE_NORMAL = 0;           // native idle shimmer
static const int      SCORE_MAX = 16;              // sections per unit (a song has ~8)

// Per-unit contract state. Index 0 = FLD (engine id 1), 1 = RLD (engine id 2).
struct ContractUnit {
  ContractEffect effect = CE_SOLID;
  RGB      color{0, 0, 0};
  uint8_t  speed = 128;
  uint8_t  bright = SAFE_MAX_BRIGHTNESS;
  uint8_t  beatMod = 0;
  uint8_t  accentMode = 0;
  uint32_t startMs = 0;
  byte     baseSeq = FX_CONTRACT;       // base look seq (106, or a native id for native:<code>)
  bool     armed = false;
  // verb P (pulse) overlay
  bool     pulseActive = false;
  RGB      pulseColor{255, 255, 255};
  uint32_t pulseDurMs = 0, pulseStartMs = 0;
  // Phase-2 score cursor
  int      curSection = -1;
  int32_t  sectionStart = 0, sectionEnd = 0;
};
static ContractUnit gContract[2];
static bool      gShowMode = false;
static BeatClock gBeat;                 // one shared beat-clock (verb C, !**C)
static uint8_t   gLevel = 0;            // verb L energy 0..255 (drives CE_METER)
static ScoreEntry gScore[2][SCORE_MAX];
static int       gScoreLen[2] = {0, 0};

// ---------------------------------------------------------------- helpers ----
static inline uint8_t _clampBright(int v) {
  return (uint8_t)(v < 0 ? 0 : (v > SAFE_MAX_BRIGHTNESS ? SAFE_MAX_BRIGHTNESS : v));
}
static inline CRGB _scaled(const RGB& c, uint8_t bri) {   // apply brightness (setPixelRGB bypasses fBri)
  return CRGB((uint16_t)c.r * bri / 255, (uint16_t)c.g * bri / 255, (uint16_t)c.b * bri / 255);
}
static inline void _fill(LogicEngineRenderer& r, const CRGB& c) {
  for (int y = 0; y < r.height(); y++)
    for (int x = 0; x < r.width(); x++)
      r.setPixelRGB(x, y, c);
}
static inline uint32_t _hash3(int x, int y, uint32_t t) {
  uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ t * 83492791u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}
// verb-dispatch context (outside an effect fn): act on one engine by unit index.
static inline void _select(uint8_t u, byte seq) { if (u == 0) FLD_selectSequence(seq); else RLD_selectSequence(seq); }
static inline void _reset(uint8_t u)            { if (u == 0) { if (FLD) FLD->resetEffect(); } else { if (RLD) RLD->resetEffect(); } }
static inline void _bright(uint8_t u, byte b)   { if (u == 0) { if (FLD) FLD->setBrightness(b); } else { if (RLD) RLD->setBrightness(b); } }

// beat-modulated brightness for sustained looks (solid/rainbow/scan/sparkle/meter).
// Level math lives in contract_core's envBright() — the SHARED envelope all three
// boards render through (b= is the ceiling, m= is the dip depth). Do not reintroduce
// a board-local floor here: it desyncs the Logics from the PSIs/HPs on every cue.
static inline uint8_t _beatBright(const ContractUnit& g, const BeatPos& bp) {
  if (g.accentMode == 0 || g.beatMod == 0 || !gBeat.running) return g.bright;
  float prog = 0.0f;
  if (g.sectionEnd > g.sectionStart)
    prog = (float)(bp.beatIndex - g.sectionStart) / (float)(g.sectionEnd - g.sectionStart);
  uint8_t amt = beatAccentAmount(g.accentMode, bp, g.beatMod, prog);
  return envBright(g.bright, g.beatMod, amt);
}

// ---- the single fork effect fn: renders unit u's contract state each frame ----
inline bool ContractRender(LogicEngineRenderer& r) {
  uint8_t u = (r.getID() == 1) ? 0 : 1;
  ContractUnit& g = gContract[u];
  uint32_t now = millis();
  BeatPos bp = beatPosAt(gBeat, now);
  // NB: the Phase-2 score is advanced in contractTick() from mainLoop (not here),
  // so section switching keeps running even while a NATIVE section look (which is
  // not rendered by ContractRender) is active.

  // verb-P overlay: layer a solid pulse over the base, auto-restore on expiry.
  if (g.pulseActive) {
    if (now - g.pulseStartMs < g.pulseDurMs) {
      _fill(r, _scaled(g.pulseColor, g.bright));
      return true;
    }
    g.pulseActive = false;
    if (g.baseSeq != FX_CONTRACT) { r.selectSequence(g.baseSeq); return true; }  // restore a native base
  }

  uint8_t bb = _beatBright(g, bp);
  int W = r.width(), H = r.height();

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
    case CE_RAINBOW: {
      uint32_t rate = 20u + (uint32_t)(255 - g.speed) / 4u;              // ms per hue step
      uint8_t hbase = (uint8_t)((now / (rate ? rate : 1)) & 0xFF);
      int step = (W > 0) ? (255 / W) : 8;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, CRGB(CHSV((uint8_t)(hbase + x * step), 255, bb)));
      break;
    }
    case CE_SCAN: {
      uint32_t period = 60u * (uint32_t)(256 - g.speed) + 200u;
      int pos = (W > 0) ? (int)(((now - g.startMs) % period) * (uint32_t)W / period) : 0;
      CRGB c = _scaled(g.color, bb);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, (x == pos) ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_SPARKLE: {
      uint32_t blk = now / (uint32_t)(80 + (255 - g.speed) / 2);
      uint32_t density = 20u + (uint32_t)g.speed / 4u;                   // ~20..84 % lit
      CRGB c = _scaled(g.color, bb);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, (_hash3(x, y, blk) % 100u) < density ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_METER: {
      int lit = (H * (int)gLevel) / 255;                                 // fill from bottom
      CRGB c = _scaled(g.color.r || g.color.g || g.color.b ? g.color : RGB{0, 255, 0}, bb);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, ((H - 1 - y) < lit) ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_COMET: {
      int head = fxHead(now - g.startMs, g.speed, W);
      for (int x = 0; x < W; x++) {
        uint8_t cb = fxCometBright(x, head, W);
        uint8_t v = (uint8_t)(((uint16_t)bb * cb) / 255);
        CRGB c = _scaled(g.color, v);
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, c);
      }
      break;
    }
    case CE_CHASE: {
      uint32_t el = now - g.startMs;
      CRGB on = _scaled(g.color, bb), off(0, 0, 0);
      for (int x = 0; x < W; x++) {
        CRGB c = fxChaseLit(x, el, g.speed) ? on : off;
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, c);
      }
      break;
    }
    case CE_WIPE: {
      uint32_t el = now - g.startMs;
      CRGB on = _scaled(g.color, bb), off(0, 0, 0);
      for (int x = 0; x < W; x++) {
        CRGB c = fxWipeLit(x, el, g.speed, W) ? on : off;
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, c);
      }
      break;
    }
    case CE_GRADIENT: {
      uint32_t el = now - g.startMs;
      for (int x = 0; x < W; x++) {
        uint8_t hue = fxGradientHue(x, W, 0, el, g.speed);   // base hue 0, color-independent (like rainbow)
        RGB c = fxHsv2rgb(hue, 255, bb);
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, CRGB(c.r, c.g, c.b));
      }
      break;
    }
    case CE_COLORCYCLE: {
      RGB c = fxHsv2rgb(fxCycleHue(0, now - g.startMs, g.speed), 255, bb);
      _fill(r, CRGB(c.r, c.g, c.b));
      break;
    }
    case CE_TWINKLE: {
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          uint8_t tb = fxTwinkleBright(y * W + x, now, g.speed);
          uint8_t v = (uint8_t)(((uint16_t)bb * tb) / 255);
          r.setPixelRGB(x, y, _scaled(g.color, v));
        }
      break;
    }
    case CE_SOLID:
    default:
      _fill(r, _scaled(g.color, bb));
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
      if (pr.hasAt) {                             // Phase-2: schedule this look, do not apply now
        ScoreEntry e;
        e.atBeat = pr.atBeat;
        e.effect = pr.hasEffect ? pr.effect : CE_SOLID;
        e.nativeCode = pr.hasEffect ? pr.nativeCode : -1;
        e.color  = pr.hasColor ? pr.color : RGB{0, 0, 0};
        e.speed  = pr.hasSpeed ? pr.speed : 128;
        e.beatMod = pr.hasBeatMod ? pr.beatMod : 0;
        e.accentMode = pr.hasAm ? pr.accentMode : 0;
        gScoreLen[u] = scoreInsert(gScore[u], gScoreLen[u], SCORE_MAX, e);
        g.curSection = -1;                        // force re-eval on the next frame
        _select(u, FX_CONTRACT); _reset(u);       // ensure ContractRender drives the score
        break;
      }
      if (pr.hasColor)   g.color   = pr.color;
      if (pr.hasSpeed)   g.speed   = pr.speed;
      if (pr.hasBright)  g.bright  = _clampBright(pr.bright);
      if (pr.hasBeatMod) g.beatMod = pr.beatMod;
      if (pr.hasAm)      g.accentMode = pr.accentMode;
      g.startMs = now; g.armed = true; g.pulseActive = false;
      if (pr.hasEffect) {
        g.effect = pr.effect;
        if (pr.effect == CE_NATIVE && pr.nativeCode >= 0) {
          g.baseSeq = (byte)pr.nativeCode; _select(u, (byte)pr.nativeCode);   // escape hatch -> native
        } else {
          g.baseSeq = FX_CONTRACT; _select(u, FX_CONTRACT); _reset(u);        // all core looks render here
        }
      } else if (g.baseSeq == FX_CONTRACT) {
        _reset(u);                                // i omitted: patch params, re-render current look
      }
      break;
    }
    case CV_PULSE: {
      if (!g.pulseActive && g.pulseStartMs != 0 && (now - g.pulseStartMs) < 2 * STROBE_MIN_STATE_MS) return;  // strobe cool-down
      g.pulseColor  = pr.hasColor ? pr.color : RGB{255, 255, 255};
      g.pulseDurMs  = pr.hasDur ? pr.durMs : 120;
      if (pr.hasBright) g.bright = _clampBright(pr.bright);
      g.pulseStartMs = now;
      g.pulseActive  = true;
      _select(u, FX_CONTRACT); _reset(u);         // ensure ContractRender active + retrigger
      break;
    }
    case CV_CLOCK:
      beatClockSeed(gBeat, pr, now);              // one shared clock; !**C fans to both units
      break;
    case CV_LEVEL:
      if (pr.hasLevel) gLevel = pr.level;         // drives CE_METER (contract meter reads gLevel)
      break;
    case CV_BRIGHT: {
      uint8_t b = _clampBright(pr.hasBright ? pr.bright : (pr.hasLevel ? pr.level : g.bright));
      g.bright = b; _bright(u, b);                // volatile fBri too; no NVS
      break;
    }
    case CV_STOP:
      g.armed = false; g.pulseActive = false; gScoreLen[u] = 0; g.curSection = -1;
      _select(u, NATIVE_LIGHTSOUT);               // blackout + de-energize
      break;
    case CV_MODE:
      if (pr.mode == 's')      { gShowMode = true;  g.armed = false; g.pulseActive = false; _select(u, NATIVE_LIGHTSOUT); }
      else if (pr.mode == 'i') { gShowMode = false; g.armed = false; gScoreLen[u] = 0; g.curSection = -1; _select(u, NATIVE_NORMAL); }
      break;
    default: break;                               // Q -> optional, not in V1
  }
}

// Fan a parsed command to the addressed unit(s) per class/unit (fork spec §4).
inline void applyContract(const ParsedContract& p) {
  if (p.cls != 'L' && p.cls != '*') return;       // not a Logic board
  if (p.unit == 'F' || p.unit == '*') applyContractToUnit(0, p);
  if (p.unit == 'R' || p.unit == '*') applyContractToUnit(1, p);
  // unit 'T' (HP) => neither
}

// Phase-2 score sequencer — runs from mainLoop() every pass (NOT from an effect fn),
// so it keeps switching sections even while a native/iconic section look is active.
// Switching to a native look leaves ContractRender (fine); switching to a contract
// look re-selects id 106; the next section switch fires here regardless.
inline void contractTick() {
  if (!gBeat.running) return;
  BeatPos bp = beatPosAt(gBeat, millis());
  for (uint8_t u = 0; u < 2; u++) {
    if (gScoreLen[u] == 0) continue;
    int idx = scoreActiveIndex(gScore[u], gScoreLen[u], bp.beatIndex);
    if (idx < 0 || idx == gContract[u].curSection) continue;
    ContractUnit& g = gContract[u];
    const ScoreEntry& s = gScore[u][idx];
    g.effect = s.effect; g.color = s.color; g.speed = s.speed;
    g.beatMod = s.beatMod; g.accentMode = s.accentMode;
    g.curSection = idx; g.startMs = millis(); g.sectionStart = s.atBeat;
    g.sectionEnd = (idx + 1 < gScoreLen[u]) ? gScore[u][idx + 1].atBeat : (s.atBeat + 9999);
    g.pulseActive = false;
    if (s.effect == CE_NATIVE && s.nativeCode >= 0) {
      g.baseSeq = (byte)s.nativeCode; _select(u, (byte)s.nativeCode);   // iconic native section look
    } else {
      g.baseSeq = FX_CONTRACT; _select(u, FX_CONTRACT); _reset(u);
    }
  }
}

// The single fork command entry point. '!' is collision-free vs every native prefix.
MARCDUINO_ACTION(Contract, !, ({
  ParsedContract p;
  if (contractParse(Marcduino::getCommand(), p)) applyContract(p);
}))

inline void contractSetup() { }   // (native mic-effect PeakValueProvider wiring is an optional follow-up)
