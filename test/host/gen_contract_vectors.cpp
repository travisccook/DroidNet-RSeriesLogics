// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Prints the shared parity vectors as JSON: the real contract_core.h, run on the host, for the
// Studio preview's JS mirror to be checked against. Deterministic: no clock, no RNG.
//   usage: gen_contract_vectors <sha256 of contract_core.h>
// Schema 1: { schema, contract_version, core_sha256, generator,
//             constants: { NAME: int },
//             lines: [ { line: "<bytes after '!'>", crc16: int } ],
//             groups: { <function>: { args: [name...], out: [name...], rows: [[args..., outs...]] } } }
// Every group value is an integer, and since CA1.3 every one is EXACT: the core has no float, so
// a mirror is compared with no tolerance. (phase16 and progress16 are Q16; until CA1.2 the phase
// was a float, carried as phase_e6 and compared within 2.)
// Groups are append-only: a later change adds a group, it never reshapes one.
// A STATEFUL group (contractFieldGate) has "seq" as its first argument: the rows of one seq run in
// order through one state, and a new seq starts from a fresh one (docs/CHANGING_THE_CORE.md).
// Fork-local to RSeries, the canonical home of the shared core; the JSON it prints is committed as
// test/host/vectors/contract_vectors.json and copied to Studio (docs/CHANGING_THE_CORE.md).
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <initializer_list>

static bool gFirstGroup = true, gFirstRow = true;
static void group(const char* name, const char* args, const char* out) {
  printf("%s\n    \"%s\": {\"args\": [%s], \"out\": [%s], \"rows\": [", gFirstGroup ? "" : "\n    ]},", name, args, out);
  gFirstGroup = false; gFirstRow = true;
}
static void row(std::initializer_list<long long> v) {
  printf("%s\n      [", gFirstRow ? "" : ",");
  gFirstRow = false;
  bool first = true;
  for (long long x : v) { printf("%s%lld", first ? "" : ", ", x); first = false; }
  printf("]");
}

// CA1.4: one FRAME through both gates, as a fork that draws colour runs them. frameSeq(ceil)
// starts a sequence; frame() is one frame: its look (effect and colour; effect 0 is a black frame,
// which the look gate never holds), its level and its luminance as drawn.
//   drawn = 0: the look gate refused it (the frame that is up stays up; nothing else changes)
//   scale: contractFieldScale()'s answer, 255 when the frame was not drawn
// and then the state after it. restart = 1: contractLookRestart() runs first (a live A or a
// section was applied).
static int gFrameSeq = 0;
static ContractUnitState gFrame;
static void frameSeq(int ceil) { gFrameSeq++; gFrame = ContractUnitState(); gFrame.ceil = (uint8_t)ceil; }
// `peak`: the brightest channel of the frame's brightest LED (CA1.7: an argument of its own; a
// frame that is the same on every LED has peak = level, which is what -1 asks for).
static void frame(uint32_t nowMs, int effect, uint32_t rgb, int level, int lum, bool restart = false, int peak = -1) {
  if (restart) contractLookRestart(gFrame);
  if (peak < 0) peak = level;
  const ContractRGB c = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb };
  const bool drawn = effect == 0 || contractLookGateAt(gFrame, contractLookSig((ContractEffect)effect, c), (uint8_t)peak, nowMs);
  const uint8_t scale = drawn ? contractFieldScale(gFrame, (uint8_t)level, (uint8_t)lum, nowMs) : 255;
  row({ gFrameSeq, gFrame.ceil, restart ? 1 : 0, (long long)nowMs, effect, (long long)rgb, level, lum, peak,
        drawn ? 1 : 0, scale, gFrame.fieldLit ? 1 : 0, gFrame.fieldRef, gFrame.fieldLitY ? 1 : 0, gFrame.fieldRefY,
        (long long)gFrame.fieldOnsetMs, (long long)gFrame.lookMs, gFrame.fieldRise ? 1 : 0 });
}
// A frame of one colour at k / 255 of its brightness, measured as a fork measures it.
static void frameOf(uint32_t nowMs, int effect, uint32_t rgb, int k, bool restart = false) {
  const uint8_t r = (uint8_t)(((rgb >> 16) & 255u) * (uint32_t)k / 255u), g = (uint8_t)(((rgb >> 8) & 255u) * (uint32_t)k / 255u),
                b = (uint8_t)((rgb & 255u) * (uint32_t)k / 255u);
  frame(nowMs, effect, rgb, r > g ? (r > b ? r : b) : (g > b ? g : b), contractLuminance(r, g, b), restart);
}

// The S9 field gate is a state machine, so its group is sequences, not independent rows.
// gateSeq(ceil) starts one from a fresh ContractUnitState with that ceiling; gate() runs one step
// and prints the answer and the state after it. reset = 1: the show reset (contractShowReset, with
// the row's ceiling) runs first, which is what every M:v=show, X and M:v=idle does.
static int gGateSeq = 0;
static ContractUnitState gGate;
static void gateSeq(int ceil) { gGateSeq++; gGate = ContractUnitState(); gGate.ceil = (uint8_t)ceil; }
static void gate(uint32_t nowMs, int level, bool reset = false, int newCeil = -1) {
  if (reset) contractShowReset(gGate, (uint8_t)(newCeil >= 0 ? newCeil : gGate.ceil));
  const uint8_t allowed = contractFieldGate(gGate, (uint8_t)level, nowMs);
  row({ gGateSeq, gGate.ceil, reset ? 1 : 0, (long long)nowMs, level,
        allowed, gGate.fieldLit ? 1 : 0, gGate.fieldRef, (long long)gGate.fieldOnsetMs });
}

// Verb L's hold (CA1.6) is a state machine too. One row is one pass of a fork's loop at nowMs:
// reset = 1 runs the show reset first; v >= 0 is an L line with that level (contractLevelOffer),
// v = -1 is a pass with no line; then contractLevelDue(). `changed`: what it returned (the unit's
// level changed in this pass). `want`: the last L since the reset.
static int gLevelSeq = 0;
static ContractUnitState gLevelUnit;
static void levelSeq() { gLevelSeq++; gLevelUnit = ContractUnitState(); }
static void levelPass(uint32_t nowMs, int v, bool reset = false) {
  if (reset) contractShowReset(gLevelUnit, 255);
  if (v >= 0) contractLevelOffer(gLevelUnit, (uint8_t)v);
  const bool changed = contractLevelDue(gLevelUnit, nowMs);
  row({ gLevelSeq, reset ? 1 : 0, (long long)nowMs, v, changed ? 1 : 0, gLevelUnit.level, gLevelUnit.levelWant, (long long)gLevelUnit.levelMs });
}

// Verb L and verb B together (CA1.7: contractHoldsDue, one window for both). One row is one pass:
// reset = 1 runs the show reset first; v >= 0 is an L line, b >= 0 a B line or a b= (-1: none);
// then contractHoldsDue(). `changed`: what it returned (bit 0 the level, bit 1 the brightness).
static void holdsPass(uint32_t nowMs, int v, int b, bool reset = false) {
  if (reset) contractShowReset(gLevelUnit, 255);
  if (v >= 0) contractLevelOffer(gLevelUnit, (uint8_t)v);
  if (b >= 0) contractBrightOffer(gLevelUnit, (uint8_t)b);
  const uint8_t changed = contractHoldsDue(gLevelUnit, nowMs);
  row({ gLevelSeq, reset ? 1 : 0, (long long)nowMs, v, b, changed, gLevelUnit.level, gLevelUnit.levelWant, gLevelUnit.bright, gLevelUnit.brightWant,
        (long long)gLevelUnit.levelMs });
}

