// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Frame exporter for the RSeries logics contract layer: the LED buffers Cantina Studio's dome
// preview is checked against, pixel for pixel (tests/fixtures/dome/frames in the Signal Booster
// repository, whose README has the file format, schema 1). Written there by the Studio side
// against this fork's host mock; it lives here since CA1.6, so that the frames are generated
// where the firmware is. The format and the file names are as Studio wrote them.
//
// It runs the REAL contract layer (src/contract/ContractLogics.h, contract_core.h) on the host
// mock of the engine (logics_harness.h, mock_engine.h): each case's wire lines go through
// contractHandleLine(), engine frames run at a fixed step, and what the front display's LED
// buffer holds is written to <out dir>/rseries-logic.<case>.json.
// run.sh stage 2f builds it, runs it, and compares the result with test/host/frames/;
// UPDATE_FRAMES=1 bash test/host/run.sh rewrites that folder. Studio copies the folder as it is.
//
// `source.commit`: a file cannot name the commit that will contain it, so the field holds the
// first twelve hex digits of the sha256 of contract_core.h followed by ContractLogics.h, the two
// files that decide every pixel here (run.sh passes it).
//
// What it cannot export, because the mock does not model it:
//   * native:<n> looks: the mock engine draws nothing for a native sequence.
//   * i=rainbow: the mock's CHSV keeps the level only, not FastLED's hue conversion.
//   * i=logic (v1.4): its pixels are the engine's own walk, whose pauses are the engine's dice
//     (FastLED's random8()), so no preview can draw them LED for LED. What can be pinned is pinned
//     elsewhere: the palette, hue, pace and level the layer gives the engine (test_contract_logics_r2.cpp),
//     and the walk itself against the region model (stage 1k).
#include "logics_harness.h"
#include <string>
#include <vector>

struct Line { uint32_t atMs; const char* line; };
struct Case {
  const char* name;
  uint32_t durationMs, stepMs;
  const char* clock;            // "elapsed": the look reads its own age; "millis": the board's uptime
  std::vector<Line> script;     // wire lines, without the leading '!'
};

static const uint32_t MILLIS0 = 100000;    // the board's millis() at t = 0
static const uint32_t SETTLE_MS = 1000;    // the idle look runs this long before t = 0 (see exportCase)
static const uint32_t SETTLE_STEP_MS = 10;

static void hex2(std::string& s, uint8_t v) { static const char* d = "0123456789abcdef"; s += d[v >> 4]; s += d[v & 15]; }
static std::string frameHex(const LogicEngineRenderer& r, bool& uniform) {
  std::string s; uniform = true;
  const CRGB first = r.px(0, 0);
  for (int y = 0; y < r.height(); y++) for (int x = 0; x < r.width(); x++) {
    const CRGB c = r.px(x, y);
    if (c.r != first.r || c.g != first.g || c.b != first.b) uniform = false;
    hex2(s, c.r); hex2(s, c.g); hex2(s, c.b);
  }
  return s;
}

