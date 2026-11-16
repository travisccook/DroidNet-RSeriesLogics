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
  // CONSTANTS ADDED BY LATER TASKS GO HERE
  printf("},\n  \"lines\": [");
  // LINES ADDED BY LATER TASKS GO HERE
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
  // GROUPS ADDED BY LATER TASKS GO HERE (keep the order: appended groups never renumber old ones)
  printf("\n    ]}\n  }\n}\n");
  return 0;
}
