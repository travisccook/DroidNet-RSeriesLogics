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
//   * ONE fork effect, ContractRender (id 106), draws the contract looks, the dark baseline and
//     the accent overlay every ~10 ms frame (LogicEngine.h:789-831). setPixelRGB (:1383)
//     bypasses fBri, so brightness and the beat pump are applied here.
//   * _present() puts a unit's state on its display: effect 106, or a native sequence. It is
//     the ONLY place a sequence is selected, and it never runs inside the effect. The engine
//     runs the effect and only THEN records the selection as seen (LogicEngine.h:818-819), so
//     a selectSequence() made from inside an effect is never noticed: effect 106 would go on
//     running. (That was the 2026-11-16 review's finding 1: an overlay ending on a dark display
//     left its last frame lit.)
//   * S9: every frame effect 106 draws is measured, by its level and by its luminance, and
//     passed through the core's field gate before it leaves (contractFieldAsk, then
//     contractFieldTell with what is on the LEDs). No line, and no order of lines, lights a
//     display from dark more often than every 340 ms.
//   * S4 for every path: a frame that would show another look or colour than the field last
//     showed is drawn 170 ms after the field's last change at the earliest (contractLookGate);
//     the frame that is up stays up meanwhile.
//   * S10: a native sequence runs as stock. Its frames are not held; the gates govern its edges
//     (contractNativeMayStart before the hand-over, contractNativeStarted at the frame it
//     starts, contractNativeEnded when effect 106 takes the display from it), and a brightness
//     the contract gives it goes through the field gate. A sequence that is not on the
//     allow-list can only come from the owner's hand; its light is left alone.
//   * contractEngineLatched() is the engine telling the layer which sequence it has just
//     started on a display (through main.cpp's effect selectors). It is how the layer knows a
//     native look is really on the LEDs, and how it notices a native take-over (L6).
//   * contractHandleLine() is the single entry point for a contract line.
//   * contractTick() runs from mainLoop() every pass: the score and its accents, the watchdog,
//     held live looks, and _present().
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
// Section 10: the pending reply to a Q, one per display (a host may query both in one burst),
// written to Serial1 by mainLoop() between WDP announces. A second Q to the same display before
// its reply has left replaces it.
static char       gQueryLine[2][CONTRACT_REPLY_MAX];
static uint8_t    gQueryLen[2] = { 0, 0 };
// What _present() last SELECTED for each display: nothing of the contract's (the engine's own
// look), effect 106, or a native sequence the contract chose; and whether the engine has started
// drawing it. The engine says so itself: it calls the fork's effect selector at the frame it
// starts a newly selected sequence (LogicEngine.h:804-813), and main.cpp's two selectors pass
// that on as contractEngineLatched(). That one signal is what makes three things exact:
//   * S9 for native looks: the gate is told a native look lit at the frame it did, and never
//     about one that was selected and then replaced before a frame;
//   * L6: a sequence the contract did not select, starting on a display the contract holds, is
//     someone's button. The show ends on that display, there and then;
//   * a timed native section that finishes (the engine falls back to its default look).
enum : uint8_t { SH_ENGINE = 0, SH_CONTRACT = 1, SH_NATIVE = 2 };
static uint8_t    gShown[2]     = { SH_ENGINE, SH_ENGINE };
static byte       gShownCode[2] = { 0, 0 };
static bool       gLatched[2]   = { true, true };
static uint16_t   gEngineSeq[2] = { 0, 0 };         // the sequence the engine last started (NORMAL at power-on)
static bool       gRestart[2]   = { false, false }; // resetEffect() was called: the engine WILL start a sequence next frame
static bool       gStopping     = false;            // an X line is being handled

// The brightness the contract is taking a native look to (B, b=, or back to the owner's), and a
// RISE it has written that the engine has not drawn yet. See _nativeBright().
static const uint32_t ENGINE_FRAME_MS = 10;         // LogicEngineRenderer::animate() draws at most this often (LogicEngine.h:792)
static uint8_t    gBriGoal[2]   = { 0, 0 };
static bool       gBriGoing[2]  = { false, false };
static bool       gRise[2]      = { false, false };
static uint8_t    gRiseLevel[2] = { 0, 0 };
static uint32_t   gRiseMs[2]    = { 0, 0 };
// S6 / verb B on native looks. The engine copies its RAM default object back over the live
// settings on every effect change (LogicEngine.h:807, :1830), so a brightness that has to last
// is written THERE, and the owner's value is put back when the show no longer needs it.
// gBriSaved is the owner's value as the layer last saw it, at all times (_briSync).
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
// Every pixel ContractRender draws goes through _px(): it applies the field gate's scale (S9) and
// adds the pixel to the frame's two measures: the field level (the mean of each pixel's brightest
// channel) and the field luminance (the mean of each pixel's Rec.709 luminance).
static uint32_t gPxSum = 0, gPxLum = 0;
static uint16_t gPxCount = 0;
static uint8_t  gPxScale = 255;
static inline void _px(LogicEngineRenderer& r, int x, int y, CRGB c) {
  if (gPxScale != 255) {
    c.r = (uint8_t)(((uint16_t)c.r * gPxScale) / 255u);
    c.g = (uint8_t)(((uint16_t)c.g * gPxScale) / 255u);
    c.b = (uint8_t)(((uint16_t)c.b * gPxScale) / 255u);
  }
  gPxSum += (c.r > c.g) ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
  gPxLum += contractLuminance(c.r, c.g, c.b);
  gPxCount++;
  r.setPixelRGB(x, y, c);
}
static inline void _fill(LogicEngineRenderer& r, const CRGB& c) {
  for (int y = 0; y < r.height(); y++)
    for (int x = 0; x < r.width(); x++)
      _px(r, x, y, c);
}
static inline uint32_t _hash3(int x, int y, uint32_t t) {
  uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ t * 83492791u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}
