// Part of the DroidNet Driveable-Animation Contract — an additive layer added to a
// fork of reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
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

static const byte     FX_CONTRACT   = 106;         // single fork effect id (appended in main.cpp)
static const byte     NATIVE_LIGHTSOUT = 14;       // native blackout / de-energize
static const byte     NATIVE_NORMAL = 0;           // native idle shimmer
static const uint8_t  SCORE_MAX = 32;              // sections per unit (v1.3 section 6)

// Per-unit contract state. Index 0 = FLD (engine id 1), 1 = RLD (engine id 2). Everything a show
// owns is in the shared ContractUnitState, so contractShowReset() cannot miss a field.
static ContractUnitState gContract[2];
static BeatClock  gBeat;                 // one shared beat-clock (verb C)
static uint8_t    gLevel = 0;            // verb L energy 0..255 (drives CE_METER)
static ScoreEntry gScore[2][SCORE_MAX];
static uint32_t   gLastLineMs = 0;       // L2: last well-formed contract line addressed to this board
// S6 / F7: verb B and b= reach native looks through the RAM default object, which the engine
// copies back on every effect change (LE:807, :1830). The owner's value is put back at a reset.
static bool    gBriTouched[2] = { false, false };
static uint8_t gBriSaved[2]   = { 0, 0 };
static uint8_t gBriWritten[2] = { 0, 0 };
// Section 10: one pending Q reply, written by mainLoop() between WDP announces.
static char    gQueryLine[CONTRACT_QUERY_MAX];
static uint8_t gQueryLen = 0;

