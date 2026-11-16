// Part of the DroidNet Driveable-Animation Contract test harness.
// Copyright (c) 2026 Travis Cook. Shared byte-identically across the DroidNet
// RSeries/PSI/Flthy forks.  SPDX-License-Identifier: LGPL-2.1-only OR MIT
//
// Prints the shared parity vectors as JSON: the real contract_core.h, run on the host, for the
// Studio preview's JS mirror to be checked against. Deterministic: no clock, no RNG.
//   usage: gen_contract_vectors <sha256 of contract_core.h>
// Schema 1: { schema, contract_version, core_sha256, generator,
//             constants: { NAME: int },
//             lines: [ { line: "<bytes after '!'>", crc16: int } ],
//             groups: { <function>: { args: [name...], out: [name...], rows: [[args..., outs...]] } } }
// Every group value is an integer. A float is carried scaled (phase_e6 = round(phase * 1e6)).
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <initializer_list>

static bool gFirstGroup = true, gFirstRow = true;
static void group(const char* name, const char* args, const char* out) {
  printf("%s\n    \"%s\": {\"args\": [%s], \"out\": [%s], \"rows\": [", gFirstGroup ? "" : "\n    ]},", name, args, out);
  gFirstGroup = false; gFirstRow = true;
}
static void row(std::initializer_list<long> v) {
  printf("%s\n      [", gFirstRow ? "" : ",");
  gFirstRow = false;
  bool first = true;
  for (long x : v) { printf("%s%ld", first ? "" : ", ", x); first = false; }
  printf("]");
}

int main(int argc, char** argv) {
#ifdef CONTRACT_VERSION
  const char* ver = CONTRACT_VERSION;
#else
  const char* ver = "1.2";
#endif
  printf("{\n  \"schema\": 1,\n  \"contract_version\": \"%s\",\n  \"core_sha256\": \"%s\",\n", ver, argc > 1 ? argv[1] : "");
  printf("  \"generator\": \"test/host/gen_contract_vectors.cpp\",\n");
  printf("  \"constants\": {");
  printf("\"STROBE_MIN_STATE_MS\": %lu, \"CONTRACT_BPM_MIN\": %u, \"CONTRACT_BPM_MAX\": %u, ",
         (unsigned long)STROBE_MIN_STATE_MS, (unsigned)CONTRACT_BPM_MIN, (unsigned)CONTRACT_BPM_MAX);
  printf("\"CONTRACT_BPB_MAX\": %u, \"ACCENT_EDGE_LATE_MS\": %u, \"ACCENT_DUR_MAX_MS\": %u",
         (unsigned)CONTRACT_BPB_MAX, (unsigned)ACCENT_EDGE_LATE_MS, (unsigned)ACCENT_DUR_MAX_MS);
  printf("},\n  \"lines\": [");
  static const char* const kLines[] = {
    "LFA:i=solid,c=ff0000,at=0", "LFA:i=wipe,c=0000ff,at=16,am=2,m=200", "L*A:i=native:4,at=-4",
    "L*A:i=colorcycle,c=3b82f6,s=255,b=255,at=99999,am=3,m=255,ae=colorcycle,ac=ffffff,ad=2550",
    "PFA:i=swipe,c=f97316,at=44,am=1", "HTA:i=twinkle,c=00ff88,b=120,at=8", "123456789", "" };
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
  group("beatPosAt", "\"bpm\", \"bpb\", \"beatOffset\", \"deltaMs\"", "\"beatIndex\", \"phase_e6\", \"barPos\"");
  for (int bpm : { 30, 97, 120, 137, 174, 300 }) for (int bpb : { 3, 4 }) for (long d : { -2500L, -1L, 0L, 1L, 499L, 500L, 12345L, 600000L, 3600000L }) {
    BeatClock bc; bc.running = true; bc.bpm = (uint16_t)bpm; bc.bpb = (uint8_t)bpb; bc.anchorMs = 4000000000u; bc.beatOffset = -3;
    BeatPos bp = beatPosAt(bc, bc.anchorMs + (uint32_t)d);
    row({ bpm, bpb, -3, d, (long)bp.beatIndex, (long)(bp.phase * 1000000.0f + 0.5f), bp.barPos });
  }
  group("beatRateDivisor", "\"bpm\"", "\"k\"");
  for (int bpm = 30; bpm <= 300; bpm++) row({ bpm, beatRateDivisor((uint16_t)bpm) });
  group("barRateDivisor", "\"bpm\", \"bpb\"", "\"k\"");
  for (int bpb = 1; bpb <= 12; bpb++) for (int bpm : { 30, 120, 176, 177, 240, 300 }) row({ bpm, bpb, barRateDivisor((uint16_t)bpm, (uint8_t)bpb) });
  group("beatAccent", "\"am\", \"bpm\", \"bpb\", \"beatIndex\", \"phase_e6\", \"beatMod\", \"progress_e6\"", "\"fires\", \"amount\"");
  for (int am = 0; am <= 5; am++) for (int bpm : { 120, 176, 177, 240, 300 }) for (int bpb : { 1, 3, 4 })
    for (long beat = -5; beat <= 9; beat++) for (long ph : { 0L, 100000L, 500000L, 900000L }) {
      BeatClock bc; bc.running = true; bc.bpm = (uint16_t)bpm; bc.bpb = (uint8_t)bpb;
      BeatPos bp; bp.beatIndex = (int32_t)beat; bp.phase = (float)ph / 1000000.0f;
      bp.barPos = (uint8_t)(((beat % bpb) + bpb) % bpb);
      row({ am, bpm, bpb, beat, ph, 200, 500000, beatAccentFires((uint8_t)am, bp, bc) ? 1 : 0,
            beatAccentAmount((uint8_t)am, bp, 200, 0.5f, bc) });
    }
  group("fxFlashOn", "\"elapsed\", \"speed\"", "\"on\"");
  for (int s : speeds) for (uint32_t t = 0; t < 2000; t += 53) row({ (long)t, s, fxFlashOn(t, (uint8_t)s) ? 1 : 0 });
  group("fxPulseBright", "\"elapsed\", \"speed\"", "\"bright\"");
  for (int s : speeds) for (uint32_t t = 0; t < 4100; t += 67) row({ (long)t, s, fxPulseBright(t, (uint8_t)s) });
  group("fxScanPos", "\"elapsed\", \"speed\", \"N\", \"wrap\"", "\"pos\"");
  for (int n : strands) for (int w : { 0, 1 }) for (int s : { 0, 128, 255 }) for (uint32_t t = 0; t < 5000; t += 211) row({ (long)t, s, n, w, fxScanPos(t, (uint8_t)s, n, w != 0) });
  printf("\n    ]}\n  }\n}\n");
  return 0;
}
