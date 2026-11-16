// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Contract v1.3 S9, tested as a PROPERTY of the real layer on the mock engine:
//
//   Whatever lines arrive, in whatever order, a display
//     (a) goes from dark to lit across its whole field at most three times in any second;
//     (b) is never brighter than its owner's ceiling while the contract draws it;
//     (c) is back on a native sequence, and stays there, after M:v=idle or a watchdog expiry;
//     (d) is dark at once after X, and stays dark until a line that may relight it;
//     (e) L6: runs the sequence a button started on it, and keeps running it, when the contract
//         held the display at that moment. The show does not come back for it by itself;
//     (f) the same limit as (a) in LUMINANCE (Rec.709), which is what a colour change moves when
//         the brightest channel stays where it was (white to blue and back);
//     (g) changes its look or colour across the whole field at most once in 170 ms (S4, for
//         every path: a live A, a section reached by time or by a seek, an accent, a restore).
//
// Lines go in through contractHandleLine(), a frame is rendered through the mock engine every
// 10 ms (now and then later), and what is measured is the PIXELS: the full-field level of each
// display (fieldLevel(): the mean of each pixel's brightest channel). (a) is counted three times
// over, with three pairs of thresholds for "dark" and "lit", because the limit must not depend on
// where an observer draws those lines: 10 % and 50 % of the ceiling, 19 % and 75 % (the pair the
// 2026-11-16 review used), and 4 % and 31 %.
//
// TWO PHOTOMETERS. The first reads each pixel's brightest channel (fieldLevel()), so a saturated
// red or blue field counts as much as a white one. The second reads the field's mean colour: its
// Rec.709 luminance for (f), and its chromaticity for (g) (a change of look that the first
// photometer cannot see at all). (a) and (f) are each counted by several observers: three with
// wide bands, and four with bands only an eighth of the ceiling wide (3 to 15 %, 40 to 52 %,
// 45 to 60 %, 80 to 95 %), because S9's step is a tenth of the ceiling. (The narrow ones on
// luminance came with CA1.6, where the two measures of one rise may light at different times.)
// The random lines use white, the three primaries, a grey, and mixed colours whose luminance is
// well under their brightest channel (22c55e, 116530, ffe500).
//
// THE ENGINE draws a frame every 10 ms at most; a quarter of the random streams pass through the
// loop every 1 to 4 ms, as the board does, so lines and ticks fall between frames.
//
// Two kinds of stream, all from one fixed seed:
//   * HOSTILE PROFILES: one per way a host was found able to flash a display (the review's
//     table), each a short script repeated with random jitter and, in half the runs, random
//     harmless lines in between;
//   * RANDOM streams: every verb, wildcards, bursts, corrupted parameters, re-anchors, seeks,
//     scored and live lines, at random times.
//
// What the test knows about the layer's inside is deliberately little: when a unit's show mode
// ended without a line saying so (the watchdog, or a failed V), read from gContract[].inShow, so
// that (c) can start its clock; and whether the contract held a display when a button was
// pressed, for (e). Everything it ASSERTS is on the pixels and the latched effect.
//
// THE BUTTON. The random streams also press buttons: a native sequence started on a display
// from outside the contract (FLD->selectSequence(), as @nT... ends up doing). A button may light
// a dark display whenever its owner likes; that is not the contract's onset, so the observers
// start counting afresh at each press. What the contract does AFTER it is counted as ever.
#include "logics_harness.h"
#include <cstring>
#include <initializer_list>
#include <string>

// ---- deterministic randomness ----
static uint32_t gRng = 0x9E3779B9u;
static uint32_t rnd() { gRng ^= gRng << 13; gRng ^= gRng >> 17; gRng ^= gRng << 5; return gRng; }
static uint32_t rnd(uint32_t n) { return n ? rnd() % n : 0; }
static const char* pick(std::initializer_list<const char*> l) { return *(l.begin() + rnd((uint32_t)l.size())); }

// ---- (a): one observer, with its own idea of dark and lit ----
struct Onsets {
  int darkPct = 10, litPct = 50;
  bool lit = false; uint32_t at[128]; long n = 0, total = 0;      // the latest onsets (a ring)
  int worst = 0; uint32_t minGap = 0xFFFFFFFFu;
  void see(int level, int ceil, uint32_t now) {
    if (!lit && level * 100 >= ceil * litPct) {
      lit = true;
      if (n > 0 && now - at[(n - 1) & 127] < minGap) minGap = now - at[(n - 1) & 127];
      at[n++ & 127] = now; total++;
      int w = 0; for (long i = n; i-- > 0 && n - i <= 128 && now - at[i & 127] < 1000; ) w++;
      if (w > worst) worst = w;
    } else if (lit && level * 100 <= ceil * darkPct) lit = false;
  }
};

// ---- (h): a PART of the display. A region of a quarter of the field or more is watched the way
// the whole field is: its level (the mean over ITS pixels of each pixel's brightest channel)
// against an observer's dark and lit, and going from dark to lit is an onset. One thing more is
// asked before a region counts as lit: that the pixels in it that are not dark number a quarter
// of the whole field or more. A lit area smaller than that is not a flash however it moves (S10
// says the same of a stock look): one column of eight that scans through a region of two.
// The limit is the spec's: no region lights more than three times in any one second.
struct Region {
  int x0, x1, y0, y1;                              // [x0, x1) by [y0, y1)
  bool lit[3] = { false, false, false }; uint32_t at[3][4]; int n[3] = { 0, 0, 0 };   // the last onsets, per observer
};
static const int kRegionBands[3][2] = { { 10, 50 }, { 19, 75 }, { 4, 31 } };   // dark at or below, lit from (% of the ceiling)

