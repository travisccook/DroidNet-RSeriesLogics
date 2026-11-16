// What does the S9 field gate hold back of each contract look, and does a look light a PART of
// the display too often by itself? Not a test: a measurement, for docs/CHANGING_THE_CORE.md
// ("What the gate costs a look"). For every look the contract draws, at five speeds, in five
// colours, on both displays (the front one 8 by 10, the rear one 28 by 4):
//   * the share of frames the gate held, over 60 s of the look running by itself, with frames
//     10 ms apart and with frames 10 to 15 ms apart (the worse of the two displays). A held frame
//     is not shown: the frame before it stays up (CA1.6; up to CA1.5 it was shown dimmed);
//   * in white: the most often any region of a quarter of the display or more went from dark
//     to lit in one second, and the closest two such onsets. A region is watched as the property
//     stage watches it (test_contract_property.cpp, (h)).
// Then the beat pump at four tempi.
//   c++ -std=c++17 -O1 -I. test/host/gate_held_table.cpp -o /tmp/held && /tmp/held
static unsigned long gStatFrames[2], gStatHeld[2];
#define CONTRACT_GATE_STAT(unit, scale) do { gStatFrames[unit]++; if ((scale) != 255) gStatHeld[unit]++; } while (0)
#include "logics_harness.h"
#include <cstdio>
#include <string>

// ---- a region of a quarter of the display or more (the property stage's definition) ----
struct Region { int x0, x1, y0, y1; bool lit[3]; uint32_t at[3][4]; int n[3]; };
static const int kBands[3][2] = { { 10, 50 }, { 19, 75 }, { 4, 31 } };
struct Regions {
  Region g[17]; int count = 0, most = 0; uint32_t closest = 0xFFFFFFFFu;
  void begin(const LogicEngineRenderer* r) {
    count = most = 0; closest = 0xFFFFFFFFu;
    const int W = r->width(), H = r->height(), cw = (W + 3) / 4, rh = (H + 3) / 4;
    auto add = [&](int x0, int x1, int y0, int y1) { Region& q = g[count++]; q = Region(); q.x0 = x0; q.x1 = x1; q.y0 = y0; q.y1 = y1; };
    add(0, W, 0, H);
    add(0, W / 2, 0, H); add(W / 2, W, 0, H); add(0, W, 0, H / 2); add(0, W, H / 2, H);
    add(0, W / 2, 0, H / 2); add(W / 2, W, 0, H / 2); add(0, W / 2, H / 2, H); add(W / 2, W, H / 2, H);
    for (int i = 0; i < 4; i++) { const int x0 = (i * (W - cw) + 1) / 3; add(x0, x0 + cw, 0, H); }
    for (int i = 0; i < 4; i++) { const int y0 = (i * (H - rh) + 1) / 3; add(0, W, y0, y0 + rh); }
  }
  void see(const LogicEngineRenderer* r, int ceil, uint32_t now) {
    const int W = r->width(), H = r->height(), N = W * H;
    static long sumV[11][33]; static int cnt[3][11][33];
    for (int x = 0; x <= W; x++) { sumV[0][x] = 0; for (int b = 0; b < 3; b++) cnt[b][0][x] = 0; }
    for (int y = 0; y < H; y++) {
      sumV[y + 1][0] = 0; for (int b = 0; b < 3; b++) cnt[b][y + 1][0] = 0;
      for (int x = 0; x < W; x++) {
        const CRGB c = r->px(x, y);
        const int v = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
        sumV[y + 1][x + 1] = sumV[y][x + 1] + sumV[y + 1][x] - sumV[y][x] + v;
        for (int b = 0; b < 3; b++) cnt[b][y + 1][x + 1] = cnt[b][y][x + 1] + cnt[b][y + 1][x] - cnt[b][y][x] + (v * 100 > ceil * kBands[b][0] ? 1 : 0);
      }
    }
    for (int i = 0; i < count; i++) {
      Region& q = g[i];
      const int area = (q.x1 - q.x0) * (q.y1 - q.y0);
      const long mean = (sumV[q.y1][q.x1] - sumV[q.y0][q.x1] - sumV[q.y1][q.x0] + sumV[q.y0][q.x0]) / area;
      for (int b = 0; b < 3; b++) {
        const int lit = cnt[b][q.y1][q.x1] - cnt[b][q.y0][q.x1] - cnt[b][q.y1][q.x0] + cnt[b][q.y0][q.x0];
        if (!q.lit[b] && mean * 100 >= (long)ceil * kBands[b][1] && lit * 4 >= N) {
          q.lit[b] = true;
          if (q.n[b] > 0 && now - q.at[b][(q.n[b] - 1) & 3] < closest) closest = now - q.at[b][(q.n[b] - 1) & 3];
          q.at[b][q.n[b]++ & 3] = now;
          int w = 0; for (int k = 0; k < 4 && k < q.n[b]; k++) if (now - q.at[b][(q.n[b] - 1 - k) & 3] < 1000) w++;
          if (w > most) most = w;
        } else if (q.lit[b] && mean * 100 <= (long)ceil * kBands[b][0]) q.lit[b] = false;
      }
    }
  }
};

