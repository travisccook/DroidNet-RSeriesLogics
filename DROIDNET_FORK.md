# DroidNet_RSeriesLogics

DroidNet contract fork of the **RSeries Logic Engine** firmware (reeltwo `WLogicEngine32`),
customized to implement the **Driveable-Animation Contract** so Cantina Studio can drive the
FLD/RLD logic displays with arbitrary RGB, beat-locked accents, a Studio-seeded beat-clock, and
autonomous board-side section playback — **additively**, with every native `~RTLE…`/`@nT…`/`#LE…`
command still working.

- **Role:** sub-project D of the Cantina dome light-show program (the easiest fork; first hardware proof).
- **Seeded from:** `travisccook/C2B5` @ `2fbd4023` (subdir `RSeriesLogic`), 2026-07-12 — git-tracked files only.
- **MCU/toolchain:** ESP32 (default env `wlogicengine32`) / ESP32-S3 (`wlogicengine32s3`), PlatformIO.
- **Fork spec (authoritative):** `DroidNetSignalBooster/docs/superpowers/specs/2026-07-12-rseries-logic-fork-design.md`
  (+ the contract v1.1 amendment and transport spec, same folder/date).
- **Fork code convention:** contract code lives in `src/ContractLogics.h` + minimal `src/main.cpp` hooks;
  `lib/Reeltwo` stays untouched (contract effects append to the `seq>=100` `CustomLogicEffectSelector`).
- **GitHub remote:** TBD (created later).
- **Status:** baseline import — no contract code added yet.
