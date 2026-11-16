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
// Every group value is an integer. A float is carried scaled (phase_e6 = round(phase * 1e6)).
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
  // MORE CONSTANTS GO HERE (each starts with ",\n    ")
  printf("\n  ");
  printf("},\n  \"lines\": [");
  // The delivery plan's verb-V reference lines come first (v_vectors.py pins the same CRCs).
  static const char* const kLines[] = {
    "LFA:i=native:0,c=3b82f6,at=0", "LFA:i=rainbow,c=f97316,at=56,am=1,m=200,ae=flash", "LFA:i=native:0,at=447",
    "L*A:i=solid,c=0080FF,at=8", "**A:i=off,at=512", "LFA:i=comet,c=ff0000,at=56",
    "L*A:i=colorcycle,c=3b82f6,s=255,b=255,at=99999,am=2,m=255,an=16,ap=15,ae=colorcycle,ac=ffffff,ad=2550",
    "PFA:i=solid,c=f97316,at=44,am=1", "HTA:i=twinkle,c=00ff88,b=120,at=-8", "123456789", "" };
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
  group("fxChaseLit", "\"p\", \"elapsed\", \"speed\"", "\"lit\"");
  for (int s : speeds) for (uint32_t t = 0; t < 2000; t += 211) for (int p = 0; p < 6; p++) row({ p, (long)t, s, fxChaseLit(p, t, (uint8_t)s) ? 1 : 0 });
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
  group("beatPosAt", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"nowMs\"", "\"beatIndex\", \"barPos\", \"phase_e6\"");
  for (int bpm : { 30, 97, 120, 137, 174, 300 }) for (int bpb : { 3, 4 }) for (uint32_t ph : { 0u, 40u, 499u })
    for (long beat : { -3L, 0L, 64L }) for (uint32_t seed : { 1000u, 4294960000u })
      for (uint32_t d : { 0u, 1u, 39u, 40u, 500u, 12345u, 600000u, 3600000u }) {
        BeatClock bc; seedClock(bc, bpm, ph, bpb, beat, seed);
        BeatPos bp = beatPosAt(bc, seed + d);
        row({ bpm, ph, bpb, beat, seed, (uint32_t)(seed + d), bp.beatIndex, bp.barPos, (long long)(bp.phase * 1000000.0f + 0.5f) });
      }
  // A full seed, then the host's bare ph/beat re-anchor (no bpm=), then the read.
  group("beatPosAtReseed", "\"bpm\", \"ph\", \"bpb\", \"beat\", \"seedMs\", \"ph2\", \"beat2\", \"seedMs2\", \"nowMs\"",
        "\"beatIndex\", \"barPos\", \"phase_e6\"");
  for (int bpm : { 97, 120, 135, 174 }) for (uint32_t seed : { 5000u, 4294900000u }) for (uint32_t gap : { 4000u, 4013u, 20000u })
    for (uint32_t ph2 : { 0u, 17u, 230u }) for (uint32_t d : { 0u, 16u, 17u, 250u, 3999u, 60000u }) {
      BeatClock bc; seedClock(bc, bpm, 0, 4, 0, seed);
      const uint32_t seed2 = seed + gap;
      const long beat2 = (long)beatPosAt(bc, seed2).beatIndex + 1;     // the next beat, as the host sends it
      ContractParams pr; pr.hasPh = true; pr.phMs = ph2; pr.hasBeat = true; pr.beatAnchor = (int32_t)beat2;
      beatClockSeed(bc, pr, seed2);
      BeatPos bp = beatPosAt(bc, seed2 + d);
      row({ bpm, 0, 4, 0, seed, ph2, beat2, seed2, (uint32_t)(seed2 + d), bp.beatIndex, bp.barPos, (long long)(bp.phase * 1000000.0f + 0.5f) });
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
      const uint8_t amt = beatAccentAmount((uint8_t)am, bp, (uint8_t)m, beatSectionProgress(bp.beatIndex, 0, 16), bc);
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
      const uint8_t amt = beatAccentAmount(2, bp, 200, 0.0f, bc, every);
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
  // 1. The plain life of a display, ceiling 200 (step 51): dark, a first light, a dip that is
  //    not a fall, a blackout (never delayed), a relight held to step - 1 above the trough until
  //    exactly 340 ms after the onset, texture under a step passing meanwhile.
  gateSeq(200);
  gate(1000, 0); gate(1010, 200); gate(1020, 180); gate(1030, 200); gate(1040, 0);
  gate(1050, 200); gate(1100, 40); gate(1200, 51); gate(1349, 200); gate(1350, 200); gate(1360, 200);
  //    The window runs from when the field last got BRIGHTER: a slow rise keeps moving it.
  gate(1700, 0); gate(1710, 60); gate(1900, 120); gate(2100, 200); gate(2110, 0);
  gate(2400, 200); gate(2439, 200); gate(2440, 200);
  //    A fall to a trough that is not dark, the hold measured from that trough, and the trough
  //    following the level down while it is held.
  gate(2450, 120); gate(2460, 200); gate(2470, 100); gate(2480, 200); gate(2490, 30); gate(2500, 200);
  gate(2779, 200); gate(2780, 200);
  // 2. The threshold from both sides, ceiling 200: a rise of 50 above the trough is texture and one
  //    of 51 an onset; a fall of 50 below the peak leaves the field lit and one of 51 does not.
  gateSeq(200);
  gate(5000, 0); gate(5010, 50); gate(5020, 0); gate(5030, 51); gate(5040, 1); gate(5050, 0);
  gate(6000, 200); gate(6010, 150); gate(6020, 200); gate(6030, 149); gate(6040, 200); gate(6400, 200);
  gate(6410, 10); gate(6420, 60); gate(6430, 61); gate(6440, 9); gate(6450, 59); gate(6460, 60); gate(6800, 60);
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
  // 6. Odd ceilings. The step is ceil / 4 + 1: 1 for ceilings under 4, so every rise is an onset
  //    there. The same script at each: light, dark, relight at once, dark, a step exactly, the hold.
  for (int c : { 0, 1, 2, 3, 4, 20, 63, 64, 160, 200, 254, 255 }) {
    const int step = c / 4 + 1;
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
  // GROUPS ADDED BY LATER TASKS GO HERE (keep the order: appended groups never renumber old ones)
  printf("\n    ]}\n  }\n}\n");
  return 0;
}