// verb-dispatch context (outside an effect fn): act on one engine by unit index.
static inline void _select(uint8_t u, byte seq) { if (u == 0) FLD_selectSequence(seq); else RLD_selectSequence(seq); }
static inline void _reset(uint8_t u)            { if (_engine(u)) _engine(u)->resetEffect(); }
// The brightness the engine has now, as far as the contract set it.
static inline uint8_t _engineBriNow(uint8_t u) { return gBriTouched[u] ? gBriWritten[u] : _ownerBri(u); }
// Does this sequence draw at a level of its own, whatever the engine's brightness is? Leia (3)
// sets its brightness itself every frame; 19..24 and the custom effects write their pixels
// directly. Asked of ANY sequence the engine may be on, the owner's included, so it is not the
// allow-list's nativeDimsLogics(): a sequence that is not known to ignore the brightness is
// taken to follow it, and a change to it goes through the gate.
static inline bool _ownLevel(unsigned seq) { return seq == 3 || (seq >= 19 && seq <= 24) || (seq >= 100 && seq <= 105); }
// The field level the S9 gate takes a native sequence at: the engine's brightness, or full scale
// for one that draws at a level of its own.
static inline uint8_t _nativeFieldLevel(unsigned seq, uint8_t brightness) {
  if (seq == NATIVE_LIGHTSOUT) return 0;
  return _ownLevel(seq) ? 255 : brightness;
}
// N3. The owner moved the display's brightness (the web slider, the trimpot, #LEBRI): the default
// object no longer holds what the layer last knew it to hold, which is the contract's own value
// while one is written there (gBriWritten) and the owner's otherwise (gBriSaved). The owner's new
// value is the display's brightness from now on: it is what goes back after the show, it is the
// ceiling (S6: a contract line never makes a board brighter than its owner set it), and on a
// native look it stands as it is until the host's next B or b=, which is then a fraction of it.
// A native look that is up changes its light with the owner's hand, at the engine's next frame:
// the S9 gate is told then, the way it is told of a rise the contract wrote (gRise).
// Called before anything reads the owner's brightness.
static inline void _briSync(uint8_t u) {
  const uint8_t def = _defaults(u).fBri;
  if (def == (gBriTouched[u] ? gBriWritten[u] : gBriSaved[u])) return;
  ContractUnitState& g = gContract[u];
  gBriTouched[u] = false; gBriSaved[u] = def;
  g.ceil = def;
  gBriGoal[u] = _level(g); gBriGoing[u] = false;
  if (gShown[u] != SH_CONTRACT && gLatched[u]) {
    gRise[u] = true; gRiseLevel[u] = _nativeFieldLevel(gEngineSeq[u], def); gRiseMs[u] = millis();
  }
}
// Does the contract set this native look's brightness (verb B in a show, b= on a native look)?
static inline bool _dimsNative(const ContractUnitState& g) { return g.inShow || g.base == CB_NATIVE; }