// ---- what is measured, per display ----
struct Watch {
  LogicEngineRenderer* r = nullptr; int ceil = 255;
  Onsets obs[3];                 // (a), wide bands
  Onsets narrow[4];              // (a), bands an eighth of the ceiling wide
  Onsets lum[3];                 // (f), on Rec.709 luminance
  Onsets lumNarrow[4];           // (f), bands an eighth of the ceiling wide (CA1.6: the two measures light at different times)
  // (g): the field's colour while it is lit, for five observers. Each has its own idea of dark
  // and lit, a step apart at least, exactly like the observers of (a): below its dark level the
  // field has no colour for it, and the colour it sees when the field is lit again is compared
  // with the one it saw last.
  struct Colour { int darkPct = 10, litPct = 50; bool lit = false; int chroma[3] = { 0, 0, 0 }; long top = 1; bool known = false; uint32_t at = 0; bool any = false; };
  Colour colour[6];              // (the sixth reads a colour from 4 levels up, whatever the ceiling: see reset())
  uint32_t colourMinGap = 0xFFFFFFFFu; long colourChanges = 0;
  Region regions[17]; int nRegions = 0;            // (h)
  uint32_t regionMinGap = 0xFFFFFFFFu; long regionOnsets = 0; int regionWorst = -1, regionMost = 0;   // most onsets of one region in a second
  int overCeiling = 0;           // (b) frames with a pixel above the ceiling that effect 106 drew
  long writesSeen = 0;           // ...the mock's count of pixel writes, to tell a drawn frame from a held one
  int notNative = 0;             // (c) frames not on a native look when the display should be
  int notDark = 0;               // (d) frames lit when the display should be dark
  uint32_t nativeFrom = 0; bool expectNative = false;
  uint32_t darkFrom = 0;   bool expectDark = false;
  long litMs = 0, darkChecked = 0, nativeChecked = 0, contractFrames = 0, buttonChecked = 0;
  int notButton = 0;             // (e) frames on something else when a button's sequence should be running
  uint32_t buttonFrom = 0; int expectButton = -1;
  bool ownerDark = false;        // the owner chose lights-out, by button: a display the contract gives back, or leaves alone, is then dark
  bool ownerDarkUp = false;      // ...and the engine has started it (a frame or two after the press)
  void reset(LogicEngineRenderer* rr, int u) {
    *this = Watch(); r = rr; ceil = (u == 0 ? LogicEngineFLDDefault : LogicEngineRLDDefault).fBri;
    obs[1].darkPct = 19; obs[1].litPct = 75; obs[2].darkPct = 4; obs[2].litPct = 31;
    lum[1].darkPct = 19; lum[1].litPct = 75; lum[2].darkPct = 4; lum[2].litPct = 31;
    static const int kNarrow[4][2] = { { 3, 15 }, { 40, 52 }, { 45, 60 }, { 80, 95 } };
    for (int i = 0; i < 4; i++) { narrow[i].darkPct = kNarrow[i][0]; narrow[i].litPct = kNarrow[i][1]; lumNarrow[i] = narrow[i]; }
    static const int kColour[5][2] = { { 10, 50 }, { 19, 75 }, { 4, 31 }, { 3, 15 }, { 45, 60 } };
    for (int i = 0; i < 5; i++) { colour[i].darkPct = kColour[i][0]; colour[i].litPct = kColour[i][1]; }
    colour[5].darkPct = colour[5].litPct = -1;      // by the brightest LED, in levels: a colour from 4 up, none under
    // (h): the whole field, its halves, its four quarters, and four bands of columns and of rows
    // a quarter of the field each (on 10 rows: three rows, the bands overlapping)
    const int W = r->width(), H = r->height(), cw = (W + 3) / 4, rh = (H + 3) / 4;
    auto add = [&](int x0, int x1, int y0, int y1) { Region& g = regions[nRegions++]; g.x0 = x0; g.x1 = x1; g.y0 = y0; g.y1 = y1; };
    add(0, W, 0, H);
    add(0, W / 2, 0, H); add(W / 2, W, 0, H); add(0, W, 0, H / 2); add(0, W, H / 2, H);
    add(0, W / 2, 0, H / 2); add(W / 2, W, 0, H / 2); add(0, W / 2, H / 2, H); add(W / 2, W, H / 2, H);
    for (int i = 0; i < 4; i++) { const int x0 = (i * (W - cw) + 1) / 3; add(x0, x0 + cw, 0, H); }
    for (int i = 0; i < 4; i++) { const int y0 = (i * (H - rh) + 1) / 3; add(0, W, y0, y0 + rh); }
  }
  void native(uint32_t now) { if (!expectNative) nativeFrom = now; expectNative = true; expectDark = false; }
  void dark(uint32_t now)   { darkFrom = now; expectDark = true; expectNative = false; expectButton = -1; }
  void unknown()            { expectNative = expectDark = false; expectButton = -1; }
  void button(uint32_t now, int seq, bool held) {
    for (Onsets& o : obs) o.n = 0;                 // the owner's light: not the contract's onset
    for (Onsets& o : narrow) o.n = 0;
    for (Onsets& o : lum) o.n = 0;
    for (Onsets& o : lumNarrow) o.n = 0;
    for (Colour& c : colour) c.known = c.any = false;   // ...nor the contract's change of look
    for (int i = 0; i < nRegions; i++) for (int b = 0; b < 3; b++) regions[i].n[b] = 0;
    expectDark = false;
    if (seq == 14) { ownerDark = true; ownerDarkUp = false; }   // (any other press ends it when the engine starts that one)
    expectButton = held ? seq : -1; buttonFrom = now;
  }
  void frame(uint32_t now, uint32_t dt) {
    const int level = fieldLevel(r);
    if (ownerDark && r->_fx == 14) ownerDarkUp = true;
    if (ownerDarkUp && r->_fx != 14) ownerDark = ownerDarkUp = false;
    if (level * 2 >= ceil) litMs += dt;
    for (Onsets& o : obs) o.see(level, ceil, now);
    for (Onsets& o : narrow) o.see(level, ceil, now);
    // the second photometer: the field's mean colour
    // Luminance as S9's second measure defines it: Rec.709 per LED in whole numbers
    // ((54 r + 183 g + 19 b) >> 8), then the mean, in whole levels. The same instrument as the
    // gate's, on purpose: one that reads luminance otherwise (in floating point, or on the mean
    // colour) differs from it by up to 2 levels, and a look that drifts across such an
    // observer's threshold inside those 2 levels reads as lighting later than it did.
    long sr = 0, sg = 0, sb = 0, sy = 0;
    const int W = r->width(), H = r->height(), N = W * H;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
      const CRGB c = r->px(x, y); sr += c.r; sg += c.g; sb += c.b; sy += (54 * c.r + 183 * c.g + 19 * c.b) >> 8;
    }
    const int luminance = (int)(sy / N);
    int peakLevel = 0;                             // the brightest LED
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { const CRGB c = r->px(x, y); const int v = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b); if (v > peakLevel) peakLevel = v; }
    for (Onsets& o : lum) o.see(luminance, ceil, now);
    for (Onsets& o : lumNarrow) o.see(luminance, ceil, now);
    const long top = sr > sg ? (sr > sb ? sr : sb) : (sg > sb ? sg : sb);
    for (Colour& k : colour) {
      // (the sixth: the colour of a field whose brightest LED is only a few levels up. A look
      // that comes up dim can be seen long before it has risen a step, and the look after it
      // must keep its 170 ms from THEN: the Flthy soak found 77 ms.)
      if (k.litPct < 0) k.lit = peakLevel >= (int)LOOK_MIN_LEVEL;   // (the core's own line between a colour and none)
      else if (!k.lit && level * 100 >= ceil * k.litPct) k.lit = true;
      else if (k.lit && level * 100 <= ceil * k.darkPct) k.lit = false;
      // too dim to have a colour: for this observer, or for anyone (CA1.6: a frame whose
      // brightest LED is under 4 levels shows none, and is not a look: contractLookGateAt)
      if (!k.lit || top == 0 || peakLevel < (int)LOOK_MIN_LEVEL) continue;
      const int c[3] = { (int)(sr * 255 / top), (int)(sg * 255 / top), (int)(sb * 255 / top) };
      int far = 0;
      for (int i = 0; i < 3; i++) { const int d = c[i] > k.chroma[i] ? c[i] - k.chroma[i] : k.chroma[i] - c[i]; if (d > far) far = d; }
      // How well a dim frame's colour can be read at all: a channel is a whole number of levels,
      // so at a field mean of m levels its share of the brightest channel is known to 255 / m.
      // (ffe500 at 2 levels is the pixel 020100: it reads as orange, and at full it is yellow.
      // That is the LEDs' 8 bits, not a change of look.) Both frames compared have that error.
      const int blur = (int)(255L * N / top) + (k.known ? (int)(255L * N / k.top) : 0);
      if (k.known && far > 96 + blur) {            // a different colour than the field last had: a change of look
        if (k.any && now - k.at < colourMinGap) colourMinGap = now - k.at;
        k.at = now; k.any = true; if (&k == &colour[0]) colourChanges++;
      }
      for (int i = 0; i < 3; i++) k.chroma[i] = c[i];
      k.known = true; k.top = top;
    }
    // (h): the regions
    {
      long sumV[11][33]; int cnt[3][11][33];       // sums over the pixels above and left of (x, y)
      for (int x = 0; x <= W; x++) { sumV[0][x] = 0; for (int b = 0; b < 3; b++) cnt[b][0][x] = 0; }
      for (int y = 0; y < H; y++) {
        sumV[y + 1][0] = 0; for (int b = 0; b < 3; b++) cnt[b][y + 1][0] = 0;
        for (int x = 0; x < W; x++) {
          const CRGB c = r->px(x, y);
          const int v = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
          sumV[y + 1][x + 1] = sumV[y][x + 1] + sumV[y + 1][x] - sumV[y][x] + v;
          for (int b = 0; b < 3; b++) cnt[b][y + 1][x + 1] = cnt[b][y][x + 1] + cnt[b][y + 1][x] - cnt[b][y][x] + (v * 100 > ceil * kRegionBands[b][0] ? 1 : 0);
        }
      }
      for (int i = 0; i < nRegions; i++) {
        Region& g = regions[i];
        const int area = (g.x1 - g.x0) * (g.y1 - g.y0);
        const long mean = (sumV[g.y1][g.x1] - sumV[g.y0][g.x1] - sumV[g.y1][g.x0] + sumV[g.y0][g.x0]) / area;
        for (int b = 0; b < 3; b++) {
          const int lit = cnt[b][g.y1][g.x1] - cnt[b][g.y0][g.x1] - cnt[b][g.y1][g.x0] + cnt[b][g.y0][g.x0];
          if (!g.lit[b] && mean * 100 >= (long)ceil * kRegionBands[b][1] && lit * 4 >= N) {
            g.lit[b] = true; regionOnsets++;
            if (g.n[b] > 0 && now - g.at[b][(g.n[b] - 1) & 3] < regionMinGap) regionMinGap = now - g.at[b][(g.n[b] - 1) & 3];
            g.at[b][g.n[b]++ & 3] = now;
            int w = 0; for (int k = 0; k < 4 && k < g.n[b]; k++) if (now - g.at[b][(g.n[b] - 1 - k) & 3] < 1000) w++;
            if (w > regionMost) { regionMost = w; regionWorst = i; }
          } else if (g.lit[b] && mean * 100 <= (long)ceil * kRegionBands[b][0]) g.lit[b] = false;
        }
      }
    }
    if (r->_fx == 106) contractFrames++;
    // (b) is about frames the contract DREW. While a change of look or a hand-over waits (S4,
    // S9, S10) effect 106 leaves the frame that is up, and that frame can be the engine's own:
    // a sequence that draws at its own level, or the idle look from before the owner turned the
    // brightness down.
    const bool drew = r->_writes != writesSeen; writesSeen = r->_writes;
    if (r->_fx == 106 && drew)
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const CRGB c = r->px(x, y);
        if (c.r > ceil || c.g > ceil || c.b > ceil) { overCeiling++; y = H; break; }
      }
    // (c): giving the display back may wait for the onset window (340 ms), and that begins a
    // frame later when effect 106 first has to take the display from a sequence whose light is
    // not known (S10); each of those frames can be a slow pass of the loop (40 ms here)
    if (expectNative && now - nativeFrom > 480) { nativeChecked++; if (r->_fx == 106 || (r->_fx == 14 && !ownerDark)) notNative++; }
    // (d): one frame for the engine to notice, then dark
    if (expectDark && now - darkFrom > 30) { darkChecked++; if (level * 10 > ceil) notDark++; }
    // (e): the frame after the press, and from then on
    if (expectButton >= 0 && now - buttonFrom > 30) { buttonChecked++; if (r->_fx != expectButton) notButton++; }
  }
};
static Watch gW[2];
static int gWatchdogLate = 0;    // (c) the watchdog did not fire when the test's own count said it must
static long gLines = 0, gPresses = 0;

