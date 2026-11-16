# Review probes and prototype, 2026-11-15

These are the scratch sources from the lighting review of 2026-11-15. **They are unreviewed.** Nothing here is part
of the host suite: `test/host/run.sh` does not build them, and the firmware build excludes `test/`.

The implementation plan promotes them into the harness:
`DroidNetSignalBooster/docs/superpowers/plans/2026-11-16-lighting-firmware-core-rseries.md`, Part 0. Delete this
directory when Part R1 has landed.

## What is here

| Path | What it is |
| ---- | ---------- |
| `mock2.h`, `probe.cpp`, `probe2.cpp`, `probe3.cpp` | The review's probes, against the code as it was reviewed |
| `part0/` | The files plan Part 0 installs in `test/host/`: the upgraded mock, the harness helpers, the regressions as known defects, the vector generator |
| `prototype/` | A prototype of contract v1.3: the shared core, the RSeries layer, the line reader, and their tests |

## The probes

They include `src/contract/ContractLogics.h` as it is in the tree, on a mock that records the selected sequence.

| Probe | Shows | Review finding |
| ----- | ----- | -------------- |
| `probe.cpp` T1 | A pulse on a freshly booted board leaves effect 106 up, solid black | F4 |
| `probe.cpp` T2, T2b | A pulse after `X` or `M:v=idle` brings the previous look back | F4 |
| `probe.cpp` T3 | A second show's pre-roll shows the first show's last colour | F4 |
| `probe.cpp` T4 | `A` with `d=` never reverts | parsed, not implemented |
| `probe.cpp` T5 | A scored `b=` is dropped; `s=300` is 44, `bpm=-1` is 65535 | F9 |
| `probe.cpp` T6 | `P:d=-1` holds the overlay for about 49 days | F9 |
| `probe.cpp` T7 | `native:25` reaches `selectSequence(25)` unchecked | F3 |
| `probe2.cpp` T8 | The host's 4 s re-anchor fires an off-beat accent and swallows the next one | F1 |
| `probe2.cpp` T9 | The beat pump swings the whole display 3.4 times a second at 200 BPM; live `A` and chase are uncapped | F2 |
| `probe2.cpp` T10 | A re-anchor just after a section boundary flips the section back and forth | F1 |
| `probe3.cpp` P1 | The float beat clock is accurate for show lengths and across the `millis()` wrap | checked, fine |
| `probe3.cpp` P2 | The pump at the host default `m=200` | F2 |
| `probe3.cpp` P3 | `i=flash` with a live pulse in its dark half stays under the cap | checked, fine |
| `probe3.cpp` P4 | An accent on a scored native section restarts the native sequence every beat | F5 |
| `probe3.cpp` P5 | `scoreInsert` at its cap keeps arrival order and drops silently | F9 |

The mock in `mock2.h` does not draw native looks. When the selected sequence is not 106, the pixel a probe prints
is stale; read the sequence number.

## Build and run

Binaries go to a temporary directory, never into the repository.

```sh
cd test/host/review-2026-11-15
out=$(mktemp -d)
for p in probe probe2 probe3; do g++ -std=c++17 -O1 -w $p.cpp -o $out/$p && $out/$p; done
```

`part0/` is written for `test/host/`, so build it from a copy laid out that way:

```sh
t=$(mktemp -d) && mkdir -p $t/test/host $t/src
cp -r ../../../src/contract $t/src/ && cp part0/* $t/test/host/
(cd $t/test/host && g++ -std=c++17 -w test_review_regressions.cpp -o reg && ./reg)
# expected: 13 lines starting XFAIL, then "review regressions OK (13 known defect(s) still open)"
```

The prototype builds in place (its tests include `../../src/contract/` inside `prototype/`):

```sh
cd prototype/test/host
for t in test_contract_core test_contract_v13 test_review_regressions test_contract_logics_v13 \
         compile_contract_logics test_contract_line; do
  g++ -std=c++17 -w $t.cpp -o $out/$t && $out/$t
done
g++ -std=c++17 -O1 -w -fsanitize=address,undefined -fno-sanitize-recover=all fuzz_parsers.cpp -o $out/fuzz && $out/fuzz
g++ -std=c++17 -w gen_contract_vectors.cpp -o $out/gen && $out/gen "$(sha256sum ../../src/contract/contract_core.h | cut -d' ' -f1)" | head
```

## What the prototype is, and is not

It is the end state the plan's Part R1 lands in slices, written in one sitting to check that the plan's code
compiles and that its tests pass:

- `test_contract_core` 341/341 (the v1.2 vectors, through two shims), `test_contract_v13` 4433/4433.
- `fuzz_parsers` 1,558,346 checks under ASan and UBSan, with the model updated for the saturating parameters.
- `test_review_regressions`: all 13 known defects fixed. `test_contract_logics_v13` and the ported
  `compile_contract_logics` pass.

It has **not** been reviewed, cross-compiled for the ESP32 or an AVR, or run on hardware. `main.cpp`, `wdp_fork.h`
and `run.sh` are not prototyped. Where the plan and the prototype disagree, the plan and the v1.3 spec win.
