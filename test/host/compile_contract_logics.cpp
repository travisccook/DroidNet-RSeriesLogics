// Host TYPE-CHECK of the firmware layer ContractLogics.h against mock_engine.h.
// Not a behavioral test — it only proves the firmware C++ compiles with the verified
// engine API signatures (catches typos / wrong arity / type errors before the bench).
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

  printf("ContractLogics.h type-check OK\n");
  return 0;
}