// ---- PROPERTY_TRACE=1: when a property first breaks, print what led up to it ----
#include <cstdlib>
#include <vector>
struct TraceEv { uint32_t t; std::string what; };
static std::vector<TraceEv> gTrace; static bool gTraceOn = false, gTraced = false;
static void trace(const std::string& what) { if (gTraceOn) gTrace.push_back({ _mock_millis, what }); }
static void traceDump(const char* why, int u) {
  if (!gTraceOn || gTraced) return;
  if (const char* only = getenv("PROPERTY_TRACE_ONLY")) if (strstr(why, only) == nullptr) return;   // e.g. "native", "lit after X"
  gTraced = true;
  printf("TRACE: %s on display %d at %u ms. The last 2.5 s:\n", why, u, _mock_millis);
  for (const TraceEv& e : gTrace) if (_mock_millis - e.t <= 2500) printf("  %7u  %s\n", e.t, e.what.c_str());
}

// ---- sending, with the bookkeeping the properties need ----
static bool gWdArmed[2], gWdSure[2]; static uint32_t gWdMs[2]; static uint32_t gLastAddressed;
static void tx(const std::string& line) {
  const char* l = line.c_str();
  ParsedContract p;
  const bool ok = contractParse(l, p) && (p.cls == 'L' || p.cls == '*');
  const bool f = ok && (p.unit == 'F' || p.unit == '*'), r = ok && (p.unit == 'R' || p.unit == '*');
  send(l); gLines++;
  trace("line  " + line);
  if (!f && !r) return;
  gLastAddressed = _mock_millis;
  const bool idle = p.verb == CV_MODE && p.params.mode == 'i', show = p.verb == CV_MODE && p.params.mode == 's';
  for (int u = 0; u < 2; u++) {
    Watch& w = gW[u];
    if (p.verb == CV_CLOCK) { w.unknown(); continue; }       // ONE clock for both displays: either may start to play
    if (!(u == 0 ? f : r)) continue;
    if (idle)       { w.native(_mock_millis); gWdArmed[u] = false; }
    else if (show)  { w.native(_mock_millis);                // armed: the native idle look until the show begins
                      gWdArmed[u] = gWdSure[u] = p.params.hasWd && p.params.wd != 0; gWdMs[u] = (uint32_t)p.params.wd * 1000u; }
    else if (p.verb == CV_STOP) w.dark(_mock_millis);
    else if (p.verb == CV_VERIFY) {                          // a failed check is an idle; a good one changes nothing
      gWdSure[u] = false;
      if (!p.params.hasN || !p.params.hasK) continue;        // not a check at all
      if (gContract[u].loadOk == 2) { w.native(_mock_millis); gWdArmed[u] = false; } else w.expectDark = false;
    }
    else if (p.verb == CV_ANIMATE || p.verb == CV_PULSE) w.unknown();   // may take, or relight, the display
    // Q, L, B, and an M that is neither show nor idle: nothing a display may light up for
  }
}
// A button: someone starts native sequence `seq` on display u.
static void press(int u, int seq) {
  // Held: the contract has the display (effect 106, or a native look it chose). NORMAL chosen
  // over a native section looks exactly like that section running out, and the section's own
  // sequence chosen again changes nothing at all: neither is a take-over.
  // And a press for the sequence the engine is ALREADY on starts nothing, so nothing tells the
  // layer; it works that out 50 ms later, and a line inside those 50 ms is handled as if there
  // had been no press. (e) is not asserted for such a press.
  const bool held = gShown[u] != SH_ENGINE && !(gShown[u] == SH_NATIVE && (seq == 0 || seq == gShownCode[u]))
                    && seq != (int)gEngineSeq[u];
  (u == 0 ? FLD : RLD)->selectSequence((byte)seq);
  { char b[48]; snprintf(b, sizeof b, "BUTTON %c sequence %d%s", u ? 'R' : 'F', seq, held ? " (held)" : ""); trace(b); }
  gW[u].button(_mock_millis, seq, held);
  if (seq == 0 && !held) gW[u].unknown();          // (the show may well take the display back by itself)
  gPresses++;
}
static void step(uint32_t ms = 10) {
  bool was[2] = { gContract[0].inShow, gContract[1].inShow };
  _mock_millis += ms; frame();
  for (int u = 0; u < 2; u++) {
    const bool due = gWdArmed[u] && _mock_millis - gLastAddressed >= gWdMs[u];
    if (due && gWdSure[u] && gContract[u].inShow) gWatchdogLate++;
    if (due) gWdArmed[u] = false;
    if (was[u] && !gContract[u].inShow) gW[u].native(_mock_millis);   // show mode ended by itself: the watchdog
    const int before = gW[u].obs[0].worst + gW[u].obs[1].worst * 10 + gW[u].obs[2].worst * 100, nn = gW[u].notNative, nd = gW[u].notDark;
    gW[u].frame(_mock_millis, ms);
    if (gTraceOn) {
      const CRGB mid = gW[u].r->px(3, 4);
      char b[160]; snprintf(b, sizeof b, "  %c level %3d effect %3d base %d  pixel %02x%02x%02x", u ? 'R' : 'F', fieldLevel(gW[u].r), gW[u].r->_fx, gContract[u].base, mid.r, mid.g, mid.b);
      static std::string last[2];
      if (last[u] != b) {
        last[u] = b;
        if (getenv("PROPERTY_TRACE_GATE"))   // the gate's own state, for whoever is debugging it
          snprintf(b + strlen(b), sizeof b - strlen(b), "   shown %d%s gate %s %3d / %s %3d, onset %u, look %u", gShown[u], gLatched[u] ? "" : "?", gContract[u].fieldLit ? "lit" : "low", gContract[u].fieldRef,
                   gContract[u].fieldLitY ? "lit" : "low", gContract[u].fieldRefY, (unsigned)gContract[u].fieldOnsetMs, (unsigned)gContract[u].lookMs);
        trace(b);
      }
      for (const Onsets& o : gW[u].obs) if (o.worst > 3) traceDump("a fourth onset in one second", u);
      if (getenv("PROPERTY_TRACE_GAP")) for (const Onsets& o : gW[u].obs) if (o.minGap < 340) traceDump("two onsets under 340 ms apart", u);
      if (gW[u].notNative > nn) traceDump("not native after idle", u);
      if (gW[u].notDark > nd) traceDump("lit after X", u);
      if (gW[u].notButton) traceDump("not on the sequence a button started", u);
      if (gW[u].overCeiling) traceDump("a frame over the ceiling", u);
      if (gW[u].colourMinGap < 170) traceDump("two colour changes under 170 ms apart", u);
      for (const Onsets& o : gW[u].lum) if (o.minGap < 340) traceDump("two luminance onsets under 340 ms apart", u);
      for (const Onsets& o : gW[u].lumNarrow) if (o.minGap < 340) traceDump("two narrow-band luminance onsets under 340 ms apart", u);
      for (const Onsets& o : gW[u].narrow) if (o.minGap < 340) traceDump("two narrow-band onsets under 340 ms apart", u);
      if (gW[u].regionMost > 3) { char why[96]; const Region& g = gW[u].regions[gW[u].regionWorst];
        snprintf(why, sizeof why, "a region lit four times in a second (columns %d to %d, rows %d to %d)", g.x0, g.x1 - 1, g.y0, g.y1 - 1); traceDump(why, u); }
      (void)before;
    }
  }
}
static void begin() {
  resetAll();
  gTrace.clear();
  for (int u = 0; u < 2; u++) { gW[u].reset(u == 0 ? FLD : RLD, u); gWdArmed[u] = gWdSure[u] = false; }
  gLastAddressed = _mock_millis;
  for (int i = 0; i < 40; i++) step();            // 400 ms of uptime before anything arrives
}

