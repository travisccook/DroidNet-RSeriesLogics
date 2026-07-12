#!/usr/bin/env bash
# Compile + run the host unit tests for contract_core.h with clang++.
set -e
cd "$(dirname "$0")"
clang++ -std=c++17 -Wall -Wextra -O0 test_contract_core.cpp -o /tmp/rseries_contract_test
/tmp/rseries_contract_test
