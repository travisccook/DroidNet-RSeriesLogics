// Part of the DroidNet Driveable-Animation Contract — an additive layer added to a
// fork of reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// src/contract/ContractLogics.h — RSeries Logic firmware layer for the
// Driveable-Animation Contract v1.3. Live cues, the beat clock, and the autonomous
// board-side score. ZERO edits to lib/Reeltwo.
//
// INCLUDE ORDER (main.cpp): included AFTER ReelTwo / LogicEngine.h / FastLED and the
// global engine pointers FLD/RLD + FLD_/RLD_selectSequence, and BEFORE
// CustomLogicEffectSelector. Relies on: CRGB, CHSV, millis(), byte, LogicEngineRenderer,
// LogicEngineSettings + LogicEngineFLDDefault/LogicEngineRLDDefault, FLD/RLD,
// FLD_/RLD_selectSequence.
//
// DESIGN. Everything a show owns on a unit is one ContractUnitState (contract_core.h), and
// the three show boundaries (M:v=show, X, M:v=idle) run the core's ONE reset. This file is
// the adapter between that state and the engine:
//   * A unit's BASE says what its display falls back to (CB_NONE: the engine's own look;
//     CB_DARK; CB_LOOK: a contract look; CB_NATIVE: a native sequence the contract chose).
//     _showBase() maps it onto an engine sequence.
//   * ONE fork effect, ContractRender (id 106), draws the contract looks and the accent
//     overlay every ~10 ms frame (LogicEngine.h:789-831). It returns false only to hand the
//     display back: the engine then selects its own default effect (LogicEngine.h:818-830).
//     setPixelRGB (:1383) bypasses fBri, so brightness and the beat pump are applied here.
//   * contractHandleLine() is the single entry point for a contract line.
//   * contractTick() runs from mainLoop() every pass: the score and its accents.
#pragma once
#include "contract_core.h"

static const byte     FX_CONTRACT   = 106;         // single fork effect id (appended in main.cpp)
static const byte     NATIVE_LIGHTSOUT = 14;       // native blackout / de-energize
static const byte     NATIVE_NORMAL = 0;           // native idle shimmer
static const uint8_t  SCORE_MAX = 32;              // sections per unit (contract v1.3 section 6)

// Per-unit contract state. Index 0 = FLD (engine id 1), 1 = RLD (engine id 2).
static ContractUnitState gContract[2];
static BeatClock  gBeat;                // ONE beat clock for both displays (verb C)
static uint8_t    gLevel = 0;           // verb L energy 0..255 (drives CE_METER)
static ScoreEntry gScore[2][SCORE_MAX];
static uint32_t   gLastLineMs = 0;      // L2: the last well-formed contract line addressed to this board
// Section 10: one pending reply to a Q, written to Serial1 by mainLoop() between WDP announces.
static char       gQueryLine[CONTRACT_REPLY_MAX];
static uint8_t    gQueryLen = 0;
// S6 / verb B on native looks. The engine copies its RAM default object back over the live
// settings on every effect change (LogicEngine.h:807, :1830), so a brightness that has to last
// is written THERE, and the owner's value is put back at every show boundary.
static bool       gBriTouched[2] = { false, false };
static uint8_t    gBriSaved[2]   = { 0, 0 };
static uint8_t    gBriWritten[2] = { 0, 0 };