// ---- a random line: mostly plausible, sometimes corrupted ----
// (Every random value is drawn into a variable first: the order in which a compiler evaluates
// function arguments is its own, and the streams must be the same streams everywhere.)
static std::string randomLine() {
  char b[160];
  const char* cls = pick({ "L", "L", "L", "L", "*", "*", "P", "H" });
  const char* unit = pick({ "F", "F", "R", "*", "*", "T" });
  const char* fx = pick({ "solid", "solid", "off", "flash", "pulse", "rainbow", "scan", "sparkle", "meter", "comet", "chase",
                          "wipe", "gradient", "colorcycle", "twinkle", "native:0", "native:4", "native:14", "native:25", "bogus",
                          "native:6", "native:2", "native:20", "native:24", "native:9" });
  const char* col = pick({ "ffffff", "ffffff", "ff0000", "000000", "0000ff", "101010", "zzzzzz", "00ff00", "22c55e", "116530", "ffe500" });
  const uint32_t kind = rnd(17);
  const uint32_t r1 = rnd(256), r2 = rnd(2) ? 255 : rnd(300), r3 = rnd(2) ? 255 : rnd(256), r4 = rnd(6), r5 = rnd(5), r6 = rnd(4);
  const uint32_t r7 = rnd(400), r8 = rnd(40), r9 = rnd(65536), r10 = rnd(2) ? rnd(256) : (rnd(2) ? 0u : 255u), r11 = rnd(600), r12 = rnd(8);
  switch (kind) {
    case 0: case 1: case 2:
      snprintf(b, sizeof b, "%s%sA:i=%s,c=%s,s=%u,b=%u,m=%u,am=%u", cls, unit, fx, col, r1, r2, r3, r4); break;
    case 3:  snprintf(b, sizeof b, "%s%sA:c=%s,b=%u", cls, unit, col, r3); break;                            // a patch
    case 4: {
      const char* ae = pick({ "flash", "solid", "off", "pulse" });
      const char* far = pick({ "2147483647", "-2147483648", "1000000000", "99999999999" });
      if (r12 == 0) snprintf(b, sizeof b, "%s%sA:i=%s,c=%s,at=%s,am=3,m=200,an=16,ap=15,ae=flash", cls, unit, fx, col, far);
      else snprintf(b, sizeof b, "%s%sA:i=%s,c=%s,at=%d,am=%u,m=%u,an=%u,ap=%u,ae=%s", cls, unit, fx, col, (int)r8 - 4, r5, r1, r4, r6, ae);
      break;
    }
    case 5: case 6: {
      const char* pfx = pick({ "solid", "flash", "off", "pulse", "wipe" });
      snprintf(b, sizeof b, "%s%sP:i=%s,c=%s,d=%u,b=%u", cls, unit, pfx, col, r11, r3); break;
    }
    case 7:  snprintf(b, sizeof b, "%s%sC:bpm=%u,ph=%u,bpb=%u,beat=%d", cls, unit, 20 + r2, r7, 1 + r5, (int)r8 - 4); break;
    case 8: {
      const char* ph = pick({ "0", "4294967295", "2147483648" });
      const char* far = pick({ "2147483647", "-2147483648", "1000000000", "-99999999999" });
      if (r12 == 0) snprintf(b, sizeof b, "%s%sC:ph=%s,beat=%s", cls, unit, ph, far);
      else snprintf(b, sizeof b, "%s%sC:ph=%u,beat=%d", cls, unit, r7, (int)r8);
      break;
    }
    case 9:  snprintf(b, sizeof b, "%s%sC:bpm=%s", cls, unit, pick({ "0", "300", "120", "abc", "", "-120", "65656" })); break;
    case 10: snprintf(b, sizeof b, "%s%sB:b=%u", cls, unit, r10); break;
    case 11: snprintf(b, sizeof b, "%s%sL:v=%u", cls, unit, r10); break;
    case 12: snprintf(b, sizeof b, "%s%sX", cls, unit); break;
    case 13: {
      const char* mode = pick({ "show", "show", "idle", "stop" });
      const char* wd = pick({ "", "", ",wd=1", ",wd=2" });
      snprintf(b, sizeof b, "%s%sM:v=%s%s", cls, unit, mode, wd); break;
    }
    case 14: snprintf(b, sizeof b, "%s%sV:n=%u,k=%04x", cls, unit, r6, r9); break;
    case 15: snprintf(b, sizeof b, "%s%sQ:r=%s", cls, unit, pick({ "0", "2", "", "zz", "100" })); break;
    default: {                                     // garbage: random printable bytes after a plausible head
      int n = snprintf(b, sizeof b, "%s%s%c:", cls, unit, "APCBLXMQVz"[r4 + r6]);
      for (int i = 0, len = (int)r8; i < len && n < 120; i++) b[n++] = (char)(33 + rnd(94));
      b[n] = '\0';
    }
  }
  return b;
}

