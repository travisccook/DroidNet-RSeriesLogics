#!/usr/bin/env bash
# Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
# reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
# Distributed as part of that LGPL-2.1 work; see the LICENSE file at the repository root.
# SPDX-License-Identifier: LGPL-2.1-only
#
# Host checks for the RSeries contract fork (no hardware / no ESP32 toolchain needed):
#   1. contract_core.h parser unit tests
#   2. ContractLogics.h firmware-layer type-check against a mock of the engine API
#   3. Marcduino command-buffer width guard (the v1.2 accent keys push a scored line past 63)
#   4. REAL cross-compile for the ESP32 (optional; needs PlatformIO)
set -e
cd "$(dirname "$0")"
echo "[1/4] contract_core parser unit tests"
clang++ -std=c++17 -Wall -Wextra -O0 test_contract_core.cpp -o /tmp/rseries_contract_test
/tmp/rseries_contract_test
echo "[1b/4] wifi_connect_core FSM unit tests"
clang++ -std=c++17 -Wall -Wextra -O0 test_wifi_connect_core.cpp -o /tmp/rseries_wifi_test
/tmp/rseries_wifi_test
echo "[2/4] ContractLogics.h firmware type-check"
clang++ -std=c++17 -Wall -Wextra compile_contract_logics.cpp -o /tmp/rseries_fw_syntax
/tmp/rseries_fw_syntax