// ---------------------------------------------------------------- helpers ----
static inline LogicEngineRenderer* _engine(uint8_t u) { return u == 0 ? FLD : RLD; }
static inline LogicEngineSettings& _defaults(uint8_t u) { return u == 0 ? LogicEngineFLDDefault : LogicEngineRLDDefault; }
// S6: the brightness ceiling is the display's OWN brightness, as its owner set it (the engine's
// fBri: its trimpot, web slider or #LEBRI baseline) — never a compile-time maximum.
static inline uint8_t _ownerBri(uint8_t u) { return gBriTouched[u] ? gBriSaved[u] : _defaults(u).fBri; }
// Give a native look this level. It goes through the RAM default object, so it survives the
// engine's restoreSettings() on the next effect change.
static inline void _engineBri(uint8_t u, uint8_t level) {
  LogicEngineSettings& d = _defaults(u);
  if (!gBriTouched[u]) { gBriSaved[u] = d.fBri; gBriTouched[u] = true; }
  d.fBri = level; gBriWritten[u] = level;
  if (_engine(u)) { _engine(u)->setBrightness(level); _engine(u)->calculateAllColors(); }
}
// Put the owner's brightness back — unless something else (#LEBRI, the web UI, the trimpot)
// changed the default while the show held it; then that newer value stands.
static inline void _engineBriRestore(uint8_t u) {
  if (!gBriTouched[u]) return;
  gBriTouched[u] = false;
  if (_defaults(u).fBri != gBriWritten[u]) return;
  _defaults(u).fBri = gBriSaved[u];
  if (_engine(u)) { _engine(u)->setBrightness(gBriSaved[u]); _engine(u)->calculateAllColors(); }
}
// A unit the contract does not hold yet takes its ceiling now.
static inline void _arm(uint8_t u) { if (gContract[u].base == CB_NONE) gContract[u].ceil = _ownerBri(u); }
// The level of the unit's look: b= / B is a fraction of the ceiling (S6).
static inline uint8_t _level(const ContractUnitState& g) { return contractLevel(g.ceil, g.bright); }
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
// verb-dispatch context (outside an effect fn): act on one engine by unit index.
static inline void _select(uint8_t u, byte seq) { if (u == 0) FLD_selectSequence(seq); else RLD_selectSequence(seq); }
static inline void _reset(uint8_t u)            { if (_engine(u)) _engine(u)->resetEffect(); }
// Does the contract hold this unit's display right now?
static inline bool _holds(const ContractUnitState& g) { return g.base != CB_NONE || g.pulseActive; }
// Put the unit's base on its display (dispatch context; no overlay up).
static inline void _showBase(uint8_t u) {
  const ContractUnitState& g = gContract[u];
  if (g.base == CB_LOOK)        _select(u, FX_CONTRACT);
  else if (g.base == CB_NATIVE) _select(u, (byte)g.nativeCode);
  else if (g.base == CB_DARK)   _select(u, NATIVE_LIGHTSOUT);
  else                        { _select(u, NATIVE_NORMAL); _reset(u); }   // give it back: the native idle look
}

// beat-modulated brightness for sustained looks. Level math lives in contract_core's
// envBright() — the SHARED envelope all three boards render through (the look's level is the
// ceiling of the pump, m= is the dip depth). Do not reintroduce a board-local floor here: it
// desyncs the Logics from the PSIs/HPs on every cue.
static inline uint8_t _beatBright(const ContractUnitState& g, const BeatPos& bp) {
  const uint8_t level = _level(g);
  if (g.accentMode == 0 || g.beatMod == 0 || !gBeat.running) return level;
  const float prog = beatSectionProgress(bp.beatIndex, g.sectionStart, g.sectionEnd);
  return envBright(level, g.beatMod, beatAccentAmount(g.accentMode, bp, g.beatMod, prog, gBeat, g.accentEvery));
}

// THE single overlay-arming path. Verb P (live) and the board-side score accent (autonomous)
// both come through here, so the two delivery paths cannot drift apart.
//
// STROBE CAP (SAFETY — photosensitivity, enforced for BOTH triggers): an overlay may not
// RESTART more often than every 2 * STROBE_MIN_STATE_MS (340 ms => <= 2.94 flashes/sec), and it
// lasts at least STROBE_MIN_STATE_MS and at most ACCENT_DUR_MAX_MS (accentDurClamp). The gate
// anchors on pulseLastMs UNCONDITIONALLY: an overlay that is still on screen is not an
// exemption, and the core's show reset keeps pulseLastMs, so a reset is not one either.
static inline void _fireAccent(uint8_t u, ContractEffect fx, const ContractRGB& c, uint32_t durMs) {
  ContractUnitState& g = gContract[u];
  uint32_t now = millis();
  if (g.base == CB_NATIVE) return;      // R1: no overlay on a native look — an accent never restarts a native sequence
  if (!strobeCoolDownExpired(g.pulseLastMs, now, 2 * STROBE_MIN_STATE_MS)) return;
  g.pulseFx     = fx;
  g.pulseColor  = c;
  g.pulseDurMs  = accentDurClamp(durMs);
  g.pulseStartMs = now;                 // ALWAYS retrigger (defeats a re-send no-op)
  g.pulseLastMs  = now;                 // the cool-down anchor: set on every accent that ARMS
  g.pulseActive  = true;
  _select(u, FX_CONTRACT);              // ContractRender draws it (it needs no engine reset)
}

