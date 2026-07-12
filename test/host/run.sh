#!/usr/bin/env bash
# Host checks for the RSeries contract fork (no hardware / no ESP32 toolchain needed):
#   1. contract_core.h parser unit tests
#   2. ContractLogics.h firmware-layer type-check against a mock of the engine API
set -e
cd "$(dirname "$0")"
echo "[1/2] contract_core parser unit tests"
clang++ -std=c++17 -Wall -Wextra -O0 test_contract_core.cpp -o /tmp/rseries_contract_test
/tmp/rseries_contract_test
echo "[2/2] ContractLogics.h firmware type-check"
clang++ -std=c++17 -Wall -Wextra compile_contract_logics.cpp -o /tmp/rseries_fw_syntax
/tmp/rseries_fw_syntax