static int exportCase(const Case& c, const char* dir, const char* commit) {
  char path[512];
  snprintf(path, sizeof path, "%s/rseries-logic.%s.json", dir, c.name);
  FILE* f = fopen(path, "w");
  if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }

  resetAll();
  // The owner's brightness at full, so a contract colour reaches the buffer unscaled (S6: the
  // look's level is a fraction of the display's own brightness).
  LogicEngineFLDDefault.fBri = 255; LogicEngineRLDDefault.fBri = 255;
  // A BOARD THAT HAS BEEN ON FOR A WHILE. The brightness above is set after the harness's
  // power-on (resetAll() leaves the mock at 160), so to the layer it is the owner turning the
  // display up on the idle look (ContractLogics.h _briSync). Sending the case's first line in
  // that same pass made every case start inside the S9 window: the contract took the display
  // less than a frame after a brighter native look was written, the layer could not know
  // whether that was ever drawn, and took the field as lit just now (contractEngineLatched:
  // fieldOnsetMs = now). A look that fades in then had its first rise held for 340 ms, which
  // no show does on a board whose brightness was set before the show. So the idle look runs
  // here for SETTLE_MS first, at that brightness, and both gates are at rest when t = 0.
  _mock_millis = MILLIS0 - SETTLE_MS;
  for (; _mock_millis != MILLIS0; _mock_millis += SETTLE_STEP_MS) frame();
  const ContractUnitState at0 = gContract[0];

  fprintf(f, "{\n  \"schema\": 1, \"board\": \"rseries-logic\", \"unit\": \"F\", \"contract\": \"%s\",\n", CONTRACT_VERSION);
  fprintf(f, "  \"source\": { \"repo\": \"DroidNet-RSeriesLogics\", \"commit\": \"%s\", \"harness\": \"test/host/logics_harness.h (mock engine)\" },\n", commit);
  fprintf(f, "  \"layout\": { \"leds\": %d, \"cols\": %d, \"rows\": %d, \"index\": \"row-major\" },\n",
          FLD->width() * FLD->height(), FLD->width(), FLD->height());
  fprintf(f, "  \"script\": [");
  for (size_t i = 0; i < c.script.size(); i++)
    fprintf(f, "%s{ \"atMs\": %u, \"line\": \"%s\" }", i ? ", " : "", (unsigned)c.script[i].atMs, c.script[i].line);
  fprintf(f, "],\n  \"clock\": \"%s\", \"millis0\": %u, \"stepMs\": %u, \"durationMs\": %u, \"brightness\": 255,\n",
          c.clock, (unsigned)MILLIS0, (unsigned)c.stepMs, (unsigned)c.durationMs);
  // start: the unit's S9 state at t = 0, before the first line: what the gates remember of
  // the idle look. Studio's mirror of the gates starts from this, not from a guess.
  fprintf(f, "  \"start\": { \"idleMs\": %u, \"gate\": { \"lit\": %s, \"ref\": %u, \"litY\": %s, \"refY\": %u, "
             "\"onsetAgoMs\": %u, \"lookSig\": %u, \"lookAgoMs\": %u } },\n",
          (unsigned)SETTLE_MS, at0.fieldLit ? "true" : "false", (unsigned)at0.fieldRef, at0.fieldLitY ? "true" : "false",
          (unsigned)at0.fieldRefY, (unsigned)(MILLIS0 - at0.fieldOnsetMs), (unsigned)at0.lookSig, (unsigned)(MILLIS0 - at0.lookMs));
  // frames: only the samples where the buffer (or the selected sequence) CHANGED; each holds
  // until the next. "fill": every LED is that colour. "seq": the engine sequence on the
  // display (106 = the contract's renderer; anything else is a native sequence).
  fprintf(f, "  \"frames\": [");
  std::string last; int lastSeq = -2, n = 0;
  size_t next = 0;
  for (uint32_t t = 0; t <= c.durationMs; t += c.stepMs) {
    _mock_millis = MILLIS0 + t;
    while (next < c.script.size() && c.script[next].atMs <= t) send(c.script[next++].line);
    frame();
    bool uniform; const std::string hex = frameHex(*FLD, uniform);
    if (hex == last && FLD->_seq == lastSeq) continue;
    fprintf(f, "%s\n    { \"t\": %u, \"seq\": %d, ", n++ ? "," : "", (unsigned)t, FLD->_seq);
    if (uniform) fprintf(f, "\"fill\": \"%s\" }", hex.substr(0, 6).c_str());
    else fprintf(f, "\"rgb\": \"%s\" }", hex.c_str());
    last = hex; lastSeq = FLD->_seq;
  }
  fprintf(f, "\n  ]\n}\n");
  fclose(f);
  printf("%-40s %4d frames\n", path, n);
  return 0;
}