// ---------------------------------------------------------------- helpers ----
static inline LogicEngineSettings& _defaults(uint8_t u) { return u == 0 ? LogicEngineFLDDefault : LogicEngineRLDDefault; }
static inline LogicEngineRenderer* _engine(uint8_t u)   { return u == 0 ? FLD : RLD; }
// S6: the owner's brightness. Never a compile-time maximum.
static inline uint8_t _ownerBri(uint8_t u) { return gBriTouched[u] ? gBriSaved[u] : _defaults(u).fBri; }
static inline uint8_t _clampBright(const ContractUnitState& g, uint8_t v) { return v > g.ceil ? g.ceil : v; }
static inline void _engineBri(uint8_t u, uint8_t b) {
  LogicEngineSettings& d = _defaults(u);
  if (!gBriTouched[u]) { gBriSaved[u] = d.fBri; gBriTouched[u] = true; }
  d.fBri = b; gBriWritten[u] = b;
  if (_engine(u)) { _engine(u)->setBrightness(b); _engine(u)->calculateAllColors(); }
}
static inline void _engineBriRestore(uint8_t u) {
  if (!gBriTouched[u]) return;
  if (_defaults(u).fBri == gBriWritten[u]) _defaults(u).fBri = gBriSaved[u];   // else #LEBRI or the web UI moved it
  gBriTouched[u] = false;
}
// S6: a unit the contract does not hold yet takes its ceiling from the owner's brightness now.
static inline void _arm(uint8_t u);
// S7: the native codes this board runs safely. 25 would index one past the engine's table (LE:2733).
static inline bool _nativeAllowed(int code) { return (code >= 0 && code <= 24) || (code >= 100 && code <= 105); }
static inline CRGB _scaled(const ContractRGB& c, uint8_t bri) {   // apply brightness (setPixelRGB bypasses fBri)
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
static inline void _select(uint8_t u, byte seq) { if (u == 0) FLD_selectSequence(seq); else RLD_selectSequence(seq); }
static inline void _reset(uint8_t u)            { if (_engine(u)) _engine(u)->resetEffect(); }
// Put the unit's base on the display (dispatch context; no overlay up).
static inline void _showBase(uint8_t u) {
  const ContractUnitState& g = gContract[u];
  if (g.base == CB_LOOK)        _select(u, FX_CONTRACT);
  else if (g.base == CB_NATIVE) _select(u, (byte)g.nativeCode);
  else if (g.base == CB_DARK)   _select(u, NATIVE_LIGHTSOUT);
  else                        { _select(u, NATIVE_NORMAL); _reset(u); }
}

static inline void _arm(uint8_t u) {
  ContractUnitState& g = gContract[u];
  if (g.base != CB_NONE) return;
  g.ceil = _ownerBri(u);
  if (g.bright > g.ceil) g.bright = g.ceil;
}

static inline uint8_t _beatBright(const ContractUnitState& g, const BeatPos& bp) {
  if (g.accentMode == 0 || g.beatMod == 0 || !gBeat.running) return g.bright;
  float prog = 0.0f;
  if (g.sectionEnd > g.sectionStart)
    prog = (float)(bp.beatIndex - g.sectionStart) / (float)(g.sectionEnd - g.sectionStart);
  uint8_t amt = beatAccentAmount(g.accentMode, bp, g.beatMod, prog, gBeat);
  return envBright(g.bright, g.beatMod, amt);
}

// THE single overlay-arming path (verb P and the scored accent). The strobe cap is unconditional.
static inline void _fireAccent(uint8_t u, ContractEffect fx, const ContractRGB& c, uint32_t durMs) {
  ContractUnitState& g = gContract[u];
  uint32_t now = millis();
  if (g.base == CB_NATIVE) return;      // F5 (R1): an accent never restarts a native sequence
  if (!strobeCoolDownExpired(g.pulseLastMs, now, 2 * STROBE_MIN_STATE_MS)) return;
  g.pulseFx = fx; g.pulseColor = c; g.pulseDurMs = accentDurClamp(durMs);
  g.pulseStartMs = now; g.pulseLastMs = now; g.pulseActive = true;
  _select(u, FX_CONTRACT);
}

// ---- the single fork effect fn: renders unit u's contract state each frame ----
inline bool ContractRender(LogicEngineRenderer& r) {
  uint8_t u = (r.getID() == 1) ? 0 : 1;
  ContractUnitState& g = gContract[u];
  uint32_t now = millis();
  BeatPos bp = beatPosAt(gBeat, now);
  // NB: the Phase-2 score is advanced in contractTick() from mainLoop (not here),
  // so section switching keeps running even while a NATIVE section look (which is
  // not rendered by ContractRender) is active.

  // v1.2 OVERLAY: the accent is an EFFECT SWAP, not a solid fill. The switch below runs
  // on a chosen SOURCE (eff/col/bri/t0) — the base look normally, the overlay while one is
  // active — so the accent reuses every existing renderer at zero flash cost. Its timeline
  // starts at pulseStartMs (the overlay owns its own t0; the base look's startMs is NEVER
  // touched, so an am=2 accent cannot restart the base look every beat), and it rides the
  // brightness CEILING rather than the pumped dip.
  //
  // BACKWARD COMPAT: with pulseActive == false this assigns eff=g.effect, col=g.color,
  // bri=bb, t0=g.startMs — the switch then evaluates exactly the v1.1 expressions. And a
  // verb P with no i= arms pulseFx = CE_SOLID, whose branch is _fill(_scaled(pulseColor,
  // g.bright)) — literally the v1.1 overlay fill.
  uint8_t bb = _beatBright(g, bp);
  ContractEffect eff = g.effect;
  ContractRGB      col = g.color;
  uint8_t  bri = bb;
  uint32_t t0  = g.startMs;
  if (g.pulseActive) {
    if (now - g.pulseStartMs < g.pulseDurMs) {
      eff = (g.pulseFx == CE_NONE) ? CE_SOLID : g.pulseFx;   // CE_NONE == the legacy solid fill
      col = g.pulseColor;
      bri = g.bright;                                        // the accent rides the ceiling, not the dip
      t0  = g.pulseStartMs;                                  // the overlay owns its own timeline
    } else {
      g.pulseActive = false;
    }
  }
  if (!g.pulseActive && g.base != CB_LOOK) {
    // Nothing of the contract's to draw: give the display back to what the unit's base is.
    if (g.base == CB_NONE) return false;          // L3: the engine returns to its own default look (LE:818-830)
    r.selectSequence(g.base == CB_NATIVE ? (byte)g.nativeCode : NATIVE_LIGHTSOUT);
    return true;
  }

  int W = r.width(), H = r.height();

  switch (eff) {
    case CE_OFF:
      _fill(r, CRGB(0, 0, 0));
      break;
    case CE_FLASH: {
      bool on = fxFlashOn(now - t0, g.speed);                            // F6: the shared definition
      // v1.2 FIX: the lit state renders at the BEAT-PUMPED level (bri), not the raw ceiling.
      // It was the only sustained look on this board that ignored the envelope, so an
      // i=flash section pumped on the HPs/PSIs and sat flat on the Logics. During an
      // overlay bri is the ceiling by design (an accent must not dip).
      _fill(r, on ? _scaled(col, bri) : CRGB(0, 0, 0));
      break;
    }
    case CE_RAINBOW: {
      uint32_t rate = 20u + (uint32_t)(255 - g.speed) / 4u;              // ms per hue step
      uint8_t hbase = (uint8_t)((now / (rate ? rate : 1)) & 0xFF);
      int step = (W > 0) ? (255 / W) : 8;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, CRGB(CHSV((uint8_t)(hbase + x * step), 255, bri)));
      break;
    }
    case CE_SCAN: {
      int pos = fxScanPos(now - t0, g.speed, W, false);                  // F6: ping-pong, shared timing
      CRGB c = _scaled(col, bri);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, (x == pos) ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_SPARKLE: {
      uint32_t blk = now / (uint32_t)(80 + (255 - g.speed) / 2);
      uint32_t density = 20u + (uint32_t)g.speed / 4u;                   // ~20..84 % lit
      CRGB c = _scaled(col, bri);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, (_hash3(x, y, blk) % 100u) < density ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_METER: {
      int lit = (H * (int)gLevel) / 255;                                 // fill from bottom
      CRGB c = _scaled(col.r || col.g || col.b ? col : ContractRGB{0, 255, 0}, bri);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          r.setPixelRGB(x, y, ((H - 1 - y) < lit) ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_COMET: {
      int head = fxHead(now - t0, g.speed, W);
      for (int x = 0; x < W; x++) {
        uint8_t cb = fxCometBright(x, head, W);
        uint8_t v = (uint8_t)(((uint16_t)bri * cb) / 255);
        CRGB c = _scaled(col, v);
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, c);
      }
      break;
    }
    case CE_CHASE: {
      uint32_t el = now - t0;
      CRGB on = _scaled(col, bri), off(0, 0, 0);
      for (int x = 0; x < W; x++) {
        CRGB c = fxChaseLit(x, el, g.speed) ? on : off;
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, c);
      }
      break;
    }
    case CE_WIPE: {
      uint32_t el = now - t0;
      CRGB on = _scaled(col, bri), off(0, 0, 0);
      for (int x = 0; x < W; x++) {
        CRGB c = fxWipeLit(x, el, g.speed, W) ? on : off;
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, c);
      }
      break;
    }
    case CE_GRADIENT: {
      uint32_t el = now - t0;
      for (int x = 0; x < W; x++) {
        uint8_t hue = fxGradientHue(x, W, 0, el, g.speed);   // base hue 0, color-independent (like rainbow)
        ContractRGB c = fxHsv2rgb(hue, 255, bri);
        for (int y = 0; y < H; y++) r.setPixelRGB(x, y, CRGB(c.r, c.g, c.b));
      }
      break;
    }
    case CE_COLORCYCLE: {
      ContractRGB c = fxHsv2rgb(fxCycleHue(0, now - t0, g.speed), 255, bri);
      _fill(r, CRGB(c.r, c.g, c.b));
      break;
    }
    case CE_TWINKLE: {
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          uint8_t tb = fxTwinkleBright(y * W + x, now, g.speed);
          uint8_t v = (uint8_t)(((uint16_t)bri * tb) / 255);
          r.setPixelRGB(x, y, _scaled(col, v));
        }
      break;
    }
    case CE_PULSE: {
      // F6: a breathe when nothing pumps it; under the beat pump the envelope IS the breathe.
      bool pumped = !g.pulseActive && g.accentMode != 0 && g.beatMod != 0 && gBeat.running;
      uint8_t v = pumped ? bri : (uint8_t)(((uint16_t)bri * fxPulseBright(now - t0, g.speed)) / 255);
      _fill(r, _scaled(col, v));
      break;
    }
    case CE_SOLID:
    default:
      _fill(r, _scaled(col, bri));
      break;
  }
  return true;   // MUST return true (LogicEngine.h:820 reverts to NORMAL on false)
}

