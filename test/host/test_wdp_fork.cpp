// Host checks for src/wdp/wdp_fork.h, the RSeries fork's WDP announce adapter: the identity the
// firmware really sends builds a valid line, and wdpForkTick() writes exactly what the core hands
// out, never more than the port says it has room for. Fork-local (not shared with PSI/Flthy).
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.

// What a build from a tree with changes carries: a 7-char sha plus "-dirty". Longer fw strings,
// up to the WCB's limit, are checked below.
#define WDP_FW "abcdef0-dirty"
#include "../../src/wdp/wdp_fork.h"
#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

static const char kLine[] =
    "@WDP1 {\"type\":\"R-Series Logics\",\"fw\":\"abcdef0-dirty\","
    "\"caps\":[\"droidnet.contract\",\"led.logic\"]}\n";

// HardwareSerial's two calls the adapter uses: availableForWrite() and write(buf, n).
struct MockPort {
  int room;
  std::string out;
  int writes, biggest;
  explicit MockPort(int r) : room(r), writes(0), biggest(0) {}
  int availableForWrite() { return room; }
  size_t write(const uint8_t* p, size_t n) {
    out.append((const char*)p, n);
    writes++;
    if ((int)n > biggest) biggest = (int)n;
    return n;
  }
};

static void run(MockPort& port, uint32_t from, uint32_t until) {
  for (uint32_t t = from; t != until; t++) wdpForkTick(port, t);
}

static void test_the_real_identity_builds_the_expected_line() {
  char out[WDP_LINE_MAX];
  uint8_t n = wdpBuildLine(out, sizeof(out), WDP_TYPE, WDP_FW, WDP_CAPS);
  CHECK(n == sizeof(kLine) - 1);
  CHECK(std::string(out, n) == kLine);
  printf("    line (%u bytes): %.*s\\n\n", (unsigned)n, (int)n - 1, out);
}

static void test_any_fw_the_wcb_accepts_fits_the_line_buffer() {
  // WDP_FW_MAX (27) chars of fw, the most the WCB keeps: still fits WDP_LINE_MAX.
  char out[WDP_LINE_MAX];
  CHECK(wdpBuildLine(out, sizeof(out), WDP_TYPE, "0123456789abcdef0123456789a", WDP_CAPS) > 0);
  CHECK(wdpBuildLine(out, sizeof(out), WDP_TYPE, "unknown", WDP_CAPS) > 0);
}

static void test_nothing_is_written_before_init() {
  MockPort port(128);
  run(port, 0, 60000);
  CHECK(port.out.empty());
  CHECK(port.writes == 0);
}

static void test_one_whole_line_in_one_write_when_the_fifo_has_room() {
  // An idle ESP32 UART reports 128 free (the TX FIFO; no TX ring buffer is installed).
  MockPort port(128);
  wdpForkInit(12345, 0);
  run(port, 0, 3001);
  CHECK(port.out == kLine);
  CHECK(port.writes == 1);
}

static void test_a_small_room_sends_the_line_in_pieces() {
  MockPort port(10);
  wdpForkInit(12345, 0);
  run(port, 0, 3001);
  CHECK(port.out == kLine);
  CHECK(port.biggest <= 10);
  CHECK(port.writes == (int)((sizeof(kLine) - 1 + 9) / 10));
}

static void test_no_room_writes_nothing_and_loses_nothing() {
  MockPort port(0);
  wdpForkInit(12345, 0);
  run(port, 0, 3001);
  CHECK(port.out.empty());
  port.room = 128;
  wdpForkTick(port, 3001);
  CHECK(port.out == kLine);
}

static void test_it_repeats_every_25_to_30_seconds() {
  MockPort port(128);
  wdpForkInit(777, 0);
  run(port, 0, 3001 + 30000 * 3);
  // The first by 3 s and three more 25-30 s apart all land by 93 s; a fifth cannot come
  // before 0.5 + 4 x 25 = 100.5 s. So exactly four whole lines.
  CHECK(port.writes == 4);
  CHECK(port.out == std::string(kLine) + kLine + kLine + kLine);
}

static void test_the_seed_differs_for_boards_whose_rng_agrees() {
  // Two boards whose RNG has no entropy yet return the same word; their MACs differ in the
  // NIC-specific bytes (3-5; 0-2 are Espressif's OUI). Each of those bytes changes the seed.
  const uint64_t a = 0x0000AABBCC0A0B0CULL;
  CHECK(wdpForkSeed(0x5EED, a) != wdpForkSeed(0x5EED, a ^ (0xFFULL << 24)));
  CHECK(wdpForkSeed(0x5EED, a) != wdpForkSeed(0x5EED, a ^ (0xFFULL << 32)));
  CHECK(wdpForkSeed(0x5EED, a) != wdpForkSeed(0x5EED, a ^ (0xFFULL << 40)));
  CHECK(wdpForkSeed(1, a) != wdpForkSeed(2, a));
}

int main() {
  test_the_real_identity_builds_the_expected_line();
  test_any_fw_the_wcb_accepts_fits_the_line_buffer();
  test_nothing_is_written_before_init();   // must run first: init is one-way
  test_one_whole_line_in_one_write_when_the_fifo_has_room();
  test_a_small_room_sends_the_line_in_pieces();
  test_no_room_writes_nothing_and_loses_nothing();
  test_it_repeats_every_25_to_30_seconds();
  test_the_seed_differs_for_boards_whose_rng_agrees();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
