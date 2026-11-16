#!/usr/bin/env bash
# Part of the DroidNet Driveable-Animation Contract test harness, added to a fork of
# reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
# SPDX-License-Identifier: LGPL-2.1-only
#
# Host checks for the RSeries contract fork (no hardware / no ESP32 toolchain needed):
#   1. contract_core.h parser unit tests
#   1f. contract_core.h v1.3 unit tests (safety limits, clock rules, lifecycle, score, query)
#   1b. wifi_connect_core.h FSM unit tests
#   1c. wdp_announce_core.h WDP device announce unit tests (shared with the PSI and Flthy forks)
#   1d. wdp_fork.h: this fork's real WDP identity builds its line, and the Serial1 adapter
#   1e. the generated parity vectors (vectors/contract_vectors.json) match the core
#   2. ContractLogics.h firmware-layer type-check against a mock of the engine API
#   2b. the 2026-11-15 review findings as behaviour tests (known defects marked)
#   3. Marcduino command-buffer width guard (the v1.2 accent keys push a scored line past 63)
#   4. parser fuzz + differential (ASan/UBSan, deterministic)
#   5. native #LEBRI brightness parser unit tests
#   6. native #LEBRI brightness action behaviour, against mock_native_brightness.h
#   7. Marcduino prefix-collision guard (registered actions + library catalog snapshot)
#   8. native #LEBRI brightness parser fuzz + differential (ASan/UBSan, deterministic)
#   9. REAL cross-compile for the ESP32 (optional; needs PlatformIO)
#
# How it runs:
#   * Any C++17 compiler: clang++ if present, else g++ (override with CXX=...).
#   * The fuzz stages need ASan + UBSan. The first of $CXX, clang++, g++ that can BUILD AND RUN
#     a sanitized binary here is used for them. If none can, the run FAILS (it does not quietly
#     skip them); ALLOW_NO_SANITIZER=1 runs them unsanitized, with a warning.
#   * Binaries go to a fresh temporary directory that is removed on exit.
#   * ONLY="1 4" runs just those stage ids. The last lines list what ran and what was skipped.
#   * Stage 9 is skipped without PlatformIO; REQUIRE_PIO=1 turns that into a failure.
set -euo pipefail
cd "$(dirname "$0")"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rseries-host.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT

CXX="${CXX:-$(command -v clang++ || command -v g++ || true)}"
[ -n "$CXX" ] || { echo "FAIL: no C++ compiler found (clang++ or g++)"; exit 1; }
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all"
san_ok() {   # can this compiler build AND run a sanitized binary here?
  echo 'int main(){return 0;}' > "$OUT/san.cpp"
  "$1" $SAN "$OUT/san.cpp" -o "$OUT/san" 2>/dev/null && "$OUT/san"
}
SANCXX=""
for c in "$CXX" clang++ g++; do
  if command -v "$c" >/dev/null 2>&1 && san_ok "$c"; then SANCXX="$c"; break; fi
done
if [ -z "$SANCXX" ] && [ "${ALLOW_NO_SANITIZER:-0}" != 1 ]; then
  echo "FAIL: no compiler here can link ASan/UBSan, so the fuzz stages cannot run."
  echo "      Install one (apt: libclang-rt-dev, or g++), or set ALLOW_NO_SANITIZER=1 to run them unsanitized."
  exit 1
fi
if [ -z "$SANCXX" ]; then
  echo "WARNING: fuzz stages run WITHOUT sanitizers (ALLOW_NO_SANITIZER=1)"; SANCXX="$CXX"; SAN=""
fi
echo "compiler: $CXX   sanitized stages: $SANCXX ${SAN:+(ASan+UBSan)}"

RAN=""; SKIPPED=""
want() { [ -z "${ONLY:-}" ] || case " $ONLY " in *" $1 "*) return 0;; *) return 1;; esac; }
# stage <id> <title> <compiler> <flags and source...>: build into $OUT, run, record.
stage() {
  local id="$1" title="$2" cxx="$3"; shift 3
  want "$id" || return 0
  echo "[$id] $title"
  "$cxx" "$@" -o "$OUT/stage_$id"       # two lines on purpose: `a && b` would hide a failed build from set -e
  "$OUT/stage_$id"
  RAN="$RAN $id"
}