// ------------------------------------------------------------ verb dispatch ----
// L1: the three boundaries. Each runs the ONE shared reset, then its own display action.
static inline void _boundary(uint8_t u, char kind, uint8_t wdSec) {   // 's'how, 'x' stop, 'i'dle
  ContractUnitState& g = gContract[u];
  _engineBriRestore(u);
  if (kind == 's')      contractOnShow(g, _ownerBri(u), wdSec);
  else if (kind == 'x') contractOnStop(g, _ownerBri(u));
  else                  contractOnIdle(g, _ownerBri(u));
  gBeat = BeatClock();                  // L1: stop the beat clock (board-wide on this fork)
  _showBase(u);
}

// A live look (now, or when its S4 hold ends).
static inline void _applyLook(uint8_t u, const ContractLookPatch& lp, uint32_t now) {
  ContractUnitState& g = gContract[u];
  if ((lp.has & LP_EFFECT) && lp.effect == CE_NATIVE && !_nativeAllowed(lp.nativeCode)) return;   // S7
  _arm(u);
  if (lp.has & LP_COLOR)   g.color = lp.color;
  if (lp.has & LP_SPEED)   g.speed = lp.speed;
  if (lp.has & LP_BRIGHT)  g.bright = _clampBright(g, lp.bright);
  if (lp.has & LP_BEATMOD) g.beatMod = lp.beatMod;
  if (lp.has & LP_AM)      g.accentMode = lp.accentMode;
  g.startMs = now; g.pulseActive = false;
  if (lp.has & LP_EFFECT) {
    g.effect = lp.effect; g.nativeCode = lp.nativeCode;
    g.base = (lp.effect == CE_NATIVE) ? CB_NATIVE : CB_LOOK;
    _showBase(u);
  }
  if (g.base == CB_NATIVE && (lp.has & LP_BRIGHT)) _engineBri(u, g.bright);
}

