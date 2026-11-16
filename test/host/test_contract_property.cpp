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
//     (d) is dark at once after X, and stays dark until a line that may relight it.
//
// Lines go in through contractHandleLine(), a frame is rendered through the mock engine every
// 10 ms (now and then later), and what is measured is the PIXELS: the full-field level of each
// display (fieldLevel(): the mean of each pixel's brightest channel). (a) is counted three times
// over, with three pairs of thresholds for "dark" and "lit", because the limit must not depend on
// where an observer draws those lines: 10 % and 50 % of the ceiling, 19 % and 75 % (the pair the
// 2026-11-16 review used), and 4 % and 31 %.
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
// that (c) can start its clock. Everything it ASSERTS is on the pixels and the latched effect.
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

// ---- what is measured, per display ----
struct Watch {
  LogicEngineRenderer* r = nullptr; int ceil = 255;
  Onsets obs[3];
  int overCeiling = 0;           // (b) frames with a pixel above the ceiling while effect 106 draws
  int notNative = 0;             // (c) frames not on a native look when the display should be
  int notDark = 0;               // (d) frames lit when the display should be dark
  uint32_t nativeFrom = 0; bool expectNative = false;
  uint32_t darkFrom = 0;   bool expectDark = false;
  long litMs = 0, darkChecked = 0, nativeChecked = 0, contractFrames = 0;
  void reset(LogicEngineRenderer* rr, int u) {
    *this = Watch(); r = rr; ceil = (u == 0 ? LogicEngineFLDDefault : LogicEngineRLDDefault).fBri;
    obs[1].darkPct = 19; obs[1].litPct = 75; obs[2].darkPct = 4; obs[2].litPct = 31;
  }
  void native(uint32_t now) { if (!expectNative) nativeFrom = now; expectNative = true; expectDark = false; }
  void dark(uint32_t now)   { darkFrom = now; expectDark = true; expectNative = false; }
  void unknown()            { expectNative = expectDark = false; }
  void frame(uint32_t now, uint32_t dt) {
    const int level = fieldLevel(r);
    if (level * 2 >= ceil) litMs += dt;
    for (Onsets& o : obs) o.see(level, ceil, now);
    if (r->_fx == 106) contractFrames++;
    if (r->_fx == 106)
      for (int y = 0; y < 10; y++) for (int x = 0; x < 8; x++) {
        const CRGB c = r->px(x, y);
        if (c.r > ceil || c.g > ceil || c.b > ceil) { overCeiling++; y = 10; break; }
      }
    // (c): giving the display back may wait for the onset window (340 ms) and an engine frame
    if (expectNative && now - nativeFrom > 400) { nativeChecked++; if (r->_fx == 106 || r->_fx == 14) notNative++; }
    // (d): one frame for the engine to notice, then dark
    if (expectDark && now - darkFrom > 30) { darkChecked++; if (level * 10 > ceil) notDark++; }
  }
};
static Watch gW[2];
static int gWatchdogLate = 0;    // (c) the watchdog did not fire when the test's own count said it must
static long gLines = 0;

// ---- PROPERTY_TRACE=1: when a property first breaks, print what led up to it ----
#include <cstdlib>
#include <vector>
struct TraceEv { uint32_t t; std::string what; };
static std::vector<TraceEv> gTrace; static bool gTraceOn = false, gTraced = false;
static void trace(const std::string& what) { if (gTraceOn) gTrace.push_back({ _mock_millis, what }); }
static void traceDump(const char* why, int u) {
  if (!gTraceOn || gTraced) return;
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
      char b[96]; snprintf(b, sizeof b, "  %c level %3d effect %3d base %d", u ? 'R' : 'F', fieldLevel(gW[u].r), gW[u].r->_fx, gContract[u].base);
      static std::string last[2]; if (last[u] != b) { last[u] = b; trace(b); }
      for (const Onsets& o : gW[u].obs) if (o.worst > 3) traceDump("a fourth onset in one second", u);
      if (getenv("PROPERTY_TRACE_GAP")) for (const Onsets& o : gW[u].obs) if (o.minGap < 340) traceDump("two onsets under 340 ms apart", u);
      if (gW[u].notNative > nn) traceDump("not native after idle", u);
      if (gW[u].notDark > nd) traceDump("lit after X", u);
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
                          "wipe", "gradient", "colorcycle", "twinkle", "native:0", "native:4", "native:14", "native:25", "bogus" });
  const char* col = pick({ "ffffff", "ffffff", "ff0000", "000000", "0000ff", "101010", "zzzzzz" });
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