stage 1  "contract_core parser unit tests" "$CXX" -std=c++17 -Wall -Wextra -O0 test_contract_core.cpp
# Shared with the PSI and Flthy forks, like stage 1. -std=c++11: the AVR forks build the core with gnu++11.
stage 1f "contract_core v1.3 unit tests" "$CXX" -std=c++11 -Wall -Wextra -O0 test_contract_v13.cpp
stage 1b "wifi_connect_core FSM unit tests" "$CXX" -std=c++17 -Wall -Wextra -O0 test_wifi_connect_core.cpp
# -std=c++11: the AVR forks build this same header with avr-gcc's gnu++11.
stage 1c "wdp_announce_core WDP device announce unit tests" "$CXX" -std=c++11 -Wall -Wextra -Werror -O0 test_wdp_announce_core.cpp
# Fork-local. Builds the line from the header's own WDP_TYPE/WDP_CAPS with a dirty build's fw
# ("<sha>-dirty") and with any fw up to the WCB's 27-char limit, and drives wdpForkTick()
# against a mock port. -std=c++11: the ESP32 core builds with gnu++11.
stage 1d "wdp_fork RSeries WDP identity + Serial1 adapter" "$CXX" -std=c++11 -Wall -Wextra -Werror -O0 test_wdp_fork.cpp
# The parity vectors: gen_contract_vectors.cpp runs the REAL contract_core.h and prints every fx /
# beat-clock / envelope vector as JSON. The committed copy (vectors/contract_vectors.json) is what
# the Studio preview's JS mirror is tested against, so a change to the math that does not
# regenerate it fails here. UPDATE_VECTORS=1 rewrites the committed copy.
if want 1e; then
  echo "[1e] parity vectors are current"
  "$CXX" -std=c++17 -Wall -Wextra gen_contract_vectors.cpp -o "$OUT/gen_vectors"
  sha="$( (sha256sum ../../src/contract/contract_core.h 2>/dev/null || shasum -a 256 ../../src/contract/contract_core.h) | cut -d' ' -f1)"
  "$OUT/gen_vectors" "$sha" > "$OUT/contract_vectors.json"
  if [ "${UPDATE_VECTORS:-0}" = 1 ]; then mkdir -p vectors; cp "$OUT/contract_vectors.json" vectors/contract_vectors.json; fi
  if ! cmp -s "$OUT/contract_vectors.json" vectors/contract_vectors.json; then
    echo "FAIL: vectors/contract_vectors.json is stale. Run: UPDATE_VECTORS=1 bash test/host/run.sh"
    echo "      then copy it to Studio: bash scripts/core-sync.sh --push"
    exit 1
  fi
  echo "OK  vectors/contract_vectors.json matches the core (sha256 $sha)"
  RAN="$RAN 1e"
fi
stage 2  "ContractLogics.h firmware type-check" "$CXX" -std=c++17 -Wall -Wextra compile_contract_logics.cpp
# The 2026-11-15 review's findings as behaviour tests on mock_engine.h, which models sequence
# selection, restoreSettings() and the engine's 25th-entry overrun. A finding that is not fixed
# yet is a KNOWN_BUG line: it prints XFAIL and keeps the suite green, and FAILS the suite the
# moment it starts passing, so the fix has to turn it into a CHECK.
stage 2b "review regressions (known defects marked)" "$CXX" -std=c++17 -Wall -Wextra test_review_regressions.cpp

if want 3; then
echo "[3] Marcduino command-buffer width guard"
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
RAN="$RAN 3"
fi

