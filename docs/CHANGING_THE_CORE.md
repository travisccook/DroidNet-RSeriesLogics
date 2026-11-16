# Changing the shared contract core

Six files are byte-identical in the three DroidNet board forks (RSeries Logics, PSI Pro, Flthy HPs). They are
listed in `test/host/core-files.txt`:

- `src/contract/contract_core.h`: the wire parser, the beat clock, the safety limits, the score, the effect math.
- `test/host/test_contract_core.cpp`, `test/host/test_contract_v13.cpp`, `test/host/fuzz_parsers.cpp`: its tests.
- `src/wdp/wdp_announce_core.h`, `test/host/test_wdp_announce_core.cpp`: the WDP device announce and its tests.

**RSeries is where they are edited.** A copy in the PSI or Flthy fork is never edited by hand.

## The procedure

1. Edit the shared file in this repository only.
2. Run `bash test/host/run.sh`. If stage 1e fails, the math changed: run `UPDATE_VECTORS=1 bash test/host/run.sh`
   and read the diff of `test/host/vectors/contract_vectors.json` before you accept it.
3. Commit here.
4. Run `bash scripts/core-sync.sh --push`. It copies the shared files into the PSI and Flthy checkouts, writes each
   one's `src/contract/CORE_PIN` (the RSeries commit the files came from), and copies the vectors to Studio
   (`DroidNetSignalBooster/web/studio/src/__tests__/fixtures/contract-vectors.json`). It commits nothing, and it
   refuses to run while a shared file has uncommitted changes.
5. In the PSI fork and in the Flthy fork: run `bash test/host/run.sh`, adapt the render layer to the change, and
   commit the shared files together with `CORE_PIN`.
6. In `DroidNetSignalBooster`: run the Studio parity test, change the JS mirror until it passes, and commit the
   vectors with the mirror.
7. Run `bash scripts/core-sync.sh`. It prints no `DRIFT` line.

## What keeps the copies honest

- `bash scripts/core-sync.sh` compares every listed file across the sibling checkouts and exits 1 on a difference.
- Each sibling's CI checks out this repository at its `CORE_PIN` and compares every listed file. A hand edit in a
  sibling fails there. A sibling that is behind is visible: its pin is an older commit.
- Stage 1e of the host suite fails when the committed vectors are not what the core generates.

## The vector file

`test/host/gen_contract_vectors.cpp` runs the real `contract_core.h` on the host and prints
`test/host/vectors/contract_vectors.json`. The Studio preview's JS mirror is tested against a copy of that file, so
the firmware and the preview cannot disagree without a test failing. Schema 1:

- `contract_version`, `core_sha256`, `generator`.
- `constants`: the safety limits and other named values (an integer, or a list of integers).
- `lines`: a wire line (the bytes after `!`) and its CRC-16/CCITT-FALSE as `crc16`.
- `groups`: one per function. `args` and `out` are name lists; each row is the arguments, then the results.
  Every value is an integer. A float is carried scaled (`phase_e6` is the phase times a million, rounded).

Groups are append-only: a later change adds a group, it never reshapes one.