// ---- the hostile profiles: the rows of the review's table, and three more of the same kind ----
struct Profile { const char* name; void (*setup)(); void (*at)(uint32_t t); uint32_t period; };
static void pShow() { tx("**M:v=show"); }
static const Profile kProfiles[] = {
  { "X between live A lines", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=solid,c=ffffff"); if (t == 50) tx("LFX"); if (t == 100) tx("LFA:i=solid,c=ffffff"); }, 170 },
  { "B 0/255 alternation", []() { pShow(); tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFB:b=0"); if (t == 50) tx("LFB:b=255"); }, 100 },
  { "P:b= alternation", []() { pShow(); tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFP:i=solid,c=ffffff,b=0"); if (t == 50) tx("LFP:i=solid,c=ffffff,b=255"); }, 100 },
  { "meter following L 0/255", []() { pShow(); tx("LFA:i=meter,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("L*L:v=0"); if (t == 50) tx("L*L:v=255"); }, 100 },
  { "C seeks across two scored sections", []() { pShow(); tx("LFA:i=solid,c=ffffff,at=0"); tx("LFA:i=off,at=100"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=120,beat=100"); if (t == 50) tx("L*C:bpm=120,beat=0"); }, 100 },
  { "C:bpm= re-seed restarting the pump", []() { pShow(); tx("LFA:i=solid,c=ffffff,am=2,m=255"); },
    [](uint32_t t) { if (t == 0) tx("L*C:bpm=300"); }, 150 },
  { "A cancelling an overlay, with P", []() { pShow(); tx("LFA:i=off"); },
    [](uint32_t t) { if (t == 0) tx("LFP:i=solid,c=ffffff"); if (t == 90) tx("LFA:i=off"); if (t == 260) tx("LFA:i=solid,c=ffffff");
                     if (t == 340) tx("LFP:i=off"); if (t == 430) tx("LFA:i=solid,c=ffffff"); if (t == 600) tx("LFA:i=off"); }, 680 },
  { "pump across section seams at 300 BPM",
    []() { pShow(); for (int b = 0; b < 32; b++) { char l[96]; snprintf(l, sizeof l, "LFA:i=solid,c=ffffff,at=%d,am=2,m=255,an=2,ap=%d", b, b % 2); tx(l); }
           tx("**C:bpm=300,ph=0,bpb=4,beat=0"); },
    [](uint32_t t) { if (t == 0) tx("**C:bpm=300,ph=0,bpb=4,beat=0"); }, 6200 },
  { "X and M:v=idle alternation (dark, then the native look)", []() { tx("LFA:i=solid,c=ffffff"); },
    [](uint32_t t) { if (t == 0) tx("LFX"); if (t == 50) tx("LFM:v=idle"); }, 100 },
  { "live A restarting a pulse", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=pulse,c=ffffff,s=255"); }, 170 },
  { "live A between white and native lights-out", pShow,
    [](uint32_t t) { if (t == 0) tx("LFA:i=solid,c=ffffff"); if (t == 50) tx("LFX"); if (t == 60) tx("LFA:i=native:14"); if (t == 120) tx("LFA:i=solid,c=ffffff"); }, 240 },
  { "a scored flash section delivered again and again", []() { pShow(); tx("LFA:i=flash,c=ffffff,s=255,at=0"); tx("L*C:bpm=120"); },
    [](uint32_t t) { if (t == 0) tx("LFA:i=flash,c=ffffff,s=255,at=0"); }, 200 },
};
static const int kProfileCount = (int)(sizeof kProfiles / sizeof kProfiles[0]);

struct Result {
  int worst = 0; uint32_t minGap = 0xFFFFFFFFu; int overCeiling = 0, notNative = 0, notDark = 0, wdLate = 0;
  long onsets = 0, darkChecked = 0, nativeChecked = 0, contractFrames = 0, lines = 0;   // how much was looked at
  void take() {
    for (int u = 0; u < 2; u++) {
      for (const Onsets& o : gW[u].obs) { if (o.worst > worst) worst = o.worst; if (o.minGap < minGap) minGap = o.minGap; }
      overCeiling += gW[u].overCeiling; notNative += gW[u].notNative; notDark += gW[u].notDark;
      onsets += gW[u].obs[0].total; darkChecked += gW[u].darkChecked; nativeChecked += gW[u].nativeChecked; contractFrames += gW[u].contractFrames;
    }
    wdLate += gWatchdogLate; gWatchdogLate = 0;
    lines += gLines; gLines = 0;
  }
  void add(const Result& o) {
    if (o.worst > worst) worst = o.worst;
    if (o.minGap < minGap) minGap = o.minGap;
    overCeiling += o.overCeiling; notNative += o.notNative; notDark += o.notDark; wdLate += o.wdLate;
    onsets += o.onsets; darkChecked += o.darkChecked; nativeChecked += o.nativeChecked; contractFrames += o.contractFrames; lines += o.lines;
  }
  // (a) is the spec's "at most three in any second". The gate promises more, and that is held to
  // as well: no two onsets less than 340 ms apart, for every observer.
  bool holds() const { return worst <= 3 && minGap >= 2 * STROBE_MIN_STATE_MS && overCeiling == 0 && notNative == 0 && notDark == 0 && wdLate == 0; }
  void print(const char* verdict, const char* name) const {
    printf("%s S9 %s: %d onsets in one second", verdict, name, worst);
    if (minGap != 0xFFFFFFFFu) printf(" (closest two %u ms apart)", minGap);
    if (notDark)     printf(", %d frames lit after X", notDark);
    if (notNative)   printf(", %d frames not native after idle", notNative);
    if (overCeiling) printf(", %d frames over the ceiling", overCeiling);
    if (wdLate)      printf(", the watchdog was late %d times", wdLate);
    printf("\n");
  }
};
static Result runProfile(const Profile& p, int rounds) {
  Result res;
  for (int round = 0; round < rounds; round++) {
    const bool noise = (round & 1) != 0;
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
    begin();
    if (rnd(3) == 0) { LogicEngineFLDDefault.fBri = (byte)(40 + rnd(216)); FLD->_liveBri = LogicEngineFLDDefault.fBri;   // the owner's own setting
                       gW[0].ceil = LogicEngineFLDDefault.fBri; }
    const uint32_t busy = 2 + rnd(30);            // one line every `busy` frames on average
    for (uint32_t t = 0; t < 12000; t += 10) {
      if (rnd(busy) == 0) { const bool burst = rnd(8) == 0; for (uint32_t n = 1 + (burst ? rnd(12) : 0); n > 0; n--) tx(randomLine()); }
      const uint32_t slow = rnd(20) == 0 ? rnd(4) * 10 : 0;
      step(10 + slow);                             // now and then a slow pass
    }
    res.take();
  }
  return res;
}

// A stream the layer does not hold yet. `still` is true while the defect is in the code: the line
// prints XFAIL and the suite stays green; the task that fixes it sets it to false, and from then
// on the stream must hold. (Fixed while still marked: the suite fails, as with KNOWN_BUG.)
static void verdict(const char* name, const Result& res, bool still) {
  if (!still) { if (!res.holds()) { res.print("FAIL ", name); gFails++; } return; }
  if (res.holds()) { res.print("XPASS", name); printf("      fixed: clear its `still` flag\n"); gFails++; }
  else { res.print("XFAIL", name); gKnown++; }
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
  begin(); tx("**M:v=show,wd=15"); tx("LFA:i=solid,c=0000ff"); tx("**C:bpm=120,ph=0,bpb=4,beat=0");
  step(); gW[1].litMs = 0;                                 // (the native look before the show was lit, of course)
  for (uint32_t t = 0; t < 10000; t += 10) { if (t % 2000 == 0) tx("L*P:c=ffffff,d=170"); if (t % 4000 == 0) tx("**Q"); step(); }
  if (gW[1].litMs > 1000) printf("      (the rear display was lit %ld ms of 10000)\n", gW[1].litMs);
  KNOWN_BUG("R1 review 1: an overlay that ends on a dark display leaves it lit", gW[1].litMs >= 700 && gW[1].litMs <= 1000);

  // On the layer of 4dbb04b every stream below fails. Clear a flag in the task that fixes it.
  static const bool kStill[] = { true, true, true, true, true, true, true, true, true, true, true, true };
  static_assert(sizeof kStill / sizeof kStill[0] == sizeof kProfiles / sizeof kProfiles[0], "one flag a profile");
  Result all;
  for (int i = 0; i < kProfileCount; i++) { const Result r = runProfile(kProfiles[i], 8); verdict(kProfiles[i].name, r, kStill[i]); all.add(r); }
  const Result rr = runRandom(streams);
  verdict("random streams", rr, true); all.add(rr);

  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("S9 property OK (%d known defect(s) still open): %ld lines, %ld frames drawn by the contract, %ld onsets, the closest two %u ms apart;\n"
         "   %ld frames checked dark after X, %ld checked native after idle or the watchdog (seed 0x%08X, %d random streams)\n",
         gKnown, all.lines, all.contractFrames, all.onsets, all.minGap, all.darkChecked, all.nativeChecked, (unsigned)seedUsed, streams);
  return 0;
}
