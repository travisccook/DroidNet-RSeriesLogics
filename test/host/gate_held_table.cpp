// How much of each contract look does the S9 field gate hold back? Not a test: a measurement,
// for docs/CHANGING_THE_CORE.md ("What the gate costs a look"). For every look the contract
// draws, at five speeds, on both displays: the share of frames the gate scaled down, and by how
// much on average, over 60 s of the look running by itself. Then the beat pump at four tempi.
//   c++ -std=c++17 -O1 -I. test/host/gate_held_table.cpp -o /tmp/held && /tmp/held
static unsigned long gStatFrames[2], gStatHeld[2], gStatLoss[2];
#define CONTRACT_GATE_STAT(unit, scale) do { gStatFrames[unit]++; if ((scale) != 255) { gStatHeld[unit]++; gStatLoss[unit] += 255u - (scale); } } while (0)
#include "logics_harness.h"
#include <cstdio>
#include <string>

static bool gJitter = false;
static void measure(const std::string& setup, const std::string& look, uint32_t ms, double out[2][2]) {
  resetAll();
  for (int i = 0; i < 50; i++) { _mock_millis += 10; frame(); }
  send("**M:v=show");
  if (!setup.empty()) send(setup.c_str());
  send(("L*A:" + look).c_str());
  for (int i = 0; i < 100; i++) { _mock_millis += 10; frame(); }      // the look is up, its first onset is behind it
  for (int u = 0; u < 2; u++) gStatFrames[u] = gStatHeld[u] = gStatLoss[u] = 0;
  // frames 10 to 15 ms apart, as a loop with other work in it draws them (a fixed generator)
  uint32_t x = 0x2545F491u;
  for (uint32_t t = 0; t < ms; ) {
    x = x * 1664525u + 1013904223u;
    const uint32_t dt = gJitter ? 10u + (x >> 24) % 6u : 10u;
    _mock_millis += dt; t += dt; frame();
  }
  for (int u = 0; u < 2; u++) {
    out[u][0] = gStatFrames[u] ? 100.0 * gStatHeld[u] / gStatFrames[u] : 0;
    out[u][1] = gStatHeld[u] ? 100.0 * gStatLoss[u] / (255.0 * gStatHeld[u]) : 0;
  }
}
static void table(bool jitter);
int main() {
  table(false);
  printf("\nThe same with frames 10 to 15 ms apart:\n\n");
  table(true);
  return 0;
}
static void table(bool jitter) {
  gJitter = jitter;
  static const char* const kLooks[] = { "solid", "flash", "pulse", "rainbow", "scan", "sparkle", "meter", "comet", "chase",
                                        "wipe", "gradient", "colorcycle", "twinkle" };
  static const int kSpeeds[] = { 0, 64, 128, 192, 255 };
  printf("Frames the field gate held back, %% of all frames (and, in brackets, how far down on average, %% of the frame's level).\n");
  printf("Front display / rear display. 60 s of each look by itself, white, b=255, no beat clock. Step: more than a tenth of the ceiling.\n\n");
  printf("%-11s", "look");
  for (int s : kSpeeds) printf("  s=%-3d              ", s);
  printf("\n");
  for (const char* look : kLooks) {
    printf("%-11s", look);
    for (int s : kSpeeds) {
      double o[2][2];
      measure(std::string(look) == "meter" ? "L*L:v=128" : "", std::string("i=") + look + ",c=ffffff,s=" + std::to_string(s), 60000, o);
      printf("  %4.1f (%2.0f) / %4.1f (%2.0f)", o[0][0], o[0][1], o[1][0], o[1][1]);
    }
    printf("\n");
  }
  printf("\nThe beat pump on a solid white look (am=1: every beat), front display:\n%-22s", "");
  static const int kBpm[] = { 60, 120, 176, 240 };
  for (int b : kBpm) printf("  %3d BPM     ", b);
  printf("\n");
  for (int m : { 64, 128, 255 }) {
    printf("m=%-3d                 ", m);
    for (int b : kBpm) {
      double o[2][2];
      measure("**C:bpm=" + std::to_string(b) + ",ph=0,bpb=4,beat=0", "i=solid,c=ffffff,am=1,m=" + std::to_string(m), 60000, o);
      printf("  %4.1f (%2.0f)   ", o[0][0], o[0][1]);
    }
    printf("\n");
  }
}
