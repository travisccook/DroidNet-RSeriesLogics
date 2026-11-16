// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// src/contract/contract_line.h: the line assembler and the drain loop behind the fork's Serial1
// reader, including contract section 11: a 400-byte burst of back-to-back lines at 9600 baud,
// with no gap, loses nothing.
#include "../../src/contract/contract_line.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

static int gFails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL :%d  %s\n", __LINE__, #cond); gFails++; } } while (0)

static std::vector<std::string> feedAll(const std::string& bytes) {
  ContractLineBuf lb; std::vector<std::string> out;
  for (char c : bytes) if (lb.feed(c)) out.push_back(lb.buf);
  return out;
}

// A UART as the reader sees it: a receive buffer of `cap` bytes that the wire fills at `baud`.
// A byte that arrives while the buffer is full is LOST, as on the real port.
struct MockUart {
  std::deque<char> rx; size_t cap; std::string wire; size_t sent = 0; size_t lost = 0; double credit = 0;
  MockUart(const std::string& w, size_t c) : cap(c), wire(w) {}
  void advance(double ms, double baud) {             // 10 bits a byte: 8N1
    credit += ms * baud / 10000.0;
    while (credit >= 1.0 && sent < wire.size()) {
      credit -= 1.0;
      if (rx.size() < cap) rx.push_back(wire[sent]); else lost++;
      sent++;
    }
    if (sent >= wire.size()) credit = 0;
  }
  int available() { return (int)rx.size(); }
  int read() { if (rx.empty()) return -1; char c = rx.front(); rx.pop_front(); return (unsigned char)c; }
};

// The firmware's loop as the review measured it from the code: a pass at most once a millisecond,
// and about 3 ms of every 10 spent inside FastLED.show(). `budget` is the bytes a pass may take.
struct LoopResult { std::vector<std::string> contract, native; size_t lost; std::string echoed; };
static LoopResult runLoop(const std::string& wire, size_t rxCap, uint8_t budget, double baud = 9600) {
  MockUart uart(wire, rxCap); ContractLineBuf lb; LoopResult res;
  for (int ms = 0; ms < 60000; ms++) {
    uart.advance(1.0, baud);
    if (ms % 10 < 3) continue;                       // blocked in show(): no pass this millisecond
    contractDrain(lb, uart, budget,
      [&res](uint8_t b) { res.echoed.push_back((char)b); },
      [&res](const char* afterBang) { res.contract.push_back(afterBang); },
      [&res](const char* line) { res.native.push_back(line); });
    if (uart.sent >= wire.size() && uart.available() == 0) break;
  }
  res.lost = uart.lost;
  return res;
}

// The same loop with a pass-through stream on it, as the firmware runs by default (every byte is
// echoed to Serial2): a UART with a FIFO of `fifoCap` bytes that drains at `baudOut` and reports
// its room. The reader asks contractDrainBudget() what it may take. A byte echoed with no room in
// the FIFO would have WAITED in write(): `waits` counts those. There must be none.
struct PassResult { std::vector<std::string> contract; size_t lost = 0, echoed = 0; int waits = 0, idlePasses = 0; };
static PassResult runLoopPass(const std::string& wire, size_t rxCap, double baudIn, double baudOut, int fifoCap = 128) {
  MockUart uart(wire, rxCap); ContractLineBuf lb; PassResult res; bool sawRoom = false;
  int fifo = 0; double outCredit = 0;
  for (int ms = 0; ms < 120000; ms++) {
    uart.advance(1.0, baudIn);
    outCredit += baudOut / 10000.0;
    while (outCredit >= 1.0) { outCredit -= 1.0; if (fifo > 0) fifo--; }
    if (ms % 10 < 3) continue;                       // blocked in show(): no pass this millisecond
    const uint8_t budget = contractDrainBudget(fifoCap - fifo, sawRoom);
    if (budget == 0) { res.idlePasses++; continue; }
    contractDrain(lb, uart, budget,
      [&](uint8_t) { if (fifo < fifoCap) fifo++; else res.waits++; res.echoed++; },
      [&res](const char* afterBang) { res.contract.push_back(afterBang); },
      [](const char*) {});
    if (uart.sent >= wire.size() && uart.available() == 0) break;
  }
  res.lost = uart.lost;
  return res;
}

