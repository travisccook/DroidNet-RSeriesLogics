// Host TYPE-CHECK of the firmware layer ContractLogics.h against mock_engine.h.
// Not a behavioral test — it only proves the firmware C++ compiles with the verified
// engine API signatures (catches typos / wrong arity / type errors before the bench).
#include "mock_engine.h"
#include "../../src/contract/ContractLogics.h"
#include <cstdio>

int main() {
  // exercise every fork entry point so the compiler instantiates + type-checks them
  ParsedContract p;
  contractParse("LFA:i=solid,c=0080FF,d=0", p);
  applyContract(p);
  applyContractToUnit(0, p);
  applyContractToUnit(1, p);

  LogicEngineRenderer r; r._id = 1;
  ContractRender(r);
  r._id = 2;
  ContractRender(r);
  contractSetup();

  // replicate the MARCDUINO_ACTION(Contract, !, ...) body verbatim to type-check it
  { ParsedContract q; if (contractParse(Marcduino::getCommand(), q)) applyContract(q); }

  printf("ContractLogics.h type-check OK\n");
  return 0;
}