echo "[3/4] Marcduino command-buffer width guard"
# lib/Reeltwo/src/core/Marcduino.h:173-175 — MarcduinoSerial::animate() reads a byte and:
#     else if (fPos < SizeOfArray(fBuffer)-1) fBuffer[fPos++] = ch;
# Every byte past that is DROPPED ON THE FLOOR, with no error and no flag; on the next CR it
# NUL-terminates whatever it has and hands the TRUNCATED line to Marcduino::processCommand().
# A truncated contract line still parses (contractParse only needs class+unit+verb, and unknown
# / half-eaten params are ignored by design) — so an over-long line does not fail loudly, it
# QUIETLY plays a different show. That is the exact bug the v1.2 line-length analysis exists to
# prevent, and PSI + Flthy both guard it; this is the RSeries half.
#
# Capacity: fPos maxes out at BUFFER_SIZE-1 stored chars, then fBuffer[fPos]='\0' fits in the
# last slot. The CR itself is never stored. So a line of N chars needs BUFFER_SIZE >= N+1.
#
# WORST-CASE STUDIO LINE. The longest thing the emitter can put on the wire is a Phase-2 SCORED
# section that also carries a v1.2 accent (verb A + at= + ae=/ac=/ad=). Per-key worst case, from
# the grammar in src/contract/contract_core.h and the key set in the Studio emitter
# (web/studio/src/dome/contract-emit.js: i,c,s,d,b,at,am,m,bpm,ph,bpb,sw,beat,v — the clock keys
# only ever ride a verb-C line, and `d` is not consumed on the score path):
#     !L*A:          5   '!' + class + unit + verb + ':'   ('*' addresses both logics)
#     i=colorcycle  12   longest effect token; `native:105` is 10 chars too, so this is the max
#     c=3b82f6       8   6 hex digits
#     s=255          5   uint8
#     b=255          5   uint8
#     at=99999       8   int32 beat index; 5 digits is >8 h of song at 200 BPM
#     am=3           4   0..3
#     m=255          5   uint8
#     ae=colorcycle 13   longest effect token again
#     ac=ffffff      9   6 hex digits
#     ad=2550        7   clamped to 2550 at parse
#     + 9 commas     9   (10 keys)
#   ------------------
#             total 90 chars, so BUFFER_SIZE must be >= 91.
# The literal below IS that line; the script measures it rather than trusting the arithmetic.
worst='!L*A:i=colorcycle,c=3b82f6,s=255,b=255,at=99999,am=3,m=255,ae=colorcycle,ac=ffffff,ad=2550'
need=$(( ${#worst} + 1 ))          # the line + its NUL terminator
MAIN=../../src/main.cpp
MARC=../../lib/Reeltwo/src/core/Marcduino.h

# What MarcduinoSerial<> (no explicit width) would mean — read from lib/, never assumed, so the
# guard cannot go stale if upstream changes its default.
DEFAULT=$(sed -n 's/.*template[[:space:]]*<[[:space:]]*uint16_t[[:space:]]*BUFFER_SIZE[[:space:]]*=[[:space:]]*\([0-9]\{1,\}\).*/\1/p' "$MARC" | head -1)
if [ -z "$DEFAULT" ]; then
  echo "FAIL: could not read MarcduinoSerial's default BUFFER_SIZE from $MARC"
  echo "      (the guard below cannot tell what MarcduinoSerial<> would give us)"
  exit 1
fi

# The instantiation in main.cpp (comments stripped: the note above it names the class too).
DECL=$(sed 's://.*::' "$MAIN" | grep -E 'MarcduinoSerial[[:space:]]*<[^>]*>[[:space:]]*[A-Za-z_]' | head -1)
if [ -z "$DECL" ]; then
  echo "FAIL: no MarcduinoSerial<...> instantiation found in $MAIN"
  exit 1
fi
WIDTH=$(printf '%s' "$DECL" | sed -n 's/.*MarcduinoSerial[[:space:]]*<[[:space:]]*\([0-9]\{1,\}\)[[:space:]]*>.*/\1/p')
SRC="explicit in $MAIN"
if [ -z "$WIDTH" ]; then
  WIDTH=$DEFAULT                   # MarcduinoSerial<> => the library default
  SRC="the library default — $MAIN instantiates MarcduinoSerial<> with no width"
fi

if [ "$WIDTH" -lt "$need" ]; then
  echo "FAIL: the Marcduino serial buffer is $WIDTH bytes ($SRC), which holds only"
  echo "      $(( WIDTH - 1 )) chars, but the longest line the Studio emitter can produce is"
  echo "      ${#worst} chars and needs a BUFFER_SIZE of $need (line + NUL):"
  echo "      $worst"
  echo "      Marcduino.h SILENTLY drops the overflow and then processes the TRUNCATED line, so"
  echo "      the board would quietly play a different show rather than erroring. Widen the"
  echo "      MarcduinoSerial<N> template argument in $MAIN (N is a template parameter, so this"
  echo "      needs no lib/ edit)."
  exit 1
fi
echo "MarcduinoSerial BUFFER_SIZE=$WIDTH >= $need (worst v1.2 scored line ${#worst} chars) OK"

echo "[4/4] REAL cross-compile (ESP32 (wlogicengine32)) -- optional"
# THIS is the stage that makes "it compiles" a claim about the FIRMWARE rather than a claim
# about our mock. Stages 1-3 are host checks: they prove the contract logic and they pin the
# board API against a HAND-WRITTEN MOCK -- and a mock is only ever as honest as its author.
# This one was not honest. The first real avr-gcc/xtensa build caught two things stages 1-3
# could never see, by construction:
#   * FastLED declares RGB as an ENUMERATOR of fl::EOrder, which name-hides a `struct RGB`.
#     Our shared core defined exactly that struct. Every plain use of the type failed to
#     compile on both FastLED boards. The mocks never modelled EOrder, so the type-check
#     passed happily.
#   * fill_column() is DEFINED in the PSI firmware but declared in no header, and our include
#     point sits above the definition. The mock DEFINED it, so the name was always in scope.
# (Both are fixed. The lesson is why this stage exists.)
#
# OPTIONAL because it needs PlatformIO and a ~200 MB toolchain. Skipped cleanly without it:
# stages 1-3 still run and still mean something -- just strictly less than they appear to.
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
[ -x "$PIO" ] || PIO="$(command -v pio 2>/dev/null || true)"
if [ -z "$PIO" ] || [ ! -x "$PIO" ]; then
  echo "SKIP: PlatformIO not found, so NOTHING here was compiled for the real MCU."
  echo "      Stages 1-3 passed, but they only type-check against our mock."
  echo "      Install it and re-run for the real check:  pip install platformio"
  echo "      Or point this at an existing install:      PIO=/path/to/pio $0"
  exit 0
fi
echo "using $PIO"
( cd ../.. && "$PIO" run -e wlogicengine32 )
echo "cross-compiles for the real MCU (ESP32 (wlogicengine32)) OK"
echo "    (a successful LINK is not a bench test: this firmware has never been flashed.)"
echo
echo "    NOTE: only the wlogicengine32 (plain ESP32) env is built. The wlogicengine32s3"
echo "    env cannot be built here at all: upstream pins platform-espressif32 v5.2.0, which"
echo "    wants espressif/toolchain-riscv32-esp @ 8.4.0+2021r2-patch3, and that package is"
echo "    no longer resolvable from the registry. That failure happens in PlatformIO's tool"
echo "    manager BEFORE a single source file is read, so it is an upstream packaging issue,"
echo "    not a fault in this fork's code. Left alone deliberately: re-pinning the platform"
echo "    would change the toolchain for the ESP32 env too, and we are not in a position to"
echo "    re-validate that on hardware."
