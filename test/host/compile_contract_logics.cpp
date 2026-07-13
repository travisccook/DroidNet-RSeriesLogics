// Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// Distributed as part of that LGPL-2.1 work; you may redistribute and/or modify it
// under the terms of the GNU Lesser General Public License, version 2.1. No warranty;
// see the LICENSE file at the repository root.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Host check of the firmware layer ContractLogics.h against mock_engine.h.
// Mostly a TYPE-CHECK (proves the firmware C++ compiles with the verified engine API
// signatures — catches typos / wrong arity / type errors before the bench), plus a
// focused behavioral guard that a scored NATIVE section threads its native code into
// the ScoreEntry (the cross-fork drift the 2026-07-12 fork audit fixed on PSI/Flthy).
#include "mock_engine.h"
#include "../../src/contract/ContractLogics.h"
#include <cstdio>

int main() {
  // exercise every fork entry point / verb / effect so the compiler type-checks them
  const char* cmds[] = {
    "LFA:i=solid,c=0080FF,d=0", "**P:c=FFFFFF,d=120", "**C:bpm=120,ph=0,bpb=4,beat=0",
    "LFB:v=120", "P*L:v=190", "L*A:i=rainbow", "LFA:i=scan,c=FF0000",
    "LRA:i=sparkle,c=00FF00,s=200", "LFA:i=meter", "LFA:i=solid,c=3B82F6,at=0,am=1",
    "LFA:i=fire,c=FF2A00,at=32,am=3", "**X", "**M:v=show", "**M:v=idle", "LFA:i=native:105",
    "L*A:i=comet,c=3B82F6,s=200",
    "L*A:i=chase,c=3B82F6,s=200", "L*A:i=wipe,c=3B82F6,s=200", "L*A:i=gradient,s=200",
    "L*A:i=colorcycle,s=200", "L*A:i=twinkle,c=3B82F6,s=200",
  };
  for (const char* s : cmds) { ParsedContract p; if (contractParse(s, p)) applyContract(p); }

  LogicEngineRenderer r;
  for (unsigned id = 1; id <= 2; id++) {
    r._id = id;
    for (uint32_t t = 0; t < 5000; t += 250) { _mock_millis = t; contractTick(); ContractRender(r); }
  }
  contractSetup();

  // replicate the MARCDUINO_ACTION(Contract, !, ...) body verbatim to type-check it
  { ParsedContract q; if (contractParse(Marcduino::getCommand(), q)) applyContract(q); }

  // behavioral guard: a scored NATIVE section must thread its native code into the
  // ScoreEntry (regression guard for the PSI/Flthy drift where nativeCode stayed -1).
  { ParsedContract p; if (contractParse("LFA:i=native:104,at=64", p)) applyContract(p); }
  if (gScoreLen[0] == 0 || gScore[0][gScoreLen[0] - 1].nativeCode != 104
      || gScore[0][gScoreLen[0] - 1].effect != CE_NATIVE) {
    printf("FAIL: RSeries scored native section did not thread nativeCode\n");
    return 1;
  }

  printf("ContractLogics.h type-check + score-native guard OK\n");
  return 0;
}