// A native look is on the LEDs: take its brightness to what the contract asks (b= on a native
// look, verb B; after the show, the owner's own again). A native look is light the layer cannot
// see, so for S9 its field level IS that brightness:
//   * dimmer: written at once, and the gate is told at once. (Darker is never delayed.)
//   * brighter: only when the gate allows it now (contractFieldAllowed); otherwise asked again
//     next pass. The gate is TOLD when the engine has drawn it: a brightness reaches the LEDs at
//     the engine's next frame, at most ENGINE_FRAME_MS and a loop pass later, and nothing reports
//     that frame. Until then (gRise) no other brightness is written. If a contract look takes the
//     display inside that one frame, nobody can say whether the brighter native look was ever
//     drawn; contractEngineLatched() then takes the safe side (it counts as an onset, now).
// The engine's brightness is only moved when what the contract WANTS changes (gBriGoal): a
// brightness the owner sets during the show (#LEBRI, the web UI, the trimpot) stands until the
// host's next B (_briSync).
static inline void _nativeBright(uint8_t u, uint32_t now) {
  ContractUnitState& g = gContract[u];
  const bool ours = _dimsNative(g);
  if (!ours && !gBriTouched[u]) return;                 // nothing of the contract's in the engine's brightness
  const uint8_t want = _level(g);                       // (no show: the ceiling, which is the owner's own value)
  if (want != gBriGoal[u]) { gBriGoal[u] = want; gBriGoing[u] = true; }
  if (!gBriGoing[u]) return;
  if (gRise[u]) return;                                 // one at a time: the last rise has not been drawn yet
  // S10: a sequence that is not on the allow-list is the owner's own (a button), and may flash by
  // itself: its light is not the contract's to move, up or down. It runs at the owner's
  // brightness (contractEngineLatched); what the contract wants waits for a look it knows.
  if (!nativeAllowedLogics((int)gEngineSeq[u])) return;
  const uint8_t cur = _engineBriNow(u);
  if (gEngineSeq[u] == NATIVE_LIGHTSOUT || _ownLevel(gEngineSeq[u])) {
    // The engine's brightness is not this sequence's light: lights-out is dark at any brightness
    // (LogicLightsOutEffect draws level 0), and 19..24 and the custom effects draw at a level of
    // their own. The value is kept in step for whatever sequence comes next; nothing is gated.
  } else if (want > cur) {
    if (contractFieldAllowed(g, want, now) < want) return;          // S9: held. Asked again next pass.
    // S4: a native look that was running dark (b=0) comes out of the dark: that is the look
    // arriving on the field, a change of look like any other.
    if (cur == 0 && !contractLookAllowed(g, contractLookSigNative((int)gEngineSeq[u]), now)) return;
    gRise[u] = true; gRiseLevel[u] = want; gRiseMs[u] = now;
  } else if (want < cur) {
    contractFieldGate(g, want, now);
  }
  gBriGoing[u] = false;
  if (!ours && want == gBriSaved[u]) _engineBriRestore(u);          // back at the owner's value: the engine's again
  else if (want != cur) _engineBri(u, want);
}
// Select a sequence. THE one place a sequence is selected, and never inside the effect (file header).
static inline void _choose(uint8_t u, uint8_t kind, byte seq) {
  _select(u, seq);
  if (kind == SH_ENGINE) { _reset(u); gRestart[u] = true; }   // give it back: the native idle look, started afresh
  gShown[u] = kind; gShownCode[u] = seq;
  // The engine starts it at its next frame and says so (contractEngineLatched), unless it is on
  // that very sequence already: then nothing changes and nothing is said.
  gLatched[u] = gEngineSeq[u] == seq && !gRestart[u];
}
// A selection the engine has not started yet can be undone from outside, without a trace: someone
// selects, before the next frame, the sequence the engine is ALREADY on (a button for the look
// that is up). Then nothing is pending in the engine, nothing will start, and nothing will be
// reported. hasEffectChanged() is the engine's own "a selection is waiting", so that case is
// visible at once: the layer selects again. A button for the look that is already up changes
// nothing; it does not cancel a line. (A button for any OTHER sequence is still waiting in the
// engine, starts at the next frame, and is the take-over of L6.)
static inline void _reassert(uint8_t u) {
  if (gLatched[u] || gShown[u] == SH_ENGINE || _engine(u) == nullptr) return;
  if (!_engine(u)->hasEffectChanged()) _select(u, gShown[u] == SH_CONTRACT ? FX_CONTRACT : gShownCode[u]);
}
// Is a selection waiting in the engine for its next frame? The layer's own last one, or someone
// else's (a button): the engine cannot say whose, only that one is. Then nothing else is selected
// until it has started, one frame later at most. That way a button pressed inside that frame is
// never replaced without a trace: it starts, and is the take-over of L6. (Not at power-on, when
// the engine's first sequence is what is waiting; and never for X, which always lands.)
static inline bool _selectionWaiting(uint8_t u) {
  return _engine(u) != nullptr && _engine(u)->hasEffectChanged() && !(gLatched[u] && gRestart[u]);
}
// Put the unit's state on its display. Runs after every line that changes a unit, and every pass
// from contractTick(), which is what ends an overlay and makes a hand-over the gate held.
static inline void _present(uint8_t u, uint32_t now) {
  ContractUnitState& g = gContract[u];
  _briSync(u);
  _reassert(u);
  if (contractOverlayUp(g, now) || g.base == CB_LOOK || g.base == CB_DARK) {   // effect 106 draws all of these
    if (gShown[u] != SH_CONTRACT) {
      // L6: a look can come due, or a line arrive, just after someone pressed a button, inside
      // one engine frame. Selecting 106 now would replace the button's sequence without a trace.
      // Only X does that.
      if (!gStopping && _selectionWaiting(u)) return;
      _choose(u, SH_CONTRACT, FX_CONTRACT);
    }
    // An X that follows a button inside one engine frame: effect 106 is the display's (running,
    // or selected a moment ago), and another sequence may be waiting to start in its place. X is
    // the later word and X always lands: 106 is selected again, which leaves nothing else
    // waiting. Only for X. Otherwise a waiting sequence is the owner's take-over, and it starts (L6).
    else if (gStopping && _engine(u) && _engine(u)->hasEffectChanged()) _choose(u, SH_CONTRACT, FX_CONTRACT);
    // Effect 106 does not go through the engine's brightness. Once it is drawing, whatever the
    // contract wrote there for a native look goes back to the owner's value: no one sees it change.
    else if (gLatched[u]) _engineBriRestore(u);
    return;
  }
  // A native look: the sequence the contract chose (CB_NATIVE), or the engine's own again (CB_NONE).
  const bool own  = g.base != CB_NATIVE;
  const byte seq  = own ? NATIVE_NORMAL : (byte)g.nativeCode;
  if (own ? gShown[u] != SH_ENGINE : (gShown[u] != SH_NATIVE || gShownCode[u] != seq)) {
    if (_selectionWaiting(u)) return;                   // L6: see above. Asked again next pass.
    if (!own && gEngineSeq[u] == seq && !gRestart[u]) {
      // The engine is on that very sequence already (the idle look taken as a native:0 look; a
      // native section the contract was about to leave and now keeps). It carries on: nothing
      // starts, so nothing will be reported, and a different brightness is a brightness change
      // on a look that is up (_nativeBright below), not part of a hand-over.
      gShown[u] = SH_NATIVE; gShownCode[u] = seq; gLatched[u] = true;
    } else {
      // S9: handing the LEDs to a native look lights them, at the brightness it will run at.
      // From a dark display, inside the window, that waits: what is up stays up, and the next
      // pass asks again. Lights-out is level 0 and is never held. This only ASKS the gate. The
      // look gets its brightness, and the gate is TOLD, at the frame the engine starts the
      // sequence (contractEngineLatched): a selection can still be replaced before that frame,
      // and nothing may be said, or written, about light that never reached the LEDs.
      // S10: the engine is on a sequence whose light is not known: one that is NOT on the
      // allow-list (someone's button, and a line now asks for a native look), or one that draws
      // at a level of its own. The display does not go from it straight to another look that is
      // lit: effect 106 takes it first and holds its last frame (ContractRender), the gate takes
      // the field as dark and just lit (contractEngineLatched), and the look follows when the
      // window has passed.
      if (seq != NATIVE_LIGHTSOUT && gEngineSeq[u] != FX_CONTRACT &&
          (!nativeAllowedLogics((int)gEngineSeq[u]) || _ownLevel(gEngineSeq[u]))) {
        if (gShown[u] != SH_CONTRACT) _choose(u, SH_CONTRACT, FX_CONTRACT);
        return;
      }
      // (S4 for every path: a native look taking the display is a change of look like any other.)
      if (!contractNativeMayStart(g, seq, _nativeFieldLevel(seq, _level(g)), now)) return;
      _choose(u, own ? SH_ENGINE : SH_NATIVE, seq);
      return;
    }
  }
  if (gLatched[u]) _nativeBright(u, now);
}