// ---- the single fork effect fn: renders unit u's contract state each frame ----
inline bool ContractRender(LogicEngineRenderer& r) {
  uint8_t u = (r.getID() == 1) ? 0 : 1;
  ContractUnitState& g = gContract[u];
  uint32_t now = millis();
  BeatPos bp = beatPosAt(gBeat, now);
  // NB: the score is advanced in contractTick() from mainLoop (not here), so section
  // switching keeps running even while a NATIVE section look (which is not rendered by
  // ContractRender) is active.

  // THE OVERLAY: the accent is an EFFECT SWAP, not a solid fill. The switch below runs
  // on a chosen SOURCE (eff/col/bri/t0) — the base look normally, the overlay while one is
  // active — so the accent reuses every existing renderer at zero flash cost. Its timeline
  // starts at pulseStartMs (the overlay owns its own t0; the base look's startMs is NEVER
  // touched, so an am=2 accent cannot restart the base look every beat), and it rides the
  // look's LEVEL rather than the pumped dip.
  uint8_t bb = _beatBright(g, bp);
  ContractEffect eff = g.effect;
  ContractRGB      col = g.color;
  uint8_t  bri = bb;
  uint32_t t0  = g.startMs;
  if (g.pulseActive) {
    if (now - g.pulseStartMs < g.pulseDurMs) {
      eff = (g.pulseFx == CE_NONE) ? CE_SOLID : g.pulseFx;   // CE_NONE == the legacy solid fill
      col = g.pulseColor;
      bri = _level(g);                                       // the accent rides the level, not the dip
      t0  = g.pulseStartMs;                                  // the overlay owns its own timeline
    } else {
      g.pulseActive = false;
    }
  }
  if (!g.pulseActive && g.base != CB_LOOK) {
    // Nothing of the contract's left to draw on this display: hand it to the unit's base.
    // CB_NONE (a pulse outside a show, L3): returning false makes the engine select its own
    // default effect (LogicEngine.h:818-830). The engine cannot tell us which timed native
    // sequence a pulse interrupted, so the return is to the default look, not to that sequence.
    if (g.base == CB_NONE) return false;
    r.selectSequence(g.base == CB_NATIVE ? (byte)g.nativeCode : NATIVE_LIGHTSOUT);
    return true;
  }

  int W = r.width(), H = r.height();

  switch (eff) {
    case CE_OFF:
      _fill(r, CRGB(0, 0, 0));
      break;
    case CE_FLASH: {
      bool on = fxFlashOn(now - t0, g.speed);            // the shared timing: 170..425 ms a state (strobe cap)
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
      int pos = fxScanPos(now - t0, g.speed, W, false);  // the shared timing: one column, ping-pong
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
      // The shared breathe (fxPulseBright: a 2000..340 ms cycle, never faster, S8) when nothing
      // pumps the look. Under the beat pump the envelope IS the breathe, so the fill is steady
      // and the pump moves it; two modulations at once would beat against each other.
      const bool pumped = !g.pulseActive && g.accentMode != 0 && g.beatMod != 0 && gBeat.running;
      const uint8_t v = pumped ? bri : (uint8_t)(((uint16_t)bri * fxPulseBright(now - t0, g.speed)) / 255u);
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
// L1: a board's beat clock stops at a show boundary only when neither display needs it any more
// (the two displays share one clock).
static inline void _stopClockIfUnused() {
  if (!contractUnitNeedsClock(gContract[0]) && !contractUnitNeedsClock(gContract[1])) gBeat = BeatClock();
}
// L1: the three show boundaries: 's' M:v=show, 'x' X, 'i' M:v=idle. Each runs the ONE shared
// reset and then puts the unit's new base on its display:
//   show  arms. The display keeps the native idle look until the show's first C, live A or P
//         (contractShowBegin), so the dome is not dark between Send and Play.
//   x     blacks out; the unit stays in its mode, with its watchdog.
//   idle  gives the display back: NORMAL runs again with nothing of the show left selected.
// A display the contract never took is left alone, whatever the engine is running on it.
static inline void _boundary(uint8_t u, char kind, uint8_t wdSec) {
  ContractUnitState& g = gContract[u];
  const bool held = _holds(g);
  _engineBriRestore(u);
  if (kind == 's')      contractOnShow(g, _ownerBri(u), wdSec);
  else if (kind == 'x') contractOnStop(g, _ownerBri(u));
  else                  contractOnIdle(g, _ownerBri(u));
  _stopClockIfUnused();
  if (held || g.base != CB_NONE) _showBase(u);
}

// A live look: now, or when its S4 hold ends.
static inline void _applyLook(uint8_t u, ContractLookPatch lp, uint32_t now) {
  ContractUnitState& g = gContract[u];
  _arm(u);
  contractShowBegin(g);                           // L1: the show's first live A (the look below replaces the dark baseline)
  lookPatchDefault(lp, g.base);                   // L5: no i= and no look yet => i=solid
  if (lp.has & LP_COLOR)   g.color = lp.color;
  if (lp.has & LP_SPEED)   g.speed = lp.speed;
  if (lp.has & LP_BRIGHT)  g.bright = lp.bright;  // a fraction of the ceiling (S6)
  if (lp.has & LP_BEATMOD) g.beatMod = lp.beatMod;
  if (lp.has & LP_AM)      g.accentMode = lp.accentMode;
  if (lp.has & LP_EVERY)   g.accentEvery = lp.every;
  g.startMs = now; g.pulseActive = false;
  if (lp.has & LP_EFFECT) {
    g.effect = lp.effect; g.nativeCode = lp.nativeCode;
    g.base = (lp.effect == CE_NATIVE && lp.nativeCode >= 0) ? CB_NATIVE : CB_LOOK;
  }
  _showBase(u);
  if (g.base == CB_NATIVE) _engineBri(u, _level(g));
}

inline void applyContractToUnit(uint8_t u, const ParsedContract& p, uint16_t lineCrc) {
  ContractUnitState& g = gContract[u];
  const ContractParams& pr = p.params;
  uint32_t now = millis();
  switch (p.verb) {
    case CV_ANIMATE: {
      // S7: a line naming a native code that is not on the allow-list is dropped whole.
      if (pr.hasEffect && pr.effect == CE_NATIVE && !nativeAllowedLogics(pr.nativeCode)) break;
      if (pr.hasAt) {                             // a scored look: store it, do not apply it now
        ScoreEntry e;
        e.atBeat = pr.atBeat;
        e.effect = pr.hasEffect ? pr.effect : CE_SOLID;
        e.nativeCode = pr.hasEffect ? pr.nativeCode : -1;
        e.color  = pr.hasColor ? pr.color : ContractRGB{0, 0, 0};
        e.speed  = pr.hasSpeed ? pr.speed : 128;
        e.bright = pr.hasBright ? pr.bright : 255;                      // absent: the ceiling
        e.beatMod = pr.hasBeatMod ? pr.beatMod : 0;
        e.accentMode = pr.hasAm ? pr.accentMode : 0;
        e.accentEvery = accentEveryFrom(pr);
        // The accent the board fires ITSELF on this section's accent beats. No ae= =>
        // accentFx stays CE_NONE and the beat-edge trigger never arms the overlay for it.
        if (pr.hasAccentFx) {
          e.accentFx = pr.accentFx;               // parse already rejected native/stateful
          e.accentColor = pr.hasAccentColor ? pr.accentColor : e.color;   // ac= resolved AT INSERT
          uint16_t t10 = (uint16_t)((pr.hasAccentDur ? pr.accentDurMs : ACCENT_DUR_DEFAULT_MS) / 10u);
          if (t10 == 0) t10 = (uint16_t)(ACCENT_DUR_DEFAULT_MS / 10u);    // never a zero-length accent
          e.accentDur10 = (uint8_t)(t10 > 255u ? 255u : t10);             // ad<=2550 at parse
        }
        scoreAccept(g, gScore[u], SCORE_MAX, e, lineCrc);                 // a full table marks the load bad
        g.lastAccentBeat = accentEdgeAfterSeed(gBeat, now);               // C1 (BEAT_NONE while no clock runs)
        break;                                    // the display is NOT taken at load: the tick does that on the beat
      }
      ContractLookPatch lp = lookPatchFrom(pr);
      if (liveLookOffer(g, lp, now)) _applyLook(u, lp, now);              // S4: else held; contractTick() applies it
      break;
    }
    case CV_PULSE: {
      // The accent overlay, live. It honours i= (an effect-swap accent, not just a solid fill).
      if (g.base == CB_NATIVE) break;             // R1: no overlay on a native look
      _arm(u);
      if (contractShowBegin(g)) _showBase(u);     // L1: the show's first P: the baseline goes dark first
      if (pr.hasBright) g.bright = pr.bright;
      _fireAccent(u, (pr.hasEffect && accentEffectAllowed(pr.effect)) ? pr.effect : CE_SOLID,
                  pr.hasColor ? pr.color : ContractRGB{255, 255, 255},
                  pr.hasDur ? pr.durMs : 120);
      break;
    }
    case CV_LEVEL:
      if (pr.hasLevel) gLevel = pr.level;         // drives CE_METER (contract meter reads gLevel)
      break;
    case CV_BRIGHT:
      _arm(u);
      g.bright = pr.hasBright ? pr.bright : (pr.hasLevel ? pr.level : g.bright);   // a fraction of the ceiling
      _engineBri(u, _level(g));                   // native looks too, and it survives restoreSettings()
      break;
    case CV_STOP:
      _boundary(u, 'x', 0);
      break;
    case CV_MODE:
      if (pr.mode == 's')      _boundary(u, 's', pr.hasWd ? pr.wd : 0);
      else if (pr.mode == 'i') _boundary(u, 'i', 0);
      break;
    case CV_VERIFY:                               // section 7: is the score this unit holds the one the host sent?
      if (!pr.hasN || !pr.hasK) break;
      if (scoreVerify(g, gScore[u], pr.n, pr.k) == 2) _boundary(u, 'i', 0);   // no: back to the native look, visibly.
      break;                                                                  // (the reset keeps ok=2 for Q)
    default: break;
  }
}

// ONE contract line: the bytes after '!', NUL-terminated. The single entry point, for the
// Serial1 reader, the WiFi receiver and the Marcduino action alike.
inline void contractHandleLine(const char* afterBang) {
  ParsedContract p;
  if (!contractParse(afterBang, p)) return;
  if (p.cls != 'L' && p.cls != '*') return;       // not a Logic board
  const bool f = (p.unit == 'F' || p.unit == '*'), r = (p.unit == 'R' || p.unit == '*');
  if (!f && !r) return;                           // unit 'T' (HP)
  const uint32_t now = millis();
  gLastLineMs = now;                              // L2: well-formed and addressed to this board feeds the watchdog
  if (p.verb == CV_QUERY) {                       // section 10 (an unanswered Q is still a keepalive)
    if (!contractQueryAnswerable(p)) return;      // no r=, or a wildcard: no reply
    const uint8_t u = f ? 0 : 1;
    gQueryLen = contractFormatQuery(gQueryLine, sizeof gQueryLine, p.params.route, 'L', p.unit, SCORE_MAX,
                                    gContract[u].scoreLen, gContract[u].loadOk, gBeat.running ? gBeat.bpm : 0);
    return;
  }
  if (p.verb == CV_CLOCK) {                       // ONE shared clock: seed it once, then both displays follow
    const ContractSeed k = beatClockSeed(gBeat, p.params, now);
    if (k == CS_IGNORED || k == CS_HOLD) return;  // S1: ignored whole. C6: held — cursors freeze because a stopped clock is not ticked
    for (uint8_t u = 0; u < 2; u++) {
      ContractUnitState& g = gContract[u];
      g.lastAccentBeat = accentEdgeAfterSeed(gBeat, now);     // C1: mid-beat, that beat counts as accented
      g.pinForward = (k == CS_NUDGE);                         // C2: a nudge never moves a section cursor back
      if (k == CS_SEEK && (u == 0 ? f : r) && contractShowBegin(g)) _showBase(u);   // L1: the show's first C
    }
    return;
  }
  const uint16_t crc = (p.verb == CV_ANIMATE && p.params.hasAt) ? contractCrc16(afterBang) : 0;
  if (f) applyContractToUnit(0, p, crc);
  if (r) applyContractToUnit(1, p, crc);
}
// The pending Q reply (section 10). mainLoop() writes it when Serial1 can take it whole.
inline bool contractReplyPending(const char*& line, uint8_t& n) { line = gQueryLine; n = gQueryLen; return n != 0; }
inline void contractReplyDone() { gQueryLen = 0; }

// Runs from mainLoop() every pass (NOT from an effect fn), so it keeps working while a native
// look, which ContractRender does not draw, is on a display.
inline void contractTick() {
  const uint32_t now = millis();
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (contractWatchdogExpired(g, gLastLineMs, now)) _boundary(u, 'i', 0);   // L2: the host went away
    ContractLookPatch lp;
    if (liveLookDue(g, now, lp)) _applyLook(u, lp, now);                      // S4: a held live look's window ended
  }
  if (!gBeat.running) return;                     // no clock (or held, C6): the score does not move
  const BeatPos bp = beatPosAt(gBeat, now);
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (g.scoreLen == 0) continue;
    int idx = scoreActiveIndex(gScore[u], g.scoreLen, bp.beatIndex);
    idx = scoreCursor(g.curSection, idx, g.pinForward);                       // C2
    if (idx < 0 || idx == g.curSection) continue;
    const ScoreEntry& s = gScore[u][idx];
    _arm(u);
    g.effect = s.effect; g.nativeCode = s.nativeCode; g.color = s.color; g.speed = s.speed;
    g.bright = s.bright;
    g.beatMod = s.beatMod; g.accentMode = s.accentMode; g.accentEvery = s.accentEvery;
    g.curSection = (int8_t)idx; g.startMs = now; g.sectionStart = s.atBeat;
    g.sectionEnd = (idx + 1 < g.scoreLen) ? gScore[u][idx + 1].atBeat : (s.atBeat + 9999);
    g.pulseActive = false;
    g.base = (s.effect == CE_NATIVE && s.nativeCode >= 0) ? CB_NATIVE : CB_LOOK;
    _showBase(u);
    if (g.base == CB_NATIVE) _engineBri(u, _level(g));
  }

  // The board fires its OWN effect-swap accent, once per accent beat. This is what makes a
  // DELIVERED show look like a LIVE-mirrored one: live gets the accent from the host's verb P,
  // the score gets it from here, and both arm the same overlay. Runs AFTER the section loop so
  // a section starting on a downbeat accents on that very beat. Gated to the SCORE path only —
  // a live verb-A never self-accents, so a live show cannot double-fire.
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    // Consume the edge FIRST and UNCONDITIONALLY: a beat that does not accent must still
    // retire its edge, or the next tick (same beat) re-tests and re-fires every frame.
    if (!beatEdge(g.lastAccentBeat, bp.beatIndex)) continue;
    if (g.scoreLen == 0 || g.curSection < 0) continue;           // no active section
    const ScoreEntry& e = gScore[u][g.curSection];
    if (e.accentFx == CE_NONE) continue;                         // no ae=: never accents
    if (!beatAccentFires(e.accentMode, bp, gBeat, e.accentEvery)) continue;   // SAME predicate as the pump (S2)
    _fireAccent(u, e.accentFx, e.accentColor, (uint32_t)e.accentDur10 * 10u);
  }
}

// The Marcduino action stays for whoever calls Marcduino::processCommand() with a '!' line
// (the web UI, a stored sequence). '!' is collision-free vs every native prefix.
MARCDUINO_ACTION(Contract, !, ({
  contractHandleLine(Marcduino::getCommand());
}))

inline void contractSetup() { }   // (native mic-effect PeakValueProvider wiring is R2: the level feed)