int main() {
  // ---- the line assembler ----
  auto a = feedAll("!L*X\r@0T1\r\r!**M:v=idle\r");
  CHECK(a.size() == 3 && a[0] == "!L*X" && a[1] == "@0T1" && a[2] == "!**M:v=idle");   // an empty line is skipped
  a = feedAll("!L*X\r\n!L*X\r\n");
  CHECK(a.size() == 2 && a[1] == "!L*X");                    // CR LF hosts work
  a = feedAll("@0T\n1\r");
  CHECK(a.size() == 1 && a[0] == "@0T\n1");                  // only a LEADING LF is skipped
  // the longest line Studio can emit: a scored A with every key
  const std::string worst =
      "!L*A:i=colorcycle,c=3b82f6,s=255,b=255,at=99999,am=2,m=255,an=16,ap=15,ae=colorcycle,ac=ffffff,ad=2550";
  CHECK(worst.size() == 102 && worst.size() < CONTRACT_LINE_BUF);
  a = feedAll(worst + "\r");
  CHECK(a.size() == 1 && a[0] == worst);
  a = feedAll("!" + std::string(CONTRACT_LINE_BUF - 2, 'x') + "\r");           // exactly full: kept
  CHECK(a.size() == 1 && a[0].size() == (size_t)CONTRACT_LINE_BUF - 1);
  a = feedAll("!L*A:i=solid,c=ff0000," + std::string(200, 'x') + "\r!L*X\r");
  CHECK(a.size() == 1 && a[0] == "!L*X");                    // an over-long contract line is dropped whole...
  a = feedAll("@" + std::string(300, '1') + "\r@0T1\r");
  CHECK(a.size() == 2 && a[0].size() == (size_t)CONTRACT_LINE_BUF - 1 && a[1] == "@0T1");   // ...a native one keeps the stock truncation

  // ---- the drain loop ----
  { // several contract lines in one pass all run, in order; a native line ends the pass
    MockUart u("!L*X\r!LFA:i=solid\r@0T1\r!L*M:v=idle\r", 4096); u.advance(1e6, 9600);
    ContractLineBuf lb; std::vector<std::string> c, n; std::string echo;
    auto pass = [&]() { return contractDrain(lb, u, CONTRACT_DRAIN_MAX, [&](uint8_t b) { echo.push_back((char)b); },
                                             [&](const char* l) { c.push_back(l); }, [&](const char* l) { n.push_back(l); }); };
    CHECK(pass() == 23);
    CHECK(c.size() == 2 && c[0] == "L*X" && c[1] == "LFA:i=solid" && n.size() == 1 && n[0] == "@0T1");
    CHECK(std::string(lb.buf) == "@0T1");                    // the native line is still in the buffer for the player
    CHECK(pass() == 12 && c.size() == 3 && c[2] == "L*M:v=idle");
    CHECK(pass() == 0);
    CHECK(echo == "!L*X\r!LFA:i=solid\r@0T1\r!L*M:v=idle\r");   // the pass-through sees every byte, once, in order
  }
  { // the budget is honoured: a pass-through with room for 5 bytes takes 5
    MockUart u("!L*X\r!L*X\r", 64); u.advance(1e6, 9600);
    ContractLineBuf lb; int lines = 0;
    CHECK(contractDrain(lb, u, 5, [](uint8_t) {}, [&](const char*) { lines++; }, [](const char*) {}) == 5 && lines == 1);
    CHECK(contractDrain(lb, u, 1, [](uint8_t) {}, [&](const char*) { lines++; }, [](const char*) {}) == 1 && lines == 1);
  }

  // ---- section 11: a 400-byte burst, back to back at 9600 baud, into the core's DEFAULT 256-byte buffer ----
  std::string burst; std::vector<std::string> sentLines;
  for (int i = 0; burst.size() < 400; i++) {
    char l[64]; snprintf(l, sizeof l, "L%cA:i=solid,c=%06x,at=%d,am=2,m=200,ae=flash", (i & 1) ? 'R' : 'F', 0x102030 * (i + 1) & 0xffffff, i * 8);
    sentLines.push_back(l); burst += "!" + std::string(l) + "\r";
  }
  CHECK(burst.size() >= 400 && burst.size() < 460);
  LoopResult r = runLoop(burst, 256, CONTRACT_DRAIN_MAX);
  CHECK(r.lost == 0 && r.contract == sentLines && r.echoed == burst);
  // ...and a whole show for all three boards (about 4 KB), same buffer
  std::string big; std::vector<std::string> bigLines;
  for (int i = 0; big.size() < 4096; i++) {
    char l[80]; snprintf(l, sizeof l, "%cFA:i=colorcycle,c=3b82f6,at=%d,am=2,m=200,an=2,ap=1,ae=flash,ac=ffffff", "LPH"[i % 3], i * 4);
    bigLines.push_back(l); big += "!" + std::string(l) + "\r";
  }
  r = runLoop(big, 256, CONTRACT_DRAIN_MAX);
  CHECK(r.lost == 0 && r.contract == bigLines);
  // The same 4 KB through a reader that takes ONE byte a pass, as the library's does: the 256-byte
  // buffer overflows and lines are lost. (This is what the test above would look like on the old
  // reader; it shows the simulation can see the fault.)
  LoopResult old = runLoop(big, 256, 1);
  CHECK(old.lost > 0 && old.contract.size() < bigLines.size());
  printf("     (4 KB burst, 256-byte RX buffer: one byte a pass loses %zu bytes; draining every pass loses 0)\n", old.lost);
  // ---- with the pass-through on, as the firmware runs by default ----
  { bool saw = false;                                         // the budget rule
    CHECK(contractDrainBudget(0, saw) == 1 && !saw);          // a stream that cannot say: the stock one byte a pass
    CHECK(contractDrainBudget(-1, saw) == 1);
    CHECK(contractDrainBudget(40, saw) == 40 && saw);         // room: that many
    CHECK(contractDrainBudget(4000, saw) == CONTRACT_DRAIN_MAX);
    CHECK(contractDrainBudget(0, saw) == 0);                  // it CAN say, and it is full: nothing this pass, and no waiting
    CHECK(contractDrainBudget(1, saw) == 1); }
  // Pass-through at the input's baud: the 400-byte burst and the 4 KB show both arrive whole
  // (through the firmware's 1024-byte receive buffer), and no pass ever waits in write().
  PassResult p = runLoopPass(burst, 1024, 9600, 9600);
  CHECK(p.lost == 0 && p.contract == sentLines && p.waits == 0 && p.echoed == burst.size());
  p = runLoopPass(big, 1024, 9600, 9600);
  CHECK(p.lost == 0 && p.contract == bigLines && p.waits == 0);
  p = runLoopPass(big, 1024, 9600, 115200);
  CHECK(p.lost == 0 && p.contract == bigLines && p.waits == 0);
  // Pass-through at 2400 baud (the stock Serial2 setting) against 9600 in. Section 11's burst
  // still arrives whole: the receive buffer and the FIFO absorb it...
  p = runLoopPass(burst, 1024, 9600, 2400);
  CHECK(p.lost == 0 && p.contract == sentLines && p.waits == 0);
  // ...and a 4 KB show does not. The reader can take bytes no faster than the pass-through
  // sends them, so the UART drops what its buffer cannot hold. That is THE DOCUMENTED LIMIT
  // (contract_line.h); verb V reports such a load bad. What this fork guarantees is that it
  // never waits: no pass sits in write() while the LEDs go unrendered.
  p = runLoopPass(big, 1024, 9600, 2400);
  CHECK(p.lost > 1000 && p.contract.size() < bigLines.size() && p.waits == 0 && p.idlePasses > 0);
  printf("     (4 KB burst, pass-through at 2400 baud: %zu bytes lost in the UART, %d passes skipped, 0 waited)\n", p.lost, p.idlePasses);

  // ---- a line from the library's WiFi receiver ----
  CHECK(std::string(contractWifiLine("!L*X", CONTRACT_LINE_BUF)) == "!L*X");
  CHECK(std::string(contractWifiLine("\n!L*X", CONTRACT_LINE_BUF)) == "!L*X");          // the LF of a CR LF host
  CHECK(std::string(contractWifiLine("\n@0T1", CONTRACT_LINE_BUF)) == "@0T1");
  CHECK(contractWifiLine("\n", CONTRACT_LINE_BUF) == nullptr && contractWifiLine("", CONTRACT_LINE_BUF) == nullptr);
  CHECK(contractWifiLine(nullptr, CONTRACT_LINE_BUF) == nullptr);
  CHECK(std::string(contractWifiLine(worst.c_str(), CONTRACT_LINE_BUF)) == worst);       // the longest real line passes
  { const std::string cut = "!" + std::string(CONTRACT_LINE_BUF - 2, 'x');               // 127 characters: the buffer was full
    CHECK(contractWifiLine(cut.c_str(), CONTRACT_LINE_BUF) == nullptr);                  // a contract line cut off: dropped
    const std::string cutLf = "\n!" + std::string(CONTRACT_LINE_BUF - 3, 'x');
    CHECK(contractWifiLine(cutLf.c_str(), CONTRACT_LINE_BUF) == nullptr);
    const std::string fits = "!" + std::string(CONTRACT_LINE_BUF - 3, 'x');              // 126: it fitted
    CHECK(contractWifiLine(fits.c_str(), CONTRACT_LINE_BUF) != nullptr);
    const std::string native = "@" + std::string(CONTRACT_LINE_BUF - 2, '1');
    CHECK(contractWifiLine(native.c_str(), CONTRACT_LINE_BUF) != nullptr); }             // a native line keeps the stock truncation

  // The 1024-byte buffer the firmware asks for rides out a pass that stalls for a full second.
  { MockUart u(big, 1024); u.advance(1000.0, 9600); CHECK(u.lost == 0 && u.available() == 960); }

  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract_line OK\n");
  return 0;
}