# The parsers are the one place this project eats bytes off a shared, noisy serial bus, and to
# save 1,008 B of flash on the PSI they were hand-rolled instead of using strtol/strtoul. This
# stage is the differential that keeps them honest: ~1.55M checks against an INDEPENDENT model
# and a true 32-BIT strtol/strtoul oracle (the host's own is 64-bit and would disagree on
# overflow -- which is the whole trap), plus hostile/whole-byte-range lines through
# contractParse() under AddressSanitizer + UndefinedBehaviorSanitizer.
# Fixed seed: deterministic and reproducible, not a slot machine.
stage 4 "parser fuzz + differential (ASan/UBSan, deterministic)" "$SANCXX" -std=c++17 -O1 $SAN fuzz_parsers.cpp

# src/native/native_brightness_parse.h: the strict reject rule (no clamp, no floor), every
# boundary, and outputs untouched on a rejected tail.
stage 5 "native #LEBRI brightness parser unit tests" "$CXX" -std=c++17 -Wall -Wextra -Werror -O0 test_native_brightness.cpp

# src/native/NativeBrightness.h against mock_native_brightness.h: controller capture, the
# raise-once baseline, the per-display correction, and a display with no controller ignored.
stage 6 "native #LEBRI brightness action behaviour (mock)" "$CXX" -std=c++17 -Wall -Wextra -Werror -O0 test_native_brightness_action.cpp

# Marcduino.h runs every action whose text prefixes the line and only the LAST one survives,
# so a prefix clash silently drops a command. Reads the registered actions from source and the
# library's example lines from catalog_example_lines.tsv (refresh_catalog_examples.py).
stage 7 "Marcduino prefix-collision guard" "$CXX" -std=c++17 -Wall -Wextra -Werror -O1 prefix_guard.cpp

# Fork-local. fuzz_parsers.cpp above is shared byte-identically with the PSI and Flthy forks
# and is not edited here.
stage 8 "native #LEBRI brightness parser fuzz + differential (ASan/UBSan, deterministic)" "$SANCXX" -std=c++17 -Wall -Wextra -Werror -O1 $SAN fuzz_native_brightness.cpp

if want 9; then
echo "[9] REAL cross-compile (ESP32 (wlogicengine32)) -- optional"
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
# stages 1-8 still run and still mean something -- just strictly less than they appear to.
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
[ -x "$PIO" ] || PIO="$(command -v pio 2>/dev/null || true)"
if [ -n "$PIO" ] && [ -x "$PIO" ]; then
  echo "using $PIO"
  ( cd ../.. && "$PIO" run -e wlogicengine32 )
  echo "cross-compiles for the real MCU (ESP32 (wlogicengine32)) OK"
  echo "    (a successful LINK is not a bench test. firmware/README.md says what has run on hardware.)"
  echo
  echo "    NOTE: this stage builds the wlogicengine32 (plain ESP32) env only. The wlogicengine32s3"
  echo "    (ESP32-S3) env DOES build -- run: pio run -e wlogicengine32s3 -- but it is left out of"
  echo "    the suite deliberately. It is a fork-local hand-translation of upstream's make TARGET=ESP32S3"
  echo "    (upstream has no S3 PlatformIO env), and it carries hardware-config guesses a green build"
  echo "    cannot check: USB-CDC-on-boot is not set, so Serial would land on UART0 and the board would"
  echo "    likely look silent. It compiles. That is ALL it does. See the README."
  RAN="$RAN 9"
elif [ "${REQUIRE_PIO:-0}" = 1 ]; then
  echo "FAIL: REQUIRE_PIO=1 and PlatformIO was not found (pip install platformio, or PIO=/path/to/pio)"
  exit 1
else
  echo "SKIP: PlatformIO not found, so NOTHING here was compiled for the real MCU."
  echo "      Install it and re-run for the real check:  pip install platformio"
  echo "      Or point this at an existing install:      PIO=/path/to/pio $0"
  SKIPPED="$SKIPPED 9(no-PlatformIO)"
fi
fi

echo
echo "stages run:$RAN"
echo "stages skipped:${SKIPPED:- none}"
if [ -n "$SKIPPED" ]; then
  echo "NOTE: a skipped stage proved nothing. Stage 9 is the only real cross-compile."
fi
