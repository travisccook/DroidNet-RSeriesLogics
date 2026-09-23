# DroidNet_RSeriesLogics — fork notes

Working notes for the fork layer. For attribution, licensing and the honest scope of what was
added, read [README.md](README.md) first. For the firmware itself, read
[README_UPSTREAM.md](README_UPSTREAM.md) — that is the original authors' documentation and it is
still the authority on everything except the `!` grammar.

## What this fork is

An additive layer on top of reeltwo's `WLogicEngine32` that implements the **Driveable-Animation
Contract**, so Cantina Studio can drive the FLD/RLD logic displays with arbitrary RGB, beat-locked
accents, a host-seeded beat clock, and autonomous board-side section playback — while every native
`~RTLE…` / `@nT…` / `#LE…` command keeps working exactly as it did. It also adds one native command,
`#LEBRI` (RAM-only brightness, below), which is not part of the contract.

- **Upstream:** [reeltwo/WLogicEngine32](https://github.com/reeltwo/WLogicEngine32) (LGPL-2.1),
  by Joymonkey and Rimim.
- **Seeded from:** a private C2B5 working collection @ `2fbd4023` (subdir `RSeriesLogic`),
  2026-07-12, git-tracked files only. That collection is not public and never will be; its
  customizations are documented in [README_C2B5.md](README_C2B5.md).
- **Role:** the RSeries board is the easiest of the three DroidNet board forks (PSI and Flthy HP
  are the other two), and is intended to be the first one proven on hardware.
- **MCU/toolchain:** ESP32 (`wlogicengine32`) / ESP32-S3 (`wlogicengine32s3`), PlatformIO.
- **GitHub remote:** <https://github.com/travisccook/DroidNet-RSeriesLogics> (public).

## Status

**Code complete for Phase 1 and Phase 2. Never run on hardware.**

Verified host-side only, via `bash test/host/run.sh`:

- 341/341 parser, beat-clock and effect-math checks (`test/host/test_contract_core.cpp`).
- Type-check of the firmware render layer against a mock of the engine API
  (`test/host/compile_contract_logics.cpp` + `mock_engine.h`), plus a behavioural guard that a
  scored `native:<code>` section threads its native code through to the `ScoreEntry`.

Not done: a PlatformIO build for the ESP32, a bench flash, or any test with LEDs attached. Do not
put this on a droid until that has happened.

Fork head: `main` in this repository. The baseline import is commit `6bba8b1`; every commit after it
is the DroidNet layer (`git log --oneline 6bba8b1..HEAD`).

### What is in

- **Phase 1 — live cues.** Verbs `A` (animate), `P` (pulse), `B` (brightness), `L` (level),
  `X` (stop), `M` (mode). Effects `solid`, `flash`, `rainbow`, `scan`, `sparkle`, `meter`,
  `native:<code>`.
- **Phase 2 — beat clock and score.** Verb `C` seeds one shared `BeatClock` (bpm / phase / beats
  per bar / beat anchor). `A` with `at=<beat>` inserts a `ScoreEntry` instead of applying
  immediately; `contractTick()` advances the section cursor from `mainLoop()`.
- **Six parametric effects** over a 1-D strand: `comet`, `chase`, `wipe`, `gradient`, `colorcycle`,
  `twinkle`. The maths lives in `contract_core.h` (`fx*` helpers) so the firmware and the Studio
  visualiser can be checked against the same vectors.

### Native brightness: `#LEBRI`

`#LEBRI[F|R]<1-255>` sets the logic displays' brightness in RAM (library id
`rseries.cfg.brightness`). No unit means both displays; `F` is the front, `R` the rear. The level
is 1 to 3 digits worth 1-255, and the line must end right after it. Anything else is ignored with no
state change and no reply: there is no clamp, and 0 is never applied.