// L6, native take-over. Something other than the contract started a sequence on a display the
// contract holds: a button on the controller (@nT...), the web page, a command through the
// engine's own handler. The show ends on this display, as M:v=idle would end it, and the sequence
// runs: nothing here touches the display. The owner's brightness goes back at this very frame, before
// the sequence draws: it is the owner's light from its first frame, and nothing is left to raise
// later on a look whose own flashes the contract could not keep clear of.
static inline void _stopClockIfUnused();
static inline void _takenOver(uint8_t u) {
  ContractUnitState& g = gContract[u];
  contractOnIdle(g, _ownerBri(u));
  _stopClockIfUnused();
  gShown[u] = SH_ENGINE;
  _engineBriRestore(u);
  gBriGoal[u] = _level(g); gBriGoing[u] = false;
}
// The engine has started sequence `seq` on display u, at this frame. Called by main.cpp's effect
// selectors from INSIDE the engine's animate(), after it has restored its settings and before the
// effect draws (LogicEngine.h:804-818). It records, and it may set the brightness the sequence
// starts with; it selects nothing (a selection made here would be lost exactly as one made inside
// an effect is).
inline void contractEngineLatched(uint8_t u, unsigned seq) {
  ContractUnitState& g = gContract[u];
  const uint32_t now = millis();
  const unsigned was = gEngineSeq[u];
  _briSync(u);
  gEngineSeq[u] = (uint16_t)seq; gRestart[u] = false;
  if (gRise[u]) {
    // A brighter native look was written less than a frame ago, and something else starts now.
    // Another native sequence starts AT that brightness, so it is on the LEDs: tell the gate.
    // Effect 106 starting leaves it unknown whether the brighter look was ever drawn. Telling
    // the gate of a peak that never was is the one thing it must not be told (contract_core.h),
    // so it is told only the safe half: the field may have lit just now. The contract look that
    // follows then waits out the window if it would light the display; a blackout does not wait.
    if (seq == FX_CONTRACT) g.fieldOnsetMs = now;
    else contractFieldForced(g, gRiseLevel[u], now);
    gRise[u] = false;
  }
  // N2 / S10. The sequence that ENDS at this frame.
  //   * One that is not on the allow-list (someone's button: FLASHCOLOR, FAILURE, fire) is not
  //     known to the gate at all, and one that draws at a level of its own (a scan line, plasma)
  //     may stand anywhere between dark and full. Whatever starts now, the field is taken as
  //     dark and just lit: the contract's next light keeps 340 ms from this frame.
  //   * A native look the contract takes over from (effect 106 starts) was not necessarily
  //     steady light either: PULSE breathes, COLORSWAP changes its hue. It may have lit the field
  //     a moment ago, and nothing told the gate: the 340 ms run from this frame. NORMAL is the
  //     exception (a steady shimmer: it is what an armed show leaves up until it begins, and a
  //     show must be able to begin), with lights-out.
  if (was != FX_CONTRACT) {
    if (!nativeAllowedLogics((int)was) || _ownLevel(was)) contractNativeEnded(g, false, now);
    else if (seq == FX_CONTRACT && was != NATIVE_NORMAL && was != NATIVE_LIGHTSOUT) contractNativeEnded(g, true, now);
  }
  if (seq == FX_CONTRACT) {                             // its frames go through the gate one by one (ContractRender)
    if (gShown[u] == SH_CONTRACT) gLatched[u] = true;
    return;                                             // (selected by number, by someone else: ContractRender hands it straight back)
  }
  // A NATIVE sequence starts on the LEDs now.
  const bool mine = gShown[u] == SH_NATIVE && seq == gShownCode[u];
  const bool back = gShown[u] == SH_ENGINE && !gLatched[u] && seq == NATIVE_NORMAL;   // the display the contract gave back
  // A timed native section that ran out: the engine fell back to its default look. Not a
  // take-over; the show goes on and its next section takes the display. (The owner choosing
  // NORMAL by hand looks the same from here, and does no harm either. An engine whose default
  // effect has been set to something other than NORMAL falls back to THAT, which reads as a
  // take-over: the show ends on the display when a timed native section runs out.)
  const bool ranOut = gShown[u] == SH_NATIVE && gLatched[u] && seq == NATIVE_NORMAL;
  if (gShown[u] != SH_ENGINE && !mine && !ranOut) _takenOver(u);     // L6: a display the contract held
  gLatched[u] = true;
  // S10: a native look runs as stock. Its frames are not held; the gate is TOLD it started, and
  // at what level (contractNativeStarted): 0 for lights-out, the engine's brightness for a
  // sequence that draws at it, full scale for one that draws at a level of its own (N4: 19..24
  // and the custom effects; the engine's brightness is left for _nativeBright() to keep).
  if (!nativeAllowedLogics((int)seq) && gBriTouched[u]) {           // the owner's own sequence: at the owner's brightness, from
    _engineBriRestore(u); gBriGoing[u] = true;                      // its first frame (_nativeBright leaves it alone)
  }
  uint8_t level = _nativeFieldLevel(seq, _engineBriNow(u));         // (someone else's sequence: at the brightness that is there)
  if ((mine || back) && seq != NATIVE_LIGHTSOUT && !_ownLevel(seq)) {
    // The look the contract selected lights the display at this frame, at the brightness the
    // contract gives it: what it asks (b=, B; the owner's own on a display given back), as far
    // as the gate allows right now. _nativeBright() takes it the rest of the way when it may.
    // (And only if a change of look may be shown now, S4: the hand-over was asked for at the
    // level the look had then, and a look asked for dark, b=0, is never held. If it is to be lit
    // by now, it starts dark all the same and comes up when the look gate allows.)
    const uint8_t want = _level(g);
    level = contractFieldAllowed(g, want, now);
    if (level != 0 && !contractLookAllowed(g, contractLookSigNative((int)seq), now)) level = 0;
    gBriGoal[u] = want; gBriGoing[u] = level != want;
    if (!_dimsNative(g) && level == _ownerBri(u)) _engineBriRestore(u);
    else if (level != _engineBriNow(u)) _engineBri(u, level);
  }
  contractNativeStarted(g, (int)seq, level, now);
}