// What a stream broke, as bits, so each fix can be seen to clear its own.
enum : int { BROKE_ONSETS = 1,      // (a), the wide observers: two onsets under 340 ms apart, or four in a second
             BROKE_NARROW = 2,      // (a), the narrow ones
             BROKE_LUMINANCE = 4,   // (f)
             BROKE_COLOUR = 8,      // (g)
             BROKE_OTHER = 16,      // (b) to (e)
             BROKE_REGION = 32 };   // (h)
// ---- the hostile profiles: the rows of the review's table, and three more of the same kind ----
struct Profile { const char* name; void (*setup)(); void (*at)(uint32_t t); uint32_t period; int expected; };
static void pShow() { tx("**M:v=show"); }
static const Profile kProfiles[] = {
  { "X between live A lines", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=solid,c=ffffff"); if (t == 50) tx("LFX"); if (t == 100) tx("LFA:i=solid,c=ffffff"); }, 170, 0 },
  { "B 0/255 alternation", []() { pShow(); tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFB:b=0"); if (t == 50) tx("LFB:b=255"); }, 100, 0 },
  { "P:b= alternation", []() { pShow(); tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFP:i=solid,c=ffffff,b=0"); if (t == 50) tx("LFP:i=solid,c=ffffff,b=255"); }, 100, 0 },
  { "meter following L 0/255", []() { pShow(); tx("LFA:i=meter,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("L*L:v=0"); if (t == 50) tx("L*L:v=255"); }, 100, 0 },
  { "C seeks across two scored sections", []() { pShow(); tx("LFA:i=solid,c=ffffff,at=0"); tx("LFA:i=off,at=100"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=120,beat=100"); if (t == 50) tx("L*C:bpm=120,beat=0"); }, 100, 0 },
  { "C:bpm= re-seed restarting the pump", []() { pShow(); tx("LFA:i=solid,c=ffffff,am=2,m=255"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=300"); }, 150, 0 },
  { "A cancelling an overlay, with P", []() { pShow(); tx("LFA:i=off"); },
    [](uint32_t t) { if (t == 0) tx("LFP:i=solid,c=ffffff"); if (t == 90) tx("LFA:i=off"); if (t == 260) tx("LFA:i=solid,c=ffffff");
                     if (t == 340) tx("LFP:i=off"); if (t == 430) tx("LFA:i=solid,c=ffffff"); if (t == 600) tx("LFA:i=off"); }, 680, 0 },
  { "pump across section seams at 300 BPM",
    []() { pShow(); for (int b = 0; b < 32; b++) { char l[96]; snprintf(l, sizeof l, "LFA:i=solid,c=ffffff,at=%d,am=2,m=255,an=2,ap=%d", b, b % 2); tx(l); }
           tx("**C:bpm=300,ph=0,bpb=4,beat=0"); },
    [](uint32_t t) { if (t == 0) tx("**C:bpm=300,ph=0,bpb=4,beat=0"); }, 6200, 0 },
  { "X and M:v=idle alternation (dark, then the native look)", []() { tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFX"); if (t == 50) tx("LFM:v=idle"); }, 100, 0 },
  { "live A restarting a pulse", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=pulse,c=ffffff,s=255"); }, 170, 0 },
  { "live A between white and native lights-out", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=solid,c=ffffff"); if (t == 50) tx("LFX"); if (t == 60) tx("LFA:i=native:14"); if (t == 120) tx("LFA:i=solid,c=ffffff"); }, 240, 0 },
  { "a scored flash section delivered again and again", []() { pShow(); tx("LFA:i=flash,c=ffffff,s=255,at=0"); tx("L*C:bpm=120"); },
    [](uint32_t t) { if (t == 0) tx("LFA:i=flash,c=ffffff,s=255,at=0"); }, 200, 0 },
  // ---- the 2026-11-16 re-review: what the gate could not see, and what it let through ----
  { "C seeks between a white and a blue section (the brightest channel never moves)",
    []() { pShow(); tx("LFA:i=solid,c=ffffff,at=0"); tx("LFA:i=solid,c=0000ff,at=100"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=120,beat=100"); if (t == 50) tx("L*C:bpm=120,beat=0"); }, 100, 0 },
  { "C seeks between a red and a blue section",
    []() { pShow(); tx("LFA:i=solid,c=ff0000,at=0"); tx("LFA:i=solid,c=0000ff,at=100"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=120,beat=100"); if (t == 50) tx("L*C:bpm=120,beat=0"); }, 100, 0 },
  { "C seeks between a native section and a red one", []() { pShow(); tx("LFA:i=native:0,at=0"); tx("LFA:i=solid,c=ff0000,at=100"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=120,beat=100"); if (t == 50) tx("L*C:bpm=120,beat=0"); }, 100, 0 },
  { "a white accent between two colours of a live look", []() { pShow(); tx("LFA:i=solid,c=ff0000"); },
    [](uint32_t t) { if (t == 0) tx("LFP:c=ffffff,d=170"); if (t == 20) tx("LFA:i=solid,c=0000ff"); if (t == 340) tx("LFP:c=ffffff,d=300");
                     if (t == 360) tx("LFA:i=solid,c=ff0000"); }, 680, 0 },
  { "white by a colour change, X, white again by an accent (two luminance onsets, one brightness onset)",
    []() { pShow(); tx("LFA:i=solid,c=0000ff"); },
    [](uint32_t t) { if (t == 0) tx("LFA:i=solid,c=ffffff"); if (t == 10) tx("LFX"); if (t == 20) tx("LFP:c=ffffff,d=170");
                     if (t == 200) tx("LFA:i=solid,c=0000ff"); }, 400, 0 },
  { "native:6 (it flashes by itself), X and a relight just after its own ON edge", []() { pShow(); tx("LFA:i=native:6"); press(0, 6); },
    [](uint32_t t) { if (t == 510) tx("LFX"); if (t == 520) tx("LFA:i=solid,c=ffffff"); if (t == 690) { tx("LFA:i=native:6"); press(0, 6); } }, 1205, 0 },
  { "B between 0 and 63 (under a quarter of the ceiling)", []() { pShow(); tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFB:b=0"); if (t == 50) tx("LFB:b=63"); }, 100, 0 },
  { "B between 192 and 255 (the top quarter)", []() { pShow(); tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFB:b=192"); if (t == 50) tx("LFB:b=255"); }, 100, 0 },
  { "the top fifth of a meter following L", []() { pShow(); tx("LFA:i=meter,c=ffffff"); tx("L*L:v=255"); },
    [](uint32_t t) { if (t == 0) tx("L*L:v=204"); if (t == 50) tx("L*L:v=255"); }, 100, 0 },
  // ---- CA1.6: one rise is one onset. Colours, whose two measures cross the step at different times ----
  { "a green pulse cut by X in its rise and started again", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=pulse,c=22c55e,s=0"); if (t == 260) tx("LFX"); }, 290, 0 },
  { "a dim green pulse at top speed, another colour every 200 ms", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=pulse,c=116530,s=255"); if (t == 200) tx("LFA:i=pulse,c=ff0000,s=255"); if (t == 400) tx("LFA:i=pulse,c=00ff00,s=255"); }, 600, 0 },
  { "blue, a white accent in the same rise, X, and again", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=solid,c=0000ff"); if (t == 20) tx("LFP:c=ffffff,d=170"); if (t == 200) tx("LFX"); }, 230, 0 },
  { "blue and white by turns under a fade (B up and down)", []() { pShow(); tx("LFA:i=solid,c=0000ff"); },
    [](uint32_t t) { if (t % 40 == 0) { char l[32]; snprintf(l, sizeof l, "LFB:b=%u", (unsigned)((t / 40) % 2 ? 255 : 20)); tx(l); }
                     if (t == 90) tx("LFA:i=solid,c=ffffff"); if (t == 270) tx("LFA:i=solid,c=0000ff"); }, 360, 0 },
  { "a dim, slow pulse and another colour 171 ms after its first frame", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=pulse,c=ff0000,s=0,b=40"); if (t == 171 || t == 180) tx("LFA:i=solid,c=0000ff,b=40"); if (t == 400) tx("LFX"); }, 600, 0 },
  { "a dim pulse in red and in blue by turns", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=pulse,c=ff0000,s=120,b=30"); if (t == 180) tx("LFA:i=pulse,c=0000ff,s=120,b=30"); }, 360, 0 },
  { "a coloured wipe restarted in its fill, in two colours", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=wipe,c=22c55e,s=255"); if (t == 180) tx("LFA:i=wipe,c=ffe500,s=255"); }, 350, 0 },
  // ---- CA1.6: a PART of the field. The whole-field gate sees none of these: the field's level stays legal ----
  { "a meter with L at 255 and 128 by turns (the top half of the display)", []() { pShow(); tx("L*A:i=meter,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("L*L:v=255"); if (t == 50) tx("L*L:v=128"); }, 100, 0 },
  { "a meter with L at 255 and 64 by turns, and X now and then", []() { pShow(); tx("L*A:i=meter,c=ffffff"); },
    [](uint32_t t) { if (t % 60 == 0) tx((t / 60) % 2 ? "L*L:v=64" : "L*L:v=255"); if (t == 420) { tx("L*X"); tx("L*A:i=meter,c=ffffff"); } }, 780, 0 },
  { "a comet at its top speed", []() { pShow(); tx("L*A:i=comet,c=ffffff,s=255"); }, [](uint32_t) {}, 1000, 0 },
  { "a comet and a full look by turns, every 170 ms", pShow,
    [](uint32_t t) { if (t == 0) tx("L*A:i=comet,c=ffffff,s=255"); if (t == 170) tx("L*A:i=solid,c=ffffff"); }, 340, 0 },
  { "a comet and a full look by turns: 300 ms of comet, 170 ms of the look", pShow,
    [](uint32_t t) { if (t == 0) tx("L*A:i=comet,c=ffffff,s=255"); if (t == 300) tx("L*A:i=solid,c=ffffff"); }, 470, 0 },
  // KNOWN, NOT FIXED (CA1.6): two things that each light a part of the field, by turns. The comet
  // passes a region and the full look that follows lights it again: two onsets in one 680 ms
  // cycle, four in a second. No whole-field measure can see either: the field's level only
  // ever goes up at the full look, from a field that is lit.
  { "a comet and a full look by turns: 510 ms of comet, 170 ms of the look", pShow,
    [](uint32_t t) { if (t == 0) tx("L*A:i=comet,c=ffffff,s=255"); if (t == 510) tx("L*A:i=solid,c=ffffff"); }, 680, BROKE_REGION },
  { "a wipe and a full look by turns", pShow,
    [](uint32_t t) { if (t == 0) tx("L*A:i=wipe,c=ffffff,s=255"); if (t == 170) tx("L*A:i=solid,c=ffffff"); }, 340, 0 },
  { "a chase at its top speed, and a full look every 510 ms", pShow,
    [](uint32_t t) { if (t == 0) tx("L*A:i=chase,c=ffffff,s=255"); if (t == 340) tx("L*A:i=solid,c=ffffff"); }, 510, 0 },
  { "sparkle, twinkle and scan at their top speeds, by turns", pShow,
    [](uint32_t t) { if (t == 0) tx("L*A:i=sparkle,c=ffffff,s=255"); if (t == 2000) tx("L*A:i=twinkle,c=ffffff,s=255"); if (t == 4000) tx("L*A:i=scan,c=ffffff,s=255"); }, 6000, 0 },
  // The same with a meter: L lights the top half, and the full look would light it again 150 ms
  // later. The gate holds that frame, and a held frame is not shown at all (while the layer
  // showed it dimmed, six tenths of a full look lit the half that was dark: four times a second).
  { "a meter at half and a full look by turns, with L going up in between", []() { pShow(); tx("L*L:v=128"); },
    [](uint32_t t) { if (t == 0) tx("L*A:i=meter,c=ffffff"); if (t == 20) tx("L*L:v=255"); if (t == 40) tx("L*L:v=128"); if (t == 170) tx("L*A:i=solid,c=ffffff"); }, 340, 0 },
};
static const int kProfileCount = (int)(sizeof kProfiles / sizeof kProfiles[0]);
static const int kRandomExpected = 0;

struct Result {
  int worst = 0; uint32_t minGap = 0xFFFFFFFFu, narrowGap = 0xFFFFFFFFu, lumGap = 0xFFFFFFFFu, colourGap = 0xFFFFFFFFu, regionGap = 0xFFFFFFFFu;
  long regionOnsets = 0; int regionMost = 0;
  int overCeiling = 0, notNative = 0, notDark = 0, wdLate = 0;
  long onsets = 0, darkChecked = 0, nativeChecked = 0, contractFrames = 0, lines = 0;   // how much was looked at
  long presses = 0, buttonChecked = 0, colourChanges = 0, lumOnsets = 0; int notButton = 0;
  void take() {
    for (int u = 0; u < 2; u++) {
      for (const Onsets& o : gW[u].obs) { if (o.worst > worst) worst = o.worst; if (o.minGap < minGap) minGap = o.minGap; }
      for (const Onsets& o : gW[u].narrow) if (o.minGap < narrowGap) narrowGap = o.minGap;
      for (const Onsets& o : gW[u].lum) if (o.minGap < lumGap) lumGap = o.minGap;
      for (const Onsets& o : gW[u].lumNarrow) if (o.minGap < lumGap) lumGap = o.minGap;
      if (gW[u].colourMinGap < colourGap) colourGap = gW[u].colourMinGap;
      if (gW[u].regionMinGap < regionGap) regionGap = gW[u].regionMinGap;
      regionOnsets += gW[u].regionOnsets; if (gW[u].regionMost > regionMost) regionMost = gW[u].regionMost;
      overCeiling += gW[u].overCeiling; notNative += gW[u].notNative; notDark += gW[u].notDark;
      onsets += gW[u].obs[0].total; darkChecked += gW[u].darkChecked; nativeChecked += gW[u].nativeChecked; contractFrames += gW[u].contractFrames;
      notButton += gW[u].notButton; buttonChecked += gW[u].buttonChecked;
      colourChanges += gW[u].colourChanges; lumOnsets += gW[u].lum[0].total;
    }
    presses += gPresses; gPresses = 0;
    wdLate += gWatchdogLate; gWatchdogLate = 0;
    lines += gLines; gLines = 0;
  }
  void add(const Result& o) {
    if (o.worst > worst) worst = o.worst;
    if (o.minGap < minGap) minGap = o.minGap;
    if (o.narrowGap < narrowGap) narrowGap = o.narrowGap;
    if (o.lumGap < lumGap) lumGap = o.lumGap;
    if (o.colourGap < colourGap) colourGap = o.colourGap;
    if (o.regionGap < regionGap) regionGap = o.regionGap;
    regionOnsets += o.regionOnsets; if (o.regionMost > regionMost) regionMost = o.regionMost;
    overCeiling += o.overCeiling; notNative += o.notNative; notDark += o.notDark; wdLate += o.wdLate;
    onsets += o.onsets; darkChecked += o.darkChecked; nativeChecked += o.nativeChecked; contractFrames += o.contractFrames; lines += o.lines;
    notButton += o.notButton; presses += o.presses; buttonChecked += o.buttonChecked;
    colourChanges += o.colourChanges; lumOnsets += o.lumOnsets;
  }
  // (a) is the spec's "at most three in any second". The gate promises more, and that is held to
  // as well: no two onsets less than 340 ms apart, for every observer.
  int broke() const {
    return (worst > 3 || minGap < 2 * STROBE_MIN_STATE_MS ? BROKE_ONSETS : 0)
         | (narrowGap < 2 * STROBE_MIN_STATE_MS ? BROKE_NARROW : 0)
         | (lumGap < 2 * STROBE_MIN_STATE_MS ? BROKE_LUMINANCE : 0)
         | (colourGap < STROBE_MIN_STATE_MS ? BROKE_COLOUR : 0)
         | (regionMost > 3 ? BROKE_REGION : 0)
         | (overCeiling || notNative || notDark || wdLate || notButton ? BROKE_OTHER : 0);
  }
  void print(const char* verdict, const char* name) const {
    printf("%s S9 %s:", verdict, name);
    const int b = broke();
    if (b == 0) printf(" holds");
    if (b & BROKE_ONSETS)    printf(" onsets %u ms apart (%d in one second);", minGap, worst);
    if (b & BROKE_NARROW)    printf(" narrow-band onsets %u ms apart;", narrowGap);
    if (b & BROKE_LUMINANCE) printf(" luminance onsets %u ms apart;", lumGap);
    if (b & BROKE_COLOUR)    printf(" colour changes %u ms apart;", colourGap);
    if (b & BROKE_REGION)    printf(" a region of a quarter of the field or more lit %d times in one second (the closest two %u ms apart);", regionMost, regionGap);
    if (notDark)     printf(" %d frames lit after X;", notDark);
    if (notNative)   printf(" %d frames not native after idle;", notNative);
    if (overCeiling) printf(" %d frames over the ceiling;", overCeiling);
    if (wdLate)      printf(" the watchdog was late %d times;", wdLate);
    if (notButton)   printf(" %d frames not on the sequence a button started;", notButton);
    printf("\n");
  }
};
// The rear display's size, by turns (the front one is 8 by 10): a look that lights columns or
// rows covers another share of a field of 4 rows, or of 28 columns.
static const int kRearSizes[5][2] = { { 8, 10 }, { 28, 4 }, { 16, 4 }, { 32, 8 }, { 24, 4 } };
static Result runProfile(const Profile& p, int rounds) {
  Result res;
  for (int round = 0; round < rounds; round++) {
    const bool noise = (round & 1) != 0;
    mockEngineSize(1, kRearSizes[(round / 2) % 5][0], kRearSizes[(round / 2) % 5][1]);
    begin(); p.setup();
    for (int i = 0; i < 50; i++) step();          // the set-up settles (and its live look's hold ends)
    const uint32_t jitter = rnd(4) * 10;
    for (uint32_t t = 0; t < 8000; t += 10) {
      p.at((t + jitter) % p.period);
      if (noise && rnd(40) == 0) { const std::string who = pick({ "R", "F" }); tx("L" + who + pick({ "Q:r=0", "L:v=128", "V:n=0,k=0000", "B:b=255" })); }
      step();
    }
    res.take();
  }
  return res;
}
static Result runRandom(int streams) {
  Result res;
  for (int sidx = 0; sidx < streams; sidx++) {
    mockEngineSize(1, kRearSizes[sidx % 5][0], kRearSizes[sidx % 5][1]);
    begin();
    if (rnd(3) == 0) { LogicEngineFLDDefault.fBri = (byte)(40 + rnd(216)); FLD->_liveBri = LogicEngineFLDDefault.fBri;   // the owner's own setting
                       gW[0].ceil = LogicEngineFLDDefault.fBri; }
    const uint32_t busy = 2 + rnd(30);            // one line every `busy` frames on average
    const bool buttons = rnd(2) == 0;             // half the streams have an owner at the controller
    const bool fine = rnd(4) == 0;                // a quarter pass through the loop as the board does: every 1 to 4 ms
    if (fine) {
      for (uint32_t t = 0; t < 12000; ) {
        if (rnd(busy * 4) == 0) { const bool burst = rnd(8) == 0; for (uint32_t n = 1 + (burst ? rnd(12) : 0); n > 0; n--) tx(randomLine()); }
        if (buttons && rnd(1400) == 0) { static const int kSeq[] = { 0, 1, 2, 4, 5, 6, 14, 20, 3 }; const int u = (int)rnd(2); press(u, kSeq[rnd(9)]); }
        const uint32_t dt = 1 + rnd(4);
        step(dt); t += dt;
      }
      res.take();
      continue;
    }
    for (uint32_t t = 0; t < 12000; t += 10) {
      if (rnd(busy) == 0) { const bool burst = rnd(8) == 0; for (uint32_t n = 1 + (burst ? rnd(12) : 0); n > 0; n--) tx(randomLine()); }
      if (buttons && rnd(350) == 0) {              // about three presses a stream
        static const int kSeq[] = { 0, 1, 2, 4, 5, 5, 14, 20, 3 };
        const int u = (int)rnd(2); press(u, kSeq[rnd(9)]);
      }
      const uint32_t slow = rnd(20) == 0 ? rnd(4) * 10 : 0;
      step(10 + slow);                             // now and then a slow pass
    }
    res.take();
  }
  return res;
}

// A stream, and what it is EXPECTED to break on the layer as it stands (BROKE_ bits; 0: it holds).
// While a defect is in the code its streams print XFAIL and the suite stays green. The task that
// fixes it clears the bits; a stream that breaks anything else than expected, or that no longer
// breaks what it is marked for, fails the suite (as KNOWN_BUG does).
static void verdict(const char* name, const Result& res, int expected) {
  const int b = res.broke();
  if (getenv("PROPERTY_DISCOVER")) { printf("%2d  %s\n", b, name); return; }
  if (b == expected) { if (b) { res.print("XFAIL", name); gKnown++; } return; }
  res.print((b & ~expected) ? "FAIL " : "XPASS", name);
  printf("      expected to break %d, broke %d (1 onsets, 2 narrow-band onsets, 4 luminance, 8 colour changes, 16 other, 32 regions)\n", expected, b);
  gFails++;
}

int main() {
  // For hunting, not for the suite: PROPERTY_SEED=<n> and PROPERTY_STREAMS=<n> run other streams,
  // and PROPERTY_TRACE=1 prints the lines and levels that led up to the first break.
  gTraceOn = getenv("PROPERTY_TRACE") != nullptr;
  if (const char* seed = getenv("PROPERTY_SEED")) gRng = (uint32_t)strtoul(seed, nullptr, 0) | 1u;
  const uint32_t seedUsed = gRng;
  const int streams = getenv("PROPERTY_STREAMS") ? atoi(getenv("PROPERTY_STREAMS")) : 1500;
  // The overlay that expires on a dark baseline (the 2026-11-16 review's finding 1): wildcard accents
  // in a show where the rear display has no look of its own. Five 170 ms pulses are 850 ms of light.
  mockEngineSize(1, 8, 10);
  begin(); tx("**M:v=show,wd=15"); tx("LFA:i=solid,c=0000ff"); tx("**C:bpm=120,ph=0,bpb=4,beat=0");
  step(); gW[1].litMs = 0;                                 // (the native look before the show was lit, of course)
  for (uint32_t t = 0; t < 10000; t += 10) { if (t % 2000 == 0) tx("L*P:c=ffffff,d=170"); if (t % 4000 == 0) tx("**Q"); step(); }
  if (gW[1].litMs > 1000) printf("      (the rear display was lit %ld ms of 10000)\n", gW[1].litMs);
  CHECK(gW[1].litMs >= 700 && gW[1].litMs <= 1000);

  Result all;
  for (int i = 0; i < kProfileCount; i++) { const Result r = runProfile(kProfiles[i], 10); verdict(kProfiles[i].name, r, kProfiles[i].expected); all.add(r); }
  Result rr = runRandom(streams);
  // (h) is ASSERTED on the profiles above: every look by itself, and the scripts that light a
  // part of the field. In the random streams it is measured and printed, not asserted: they mix
  // the two known cases above in every way (a comet or a meter, and a change of look or a
  // hand-back just after it), and a random stream breaks or holds by the luck of its seed.
  const int randomRegionMost = rr.regionMost; const uint32_t randomRegionGap = rr.regionGap;
  rr.regionMost = 0; rr.regionGap = 0xFFFFFFFFu;
  verdict("random streams", rr, kRandomExpected); all.add(rr);

  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("     (colour changes: %ld, the closest two %u ms apart; luminance onsets: %ld, the closest two %u ms apart; narrow-band onsets the closest two %u ms apart;\n"
         "      onsets of a region of a quarter of the field or more: %ld; in the random streams, NOT asserted: at most %d of one region in a\n"
         "      second, the closest two %u ms apart)\n",
         all.colourChanges, all.colourGap, all.lumOnsets, all.lumGap, all.narrowGap, all.regionOnsets, randomRegionMost, randomRegionGap);
  printf("S9 property OK (%d known defect(s) still open): %ld lines, %ld frames drawn by the contract, %ld onsets, the closest two %u ms apart;\n"
         "   %ld frames checked dark after X, %ld checked native after idle or the watchdog, %ld button presses and %ld frames\n"
         "   checked on the button's sequence (seed 0x%08X, %d random streams)\n",
         gKnown, all.lines, all.contractFrames, all.onsets, all.minGap, all.darkChecked, all.nativeChecked, all.presses, all.buttonChecked,
         (unsigned)seedUsed, streams);
  return 0;
}