inline void applyContractToUnit(uint8_t u, const ParsedContract& p, uint16_t lineCrc) {
  ContractUnitState& g = gContract[u];
  const ContractParams& pr = p.params;
  uint32_t now = millis();
  switch (p.verb) {
    case CV_ANIMATE: {
      if (pr.hasAt) {                             // Phase-2: schedule this look, do not apply now
        if (pr.hasEffect && pr.effect == CE_NATIVE && !_nativeAllowed(pr.nativeCode)) break;   // S7
        ScoreEntry e;
        e.atBeat = pr.atBeat;
        e.effect = pr.hasEffect ? pr.effect : CE_SOLID;
        e.nativeCode = pr.hasEffect ? pr.nativeCode : -1;
        e.color  = pr.hasColor ? pr.color : ContractRGB{0, 0, 0};
        e.speed  = pr.hasSpeed ? pr.speed : 128;
        e.bright = pr.hasBright ? pr.bright : 255;                        // 255 == the ceiling
        e.beatMod = pr.hasBeatMod ? pr.beatMod : 0;
        e.accentMode = pr.hasAm ? pr.accentMode : 0;
        if (pr.hasAccentFx) {
          e.accentFx = pr.accentFx;
          e.accentColor = pr.hasAccentColor ? pr.accentColor : e.color;
          uint16_t t10 = (uint16_t)((pr.hasAccentDur ? pr.accentDurMs : 180) / 10u);
          if (t10 == 0) t10 = 18;
          e.accentDur10 = (uint8_t)(t10 > 255u ? 255u : t10);
        }
        scoreAccept(g, gScore[u], SCORE_MAX, e, lineCrc);                 // full => the load is marked bad
        g.lastAccentBeat = accentEdgeAfterSeed(gBeat, now);               // C1 rule, not a blind reset
        break;                                    // F4: the display is NOT taken at load
      }
      ContractLookPatch lp = lookPatchFrom(pr);
      if (liveLookOffer(g, lp, now)) _applyLook(u, lp, now);              // S4: else held for the tick
      break;
    }
    case CV_PULSE:
      _arm(u);
      if (pr.hasBright) g.bright = _clampBright(g, pr.bright);
      _fireAccent(u, (pr.hasEffect && accentEffectAllowed(pr.effect)) ? pr.effect : CE_SOLID,
                  pr.hasColor ? pr.color : ContractRGB{255, 255, 255},
                  pr.hasDur ? pr.durMs : 120);
      break;
    case CV_LEVEL:
      if (pr.hasLevel) gLevel = pr.level;
      break;
    case CV_BRIGHT:
      _arm(u);
      g.bright = _clampBright(g, pr.hasBright ? pr.bright : (pr.hasLevel ? pr.level : g.bright));
      _engineBri(u, g.bright);                    // F7: native looks too, and it survives restoreSettings()
      break;
    case CV_STOP:
      _boundary(u, 'x', 0);
      break;
    case CV_MODE:
      if (pr.mode == 's')      _boundary(u, 's', pr.hasWd ? pr.wd : 0);
      else if (pr.mode == 'i') _boundary(u, 'i', 0);
      break;
    case CV_VERIFY:
      if (!pr.hasN || !pr.hasK) break;
      if (scoreVerify(g, pr.n, pr.k) == 2) { _boundary(u, 'i', 0); g.loadOk = 2; }   // back to native, visibly
      break;
    default: break;
  }
}