// vu under a C seek every 50 ms: each seek puts the beat back at its start, so the pump offers its
// top every 50 ms and the level hold takes a raise at most once in 340 ms.
static std::vector<Line> vuSeek() {
  static std::vector<std::string> text;
  std::vector<Line> v;
  if (text.empty()) for (uint32_t t = 0; t < 3000; t += 50) { char l[48]; snprintf(l, sizeof l, "L*C:bpm=120,ph=0,bpb=4,beat=%u", (unsigned)((t / 50) % 4)); text.push_back(l); }
  v.push_back({ 0, "LFA:i=vu,c=22c55e,am=2,m=255" });
  for (size_t i = 0; i < text.size(); i++) v.push_back({ (uint32_t)(i * 50), text[i].c_str() });
  return v;
}

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s <out dir> <source id>\n", argv[0]); return 2; }
  const std::vector<Case> cases = {
    // ---- one look each: Studio's preview must draw these pixel for pixel ----
    { "solid",       1000, 10, "elapsed", { { 0, "LFA:i=solid,c=3b82f6" } } },
    { "flash",       3000, 10, "elapsed", { { 0, "LFA:i=flash,c=ff0000" } } },
    { "flash-s255",  2000, 10, "elapsed", { { 0, "LFA:i=flash,c=ff0000,s=255" } } },
    { "pulse-s0",    4200, 20, "elapsed", { { 0, "LFA:i=pulse,c=22c55e,s=0" } } },
    { "pulse-s255",  1400, 10, "elapsed", { { 0, "LFA:i=pulse,c=22c55e,s=255" } } },
    { "scan",        3000, 10, "elapsed", { { 0, "LFA:i=scan,c=ff0000" } } },
    { "comet",       3000, 10, "elapsed", { { 0, "LFA:i=comet,c=3b82f6" } } },
    { "chase",       2000, 10, "elapsed", { { 0, "LFA:i=chase,c=f97316" } } },
    { "wipe",        4000, 10, "elapsed", { { 0, "LFA:i=wipe,c=a855f7" } } },
    { "gradient",    3000, 10, "elapsed", { { 0, "LFA:i=gradient" } } },
    { "colorcycle",  4000, 10, "elapsed", { { 0, "LFA:i=colorcycle" } } },
    { "twinkle",     2000, 20, "millis",  { { 0, "LFA:i=twinkle,c=ffffff" } } },
    // ---- accents: which frames show the overlay (Studio's strobe rules, dome/strobe.js) ----
    // a pulse over a contract look, then the look again
    { "accent-over-look",   2000, 10, "elapsed", { { 0, "LFA:i=solid,c=0000ff" }, { 1000, "LFP:c=ff0000,d=400" } } },
    // a pulse shorter than one strobe state is held for 170 ms
    { "accent-short",       2000, 10, "elapsed", { { 0, "LFA:i=solid,c=0000ff" }, { 1000, "LFP:c=ff0000,d=50" } } },
    // two pulses 200 ms apart: the second is inside the 340 ms cool-down and is dropped; one
    // 340 ms after the first fires
    { "accent-pair",        2400, 10, "elapsed", { { 0, "LFA:i=solid,c=0000ff" }, { 1000, "LFP:c=ff0000,d=180" },
                                                   { 1200, "LFP:c=ff0000,d=180" }, { 1340, "LFP:c=ff0000,d=180" } } },
    // R1: no overlay on a native look. The display stays on the native sequence throughout.
    { "accent-over-native", 2000, 10, "elapsed", { { 0, "LFA:i=native:0" }, { 1000, "LFP:c=ff0000,d=400" } } },
    // ---- v1.4 vu: the meter on the pump (N10): its level rides the beat through the level hold, its
    // brightness stays at b=. At 120 BPM, and under a C seek every 50 ms (N11). Put up while the clock
    // runs, it starts dark and its first rise comes 340 ms later (the hand-over, 2026-11-24) ----
    { "vu-120",      4000, 10, "elapsed", { { 0, "LFA:i=vu,c=22c55e,am=2,m=255" }, { 0, "L*C:bpm=120,ph=0,bpb=4,beat=0" } } },
    { "vu-seek",     3000, 10, "elapsed", vuSeek() },
  };
  int bad = 0;
  for (const Case& c : cases) bad += exportCase(c, argv[1], argv[2]);
  if (gFails) { fprintf(stderr, "harness failures: %d\n", gFails); bad++; }
  return bad ? 1 : 0;
}
