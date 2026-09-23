// DroidNet native brightness: DIFFERENTIAL + ROBUSTNESS fuzzer for nativeBrightnessParse().
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// FORK-LOCAL. This is not fuzz_parsers.cpp, which is shared byte-identically across the
// RSeries, PSI and Flthy forks and must not be edited here.
//
// Build it the way test/host/run.sh does, with the sanitizers on and a fixed seed:
//
//   clang++ -std=c++17 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined \
//           -fno-sanitize-recover=all test/host/fuzz_native_brightness.cpp \
//           -o /tmp/rseries_native_brightness_fuzz && /tmp/rseries_native_brightness_fuzz
//
// Every tail is copied into a HEAP buffer of exactly strlen()+1 bytes, so one byte read past
// the NUL is an AddressSanitizer failure rather than a silent success. Every result is
// compared with modelParse(), written independently with std::string, and a rejected tail
// must leave both outputs untouched.
//
//   A. exhaustive: every string of length 0..5 over a 14-character alphabet
//   B. every byte 0x01..0xFF at every position of a set of valid tails
//   C. every digit string of length 1..4 behind every unit prefix
//   D. 400,000 fixed-seed random tails over the whole byte range, length 0..12
#include "../../src/native/native_brightness_parse.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static long g_total = 0;
static long g_fail = 0;
static int g_reported = 0;

static constexpr uint64_t kSeed = 0x0dd1e5eedbeef0b1ULL;
static constexpr uint8_t kSentinel = 0x5A;

// Independent model of the grammar: optional uppercase F/R, then 1-3 ASCII digits worth
// 1..255, then the end. Written with std::string and std::stoi, not a character walk.
static bool modelParse(const std::string& s, uint8_t& mask, uint8_t& level) {
  std::string body = s;
  uint8_t m = 0x03;
  if (!body.empty() && body[0] == 'F') { m = 0x01; body.erase(0, 1); }
  else if (!body.empty() && body[0] == 'R') { m = 0x02; body.erase(0, 1); }
  if (body.empty() || body.size() > 3) return false;
  if (body.find_first_not_of("0123456789") != std::string::npos) return false;
  const int v = std::stoi(body);
  if (v < 1 || v > 255) return false;
  mask = m;
  level = static_cast<uint8_t>(v);
  return true;
}

static std::string printable(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    if (c >= 0x20 && c < 0x7F) out += static_cast<char>(c);
    else { char buf[8]; snprintf(buf, sizeof buf, "\\x%02x", c); out += buf; }
  }
  return out;
}

static void check(const std::string& tail) {
  // Heap copy of exactly strlen()+1 bytes. An embedded NUL ends the C string, as on the wire.
  const size_t n = strlen(tail.c_str());
  char* buf = static_cast<char*>(malloc(n + 1));
  memcpy(buf, tail.c_str(), n + 1);
  const std::string cstr(buf, n);

  uint8_t gotMask = kSentinel, gotLevel = kSentinel;
  uint8_t wantMask = kSentinel, wantLevel = kSentinel;
  const bool got = nativeBrightnessParse(buf, gotMask, gotLevel);
  const bool want = modelParse(cstr, wantMask, wantLevel);
  free(buf);

  g_total++;
  const bool same = (got == want) && (gotMask == wantMask) && (gotLevel == wantLevel);
  if (!same) {
    g_fail++;
    if (g_reported++ < 20)
      printf("FAIL tail \"%s\": parser ok=%d mask=0x%02x level=%u, model ok=%d mask=0x%02x level=%u\n",
             printable(cstr).c_str(), got, gotMask, gotLevel, want, wantMask, wantLevel);
  }
}

// A. Every string of length 0..5 over an alphabet that covers every grammar edge.
static void sectionA() {
  static const char kAlpha[] = {'F', 'R', 'f', 'r', 'X', '0', '1', '2', '5', '9', ' ', '-', '+', '|'};
  const int k = static_cast<int>(sizeof kAlpha);
  check("");
  for (int len = 1; len <= 5; len++) {
    long combos = 1;
    for (int i = 0; i < len; i++) combos *= k;
    std::string s(static_cast<size_t>(len), ' ');
    for (long idx = 0; idx < combos; idx++) {
      long x = idx;
      for (int i = 0; i < len; i++) { s[static_cast<size_t>(i)] = kAlpha[x % k]; x /= k; }
      check(s);
    }
  }
}

// B. Every non-NUL byte at every position (and appended) of a set of valid tails.
static void sectionB() {
  static const char* const kBase[] = {"1", "128", "255", "F1", "F255", "R128", "007"};
  for (const char* base : kBase) {
    const std::string b(base);
    for (size_t pos = 0; pos <= b.size(); pos++) {
      for (int byte = 1; byte <= 0xFF; byte++) {
        std::string replaced = b;
        if (pos < b.size()) replaced[pos] = static_cast<char>(byte);
        else replaced.push_back(static_cast<char>(byte));
        check(replaced);
        std::string inserted = b;
        inserted.insert(pos, 1, static_cast<char>(byte));
        check(inserted);
      }
    }
  }
}

// C. Every digit string of length 1..4 behind every unit prefix, leading zeros included.
static void sectionC() {
  static const char* const kUnits[] = {"", "F", "R", "f", "r", "X", " "};
  for (const char* unit : kUnits) {
    for (int len = 1; len <= 4; len++) {
      int limit = 1;
      for (int i = 0; i < len; i++) limit *= 10;
      for (int v = 0; v < limit; v++) {
        char digits[8];
        snprintf(digits, sizeof digits, "%0*d", len, v);
        check(std::string(unit) + digits);
      }
    }
  }
}

// D. Fixed-seed random tails over the whole byte range (xorshift64*: deterministic).
static uint64_t g_rng = kSeed;
static uint64_t nextRand() {
  g_rng ^= g_rng >> 12; g_rng ^= g_rng << 25; g_rng ^= g_rng >> 27;
  return g_rng * 0x2545F4914F6CDD1DULL;
}
static void sectionD() {
  static const char kBias[] = "FRfr0123456789 -+|X";
  for (int iter = 0; iter < 400000; iter++) {
    const size_t len = static_cast<size_t>(nextRand() % 13);
    std::string s(len, ' ');
    const bool biased = (iter % 2) == 0;
    for (size_t i = 0; i < len; i++) {
      if (biased) s[i] = kBias[nextRand() % (sizeof kBias - 1)];
      else s[i] = static_cast<char>(1 + (nextRand() % 0xFF));
    }
    check(s);
  }
}

int main() {
  sectionA();
  sectionB();
  sectionC();
  sectionD();
  printf("%s  %ld/%ld checks passed  (seed 0x%016llx)\n", g_fail ? "FAILURES" : "OK",
         g_total - g_fail, g_total, static_cast<unsigned long long>(kSeed));
  return g_fail ? 1 : 0;
}