// One contract line (the bytes after '!'). The single entry point for Serial1, WiFi and the
// Marcduino action.
inline void contractHandleLine(const char* afterBang) {
  ParsedContract p;
  if (!contractParse(afterBang, p)) return;
  if (p.cls != 'L' && p.cls != '*') return;       // not a Logic board
  const bool f = (p.unit == 'F' || p.unit == '*'), r = (p.unit == 'R' || p.unit == '*');
  if (!f && !r) return;                           // unit 'T' (HP)
  uint32_t now = millis();
  gLastLineMs = now;                              // L2: well-formed and addressed to this board
  if (p.verb == CV_CLOCK) {                       // ONE shared clock: seed once, then both units follow
    ContractSeed k = beatClockSeed(gBeat, p.params, now);
    if (k == CS_IGNORED) return;
    for (uint8_t u = 0; u < 2; u++) {
      gContract[u].lastAccentBeat = accentEdgeAfterSeed(gBeat, now);      // C1
      gContract[u].pinForward = (k == CS_NUDGE);                          // C2
    }
    return;
  }
  if (p.verb == CV_QUERY) {                       // targeted only: never answer a wildcard
    if (p.cls != 'L' || p.unit == '*') return;
    uint8_t u = f ? 0 : 1;
    gQueryLen = contractFormatQuery(gQueryLine, sizeof gQueryLine, 'L', p.unit, SCORE_MAX,
                                    gContract[u].scoreLen, gContract[u].loadOk,
                                    gBeat.running ? gBeat.bpm : 0);
    return;
  }
  uint16_t crc = (p.verb == CV_ANIMATE && p.params.hasAt) ? contractCrc16(afterBang) : 0;
  if (f) applyContractToUnit(0, p, crc);
  if (r) applyContractToUnit(1, p, crc);
}
inline bool contractReplyPending(const char*& line, uint8_t& n) { line = gQueryLine; n = gQueryLen; return n != 0; }
inline void contractReplyDone() { gQueryLen = 0; }

// Runs from mainLoop() every pass (NOT from an effect fn): watchdog, held looks, the score.
inline void contractTick() {
  uint32_t now = millis();
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (contractWatchdogExpired(g, gLastLineMs, now)) _boundary(u, 'i', 0);   // L2
    ContractLookPatch lp;
    if (liveLookDue(g, now, lp)) _applyLook(u, lp, now);                      // S4
  }
  if (!gBeat.running) return;
  BeatPos bp = beatPosAt(gBeat, now);
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (g.scoreLen == 0) continue;
    int idx = scoreCursor(g.curSection, scoreActiveIndex(gScore[u], g.scoreLen, bp.beatIndex), g.pinForward);
    if (idx < 0 || idx == g.curSection) continue;
    const ScoreEntry& s = gScore[u][idx];
    g.effect = s.effect; g.nativeCode = s.nativeCode; g.color = s.color; g.speed = s.speed;
    g.bright = _clampBright(g, s.bright);
    g.beatMod = s.beatMod; g.accentMode = s.accentMode;
    g.curSection = (int8_t)idx; g.startMs = now; g.sectionStart = s.atBeat;
    g.sectionEnd = (idx + 1 < g.scoreLen) ? gScore[u][idx + 1].atBeat : (s.atBeat + 9999);
    g.pulseActive = false;
    g.base = (s.effect == CE_NATIVE) ? CB_NATIVE : CB_LOOK;
    if (g.base == CB_NATIVE) _engineBri(u, g.bright);
    _showBase(u);
  }
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (!beatEdge(g.lastAccentBeat, bp.beatIndex)) continue;     // consume the edge first, always
    if (g.scoreLen == 0 || g.curSection < 0) continue;
    const ScoreEntry& e = gScore[u][g.curSection];
    if (e.accentFx == CE_NONE) continue;
    if (!beatAccentFires(e.accentMode, bp, gBeat)) continue;     // SAME predicate as the pump (S2)
    _fireAccent(u, e.accentFx, e.accentColor, (uint32_t)e.accentDur10 * 10u);
  }
}

// The Marcduino action stays for callers of Marcduino::processCommand (the web UI, stored sequences).
MARCDUINO_ACTION(Contract, !, ({
  contractHandleLine(Marcduino::getCommand());
}))

inline void contractSetup() { }