- `src/native/native_brightness_parse.h` is the pure parser; `src/native/NativeBrightness.h` holds
  the `MARCDUINO_ACTION(LEBrightness, #LEBRI, …)`, the two captured FastLED controllers and the
  raise-once flags. `src/main.cpp` includes it right after `MarcduinoLogics.h` and captures each
  display's controller around `FLD_init()` / `RLD_init()` (lines marked `DroidNet native
  brightness`).
- The first command for a display sets its RAM default and live `fBri` to 255 (once); every command
  sets that display's controller correction to (L, L, L). Nothing goes to NVS.
- Host checks: `test_native_brightness.cpp` (accept/reject table), `test_native_brightness_action.cpp`
  (the action against `mock_native_brightness.h`), `fuzz_native_brightness.cpp` (fork-local
  differential fuzz) and `prefix_guard.cpp` (below).

### What is not in

- Verb `Q` (query / status readback). Parsed, not answered.
- The mic `PeakValueProvider` wiring for the native mic-reactive effects (`contractSetup()` is a
  deliberate no-op).
- Any hardware validation at all.

## Layout

| File | Owner | Notes |
| --- | --- | --- |
| `src/contract/contract_core.h` | fork | Pure C++, no Arduino/FastLED/Reeltwo. **Byte-identical across all three board forks** — if you change it here, change it identically there. |
| `src/contract/ContractLogics.h` | fork | Render layer. Registers as custom effect id **106**. |
| `src/native/native_brightness_parse.h` | fork | Pure `#LEBRI` parser, no Arduino/FastLED/Reeltwo. Fork-local (not shared with PSI/Flthy). |
| `src/native/NativeBrightness.h` | fork | The `#LEBRI` Marcduino action and the FastLED controller capture. Not part of the contract. |
| `test/host/` | fork | Host harness. Keep it green. `fuzz_parsers.cpp` is shared byte-identically with the PSI and Flthy forks; `fuzz_native_brightness.cpp` is fork-local. |
| `test/host/catalog_example_lines.tsv` | fork | Snapshot of every command-library example line, for `prefix_guard.cpp`. Regenerate with `refresh_catalog_examples.py <library checkout>`. |
| `src/main.cpp` | upstream | Four added lines (include, effect-table entry, `contractSetup()`, `contractTick()`) plus a one-character fix to a latent off-by-one in `CustomLogicEffectSelector`. Five more lines for `#LEBRI`: its include and the controller capture around `FLD_init()` / `RLD_init()`. |
| `lib/Reeltwo` | upstream | **Do not edit.** The contract plugs in through the public `seq >= 100` `CustomLogicEffectSelector` hook. The two `wifi/` headers carry pre-existing C2B5 modifications and an LGPL §2(b) notice. |

## Conventions that matter

- `ContractRender` **must return `true`** — `LogicEngine.h:820-830` reverts the display to NORMAL if
  a custom effect returns false.
- `setPixelRGB` bypasses the engine's `fBri`, so brightness and the beat envelope are applied inside
  the render layer.
- The score sequencer runs in `contractTick()` from `mainLoop()`, **not** inside the effect
  function, so sections keep advancing even while a `native:<code>` section look (which
  `ContractRender` does not draw) is on screen.
- `!` is the contract's only command prefix, and it was chosen because every stock parser ignores it.
  The one native command the fork adds is `#LEBRI`.
- **No Marcduino action text may be a prefix of another.** `Marcduino::processCommand` runs every
  action whose text prefixes the line, and only the last-registered one survives, so a clash drops a
  command silently. `test/host/prefix_guard.cpp` fails on any prefix pair except the three stock ones
  (`@0T1`→`@0T11`, `@1T1`→`@1T11`, `@2T1`→`@2T11`, which work because the longer text is registered
  later), and on any command-library example line that would run `#LEBRI` without being
  `rseries.cfg.brightness`.
- **`STROBE_MIN_STATE_MS = 170` is a safety limit, not a tuning knob.** An accent overlay may not
  restart more often than every `2 * 170 = 340 ms` (≤ 2.94 flashes/sec, inside the ≤ 3 flashes/sec
  photosensitivity guidance), the cool-down is anchored on `pulseLastMs` **unconditionally** — an
  overlay that is still on screen is not an exemption — and all three board forks must use the same
  number. `test/host/` fails if any of that stops being true.
- **`MarcduinoSerial<96>`, not the library default of 64.** `Marcduino.h` drops overflow bytes
  silently and then processes the truncated line, and a truncated contract line still parses — so
  the board would quietly play a *different* show. The longest line Studio can emit is 90 chars;
  `test/host/run.sh` recomputes that and fails if the buffer cannot hold it.
- **The Phase-2 score is cleared at *every* show boundary — `X`, `M:v=idle`, and `M:v=show`.**
  `v=show` is the one that is easy to miss, and it is the one the product actually hits: Studio's
  Deliver **Resend** re-pushes the load burst (`!**M:v=show` first, then the scored `A` entries)
  with no intervening stop. Clear it there and a resend *replaces* the show; forget to, and show B
  is inserted **into** show A — the board plays A's sections in between B's, and once the table is
  at `SCORE_MAX` `scoreInsert()` silently drops B outright and the board replays A forever.
