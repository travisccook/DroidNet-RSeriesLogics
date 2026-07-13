# DroidNet_RSeriesLogics — fork notes

Working notes for the fork layer. For attribution, licensing and the honest scope of what was
added, read [README.md](README.md) first. For the firmware itself, read
[README_UPSTREAM.md](README_UPSTREAM.md) — that is the original authors' documentation and it is
still the authority on everything except the `!` grammar.

## What this fork is

An additive layer on top of reeltwo's `WLogicEngine32` that implements the **Driveable-Animation
Contract**, so Cantina Studio can drive the FLD/RLD logic displays with arbitrary RGB, beat-locked
accents, a host-seeded beat clock, and autonomous board-side section playback — while every native
`~RTLE…` / `@nT…` / `#LE…` command keeps working exactly as it did.

- **Upstream:** [reeltwo/WLogicEngine32](https://github.com/reeltwo/WLogicEngine32) (LGPL-2.1),
  by Joymonkey and Rimim.
- **Seeded from:** a private C2B5 working collection @ `2fbd4023` (subdir `RSeriesLogic`),
  2026-07-12, git-tracked files only. That collection is not public and never will be; its
  customizations are documented in [README_C2B5.md](README_C2B5.md).
- **Role:** the RSeries board is the easiest of the three DroidNet board forks (PSI and Flthy HP
  are the other two), and is intended to be the first one proven on hardware.
- **MCU/toolchain:** ESP32 (`wlogicengine32`) / ESP32-S3 (`wlogicengine32s3`), PlatformIO.
- **GitHub remote:** TBD (set on publication).

## Status

**Code complete for Phase 1 and Phase 2. Never run on hardware.**

Verified host-side only, via `bash test/host/run.sh`:

- 154/154 parser, beat-clock and effect-math checks (`test/host/test_contract_core.cpp`).
- Type-check of the firmware render layer against a mock of the engine API
  (`test/host/compile_contract_logics.cpp` + `mock_engine.h`), plus a behavioural guard that a
  scored `native:<code>` section threads its native code through to the `ScoreEntry`.

Not done: a PlatformIO build for the ESP32, a bench flash, or any test with LEDs attached. Do not
put this on a droid until that has happened.

Fork head: `0584535` (see `git log --oneline b47a5f9..HEAD` for the layer's history; `b47a5f9` is
the baseline import).

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
| `test/host/` | fork | Host harness. Keep it green. |
| `src/main.cpp` | upstream | Four added lines (include, effect-table entry, `contractSetup()`, `contractTick()`) plus a one-character fix to a latent off-by-one in `CustomLogicEffectSelector`. |
| `lib/Reeltwo` | upstream | **Do not edit.** The contract plugs in through the public `seq >= 100` `CustomLogicEffectSelector` hook. The two `wifi/` headers carry pre-existing C2B5 modifications and an LGPL §2(b) notice. |

## Conventions that matter

- `ContractRender` **must return `true`** — `LogicEngine.h:820-830` reverts the display to NORMAL if
  a custom effect returns false.
- `setPixelRGB` bypasses the engine's `fBri`, so brightness and the beat envelope are applied inside
  the render layer.
- The score sequencer runs in `contractTick()` from `mainLoop()`, **not** inside the effect
  function, so sections keep advancing even while a `native:<code>` section look (which
  `ContractRender` does not draw) is on screen.
- `!` is the fork's only command prefix, and it was chosen because every stock parser ignores it.