// beat-modulated brightness for sustained looks. Level math lives in contract_core's
// envBright() — the SHARED envelope all three boards render through (the look's level is the
// ceiling of the pump, m= is the dip depth). Do not reintroduce a board-local floor here: it
// desyncs the Logics from the PSIs/HPs on every cue.
static inline uint8_t _beatBright(const ContractUnitState& g, const BeatPos& bp) {
  const uint8_t level = _level(g);
  if (g.accentMode == 0 || g.beatMod == 0 || !gBeat.running) return level;
  const uint32_t prog = beatSectionProgress16(bp.beatIndex, g.sectionStart, g.sectionEnd);   // Q16 (no float in the frame path)
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
//
// `bright` is the overlay's own brightness fraction (P's b=; the look's for a scored accent). It
// is taken only by an accent that ARMS, and it never touches the base look (S9).
static inline void _fireAccent(uint8_t u, ContractEffect fx, const ContractRGB& c, uint32_t durMs, uint8_t bright) {
  ContractUnitState& g = gContract[u];
  uint32_t now = millis();
  if (g.base == CB_NATIVE) return;      // R1: no overlay on a native look — an accent never restarts a native sequence
  if (!strobeCoolDownExpired(g.pulseLastMs, now, 2 * STROBE_MIN_STATE_MS)) return;
  g.pulseFx     = fx;
  g.pulseColor  = c;
  g.pulseBright = bright;
  g.pulseDurMs  = accentDurClamp(durMs);
  g.pulseStartMs = now;                 // ALWAYS retrigger (defeats a re-send no-op)
  g.pulseLastMs  = now;                 // the cool-down anchor: set on every accent that ARMS
  g.pulseActive  = true;
  _present(u, now);                     // ContractRender draws it (it needs no engine reset)
}

// ---- one frame of one look: the pixels of `eff` at `now`, in colour `col` at level `bri`, on a
// timeline that began at t0. A pure function of its arguments and the clock, so the same frame
// can be drawn twice (S9: once to measure it, and again scaled when the gate holds it down).
static inline void _draw(LogicEngineRenderer& r, const ContractUnitState& g, ContractEffect eff, const ContractRGB& col,
                         uint8_t bri, uint32_t t0, uint32_t now, bool overlay) {
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
      // overlay bri is the overlay's own level by design (an accent must not dip).
      _fill(r, on ? _scaled(col, bri) : CRGB(0, 0, 0));
      break;
    }
    case CE_RAINBOW: {
      uint32_t rate = 20u + (uint32_t)(255 - g.speed) / 4u;              // ms per hue step
      uint8_t hbase = (uint8_t)((now / (rate ? rate : 1)) & 0xFF);
      int step = (W > 0) ? (255 / W) : 8;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          _px(r, x, y, CRGB(CHSV((uint8_t)(hbase + x * step), 255, bri)));
      break;
    }
    case CE_SCAN: {
      int pos = fxScanPos(now - t0, g.speed, W, false);  // the shared timing: one column, ping-pong
      CRGB c = _scaled(col, bri);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          _px(r, x, y, (x == pos) ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_SPARKLE: {
      uint32_t blk = now / (uint32_t)(80 + (255 - g.speed) / 2);
      uint32_t density = 20u + (uint32_t)g.speed / 4u;                   // ~20..84 % lit
      CRGB c = _scaled(col, bri);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          _px(r, x, y, (_hash3(x, y, blk) % 100u) < density ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_METER: {
      int lit = (H * (int)gLevel) / 255;                                 // fill from bottom
      CRGB c = _scaled(col.r || col.g || col.b ? col : ContractRGB{0, 255, 0}, bri);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          _px(r, x, y, ((H - 1 - y) < lit) ? c : CRGB(0, 0, 0));
      break;
    }
    case CE_COMET: {
      int head = fxHead(now - t0, g.speed, W);
      for (int x = 0; x < W; x++) {
        uint8_t cb = fxCometBright(x, head, W);
        uint8_t v = (uint8_t)(((uint16_t)bri * cb) / 255);
        CRGB c = _scaled(col, v);
        for (int y = 0; y < H; y++) _px(r, x, y, c);
      }
      break;
    }
    case CE_CHASE: {
      uint32_t el = now - t0;
      CRGB on = _scaled(col, bri), off(0, 0, 0);
      for (int x = 0; x < W; x++) {
        CRGB c = fxChaseLit(x, el, g.speed) ? on : off;
        for (int y = 0; y < H; y++) _px(r, x, y, c);
      }
      break;
    }
    case CE_WIPE: {
      uint32_t el = now - t0;
      CRGB on = _scaled(col, bri), off(0, 0, 0);
      for (int x = 0; x < W; x++) {
        CRGB c = fxWipeLit(x, el, g.speed, W) ? on : off;
        for (int y = 0; y < H; y++) _px(r, x, y, c);
      }
      break;
    }
    case CE_GRADIENT: {
      uint32_t el = now - t0;
      for (int x = 0; x < W; x++) {
        uint8_t hue = fxGradientHue(x, W, 0, el, g.speed);   // base hue 0, color-independent (like rainbow)
        ContractRGB c = fxHsv2rgb(hue, 255, bri);
        for (int y = 0; y < H; y++) _px(r, x, y, CRGB(c.r, c.g, c.b));
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
          _px(r, x, y, _scaled(col, v));
        }
      break;
    }
    case CE_PULSE: {
      // The shared breathe (fxPulseBright: a 2000..340 ms cycle, never faster, S8) when nothing
      // pumps the look. Under the beat pump the envelope IS the breathe, so the fill is steady
      // and the pump moves it; two modulations at once would beat against each other.
      const bool pumped = !overlay && g.accentMode != 0 && g.beatMod != 0 && gBeat.running;
      const uint8_t v = pumped ? bri : (uint8_t)(((uint16_t)bri * fxPulseBright(now - t0, g.speed)) / 255u);
      _fill(r, _scaled(col, v));
      break;
    }
    case CE_SOLID:
    default:
      _fill(r, _scaled(col, bri));
      break;
  }
}

// A measuring build counts the frames the gate holds back (test/host/gate_held_table.cpp).
#ifndef CONTRACT_GATE_STAT
#define CONTRACT_GATE_STAT(unit, scale)
#endif

// ---- the single fork effect fn: renders unit u's contract state each frame ----
inline bool ContractRender(LogicEngineRenderer& r) {
  uint8_t u = (r.getID() == 1) ? 0 : 1;
  ContractUnitState& g = gContract[u];
  uint32_t now = millis();
  // NB: the score is advanced in contractTick() from mainLoop (not here), so section
  // switching keeps running even while a NATIVE section look (which is not rendered by
  // ContractRender) is active.
  const bool overlay = contractOverlayUp(g, now);
  if (!overlay && g.base != CB_LOOK && g.base != CB_DARK) {
    // Nothing of the contract's left to draw: the display is on its way to a native look. That
    // selection is _present()'s, made from contractTick() — NEVER from in here, where the engine
    // would not notice it (see the file header). Until it is made the LEDs keep their last frame;
    // the tick runs every pass, so that is one frame, or the S9 window when the gate holds the
    // hand-over (and then the last frame is a dark one).
    // If the contract did not put effect 106 here at all (someone selected it by number), there
    // is nothing to wait for: false sends the engine to its default effect (LogicEngine.h:820-830).
    return gShown[u] == SH_CONTRACT;
  }

  // WHAT to draw. The base look, pumped by the beat; or the show's dark baseline; or THE OVERLAY:
  // the accent is an EFFECT SWAP, not a solid fill, so it reuses every renderer below. Its
  // timeline starts at pulseStartMs (the base look's startMs is NEVER touched, so an am=2 accent
  // cannot restart the base look every beat), and it has its own level: the accent rides a
  // level, not the pumped dip, and a P's b= is the overlay's alone.
  ContractEffect eff = CE_OFF;
  ContractRGB    col = g.color;
  uint8_t  bri = 0;
  uint32_t t0  = g.startMs;
  if (overlay) {
    eff = (g.pulseFx == CE_NONE) ? CE_SOLID : g.pulseFx;     // CE_NONE == the legacy solid fill
    col = g.pulseColor;
    bri = contractLevel(g.ceil, g.pulseBright);
    t0  = g.pulseStartMs;
  } else if (g.base == CB_LOOK) {
    eff = g.effect;
    bri = _beatBright(g, beatPosAt(gBeat, now));
  }

  // S4 FOR EVERY PATH: a frame that would show another look or colour than the field last showed
  // comes at least 170 ms after the field changed to that one, whichever path asked for it (an
  // accent between two looks, a section, a look after a native one). Until then the frame that is
  // up stays up. Black is not a look: a blackout is never held.
  if (eff != CE_OFF && bri != 0 && !contractLookGate(g, contractLookSig(eff, col), now)) return true;

  // S9: draw, measure, and ASK the field gate, by the frame's level and by its luminance. A
  // frame the gate holds down is drawn again, scaled (the scale rounds down, LED by LED, so the
  // second frame is at or under what the gate allowed) and measured again: the gate is TOLD
  // what is on the LEDs.
  gPxSum = 0; gPxLum = 0; gPxCount = 0; gPxScale = 255;
  _draw(r, g, eff, col, bri, t0, now, overlay);
  uint8_t level = gPxCount ? (uint8_t)(gPxSum / gPxCount) : 0;
  uint8_t lum   = gPxCount ? (uint8_t)(gPxLum / gPxCount) : 0;
  const uint8_t scale = contractFieldAsk(g, level, lum, now);
  CONTRACT_GATE_STAT(u, scale);
  if (scale != 255) {
    gPxSum = 0; gPxLum = 0; gPxCount = 0; gPxScale = scale;
    _draw(r, g, eff, col, bri, t0, now, overlay);
    gPxScale = 255;
    level = gPxCount ? (uint8_t)(gPxSum / gPxCount) : 0;
    lum   = gPxCount ? (uint8_t)(gPxLum / gPxCount) : 0;
  }
  contractFieldTell(g, level, lum, now);
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
//   x     blacks out, at once; the unit stays in its mode, with its watchdog.
//   idle  gives the display back: NORMAL runs again with nothing of the show left selected.
// A display the contract never took is left alone, whatever the engine is running on it.
// Giving a DARK display back lights it, so that waits for the S9 window when it has to (at most
// 340 ms); the owner's brightness goes back with it (_nativeBright).
static inline void _boundary(uint8_t u, char kind, uint8_t wdSec) {
  ContractUnitState& g = gContract[u];
  if (kind == 's')      contractOnShow(g, _ownerBri(u), wdSec);
  else if (kind == 'x') contractOnStop(g, _ownerBri(u));
  else                  contractOnIdle(g, _ownerBri(u));
  _stopClockIfUnused();
  gBriGoal[u] = _level(g); gBriGoing[u] = gBriTouched[u];    // the engine's brightness: back to the owner's, if the show moved it
  _present(u, millis());
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
  contractLookRestart(g);                         // S4 at the output: the look's timeline starts again
  if (lp.has & LP_EFFECT) {
    g.effect = lp.effect; g.nativeCode = lp.nativeCode;
    g.base = (lp.effect == CE_NATIVE && lp.nativeCode >= 0) ? CB_NATIVE : CB_LOOK;
  }
  _present(u, now);
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
        // A scored A carries its look. Without i= (or with one this firmware does not know) the
        // line is dropped: an entry is a whole section, not a patch, and "no look" used to be
        // stored as solid black.
        if (!pr.hasEffect) break;
        ScoreEntry e;
        e.atBeat = pr.atBeat;
        e.effect = pr.effect;
        e.nativeCode = pr.nativeCode;
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
      if (contractShowBegin(g)) _present(u, now); // L1: the show's first P: the baseline goes dark first
      _fireAccent(u, (pr.hasEffect && accentEffectAllowed(pr.effect)) ? pr.effect : CE_SOLID,
                  pr.hasColor ? pr.color : ContractRGB{255, 255, 255},
                  pr.hasDur ? pr.durMs : 120,
                  pr.hasBright ? pr.bright : g.bright);       // S9: b= is the overlay's, never the base look's
      break;
    }
    case CV_LEVEL:
      if (pr.hasLevel) gLevel = pr.level;         // drives CE_METER (contract meter reads gLevel)
      break;
    case CV_BRIGHT:
      if (!contractBrightHonoured(g)) break;      // S6: only in show mode (a host sends B after M:v=show);
                                                  // every reset clears it, so a B never outlives its show
      g.bright = pr.hasBright ? pr.bright : (pr.hasLevel ? pr.level : g.bright);   // a fraction of the ceiling
      _present(u, now);                           // a contract look takes it at its next frame; a native look through
      break;                                      // _nativeBright() (it survives restoreSettings()). Both through the S9 gate.
    case CV_STOP:
      gStopping = true; _boundary(u, 'x', 0); gStopping = false;
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
    gQueryLen[u] = contractFormatQuery(gQueryLine[u], sizeof gQueryLine[u], p.params.route, 'L', p.unit, SCORE_MAX,
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
      if (k == CS_SEEK && (u == 0 ? f : r) && contractShowBegin(g)) _present(u, now);   // L1: the show's first C
    }
    return;
  }
  const uint16_t crc = (p.verb == CV_ANIMATE && p.params.hasAt) ? contractCrc16(afterBang) : 0;
  if (f) applyContractToUnit(0, p, crc);
  if (r) applyContractToUnit(1, p, crc);
}
// The pending Q reply (section 10): the front display's first, then the rear's. mainLoop() writes
// one a pass, when Serial1 can take it whole, and then calls contractReplyDone() for that one.
inline bool contractReplyPending(const char*& line, uint8_t& n) {
  const uint8_t u = gQueryLen[0] ? 0 : 1;
  line = gQueryLine[u]; n = gQueryLen[u];
  return n != 0;
}
inline void contractReplyDone() { gQueryLen[gQueryLen[0] ? 0 : 1] = 0; }

// Runs from mainLoop() every pass (NOT from an effect fn), so it keeps working while a native
// look, which ContractRender does not draw, is on a display, and so that every sequence
// selection is made outside the effect.
inline void contractTick() {
  const uint32_t now = millis();
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (contractWatchdogExpired(g, gLastLineMs, now)) _boundary(u, 'i', 0);   // L2: the host went away
    ContractLookPatch lp;
    if (liveLookDue(g, now, lp)) _applyLook(u, lp, now);                      // S4: a held live look's window ended
    if (gRise[u] && (uint32_t)(now - gRiseMs[u]) >= ENGINE_FRAME_MS) {        // S9: the engine has drawn the brighter native
      contractNativeLevel(g, gRiseLevel[u], now); gRise[u] = false;           // look by now: tell the gate it lit
      if (gRiseLevel[u] != 0) contractLookForced(g, contractLookSigNative((int)gEngineSeq[u]), now);
    }
    _present(u, now);                             // an overlay that ended; a hand-over or a brightness the S9 gate held
  }
  if (!gBeat.running) return;                     // no clock (or held, C6): the score does not move
  const BeatPos bp = beatPosAt(gBeat, now);
  for (uint8_t u = 0; u < 2; u++) {
    ContractUnitState& g = gContract[u];
    if (g.scoreLen == 0) continue;
    int idx = scoreActiveIndex(gScore[u], g.scoreLen, bp.beatIndex);
    idx = scoreCursor(g.curSection, idx, g.pinForward);                       // C2
    if (idx < 0 || idx == g.curSection) continue;
    // S4 for every path: a section is a look change like a live A, and comes 170 ms after the
    // unit's last one at the earliest. (A C seek can put the beat in another section every few
    // milliseconds.) A section that has to wait is taken by a later pass, if the beat is still in it.
    if (!contractSectionMayApply(g, now)) continue;
    const ScoreEntry& s = gScore[u][idx];
    _arm(u);
    g.effect = s.effect; g.nativeCode = s.nativeCode; g.color = s.color; g.speed = s.speed;
    g.bright = s.bright;
    g.beatMod = s.beatMod; g.accentMode = s.accentMode; g.accentEvery = s.accentEvery;
    g.curSection = (int8_t)idx; g.startMs = now; g.sectionStart = s.atBeat;
    contractLookRestart(g);
    g.sectionEnd = (idx + 1 < g.scoreLen) ? gScore[u][idx + 1].atBeat : (s.atBeat + 9999);
    g.pulseActive = false;
    g.base = (s.effect == CE_NATIVE && s.nativeCode >= 0) ? CB_NATIVE : CB_LOOK;
    _present(u, now);
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
    _fireAccent(u, e.accentFx, e.accentColor, (uint32_t)e.accentDur10 * 10u, g.bright);   // it rides the look's level
  }
}

// The Marcduino action stays for whoever calls Marcduino::processCommand() with a '!' line
// (the web UI, a stored sequence). '!' is collision-free vs every native prefix.
MARCDUINO_ACTION(Contract, !, ({
  contractHandleLine(Marcduino::getCommand());
}))

// Called once from setup(). At power-on the engine's own look is on both displays, and the S9
// gate has to know a display it has never drawn is lit: otherwise the first thing the contract
// does to it would be counted as lighting it. (The native mic-effect PeakValueProvider wiring is
// R2: the level feed.)
inline void contractSetup() {
  for (uint8_t u = 0; u < 2; u++) {
    gContract[u].fieldLit = gContract[u].fieldLitY = true;
    gContract[u].fieldRef = gContract[u].fieldRefY = _ownerBri(u);
    gBriGoal[u] = gBriSaved[u] = _ownerBri(u);
    gRestart[u] = true;                           // the engine starts its first sequence at its first frame
  }
}
