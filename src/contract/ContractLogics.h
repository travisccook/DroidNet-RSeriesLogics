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

static const uint8_t  SAFE_MAX_BRIGHTNESS = 200;   // heat/USB clamp (fork spec §11)
// PHOTOSENSITIVITY (fork spec §11). >=170 ms/state => an accent may not RESTART more often
// than every 2 * 170 = 340 ms, i.e. <= 2.94 flashes/sec, inside the <= 3 flashes/sec
// guidance. This was 160 (a 320 ms floor => 3.13 Hz — over the line, and the only one of the
// three boards that was), while PSI and Flthy both used 170. The three boards fire the same
// show at the same instant; the cap must be the SAME number on all three.
static const uint32_t STROBE_MIN_STATE_MS = 170;   // >=170 ms/state => <= ~2.9 Hz (fork spec §11)
static const byte     FX_CONTRACT   = 106;         // single fork effect id (appended in main.cpp)
static const byte     NATIVE_LIGHTSOUT = 14;       // native blackout / de-energize
static const byte     NATIVE_NORMAL = 0;           // native idle shimmer
static const int      SCORE_MAX = 16;              // sections per unit (a song has ~8)

// Per-unit contract state. Index 0 = FLD (engine id 1), 1 = RLD (engine id 2).
struct ContractUnit {
  ContractEffect effect = CE_SOLID;
  ContractRGB      color{0, 0, 0};
  uint8_t  speed = 128;
  uint8_t  bright = SAFE_MAX_BRIGHTNESS;
  uint8_t  beatMod = 0;
  uint8_t  accentMode = 0;
  uint32_t startMs = 0;
  byte     baseSeq = FX_CONTRACT;       // base look seq (106, or a native id for native:<code>)
  bool     armed = false;
  // The ONE overlay (v1.2). Armed from TWO triggers — a live verb P, and a Phase-2
  // score entry's own ae=/ac=/ad= on an accent beat — so a mirrored show and a
  // delivered blueprint fire the SAME effect-swap accent.
  bool     pulseActive = false;
  ContractEffect pulseFx = CE_NONE;     // CE_NONE == the v1.1 solid fill (verb P with no i=)
  ContractRGB      pulseColor{255, 255, 255};
  uint32_t pulseDurMs = 0, pulseStartMs = 0;
  // Strobe cool-down anchor: the millis of the last accent that was actually ARMED, kept
  // separately from pulseStartMs so the cap can be applied UNCONDITIONALLY (an overlay that
  // is still on screen must not be a hole in it). Mirrors Flthy's u.pulseLastMs.
  uint32_t pulseLastMs = 0;
  // Phase-2 score cursor
  int      curSection = -1;
  int32_t  sectionStart = 0, sectionEnd = 0;
  // v1.2 once-per-beat edge guard for the board-side accent self-fire. BEAT_NONE ==
  // "no beat accented yet"; every show boundary resets it (see applyContractToUnit).
  int32_t  lastAccentBeat = BEAT_NONE;
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

// ---- v1.2: THE single overlay-arming path. Verb P (live, Phase-1) and the board-side
// score accent (autonomous, Phase-2) both come through here, so the two delivery paths
// cannot drift apart. fx == CE_NONE renders as the v1.1 solid fill.
//
// STROBE CAP (SAFETY — photosensitivity, enforced for BOTH triggers): an overlay may not
// RESTART more often than every 2 * STROBE_MIN_STATE_MS (340 ms => <= 2.94 flashes/sec), so a
// scored am=2 at a very high BPM silently drops the beats it cannot safely honour instead of
// strobing. The duration is floored at STROBE_MIN_STATE_MS so a state is never shorter than
// the cap.
//
// The gate anchors on pulseLastMs UNCONDITIONALLY (Flthy's shape). It used to read
//     !g.pulseActive && g.pulseStartMs != 0 && (now - g.pulseStartMs) < 2 * STROBE_MIN_STATE_MS
// and that `!g.pulseActive` short-circuit was a HOLE STRAIGHT THROUGH THE CAP: while an overlay
// was still on screen the whole test was skipped, so every re-arm was accepted. It is reachable
// with ordinary Studio output — any accent longer than the beat (e.g. ae=flash,ad=2550 with am=2)
// leaves the overlay up when the next beat lands, and at 300 BPM the logics then re-armed every
// 200 ms = 5 Hz. An accent restarting mid-overlay is exactly the flash a viewer sees, so the
// cool-down cannot care whether one is currently up.
static inline void _fireAccent(uint8_t u, ContractEffect fx, const ContractRGB& c, uint32_t durMs) {
  ContractUnit& g = gContract[u];
  uint32_t now = millis();
  if (!strobeCoolDownExpired(g.pulseLastMs, now, 2 * STROBE_MIN_STATE_MS)) return;
  if (durMs < STROBE_MIN_STATE_MS) durMs = STROBE_MIN_STATE_MS;
  g.pulseFx     = fx;
  g.pulseColor  = c;
  g.pulseDurMs  = durMs;
  g.pulseStartMs = now;                 // ALWAYS retrigger (defeats a re-send no-op)
  g.pulseLastMs  = now;                 // the cool-down anchor: set on every accent that ARMS
  g.pulseActive  = true;
  _select(u, FX_CONTRACT); _reset(u);   // ensure ContractRender is active + retrigger
}

// Every show boundary (stop / mode idle / mode show / clock re-seed / score load) must
// drop the beat-edge guard, or a stale edge from the previous show swallows the first
// accent of the next one (or, after a re-anchor, re-fires an already-consumed beat).
static inline void _resetAccentEdge(ContractUnit& g) { g.lastAccentBeat = BEAT_NONE; }

// ---- the single fork effect fn: renders unit u's contract state each frame ----
inline bool ContractRender(LogicEngineRenderer& r) {
  uint8_t u = (r.getID() == 1) ? 0 : 1;
  ContractUnit& g = gContract[u];
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
      if (g.baseSeq != FX_CONTRACT) { r.selectSequence(g.baseSeq); return true; }  // restore a native base
    }
  }

  int W = r.width(), H = r.height();

  switch (eff) {
    case CE_OFF:
      _fill(r, CRGB(0, 0, 0));
      break;
    case CE_FLASH: {
      uint32_t half = STROBE_MIN_STATE_MS + (uint32_t)(255 - g.speed);   // >=170 ms/state (strobe cap)
      bool on = ((now - t0) % (2 * half)) < half;
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
      uint32_t period = 60u * (uint32_t)(256 - g.speed) + 200u;
      int pos = (W > 0) ? (int)(((now - t0) % period) * (uint32_t)W / period) : 0;
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
    case CE_SOLID:
    default:
      // (CE_PULSE has no dedicated renderer on this board and lands here as a solid —
      // a pre-existing cross-fork divergence vs Flthy's triangle breathe, unchanged by v1.2.)
      _fill(r, _scaled(col, bri));
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
        e.color  = pr.hasColor ? pr.color : ContractRGB{0, 0, 0};
        e.speed  = pr.hasSpeed ? pr.speed : 128;
        e.beatMod = pr.hasBeatMod ? pr.beatMod : 0;
        e.accentMode = pr.hasAm ? pr.accentMode : 0;
        // v1.2: the accent the board fires ITSELF on this section's accent beats.
        // No ae= => accentFx stays CE_NONE => the entry is bit-identical to v1.1 and the
        // beat-edge trigger never arms the overlay for it.
        if (pr.hasAccentFx) {
          e.accentFx = pr.accentFx;               // parse already rejected native/stateful
          e.accentColor = pr.hasAccentColor ? pr.accentColor : e.color;   // ac= resolved AT INSERT
          uint16_t ad = pr.hasAccentDur ? pr.accentDurMs : 180;           // ad= default 180 ms
          uint16_t t10 = (uint16_t)(ad / 10u);
          if (t10 == 0) t10 = 18;                 // never a zero-length accent -> the 180 ms default
          e.accentDur10 = (uint8_t)(t10 > 255u ? 255u : t10);             // ad<=2550 at parse
        }
        gScoreLen[u] = scoreInsert(gScore[u], gScoreLen[u], SCORE_MAX, e);
        g.curSection = -1;                        // force re-eval on the next frame
        _resetAccentEdge(g);                      // a new show is loading: drop any stale beat edge
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
      // v1.2: verb P now goes through the SAME overlay path the scored accent uses, and it
      // honours i= (an effect-swap accent, not just a solid fill). With no i= it arms
      // CE_SOLID — bit-identical to the v1.1 fill. The strobe cool-down lives in _fireAccent.
      if (pr.hasBright) g.bright = _clampBright(pr.bright);
      _fireAccent(u, (pr.hasEffect && accentEffectAllowed(pr.effect)) ? pr.effect : CE_SOLID,
                  pr.hasColor ? pr.color : ContractRGB{255, 255, 255},
                  pr.hasDur ? pr.durMs : 120);
      break;
    }
    case CV_CLOCK:
      beatClockSeed(gBeat, pr, now);              // one shared clock; !**C fans to both units
      _resetAccentEdge(gContract[0]);             // the beat ORIGIN moved: a beat index from the
      _resetAccentEdge(gContract[1]);             // old timeline must not gate the new one
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
      g.armed = false; g.pulseActive = false;
      scoreClear(gScoreLen[u], g.curSection);     // ONE place to clear the score...
      _resetAccentEdge(g);                        // ...and to drop the beat edge with it
      _select(u, NATIVE_LIGHTSOUT);               // blackout + de-energize
      break;
    case CV_MODE:
      if (pr.mode == 's')      { gShowMode = true;  g.armed = false; g.pulseActive = false;
                                 // A new show starts HERE: M:v=show is the first line of
                                 // Studio's load burst and its Resend re-sends that burst with
                                 // no intervening X, so entering show mode must mean "forget the
                                 // last show" — otherwise show B's sections MERGE into show A's
                                 // (and are dropped outright once the table hits cap).
                                 scoreClear(gScoreLen[u], g.curSection);
                                 _resetAccentEdge(g);   // ...and no beat edge survives the boundary
                                 _select(u, NATIVE_LIGHTSOUT); }
      else if (pr.mode == 'i') { gShowMode = false; g.armed = false;
                                 scoreClear(gScoreLen[u], g.curSection);
                                 _resetAccentEdge(g);
                                 _select(u, NATIVE_NORMAL); }
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

  // ---- v1.2: the board fires its OWN effect-swap accent, once per accent beat ----
  // This is what makes a DELIVERED blueprint look like a LIVE-mirrored show: Phase-1 gets
  // the accent from the Pi's verb P, Phase-2 gets it from here, and both arm the same
  // overlay. Runs AFTER the section loop so a section starting on a downbeat accents on
  // that very beat. Gated to the SCORE path only — a live verb-A never self-accents, so a
  // board in Phase-1 cannot double-fire (the Pi's P plus a board-side edge).
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnit& g = gContract[u];
    // Consume the edge FIRST and UNCONDITIONALLY: a beat that does not accent must still
    // retire its edge, or the next tick (same beat) re-tests and re-fires every frame.
    if (!beatEdge(g.lastAccentBeat, bp.beatIndex)) continue;
    if (gScoreLen[u] == 0 || g.curSection < 0) continue;         // no active section
    const ScoreEntry& e = gScore[u][g.curSection];
    if (e.accentFx == CE_NONE) continue;                         // v1.1 entry -> never accents
    if (!beatAccentFires(e.accentMode, bp.barPos)) continue;     // SAME predicate as the pump
    _fireAccent(u, e.accentFx, e.accentColor, (uint32_t)e.accentDur10 * 10u);
  }
}

// The single fork command entry point. '!' is collision-free vs every native prefix.
MARCDUINO_ACTION(Contract, !, ({
  ParsedContract p;
  if (contractParse(Marcduino::getCommand(), p)) applyContract(p);
}))

inline void contractSetup() { }   // (native mic-effect PeakValueProvider wiring is an optional follow-up)