// A C line with all four keys, received at seedMs.
static void seedClock(BeatClock& bc, int bpm, uint32_t ph, int bpb, long beat, uint32_t seedMs) {
  ContractParams pr;
  pr.hasBpm = true; pr.bpm = (uint16_t)bpm; pr.hasPh = true; pr.phMs = ph;
  pr.hasBpb = true; pr.bpb = (uint8_t)bpb;  pr.hasBeat = true; pr.beatAnchor = (int32_t)beat;
  beatClockSeed(bc, pr, seedMs);
}

int main(int argc, char** argv) {
  const char* ver = CONTRACT_VERSION;
  printf("{\n  \"schema\": 1,\n  \"contract_version\": \"%s\",\n  \"core_sha256\": \"%s\",\n", ver, argc > 1 ? argv[1] : "");
  printf("  \"generator\": \"test/host/gen_contract_vectors.cpp\",\n");
  printf("  \"constants\": {");
  printf("\n    \"CONTRACT_RELEASE\": %d,\n    \"STROBE_MIN_STATE_MS\": %lu,\n", CONTRACT_RELEASE, (unsigned long)STROBE_MIN_STATE_MS);
  printf("    \"CONTRACT_BPM_MIN\": %u,\n    \"CONTRACT_BPM_MAX\": %u,\n    \"CONTRACT_BPB_MAX\": %u,\n",
         (unsigned)CONTRACT_BPM_MIN, (unsigned)CONTRACT_BPM_MAX, (unsigned)CONTRACT_BPB_MAX);
  printf("    \"ACCENT_EDGE_LATE_MS\": %u,\n    \"ACCENT_DUR_DEFAULT_MS\": %u,\n    \"ACCENT_DUR_MAX_MS\": %u,\n",
         (unsigned)ACCENT_EDGE_LATE_MS, (unsigned)ACCENT_DUR_DEFAULT_MS, (unsigned)ACCENT_DUR_MAX_MS);
  printf("    \"ACCENT_EVERY_MAX\": %u,\n    \"CONTRACT_ROUTE_MAX\": %u", (unsigned)ACCENT_EVERY_MAX, (unsigned)CONTRACT_ROUTE_MAX);
  printf(",\n    \"CONTRACT_BEAT_MAX\": %ld", (long)CONTRACT_BEAT_MAX);   // CA1.2: at= and beat= are clamped to +/- this
  printf(",\n    \"CONTRACT_QUERY_MAX\": %u,\n    \"CONTRACT_REPLY_MAX\": %u", (unsigned)CONTRACT_QUERY_MAX, (unsigned)CONTRACT_REPLY_MAX);
  // S7: the native:<n> codes a fork accepts, as the firmware decides them.
  printf(",\n    \"NATIVE_ALLOW_LOGICS\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeAllowedLogics(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  printf(",\n    \"NATIVE_ALLOW_PSI\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeAllowedPsi(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  printf(",\n    \"NATIVE_ALLOW_FLTHY\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeAllowedFlthy(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  // S6: the logics codes that take the board's brightness (b= and B reach them); the others on
  // the list draw at a level of their own (CA1.4).
  printf(",\n    \"NATIVE_DIMS_LOGICS\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeDimsLogics(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  // S10: the allow-listed codes a fork passes through the field gate frame by frame (GATED); the
  // rest of each list runs as stock.
  printf(",\n    \"NATIVE_GATED_LOGICS\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeGatedLogics(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  printf(",\n    \"NATIVE_GATED_PSI\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeGatedPsi(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  printf(",\n    \"NATIVE_GATED_FLTHY\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeGatedFlthy(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  // S10: the engine speed digit the contract starts each logics code with, in the order of
  // NATIVE_ALLOW_LOGICS (CA1.5: the fastest speed at which the stock sequence passes S10's test).
  printf(",\n    \"NATIVE_SPEED_LOGICS\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeAllowedLogics(c)) { printf("%s%d", first ? "" : ", ", nativeSpeedLogics(c)); first = false; } }
  printf("]");
  // CA1.6: the logics codes whose whole field stays within a tenth of the level the gate is told
  // (nativeSteadyLogics). The rest of NATIVE_ALLOW_LOGICS moves its own light: after one of
  // those the field is taken as dark and just lit, and a brighter B reaches it at its next start.
  printf(",\n    \"NATIVE_STEADY_LOGICS\": [");
  { bool first = true; for (int c = 0; c < 256; c++) if (nativeSteadyLogics(c)) { printf("%s%d", first ? "" : ", ", c); first = false; } }
  printf("]");
  // CA1.7: the shortest step of a chase, the shortest lap of a scan there and back (round a ring
  // it is FX_CYCLE_MIN_MS, as a comet's), and the level under which a frame is not a look.
  printf(",\n    \"FX_CHASE_STEP_MIN_MS\": %lu,\n    \"FX_SCAN_LAP_MIN_MS\": %lu,\n    \"LOOK_MIN_LEVEL\": %u",
         (unsigned long)FX_CHASE_STEP_MIN_MS, (unsigned long)FX_SCAN_LAP_MIN_MS, (unsigned)LOOK_MIN_LEVEL);
  // CA1.5, the shared timing: the shortest cycle of pulse and of wipe, and flash's shortest state.
  printf(",\n    \"FX_CYCLE_MIN_MS\": %lu,\n    \"FX_FLASH_MIN_STATE_MS\": %lu", (unsigned long)FX_CYCLE_MIN_MS, (unsigned long)FX_FLASH_MIN_STATE_MS);
  // S9: the gate's step is more than a tenth of the ceiling and at least 2 levels: ceil / DIV + 1.
  printf(",\n    \"FIELD_STEP_DIV\": 10,\n    \"FIELD_STEP_MIN\": 2");
  // CA2 (v1.4): logic's grey, its hue base and the floors of its pace; projector's step and the
  // spacing of its brightening steps.
  printf(",\n    \"FX_LOGIC_GREY\": %u,\n    \"FX_LOGIC_HUE_BASE\": %u,\n    \"FX_LOGIC_FADE_MIN\": %u,\n    \"FX_LOGIC_DELAY_MIN\": %u",
         (unsigned)FX_LOGIC_GREY, (unsigned)FX_LOGIC_HUE_BASE, (unsigned)FX_LOGIC_FADE_MIN, (unsigned)FX_LOGIC_DELAY_MIN);
  printf(",\n    \"FX_PROJECTOR_STEP_MIN_MS\": %lu,\n    \"FX_PROJECTOR_STEP_MAX_MS\": %lu,\n    \"FX_PROJECTOR_BRIGHT_MS\": %lu",
         (unsigned long)FX_PROJECTOR_STEP_MIN_MS, (unsigned long)FX_PROJECTOR_STEP_MAX_MS, (unsigned long)FX_PROJECTOR_BRIGHT_MS);
  // CA2, from the model of the walk (test_logic_regions.cpp): the shortest gap between two of logic's pump rises.
  printf(",\n    \"FX_LOGIC_PUMP_MIN_MS\": %lu", (unsigned long)FX_LOGIC_PUMP_MIN_MS);
  // MORE CONSTANTS GO HERE (each starts with ",\n    ")
  printf("\n  ");
  printf("},\n  \"lines\": [");
  // The delivery plan's verb-V reference lines come first (v_vectors.py pins the same CRCs).
  static const char* const kLines[] = {
    "LFA:i=native:0,c=3b82f6,at=0", "LFA:i=rainbow,c=f97316,at=56,am=1,m=200,ae=flash", "LFA:i=native:0,at=447",
    "L*A:i=solid,c=0080FF,at=8", "**A:i=off,at=512", "LFA:i=comet,c=ff0000,at=56",
    "L*A:i=colorcycle,c=3b82f6,s=255,b=255,at=99999,am=2,m=255,an=16,ap=15,ae=colorcycle,ac=ffffff,ad=2550",
    "PFA:i=solid,c=f97316,at=44,am=1", "HTA:i=twinkle,c=00ff88,b=120,at=-8", "123456789", "",
    // CA2 (v1.4): one line for each new id
    "LFA:i=logic,c=116530,s=200,at=16,am=2,m=255", "HTA:i=projector,c=ff0000,b=200,at=32", "PFA:i=vu,am=2,m=200,at=8" };
  for (size_t i = 0; i < sizeof kLines / sizeof kLines[0]; i++)
    printf("%s\n    {\"line\": \"%s\", \"crc16\": %u}", i ? "," : "", kLines[i], (unsigned)contractCrc16(kLines[i]));
  printf("\n  ");
  printf("],\n  \"groups\": {");
  static const int speeds[] = { 0, 1, 64, 128, 200, 254, 255 };
  static const int strands[] = { 1, 6, 8, 10, 24, 28 };

  group("fxStepMs", "\"speed\"", "\"ms\"");
  for (int s = 0; s <= 255; s += 5) row({ s, (long)fxStepMs((uint8_t)s) });
  group("fxHash16", "\"x\"", "\"h\"");
  for (uint32_t x : { 0u, 1u, 2u, 7u, 255u, 256u, 65535u, 65536u, 2654435761u, 4294967295u }) row({ (long)x, (long)fxHash16(x) });
  group("fxHsv2rgb", "\"h\", \"s\", \"v\"", "\"r\", \"g\", \"b\"");
  for (int h = 0; h <= 255; h += 3) for (int s : { 0, 128, 255 }) for (int v : { 64, 200, 255 }) {
    ContractRGB c = fxHsv2rgb((uint8_t)h, (uint8_t)s, (uint8_t)v); row({ h, s, v, c.r, c.g, c.b });
  }
  group("fxHead", "\"elapsed\", \"speed\", \"N\"", "\"head\"");
  for (int n : strands) for (int s : speeds) for (uint32_t t = 0; t < 6000; t += 373) row({ (long)t, s, n, fxHead(t, (uint8_t)s, n) });
  group("fxCometBright", "\"p\", \"head\", \"N\"", "\"bright\"");
  for (int n : strands) for (int h = 0; h < n; h += (n > 8 ? 5 : 1)) for (int p = 0; p < n; p++) row({ p, h, n, fxCometBright(p, h, n) });
  // CA1.5: fxChaseLit takes N (every third position of a ring of N: a constant lit count).
  group("fxChaseLit", "\"p\", \"elapsed\", \"speed\", \"N\"", "\"lit\"");
  for (int n : { 6, 7, 8, 10 }) for (int s : { 0, 128, 255 }) for (uint32_t t = 0; t < 2000; t += 211) for (int p = 0; p < n; p++)
    row({ p, (long)t, s, n, fxChaseLit(p, t, (uint8_t)s, n) ? 1 : 0 });
  group("fxWipeLit", "\"p\", \"elapsed\", \"speed\", \"N\"", "\"lit\"");
  for (int n : { 6, 8, 10 }) for (int s : { 0, 128, 255 }) for (uint32_t t = 0; t < 4000; t += 331) for (int p = 0; p < n; p++) row({ p, (long)t, s, n, fxWipeLit(p, t, (uint8_t)s, n) ? 1 : 0 });
  group("fxGradientHue", "\"p\", \"N\", \"baseHue\", \"elapsed\", \"speed\"", "\"hue\"");
  for (int n : strands) for (int s : { 0, 128, 255 }) for (uint32_t t = 0; t < 9000; t += 1777) for (int p = 0; p < n; p += (n > 8 ? 5 : 1)) row({ p, n, 0, (long)t, s, fxGradientHue(p, n, 0, t, (uint8_t)s) });
  group("fxCycleHue", "\"baseHue\", \"elapsed\", \"speed\"", "\"hue\"");
  for (int s : speeds) for (uint32_t t = 0; t < 60000; t += 3331) row({ 0, (long)t, s, fxCycleHue(0, t, (uint8_t)s) });
  group("fxTwinkleBright", "\"idx\", \"now\", \"speed\"", "\"bright\"");
  for (int s : { 0, 128, 255 }) for (int i : { 0, 1, 2, 7, 47, 79, 111 }) for (uint32_t t = 0; t < 5000; t += 137) row({ i, (long)t, s, fxTwinkleBright(i, t, (uint8_t)s) });
  group("envBright", "\"bright\", \"beatMod\", \"amt\"", "\"level\"");
  for (int b : { 0, 1, 90, 160, 200, 255 }) for (int m : { 0, 1, 64, 200, 255 }) for (int a : { 0, 1, 100, 200, 255 }) row({ b, m, a, envBright((uint8_t)b, (uint8_t)m, (uint8_t)a) });
  group("strobeCoolDownExpired", "\"lastMs\", \"nowMs\", \"gapMs\"", "\"ok\"");
  for (uint32_t last : { 0u, 1u, 1000u, 4294967040u }) for (uint32_t d : { 0u, 1u, 169u, 170u, 339u, 340u, 341u, 5000u })
    for (uint32_t gap : { 170u, 340u }) row({ last, (uint32_t)(last + d), gap, strobeCoolDownExpired(last, last + d, gap) ? 1 : 0 });
  // The clock, driven the way the wire drives it: a C line (bpm, ph, bpb, beat) received at seedMs, read
  // at nowMs. Times are uint32 millis and some rows cross the 2^32 wrap.
  group("beatPosAt", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\"", "\"beatIndex\", \"barPos\", \"phase16\"");
  for (int bpm : { 30, 97, 120, 137, 174, 300 }) for (int bpb : { 3, 4 }) for (uint32_t ph : { 0u, 40u, 499u })
    for (long beat : { -3L, 0L, 64L }) for (uint32_t seed : { 1000u, 4294960000u })
      for (uint32_t d : { 0u, 1u, 39u, 40u, 500u, 12345u, 600000u, 3600000u }) {
        BeatClock bc; seedClock(bc, bpm, ph, bpb, beat, seed);
        BeatPos bp = beatPosAt(bc, seed + d);
        row({ bpm, ph, bpb, beat, seed, (uint32_t)(seed + d), bp.beatIndex, bp.barPos, bp.phase16 });
      }
  // A full seed, then the host's bare ph/beat re-anchor (no bpm=), then the read.
  group("beatPosAtReseed", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"ph2\", \"beat2\", \"seedMs2\", \"nowMs\"",
        "\"beatIndex\", \"barPos\", \"phase16\"");
  for (int bpm : { 97, 120, 135, 174 }) for (uint32_t seed : { 5000u, 4294900000u }) for (uint32_t gap : { 4000u, 4013u, 20000u })
    for (uint32_t ph2 : { 0u, 17u, 230u }) for (uint32_t d : { 0u, 16u, 17u, 250u, 3999u, 60000u }) {
      BeatClock bc; seedClock(bc, bpm, 0, 4, 0, seed);
      const uint32_t seed2 = seed + gap;
      const long beat2 = (long)beatPosAt(bc, seed2).beatIndex + 1;     // the next beat, as the host sends it
      ContractParams pr; pr.hasPh = true; pr.phMs = ph2; pr.hasBeat = true; pr.beatAnchor = (int32_t)beat2;
      beatClockSeed(bc, pr, seed2);
      BeatPos bp = beatPosAt(bc, seed2 + d);
      row({ bpm, 0, 4, 0, seed, ph2, beat2, seed2, (uint32_t)(seed2 + d), bp.beatIndex, bp.barPos, bp.phase16 });
    }
  group("beatRateDivisor", "\"bpm\"", "\"k\"");
  for (int bpm = 30; bpm <= 300; bpm++) row({ bpm, beatRateDivisor((uint16_t)bpm) });
  group("barRateDivisor", "\"bpm\", \"bpb\"", "\"k\"");
  for (int bpb = 1; bpb <= 16; bpb++) for (int bpm : { 30, 120, 176, 177, 240, 300 }) row({ bpm, bpb, barRateDivisor((uint16_t)bpm, (uint8_t)bpb) });
  // The whole accent path from integers: seed a clock, read it at nowMs, then the fire predicate,
  // the envelope amount for depth m (progress from the section bounds), and the level at b=200.
  group("accentAt", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\", \"am\", \"m\", \"sectionStart\", \"sectionEnd\"",
        "\"fires\", \"amt\", \"env200\"");
  for (int bpm : { 97, 120, 176, 177, 240, 300 }) for (int bpb : { 3, 4 }) for (int am = 0; am <= 5; am++)
    for (int m : { 0, 64, 200, 255 }) for (uint32_t d : { 0u, 30u, 130u, 260u, 510u, 1040u, 2011u, 4003u, 7990u }) {
      const uint32_t seed = 4294965000u;                              // crosses the millis wrap
      BeatClock bc; seedClock(bc, bpm, 0, bpb, 0, seed);
      const BeatPos bp = beatPosAt(bc, seed + d);
      const uint8_t amt = beatAccentAmount((uint8_t)am, bp, (uint8_t)m, beatSectionProgress16(bp.beatIndex, 0, 16), bc);
      row({ bpm, 0, bpb, 0, seed, (uint32_t)(seed + d), am, m, 0, 16,
            beatAccentFires((uint8_t)am, bp, bc) ? 1 : 0, amt, envBright(200, (uint8_t)m, amt) });
    }
  // The same path for am=2 with an=/ap= (every-N accents).
  group("accentAtEvery", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\", \"m\", \"an\", \"ap\"",
        "\"fires\", \"amt\", \"env200\"");
  for (int bpm : { 120, 177, 300 }) for (int an : { 1, 2, 3, 4, 8, 16 }) for (int ap : { 0, 1, 3, 15 })
    for (long beat0 : { -5L, 0L }) for (uint32_t d = 0; d < 9000; d += 621) {
      if (ap >= an && ap != 15) continue;                             // ap=15 with a small n: the out-of-range case
      const uint32_t seed = 60000u;
      BeatClock bc; seedClock(bc, bpm, 0, 4, beat0, seed);
      const BeatPos bp = beatPosAt(bc, seed + d);
      const uint8_t every = accentEveryPack((uint8_t)an, (uint8_t)ap);
      const uint8_t amt = beatAccentAmount(2, bp, 200, 0, bc, every);
      row({ bpm, 0, 4, beat0, seed, (uint32_t)(seed + d), 200, an, ap,
            beatAccentFires(2, bp, bc, every) ? 1 : 0, amt, envBright(200, 200, amt) });
    }
  group("fxFlashOn", "\"elapsed\", \"speed\"", "\"on\"");
  for (int s : speeds) for (uint32_t t = 0; t < 2000; t += 53) row({ t, s, fxFlashOn(t, (uint8_t)s) ? 1 : 0 });
  group("fxPulseBright", "\"elapsed\", \"speed\"", "\"bright\"");
  for (int s : speeds) for (uint32_t t = 0; t < 4100; t += 67) row({ t, s, fxPulseBright(t, (uint8_t)s) });
  group("fxScanPos", "\"elapsed\", \"speed\", \"N\", \"wrap\"", "\"pos\"");
  for (int n : strands) for (int w : { 0, 1 }) for (int s : { 0, 128, 255 }) for (uint32_t t = 0; t < 5000; t += 211) row({ t, s, n, w, fxScanPos(t, (uint8_t)s, n, w != 0) });
  // S9, the field gate (CA1.2). STATEFUL: see gate() above and docs/CHANGING_THE_CORE.md.
  group("contractFieldGate", "\"seq\", \"ceil\", \"reset\", \"nowMs\", \"level\"",
        "\"allowed\", \"lit\", \"ref\", \"onsetMs\"");
  // 1. The plain life of a display, ceiling 200 (step 21): dark, a first light, a dip that is
  //    not a fall, a blackout (never delayed), a relight held to step - 1 above the trough until
  //    exactly 340 ms after the onset, texture under a step passing meanwhile.
  gateSeq(200);
  gate(1000, 0); gate(1010, 200); gate(1020, 180); gate(1030, 200); gate(1040, 0);
  gate(1050, 200); gate(1100, 20); gate(1200, 21); gate(1349, 200); gate(1350, 200); gate(1360, 200);
  //    The window runs from when the field last got BRIGHTER: a slow rise keeps moving it.
  gate(1700, 0); gate(1710, 60); gate(1900, 120); gate(2100, 200); gate(2110, 0);
  gate(2400, 200); gate(2439, 200); gate(2440, 200);
  //    A fall to a trough that is not dark, the hold measured from that trough, and the trough
  //    following the level down while it is held.
  gate(2450, 120); gate(2460, 200); gate(2470, 100); gate(2480, 200); gate(2490, 30); gate(2500, 200);
  gate(2779, 200); gate(2780, 200);
  // 2. The threshold from both sides, ceiling 200: a rise of 20 above the trough is texture and one
  //    of 21 an onset; a fall of 20 below the peak leaves the field lit and one of 21 does not.
  gateSeq(200);
  gate(5000, 0); gate(5010, 20); gate(5020, 0); gate(5030, 21); gate(5040, 1); gate(5050, 0);
  gate(6000, 200); gate(6010, 180); gate(6020, 200); gate(6030, 179); gate(6040, 200); gate(6400, 200);
  gate(6410, 10); gate(6420, 30); gate(6430, 31); gate(6440, 9); gate(6450, 29); gate(6460, 30); gate(6800, 30);
  // 3. Power-on: the anchor starts at 0, so the first 340 ms are held.
  gateSeq(255);
  gate(0, 255); gate(100, 255); gate(339, 255); gate(340, 255); gate(350, 0); gate(679, 255); gate(680, 255);
  // 4. millis() wrapping, ceiling 255: an onset before the wrap, the relight held across it.
  gateSeq(255);
  gate(4294967040u, 255); gate(4294967168u, 0); gate(4294967295u, 255); gate(0u, 255); gate(16u, 255);
  gate(83u, 255); gate(84u, 255); gate(100u, 0); gate(423u, 255); gate(424u, 255);
  // 5. State kept across a show reset: X, M:v=show and M:v=idle cannot be used to open the window,
  //    and a reset that brings a new ceiling changes the step, not the anchor or the level reference.
  gateSeq(200);
  gate(3000, 200); gate(3010, 0); gate(3100, 200, true); gate(3200, 200, true); gate(3300, 200, true);
  gate(3339, 200); gate(3340, 200); gate(3350, 0, true); gate(3400, 100, true, 100); gate(3679, 100); gate(3680, 100);
  gate(3690, 80, true, 255); gate(3700, 255); gate(3710, 100); gate(3720, 255); gate(4059, 255); gate(4060, 255);
  // 6. Odd ceilings. The step is ceil / 10 + 1 and never under 2 (CA1.4; it was ceil / 4 + 1).
  //    The same script at each: light, dark, relight at once, dark, a step exactly, the hold.
  for (int c : { 0, 1, 2, 3, 4, 9, 10, 19, 20, 63, 64, 160, 200, 254, 255 }) {
    const int step = contractFieldStep((uint8_t)c);
    gateSeq(c);
    gate(10000, 0); gate(10010, c); gate(10020, 0); gate(10030, c); gate(10349, c); gate(10350, c);
    gate(10360, c > step ? c - step : 0); gate(10370, c); gate(10380, c >= step ? c - step + 1 : 0); gate(10390, c);
    gate(10400, 0); gate(10410, step - 1 <= c ? step - 1 : c); gate(10420, step <= c ? step : c); gate(10730, c);
    gate(10740, c / 2); gate(10750, c); gate(11090, c);
  }
  // 7. A long deterministic walk (a fixed LCG: no clock, no library RNG), at three ceilings: jumps,
  //    ramps, texture and irregular frame times, so a mirror is pinned well away from the hand-made cases.
  for (int c : { 37, 160, 255 }) {
    gateSeq(c);
    uint32_t x = 0x1234567u + (uint32_t)c, now = 2000; int level = 0;
    for (int i = 0; i < 400; i++) {
      x = x * 1664525u + 1013904223u;
      const uint32_t r = x >> 8;
      now += (r % 13 == 0) ? 10 + (r >> 4) % 400 : (r % 5 == 0 ? 1 + (r >> 4) % 30 : 10);
      switch ((r >> 12) % 6) {
        case 0: level = 0; break;
        case 1: level = c; break;
        case 2: level = (int)((r >> 16) % (uint32_t)(c + 1)); break;
        case 3: level = level + (int)((r >> 16) % 9) > c ? c : level + (int)((r >> 16) % 9); break;
        case 4: level = level - (int)((r >> 16) % 9) < 0 ? 0 : level - (int)((r >> 16) % 9); break;
        default: break;                                              // the same level again
      }
      gate(now, level, (r >> 20) % 97 == 0);
    }
  }
  // CA1.4: two measures and the look gate, a frame at a time. STATEFUL, like contractFieldGate.
  group("contractFrameGate", "\"seq\", \"ceil\", \"restart\", \"nowMs\", \"effect\", \"rgb\", \"level\", \"lum\", \"peak\"",
        "\"drawn\", \"scale\", \"lit\", \"ref\", \"litY\", \"refY\", \"onsetMs\", \"lookMs\", \"rise\"");
  {
    const int solid = (int)CE_SOLID, pulse = (int)CE_PULSE, wipe = (int)CE_WIPE;
    const uint32_t white = 0xffffffu, blue = 0x0000ffu, red = 0xff0000u;
    // 1. White and blue by turns at one level, every 50 ms (N1): the brightest channel never
    //    moves. The look gate holds each change to 170 ms after the last, and white after blue is
    //    an onset in luminance: it waits for 340 ms after the field last got brighter.
    frameSeq(200);
    for (uint32_t t = 1000; t < 3000; t += 10) {
      const bool w = ((t - 1000) / 50) % 2 == 0;
      frame(t, solid, w ? white : blue, 200, w ? 200 : 14);
    }
    // 2. Red and blue by turns (no luminance onset to speak of: the look gate alone).
    frameSeq(160);
    for (uint32_t t = 1000; t < 2500; t += 10) {
      const bool r = ((t - 1000) / 30) % 2 == 0;
      frame(t, solid, r ? red : blue, 160, r ? 33 : 11);
    }
    // 3. A look that fades in is still arriving while it gets brighter: the change that follows
    //    keeps 170 ms from the last frame it rose in, not from its first frame.
    frameSeq(200);
    frame(1000, solid, white, 200, 200);
    for (uint32_t t = 1200; t <= 1500; t += 10) frame(t, wipe, red, (int)((t - 1200) / 2), (int)((t - 1200) / 10));
    for (uint32_t t = 1510; t < 1800; t += 10) frame(t, solid, white, 200, 200);
    // 4. A black frame is never held, and is no look: the look after it is gated against the one before.
    frameSeq(200);
    frame(1000, solid, red, 200, 42); frame(1050, 0, 0, 0, 0); frame(1060, solid, blue, 200, 14);
    frame(1160, solid, blue, 200, 14); frame(1170, solid, blue, 200, 14); frame(1400, solid, blue, 200, 14);
    // 5. The same look applied again (its timeline restarts) is a change of look.
    frameSeq(200);
    frame(1000, pulse, red, 100, 21); frame(1100, pulse, red, 100, 21, true); frame(1160, pulse, red, 100, 21);
    frame(1170, pulse, red, 100, 21); frame(1180, pulse, red, 120, 25, true); frame(1340, pulse, red, 120, 25);
    // 6. One measure held, both recorded as shown: a frame the luminance holds down is a FALL in level.
    frameSeq(200);
    frame(1000, solid, white, 200, 200); frame(1050, solid, blue, 200, 14); frame(1230, solid, white, 200, 200);
    frame(1240, solid, white, 200, 200); frame(1339, solid, white, 200, 200); frame(1340, solid, white, 200, 200);
    // CA1.6: ONE RISE IS ONE ONSET.
    // 8. Coloured looks that fade in from black, 20 ms a frame, the window open: the brightest
    //    channel crosses the step first, luminance some frames later, in the same rise. No frame
    //    is held. Then down again (each measure falls in its own frame, and the first fall ends
    //    the rise), and up a second time 340 ms and more after the peak.
    for (uint32_t rgb : { 0x116530u, 0x22c55eu, 0xff0000u, 0x00ff00u, 0x0000ffu, 0xffffffu }) for (int c : { 255, 160 }) {
      frameSeq(c);
      uint32_t t = 1000;
      for (int cycle = 0; cycle < 2; cycle++) {
        for (int k = 0; k <= c; k += 5, t += 20) frameOf(t, pulse, rgb, k);
        for (int k = c; k >= 0; k -= 5, t += 20) frameOf(t, pulse, rgb, k);
      }
    }
    // 9. The same fade begun inside the window (a white frame, black, and the fade 60 ms later):
    //    held under the step until 340 ms after the white, in whichever measure crosses first.
    for (uint32_t rgb : { 0x116530u, 0xff0000u }) {
      frameSeq(255);
      frame(1000, solid, white, 255, 255); frame(1050, 0, 0, 0, 0);
      for (int k = 0; k <= 255; k += 5) frameOf(1110u + (uint32_t)k * 2u, pulse, rgb, k);
    }
    // 10. What the shared time is for: white by a colour change, black, white again 20 ms later.
    //     The second white is a second onset in luminance, and in level too. Held.
    frameSeq(255);
    frame(1000, solid, blue, 255, 18); frame(2000, solid, white, 255, 255); frame(2010, 0, 0, 0, 0);
    for (uint32_t t = 2030; t <= 2350; t += 10) frame(t, solid, white, 255, 255);
    // 11. A fall ends the rise. One look whose luminance alone moves (a gradient between blue
    //     and white, say): up in the level's rise, down, and up again 100 ms after its peak.
    //     The second time it waits.
    frameSeq(200);
    const int grad = (int)CE_GRADIENT;
    frame(1000, grad, blue, 100, 7); frame(1200, grad, blue, 100, 100); frame(1250, grad, blue, 100, 7);
    for (uint32_t t = 1300; t <= 1560; t += 10) frame(t, grad, blue, 100, 100);
    // 12. One measure lights as the other falls, in one frame (3000): no rise begins, and the
    //     measure that fell waits for the window.
    frameSeq(255);
    frame(1000, grad, 0xffe500u, 255, 217); frame(2000, grad, 0xffe500u, 229, 212); frame(3000, grad, 0xffe500u, 255, 182);
    for (uint32_t t = 3200; t <= 3350; t += 10) frame(t, grad, 0xffe500u, 255, 217);
    // 13. CA1.7: the look gate goes by the frame's BRIGHTEST LED, not by its field level. One
    //     full column of a 28-column panel is a field level of 9 at most, and here of 3: under
    //     LOOK_MIN_LEVEL, and a colour anyone can see. Red is up; a blue frame of that kind
    //     60 ms later is held (not drawn), and drawn from 170 ms. The same field level with no
    //     LED over 3 is no look: it is drawn at once and does not take the gate.
    frameSeq(255);
    frame(1000, solid, red, 200, 42);
    for (uint32_t t = 1060; t <= 1180; t += 10) frame(t, wipe, blue, 3, 0, false, 255);
    frameSeq(255);
    frame(1000, solid, red, 200, 42);
    for (uint32_t t = 1060; t <= 1100; t += 10) frame(t, pulse, blue, 3, 0, false, 3);
    frame(1110, pulse, blue, 6, 0, false, 6); frame(1170, pulse, blue, 12, 0, false, 12);
    // 7. A long deterministic walk at three ceilings: looks, levels and luminances from a fixed LCG.
    for (int c : { 37, 160, 255 }) {
      frameSeq(c);
      uint32_t x = 0x89abcdeu + (uint32_t)c, now = 2000; int level = 0, lum = 0, fx = solid; uint32_t rgb = white;
      for (int i = 0; i < 300; i++) {
        x = x * 1664525u + 1013904223u;
        const uint32_t r = x >> 8;
        now += (r % 11 == 0) ? 10 + (r >> 4) % 300 : 10;
        if ((r >> 10) % 7 == 0) { static const uint32_t kC[] = { 0xffffffu, 0xff0000u, 0x0000ffu, 0x00ff00u }; rgb = kC[(r >> 14) % 4]; }
        if ((r >> 12) % 9 == 0) fx = ((r >> 16) % 3 == 0) ? 0 : ((r >> 16) % 3 == 1 ? solid : pulse);
        switch ((r >> 18) % 5) {
          case 0: level = 0; break;
          case 1: level = c; break;
          case 2: level = (int)((r >> 20) % (uint32_t)(c + 1)); break;
          default: break;
        }
        lum = rgb == white ? level : (rgb == 0x00ff00u ? level * 183 / 256 : (rgb == red ? level * 54 / 256 : level * 19 / 256));
        frame(now, fx, rgb, fx == 0 ? 0 : level, fx == 0 ? 0 : lum, (r >> 22) % 23 == 0);
      }
    }
  }
  // CA1.6: verb L. A higher level is taken at most once in 340 ms; a fall at once. STATEFUL.
  group("contractLevelHold", "\"seq\", \"reset\", \"nowMs\", \"v\"", "\"changed\", \"level\", \"want\", \"levelMs\"");
  // 1. 255 and 128 by turns every 50 ms (the PSI review's finding), a pass every 10 ms.
  levelSeq();
  for (uint32_t t = 1000; t < 3000; t += 10) levelPass(t, t % 50 == 0 ? ((t / 50) % 2 ? 255 : 128) : -1);
  // 2. A raise that waits, a newer one that replaces it, a fall that cancels it, the edges of the window.
  levelSeq();
  levelPass(1000, 200); levelPass(1010, 100); levelPass(1020, 255); levelPass(1100, 180); levelPass(1339, -1); levelPass(1340, -1);
  levelPass(1341, -1); levelPass(1400, 255); levelPass(1450, 90); levelPass(1679, 91); levelPass(1680, 91); levelPass(2019, 92);
  levelPass(2020, -1); levelPass(2021, 92); levelPass(2100, 0); levelPass(2110, 1); levelPass(2360, -1); levelPass(2361, -1);
  // 3. The show reset clears the level and a raise that waits, and keeps the time.
  levelSeq();
  levelPass(1000, 255); levelPass(1020, 0, true); levelPass(1030, 255); levelPass(1339, -1); levelPass(1340, -1);
  levelPass(1400, 128); levelPass(1500, 255); levelPass(1510, -1, true); levelPass(1700, -1); levelPass(1710, 64);
  // 4. A walk: levels and times from a fixed LCG, across the millis() wrap.
  for (uint32_t start : { 5000u, 4294960000u }) {
    levelSeq();
    uint32_t x = 0x1234567u + start, now = start;
    for (int i = 0; i < 400; i++) {
      x = x * 1664525u + 1013904223u;
      const uint32_t r = x >> 8;
      now += (r % 7 == 0) ? 10 + (r >> 4) % 500 : 10 + (r >> 4) % 40;
      const int v = (r >> 12) % 3 == 0 ? -1 : ((r >> 14) % 5 == 0 ? 255 : ((r >> 14) % 5 == 1 ? 0 : (int)((r >> 17) % 256)));
      levelPass(now, v, (r >> 20) % 61 == 0);
    }
  }
  // CA1.7: verb L and verb B (and b=) through one window. STATEFUL. A unit starts with its
  // brightness at the ceiling (255) and its level at 0, and the show reset puts both back.
  group("contractHoldsDue", "\"seq\", \"reset\", \"nowMs\", \"v\", \"b\"",
        "\"changed\", \"level\", \"levelWant\", \"bright\", \"brightWant\", \"riseMs\"");
  // 1. b=0 and b=200 by turns every 30 ms (the PSI's finding), a pass every 10 ms.
  levelSeq();
  for (uint32_t t = 1000; t < 3000; t += 10) holdsPass(t, -1, t % 30 == 0 ? ((t / 30) % 2 ? 200 : 0) : -1);
  // 2. A raise that waits, a newer one, a fall that cancels it, the window's edges, the reset.
  levelSeq();
  holdsPass(1000, -1, 0); holdsPass(1010, -1, 200); holdsPass(1040, -1, 0); holdsPass(1070, -1, 200); holdsPass(1100, -1, 90);
  holdsPass(1349, -1, -1); holdsPass(1350, -1, -1); holdsPass(1351, -1, -1); holdsPass(1400, -1, 255); holdsPass(1420, -1, 60);
  holdsPass(1689, -1, 61); holdsPass(1690, -1, -1); holdsPass(1700, -1, 255, true); holdsPass(1710, -1, 0); holdsPass(1720, -1, 255);
  holdsPass(2029, -1, -1); holdsPass(2030, -1, -1);
  // 3. L and B by turns: one window for both, so the two verbs cannot take turns inside it.
  levelSeq();
  for (uint32_t t = 1000; t < 3000; t += 10) {
    const uint32_t k = (t / 20) % 4;
    holdsPass(t, t % 20 == 0 && (k == 0 || k == 2) ? (k == 0 ? 255 : 0) : -1, t % 20 == 0 && (k == 1 || k == 3) ? (k == 1 ? 0 : 255) : -1);
  }
  // 4. Two raises due in one pass are both taken; a fall of one while the other waits.
  levelSeq();
  holdsPass(1000, 100, 100); holdsPass(1010, 50, 50); holdsPass(1020, 200, 200); holdsPass(1339, -1, -1); holdsPass(1340, -1, -1);
  holdsPass(1350, 250, 20); holdsPass(1679, -1, 30); holdsPass(1680, -1, -1); holdsPass(1690, 0, 255); holdsPass(2020, -1, -1);
  // 5. A walk.
  levelSeq();
  { uint32_t x = 0x7654321u, now = 9000;
    for (int i = 0; i < 500; i++) {
      x = x * 1664525u + 1013904223u;
      const uint32_t r = x >> 8;
      now += (r % 7 == 0) ? 10 + (r >> 4) % 500 : 10 + (r >> 4) % 40;
      const int b = (r >> 12) % 3 == 0 ? -1 : ((r >> 14) % 5 == 0 ? 255 : ((r >> 14) % 5 == 1 ? 0 : (int)((r >> 17) % 256)));
      const int v = (r >> 9) % 4 != 0 ? -1 : (int)((r >> 3) % 256);
      holdsPass(now, v, b, (r >> 20) % 61 == 0);
    } }
  // CA1.3: the integer clock, piece by piece, so a mirror can be pinned on each step.
  // The clock position long after the seed, where a float clock drifted: the same C line as
  // beatPosAt, read up to 24 days later and across the millis() wrap. `units` is the exact
  // position inside the beat, in 60000ths (ms * bpm mod 60000); phase16 = floor(units * 65536 / 60000).
  group("beatPosAtLong", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\"", "\"beatIndex\", \"barPos\", \"phase16\", \"units\"");
  for (int bpm : { 30, 59, 97, 120, 177, 299, 300 }) for (uint32_t seed : { 777u, 4294000000u }) for (long beat : { -3L, 0L })
    for (uint32_t d : { 599999u, 600000u, 600001u, 3599999u, 3600000u, 3600001u, 7200000u, 14400000u, 86400000u, 86400001u,
                        1000000000u, 2147483647u, 2147483648u, 4294967295u }) {
      BeatClock bc; seedClock(bc, bpm, 0, 4, beat, seed);
      const BeatPos bp = beatPosAt(bc, seed + d);
      uint16_t units; _beatSplit(bc, seed + d, units);
      row({ bpm, 0, 4, beat, seed, (uint32_t)(seed + d), bp.beatIndex, bp.barPos, bp.phase16, units });
    }
  // The build ramp's input: how far through its section a beat is, Q16, clamped to 0 .. 65536.
  group("beatSectionProgress16", "\"beatIndex\", \"sectionStart\", \"sectionEnd\"", "\"progress16\"");
  for (long start : { 0L, -8L, 64L }) for (long len : { 0L, 1L, 3L, 7L, 16L, 100L, 9999L, 65535L, 65536L, 100000L })
    for (long off : { -2L, 0L, 1L, 2L, 5L, 15L, 99L, 5000L, 65534L, 99999L, 100000L }) row({ start + off, start, start + len, (long long)beatSectionProgress16((int32_t)(start + off), (int32_t)start, (int32_t)(start + len)) });
  for (long b : { -1000000000L, -999999999L, -1L, 0L, 1L, 500000000L, 999999999L, 1000000000L })
    row({ b, -1000000000L, 1000000000L, (long long)beatSectionProgress16((int32_t)b, -1000000000L, 1000000000L) });
  row({ 4, 8, 4, (long long)beatSectionProgress16(4, 8, 4) });
  // The envelope alone, from its integer inputs (no clock): am, barPos, phase16, m, progress16.
  // `fires` is taken as true (120 BPM, 4 beats a bar: every beat may accent).
  group("beatAccentAmount", "\"am\", \"barPos\", \"phase16\", \"m\", \"progress16\"", "\"amt\"");
  { BeatClock bc; bc.running = true; bc.bpm = 120; bc.bpb = 4;
    for (int am : { 1, 2, 3 }) for (int bar : { 0, 1 }) for (int m : { 1, 64, 200, 255 })
      for (uint32_t prog : { 0u, 21845u, 32768u, 65535u, 65536u }) {
        if (am != 3 && prog != 0u) continue;
        for (uint32_t ph : { 0u, 1u, 255u, 256u, 4096u, 8191u, 8192u, 8193u, 16384u, 30000u, 32767u, 32768u, 32769u, 49152u, 60000u, 65279u, 65534u, 65535u }) {
          BeatPos bp; bp.beatIndex = bar; bp.barPos = (uint8_t)bar; bp.phase16 = (uint16_t)ph;
          row({ am, bar, ph, m, prog, beatAccentAmount((uint8_t)am, bp, (uint8_t)m, prog, bc) });
        }
      }
  }
  // What a bare re-anchor (C with ph= and beat=, no bpm=) does to a running clock: 2 = a nudge
  // (the position moved by less than one beat either way), 1 = a seek. Seeded as beatPosAtReseed.
  group("beatClockSeedKind", "\"bpm\", \"seedMs\", \"ph2\", \"beat2\", \"seedMs2\"", "\"kind\"");
  for (int bpm : { 60, 97, 120, 300 }) for (uint32_t seed : { 5000u, 4294900000u }) for (uint32_t gap : { 4000u, 4013u, 20000u })
    for (uint32_t ph2 : { 0u, 17u, 230u, 999u, 1001u }) for (long step : { -2L, -1L, 0L, 1L, 2L, 3L }) {
      BeatClock bc; seedClock(bc, bpm, 0, 4, 0, seed);
      const uint32_t seed2 = seed + gap;
      const long beat2 = (long)beatPosAt(bc, seed2).beatIndex + step;
      ContractParams pr; pr.hasPh = true; pr.phMs = ph2; pr.hasBeat = true; pr.beatAnchor = (int32_t)beat2;
      row({ bpm, seed, ph2, beat2, seed2, (int)beatClockSeed(bc, pr, seed2) });
    }
  // C1's edge guard after a seed: counted = 1 when the current beat counts as already accented
  // (more than 60 whole ms into it), and then `beat` is that beat's index; otherwise 0 and 0.
  group("accentEdgeAfterSeed", "\"bpm\", \"ph\", \"beat\", \"seedMs\", \"nowMs\"", "\"counted\", \"beat\"");
  for (int bpm : { 30, 97, 120, 177, 300 }) for (uint32_t seed : { 9000u, 4294967000u }) for (long beat : { -2L, 40L })
    for (uint32_t d : { 0u, 1u, 59u, 60u, 61u, 62u, 199u, 200u, 260u, 261u, 262u, 618u, 619u, 620u, 679u, 680u, 681u, 2000u, 2060u, 2061u, 3600061u }) {
      BeatClock bc; seedClock(bc, bpm, 0, 4, beat, seed);
      const int32_t e = accentEdgeAfterSeed(bc, seed + d);
      row({ bpm, 0, beat, seed, (uint32_t)(seed + d), e == BEAT_NONE ? 0 : 1, e == BEAT_NONE ? 0 : e });
    }
  // CA2 (v1.4). logic: is a colour the display's own (no hue), its hue on FastLED's rainbow wheel,
  // and the engine's fade or delay for s= from the display's own value and the floor.
  group("fxLogicOwn", "\"r\", \"g\", \"b\"", "\"own\"");
  for (int r = 0; r < 256; r += 51) for (int g = 0; g < 256; g += 51) for (int b = 0; b < 256; b += 51)
    row({ r, g, b, fxLogicOwn(ContractRGB{ (uint8_t)r, (uint8_t)g, (uint8_t)b }) ? 1 : 0 });
  for (int base : { 0, 100, 224 }) for (int d : { 30, 31, 32, 33 }) for (int ch = 0; ch < 3; ch++) {
    uint8_t v[3] = { (uint8_t)base, (uint8_t)base, (uint8_t)base }; v[ch] = (uint8_t)(base + d);
    row({ v[0], v[1], v[2], fxLogicOwn(ContractRGB{ v[0], v[1], v[2] }) ? 1 : 0 });
  }
  group("fxLogicHue", "\"r\", \"g\", \"b\"", "\"hue\"");
  for (int r = 0; r < 256; r += 51) for (int g = 0; g < 256; g += 51) for (int b = 0; b < 256; b += 51)
    row({ r, g, b, fxLogicHue(ContractRGB{ (uint8_t)r, (uint8_t)g, (uint8_t)b }) });
  for (int x = 0; x < 256; x++) {                     // every step of the six edges of the wheel at full saturation
    const uint8_t u = (uint8_t)x;
    const ContractRGB e[6] = { { 255, u, 0 }, { u, 255, 0 }, { 0, 255, u }, { 0, u, 255 }, { u, 0, 255 }, { 255, 0, u } };
    for (int k = 0; k < 6; k++) row({ e[k].r, e[k].g, e[k].b, fxLogicHue(e[k]) });
  }
  for (uint32_t c : { 0x116530u, 0x3b82f6u, 0xf97316u, 0x00ff88u, 0xc86496u, 0x202000u, 0x000020u, 0x808080u, 0x646484u, 0x010203u })
    row({ (long long)(c >> 16), (long long)((c >> 8) & 255u), (long long)(c & 255u),
          fxLogicHue(ContractRGB{ (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }) });
  group("fxLogicPace", "\"s\", \"x\", \"floor\"", "\"y\"");
  for (int sp = 0; sp < 256; sp++) for (int x : { 0, 1, 3, 9, 10, 11, 40, 255 }) for (int fl : { 1, 10 })
    row({ sp, x, fl, fxLogicPace((uint8_t)sp, (uint8_t)x, (uint8_t)fl) });
  // projector: the ten slots of a colour's row, and the crumbling rule for two colours.
  group("fxProjectorSlot", "\"rgb\", \"slot\"", "\"r\", \"g\", \"b\"");
  for (uint32_t c : { 0x000000u, 0xffffffu, 0xff0000u, 0x00ff00u, 0x0000ffu, 0x00ffffu, 0xff00ffu, 0xffff00u, 0x116530u, 0x3b82f6u, 0x010101u, 0x808080u })
    for (int k = 0; k <= 11; k++) {
      const ContractRGB o = fxProjectorSlot(ContractRGB{ (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }, (uint8_t)k);
      row({ (long long)c, k, o.r, o.g, o.b });
    }
  group("fxProjectorTake", "\"cur\", \"cand\"", "\"take\"");
  for (uint32_t c : { 0x000000u, 0xff0000u, 0x00ff00u, 0x116530u })                   // every pair of slots of four rows
    for (int a = 0; a < 10; a++) for (int b = 0; b < 10; b++) {
      const ContractRGB row0 = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
      const ContractRGB x = fxProjectorSlot(row0, (uint8_t)a), y = fxProjectorSlot(row0, (uint8_t)b);
      row({ (long long)(((uint32_t)x.r << 16) | ((uint32_t)x.g << 8) | x.b), (long long)(((uint32_t)y.r << 16) | ((uint32_t)y.g << 8) | y.b),
            fxProjectorTake(x, y) ? 1 : 0 });
    }
  for (uint32_t x : { 0x0000ffu, 0x008000u, 0x808080u, 0x404040u, 0x000000u, 0xffffffu })   // the two measures against each other
    for (uint32_t y : { 0x0000ffu, 0x008000u, 0x808080u, 0x404040u, 0x000000u, 0xffffffu, 0x010000u, 0x7f7f7fu })
      row({ (long long)x, (long long)y, fxProjectorTake(ContractRGB{ (uint8_t)(x >> 16), (uint8_t)(x >> 8), (uint8_t)x },
                                                        ContractRGB{ (uint8_t)(y >> 16), (uint8_t)(y >> 8), (uint8_t)y }) ? 1 : 0 });
  // vu (N10): while it is pumped, the level a pass offers to the level hold is envBright(255, m, amt),
  // with amt the envelope of the look's own accent mode. Seeded as accentAt; level is what is offered.
  group("vuLevel", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\", \"am\", \"m\", \"an\", \"ap\"",
        "\"fires\", \"amt\", \"level\"");
  for (int bpm : { 30, 120, 176, 177, 300 }) for (int am : { 1, 2, 4 }) for (int m : { 64, 200, 255 }) for (int an : { 1, 2 })
    for (uint32_t d = 0; d < 4100; d += 97) {
      if (am != 2 && an != 1) continue;                               // an= is for am=2 alone
      const uint32_t seed = 4294965000u;                              // crosses the millis wrap
      BeatClock bc; seedClock(bc, bpm, 0, 4, 0, seed);
      const BeatPos bp = beatPosAt(bc, seed + d);
      const uint8_t every = accentEveryPack((uint8_t)an, 0);
      const uint8_t amt = beatAccentAmount((uint8_t)am, bp, (uint8_t)m, 0, bc, every);
      row({ bpm, 0, 4, 0, seed, (uint32_t)(seed + d), am, m, an, 0, beatAccentFires((uint8_t)am, bp, bc, every) ? 1 : 0, amt,
            envBright(255, (uint8_t)m, amt) });
    }
  // logic's pump (CA2): beatAccentFires() thinned to rises FX_LOGIC_PUMP_MIN_MS apart, and the
  // envelope amount it gives (beatAccentAmount()'s on a beat it fires, else 0). Seeded as accentAt.
  group("fxLogicPump", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\", \"am\", \"m\", \"an\", \"ap\"",
        "\"fires\", \"amt\"");
  for (int bpm : { 30, 120, 150, 151, 160, 176, 177, 240, 300 }) for (int bpb : { 1, 4 }) for (int am = 0; am <= 4; am++)
    for (int an : { 1, 2 }) for (uint32_t d = 0; d < 3000; d += 71) {
      if (am != 2 && an != 1) continue;
      const uint32_t seed = 4294965000u;                              // crosses the millis wrap
      BeatClock bc; seedClock(bc, bpm, 0, bpb, -3, seed);
      const BeatPos bp = beatPosAt(bc, seed + d);
      const uint8_t every = accentEveryPack((uint8_t)an, an > 1 ? 1 : 0);
      row({ bpm, 0, bpb, -3, seed, (uint32_t)(seed + d), am, 255, an, an > 1 ? 1 : 0, fxLogicPumpFires((uint8_t)am, bp, bc, every) ? 1 : 0,
            fxLogicPumpAmount((uint8_t)am, bp, 255, beatSectionProgress16(bp.beatIndex, 0, 16), bc, every) });
    }
  // GROUPS ADDED BY LATER TASKS GO HERE (keep the order: appended groups never renumber old ones)
  printf("\n    ]}\n  }\n}\n");
  return 0;
}