struct Measured { double held[2]; int most[2]; uint32_t closest[2]; };
static Measured measure(const std::string& setup, const std::string& look, uint32_t ms, bool jitter) {
  mockEngineSize(1, 28, 4);
  resetAll();
  for (int i = 0; i < 50; i++) { _mock_millis += 10; frame(); }
  send("**M:v=show");
  if (!setup.empty()) send(setup.c_str());
  send(("L*A:" + look).c_str());
  for (int i = 0; i < 100; i++) { _mock_millis += 10; frame(); }      // the look is up, its first onset is behind it
  for (int u = 0; u < 2; u++) gStatFrames[u] = gStatHeld[u] = 0;
  static Regions reg[2];
  reg[0].begin(FLD); reg[1].begin(RLD);
  // frames 10 to 15 ms apart, as a loop with other work in it draws them (a fixed generator)
  uint32_t x = 0x2545F491u;
  for (uint32_t t = 0; t < ms; ) {
    x = x * 1664525u + 1013904223u;
    const uint32_t dt = jitter ? 10u + (x >> 24) % 6u : 10u;
    _mock_millis += dt; t += dt; frame();
    reg[0].see(FLD, 160, _mock_millis); reg[1].see(RLD, 140, _mock_millis);
  }
  Measured m;
  for (int u = 0; u < 2; u++) {
    m.held[u] = gStatFrames[u] ? 100.0 * gStatHeld[u] / gStatFrames[u] : 0;
    m.most[u] = reg[u].most; m.closest[u] = reg[u].closest;
  }
  return m;
}
static const char* const kLooks[] = { "solid", "flash", "pulse", "rainbow", "scan", "sparkle", "meter", "comet", "chase",
                                      "wipe", "gradient", "colorcycle", "twinkle" };
static const int kSpeeds[] = { 0, 64, 128, 192, 255 };
static double worse(const Measured& m) { return m.held[0] > m.held[1] ? m.held[0] : m.held[1]; }

int main() {
  printf("Frames the field gate held, %% of all frames: with frames 10 ms apart / with frames 10 to 15 ms apart.\n");
  printf("The worse of the two displays. 60 s of each look by itself, b=255, no beat clock. Step: more than a tenth of the ceiling.\n");
  for (const char* colour : { "ffffff", "ff0000", "00ff00", "0000ff", "116530" }) {
    printf("\nc=%s\n%-11s", colour, "look");
    for (int s : kSpeeds) printf("  s=%-3d       ", s);
    printf("\n");
    for (const char* look : kLooks) {
      printf("%-11s", look);
      for (int s : kSpeeds) {
        const std::string setup = std::string(look) == "meter" ? "L*L:v=128" : "";
        const std::string line = std::string("i=") + look + ",c=" + colour + ",s=" + std::to_string(s);
        printf("  %4.1f / %4.1f ", worse(measure(setup, line, 60000, false)), worse(measure(setup, line, 60000, true)));
      }
      printf("\n");
    }
  }
  printf("\nA part of the display, in white: the most often one region of a quarter of the display or more lit in a second\n");
  printf("(and the closest two of its onsets, ms). Front display 8 by 10 / rear display 28 by 4. Frames 10 to 15 ms apart.\n\n%-11s", "look");
  for (int s : kSpeeds) printf("  s=%-3d                 ", s);
  printf("\n");
  for (const char* look : kLooks) {
    printf("%-11s", look);
    for (int s : kSpeeds) {
      const Measured m = measure(std::string(look) == "meter" ? "L*L:v=128" : "", std::string("i=") + look + ",c=ffffff,s=" + std::to_string(s), 60000, true);
      char a[24], b[24];
      if (m.closest[0] == 0xFFFFFFFFu) snprintf(a, sizeof a, "%d", m.most[0]); else snprintf(a, sizeof a, "%d (%u)", m.most[0], (unsigned)m.closest[0]);
      if (m.closest[1] == 0xFFFFFFFFu) snprintf(b, sizeof b, "%d", m.most[1]); else snprintf(b, sizeof b, "%d (%u)", m.most[1], (unsigned)m.closest[1]);
      printf("  %9s / %-9s ", a, b);
    }
    printf("\n");
  }
  printf("\nThe beat pump on a solid look (am=1: every beat), frames 10 to 15 ms apart, %% of frames held (the worse display):\n%-22s", "");
  static const int kBpm[] = { 60, 120, 176, 240 };
  for (int b : kBpm) printf("  %3d BPM  ", b);
  printf("\n");
  for (const char* colour : { "ffffff", "116530" }) for (int m : { 64, 128, 255 }) {
    printf("c=%s m=%-3d        ", colour, m);
    for (int b : kBpm)
      printf("  %5.1f    ", worse(measure("**C:bpm=" + std::to_string(b) + ",ph=0,bpb=4,beat=0", std::string("i=solid,c=") + colour + ",am=1,m=" + std::to_string(m), 60000, true)));
    printf("\n");
  }
  return 0;
}
