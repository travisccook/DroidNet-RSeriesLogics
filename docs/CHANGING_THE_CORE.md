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

### A stateful group

Most groups are pure functions: every row stands alone. `contractFieldGate` is a state machine, so its group is
sequences. Its first argument is `seq`:

- `args`: `seq`, `ceil`, `reset`, `nowMs`, `level`. `out`: `allowed`, `lit`, `ref`, `onsetMs`.
- The rows of one `seq` run **in file order through one state**. A new `seq` starts from a fresh state: not lit,
  `ref` 0, `onsetMs` 0, and the row's `ceil`.
- Each row is one call: `allowed = contractFieldGate(state, level, nowMs)`. `allowed` is what a mirror has to
  reproduce. `lit`, `ref` and `onsetMs` are the state after the call (`fieldLit`, `fieldRef`, `fieldOnsetMs`), for a
  mirror that keeps the same three.
- `reset` = 1: the show reset runs before the call, with the row's `ceil` as the new ceiling. It keeps the gate's
  three fields, which is the point of those rows. `ceil` changes only on such a row.
- `nowMs` is a uint32 `millis()` and wraps inside one sequence; the differences are what count.

The sequences: a display's plain life (first light, a relight held and released at exactly 340 ms, a blackout
never delayed, a slow rise, a trough that is not dark); the quarter-of-the-ceiling step from both sides; power-on;
the `millis()` wrap; resets inside the window, one with a new ceiling; the same script at twelve ceilings from 0
to 255; and three long walks from a fixed generator. A later stateful group follows the same shape: `seq` first,
rows in order, the state after each call in `out`.

## Core revision CA1.2 (2026-11-16): what a fork has to do

CA1.2 closes the findings of the 2026-11-16 review of R1. Most of it needs nothing from a fork beyond the re-copy:
`bpm=` and `r=` are taken only as plain numbers, `at=` and `beat=` are clamped to `CONTRACT_BEAT_MAX`, a unit that
failed its `V` takes no scored line until the next `M:v=show`, a scored line delivered twice does not move the
section cursor, and the logics allow-list loses code 3. Three things are a fork's to wire up:

1. **The field gate (S9).** After a fork has rendered a unit's frame and before the frame leaves, it measures the
   field level (0 to 255: the mean over the unit's LEDs of each LED's brightest channel) and calls
   `contractFieldGate(state, level, now)`. When the answer is lower than the level, the frame is scaled down to
   it. That is the whole rule for light the fork draws. The gate's state lives in `ContractUnitState` and
   survives every reset.
2. **Native looks: ask, then tell.** For light the fork does not draw, ask before handing the LEDs over
   (`contractFieldAllowed(state, level, now) >= level`, or `contractFieldMayLight()` for a native look at the
   ceiling) and wait a pass when the answer is no. Tell the gate (`contractFieldGate`) at the moment the native
   look is really on the LEDs, with level 0 for a native blackout. Never tell it about a look that was selected
   and then replaced before it was shown. A look someone else started (a button) is recorded with
   `contractFieldForced()`.
3. **The overlay.** `contractOverlayUp(state, now)` is the one expiry rule; `state.pulseBright` is the overlay's
   own brightness fraction. A `P` with `b=` sets it, when the accent arms, and never touches `state.bright`.

Each fork also needs its own copy of the S9 property stage (`test/host/test_contract_property.cpp` here): the
streams and the bookkeeping carry over; the part that renders a frame and measures it is the fork's.
