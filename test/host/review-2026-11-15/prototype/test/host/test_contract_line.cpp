// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
#include "../../src/contract/contract_line.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int gFails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL :%d  %s\n", __LINE__, #cond); gFails++; } } while (0)

static std::vector<std::string> feedAll(const std::string& bytes) {
  ContractLineBuf lb; std::vector<std::string> out;
  for (char c : bytes) if (lb.feed(c)) out.push_back(lb.buf);
  return out;
}

int main() {
  auto a = feedAll("!L*X\r@0T1\r\r!**M:v=idle\r");
  CHECK(a.size() == 3 && a[0] == "!L*X" && a[1] == "@0T1" && a[2] == "!**M:v=idle");   // empty line skipped
  a = feedAll("!L*X\r\n!L*X\r\n");
  CHECK(a.size() == 2 && a[1] == "!L*X");                    // CR LF hosts work
  std::string worst = "!L*A:i=colorcycle,c=3b82f6,s=255,b=255,at=99999,am=3,m=255,ae=colorcycle,ac=ffffff,ad=2550";
  CHECK(worst.size() == 90);
  a = feedAll(worst + "\r");
  CHECK(a.size() == 1 && a[0] == worst);                     // the longest Studio line fits
  a = feedAll("!L*A:i=solid,c=ff0000," + std::string(100, 'x') + "\r!L*X\r");
  CHECK(a.size() == 1 && a[0] == "!L*X");                    // an overlong contract line is dropped whole
  a = feedAll("@" + std::string(120, '1') + "\r");
  CHECK(a.size() == 1 && a[0].size() == 95);                 // a native line keeps the stock truncation
  if (gFails) { printf("%d CHECK(s) FAILED\n", gFails); return 1; }
  printf("contract_line OK\n");
  return 0;
}
