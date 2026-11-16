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

**Contract v1.3, release R1, in source. Not on hardware.**

- The prebuilt classic ESP32 image in `firmware/` (`f7bd207`, 2026-10-16) runs on the C2-B5 dome's R-Series board;
  it was flashed over WiFi through the board's own Reflash page. That image predates contract v1.3.
- Nothing in this repository records a contract (`!`) command observed on real LEDs, with any image. Treat the
  contract layer as untested on hardware until the R1 bench checklist has been run and recorded.
- The ESP32-S3 image has never been flashed.

What is verified is host-side, by `bash test/host/run.sh` (and by CI, `.github/workflows/host-tests.yml`):

- the shared core's unit tests, v1.2 and v1.3, and a differential fuzz of its parser under ASan/UBSan;
- the RSeries layer on a mock of the engine that models sequence selection in the engine's own order (the effect
  runs, and only then is the selection recorded as seen), `restoreSettings()`, the effect selector being called
  when a sequence starts, and the engine's out-of-bounds effect id; every finding of the 2026-11-15 review and of
  the 2026-11-16 review of R1 is a regression test there;
- contract S9 as a property (stage 2e): hostile scripts and random line streams, with random button presses, go
  through the real layer; every frame is rendered and its pixels measured. Asserted: no two dark-to-lit onsets of
  a display less than 340 ms apart, for three different readings of "dark" and "lit"; no pixel above the owner's
  brightness; the native look after idle or the watchdog; dark after `X`; a button's sequence left running;
- the Serial1 reader, including a 400-byte burst of back-to-back lines, with and without the pass-through;
- the real ESP32 cross-compile (`pio run -e wlogicengine32`), which is a link, not a run.

Run the suite for the current numbers; this file does not repeat them.

Fork head: `main` in this repository. The baseline import is commit `6bba8b1`; every commit after it
is the DroidNet layer (`git log --oneline 6bba8b1..HEAD`).

### What is in (contract v1.3, R1)

- **Verbs.** `A` (animate: live, or scored with `at=`), `P` (pulse: the accent overlay), `C` (beat clock),
  `B` (brightness), `L` (level), `X` (stop), `M` (mode: `show` / `idle`), `V` (verify the load), `Q` (query).
- **Looks.** `off`, `solid`, `flash`, `pulse`, `rainbow`, `scan`, `sparkle`, `meter`, `comet`, `chase`, `wipe`,
  `gradient`, `colorcycle`, `twinkle`, and `native:<code>` for the codes on the allow-list (0-2, 4-14, 19-24,
  101-105; not 3, the stock Leia effect, which sets its own brightness past the owner's).
  `flash`, `pulse` and `scan` use the core's shared timing, the same on all three boards: `pulse` is a breathe
  whose cycle is never shorter than 340 ms (and a steady fill moved by the beat pump when one is on), `scan` one
  column sweeping back and forth.
- **Beat clock.** One clock for both displays. `C:bpm=,ph=,bpb=,beat=` seeds it; a bare `C:ph=,beat=` re-anchors
  it (the host's drift correction); `C:bpm=0` holds the show. A tempo outside 30-300, or a `bpm=` that is not a
  plain number, makes the whole line a no-op. The clock and the accent envelope are integer arithmetic
  (no float in the core or in this layer): a beat falls on the exact millisecond however long the show runs, and
  the Studio preview's mirror can be tested against it with no tolerance.
- **Score.** 32 entries a display. Each carries its look, `b=`, `m=`, `am=`, `an=`/`ap=` (every-N accents) and an
  optional accent (`ae=`/`ac=`/`ad=`), which the board fires itself on the section's accent beats. A scored `A`
  with no `i=` is dropped. A line delivered twice changes nothing, the section that is playing included.
- **Load check.** `V:n=,k=` compares the host's entry count and checksum with what the display holds. A display
  that did not get its whole show goes back to its native look, reports `ok=2`, and takes no scored line until
  the next `M:v=show`.
- **Lifecycle.** `M:v=show[,wd=<seconds>]` arms a show without darkening the display; the show's first `C`, live
  `A` or `P` starts it. `X` blacks out. `M:v=idle` gives the display back. With `wd=`, that many seconds without
  a contract line addressed to the board is an idle.
- **Safety.** No accent, pump swing or live look change recurs faster than the photosensitivity limits in
  `contract_core.h`. On top of those, every frame the contract draws goes through one gate (S9): whatever lines
  arrive, in whatever order, a display does not go from dark to lit less than 340 ms after it last did. A
  blackout is never held back; what follows it is. `b=` and `B` are fractions of the display's own brightness, as
  its owner set it. `B` is honoured only in show mode, and a reset clears it. `b=` on a `P` is that pulse's alone.
- **A button always works (L6).** A native sequence started on a display the contract holds (`@nT...`, the web
  page, the engine's own commands) ends the show on that display, as `M:v=idle` would, and runs. The other
  display plays on.
- **Query.** `Q:r=<route>` addressed to one display is answered on Serial1 with
  `;S0!L<u>q:ver=1.3,rel=1,cap=32,n=<held>,ok=<0|1|2>,bpm=<n>` (`;W<n>,;S0...` for a remote WCB). `r` is a
  number 0 to 99; anything else gets no reply. One reply a display can wait to be written.

### What is not in

- Any hardware validation of the contract layer (see Status).
- Release R2: the `logic` look, the `kick` accent (so a `native:` section takes no accent in R1), the level feed
  to the native mic-reactive effects (`contractSetup()` is still a no-op).
- `d=` on `A` (retired in v1.3), `c=p<n>` palette colours, `sw=` and `band=`: parsed past, never acted on.
- A pulse that interrupts a timed native sequence returns to the display's default look, not to that sequence:
  the engine has no public way to ask which sequence is running.
- `native:3` (Leia). The stock effect sets its own brightness, 50 to 255, every frame, so it can neither be held
  under the owner's brightness nor dimmed by `B`. It comes back with a look of the contract's own.
- L6 on a show that is armed but has not begun. The display is the engine's own then, and the engine starts
  sequences there by itself (RANDOM does, every few seconds), so a button cannot be told from that: it plays, and
  the show's first `C`, `A` or `P` takes the display as usual. Likewise a button for the very sequence that is
  already running changes nothing and cancels nothing.
- A load longer than about 1.2 KB, sent back to back, while the serial pass-through runs slower than the input
  (the stock Serial2 pass-through is 2400 baud). The reader never waits on the pass-through, so it can take bytes
  no faster than the pass-through sends them, and the UART drops what its 1024-byte buffer cannot hold. `V`
  reports such a load bad. Pace the load, raise the pass-through's baud, or turn the pass-through off.
- A change of the whole field smaller than a quarter of the display's brightness is a look's own texture to the
  S9 gate and is not rate-limited.

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

## Layout

| File | Owner | Notes |
| --- | --- | --- |
| `src/contract/contract_core.h` | fork | Pure C++, no Arduino/FastLED/Reeltwo. **Byte-identical across all three board forks.** This repository is where it is edited: see [docs/CHANGING_THE_CORE.md](docs/CHANGING_THE_CORE.md). |
| `src/contract/ContractLogics.h` | fork | The RSeries layer: verbs, the tick, and the renderer, custom effect id **106**. |
| `src/contract/contract_line.h` | fork | Pure C++: the reader's 128-byte line assembler and drain loop. Fork-local. |
| `src/contract/ContractSerial.h` | fork | The Serial1 reader, in place of the library's `MarcduinoSerial<>`. Fork-local. |
| `src/native/native_brightness_parse.h` | fork | Pure `#LEBRI` parser, no Arduino/FastLED/Reeltwo. Fork-local (not shared with PSI/Flthy). |
| `src/native/NativeBrightness.h` | fork | The `#LEBRI` Marcduino action and the FastLED controller capture. Not part of the contract. |
| `test/host/` | fork | Host harness. Keep it green. The files in `test/host/core-files.txt` are shared byte-identically with the PSI and Flthy forks; the rest are fork-local. |
| `test/host/vectors/contract_vectors.json` | fork | Generated by `gen_contract_vectors.cpp` from the real core. The Studio preview is tested against a copy of it. |
| `scripts/core-sync.sh` | fork | Compares, and copies, the shared files across the sibling fork checkouts. |
| `test/host/catalog_example_lines.tsv` | fork | Snapshot of every command-library example line, for `prefix_guard.cpp`. Regenerate with `refresh_catalog_examples.py <library checkout>`. |
| `src/main.cpp` | upstream | Fork edits, each marked `DroidNet`: the contract includes and `ContractRender` in the effect table; `ContractSerial` as the Serial1 reader and a 1024-byte receive buffer; the WiFi receiver's line width, `contractWifiLine()` and its contract dispatch; one effect selector per display, reporting to `contractEngineLatched()`; `contractSetup()`, `contractTick()` and the `Q` reply write in `mainLoop()`; the `#LEBRI` include and controller capture; the WDP init and tick; a one-character fix to a latent off-by-one in `CustomLogicEffectSelector`. |
| `lib/Reeltwo` | upstream | **Do not edit.** The contract plugs in through the public `seq >= 100` `CustomLogicEffectSelector` hook. The two `wifi/` headers carry pre-existing C2B5 modifications and an LGPL §2(b) notice. |

## Conventions that matter

- **Nothing selects a sequence from inside the effect.** The engine runs the effect and only then records the
  selection as seen (`LogicEngine.h:818-819`), so a `selectSequence()` made inside `ContractRender` is never
  noticed and effect 106 goes on running. `_present()` is the one place a sequence is selected; it runs from the
  line dispatch and from `contractTick()`. `ContractRender` returns `false` only when someone else selected
  effect 106 by its number.
- **The engine tells the layer when a sequence starts.** `main.cpp` gives each display its own effect selector
  (`ContractSelectorFLD` / `ContractSelectorRLD`), and the engine calls a selector at the frame it starts a
  newly selected sequence, never otherwise. `contractEngineLatched()` is that report. It is how the layer knows a
  native look is really on the LEDs, and how it sees a sequence it did not select (L6). It selects nothing.
- **S9: ask, then tell.** A frame effect 106 draws is measured and passed through `contractFieldGate()` before
  it leaves. Light the layer does not draw (a native look, at the brightness the contract gives it) is ASKED for
  with `contractFieldAllowed()` before the hand-over and TOLD to the gate at the frame the engine starts it.
  The gate is never told about light that did not reach the LEDs.
- `setPixelRGB` bypasses the engine's `fBri`, so brightness and the beat envelope are applied inside the layer.
- **Brightness.** The ceiling is the display's own `fBri` (the RAM default object, read when a show arms or the
  first cue arrives). `b=`/`B` are fractions of it. For native looks the level is written to that default object,
  because the engine copies it back on every effect change. The owner's value goes back when the show no longer
  needs it: at once while effect 106 is drawing (nobody sees the engine's brightness then), through the S9 gate
  when a native look is up.
- **Show boundaries run the core's one reset** (`contractOnShow` / `contractOnStop` / `contractOnIdle`). Do not
  reset fields by hand in the layer: a field added to `ContractUnitState` is then reset everywhere.
- The score, its accents, the watchdog and held live looks run in `contractTick()` from `mainLoop()`, **not**
  inside the effect function, so they keep running while a `native:<code>` look (which `ContractRender` does not
  draw) is on a display.
- `!` is the contract's only command prefix, and it was chosen because every stock parser ignores it.
  The one native command the fork adds is `#LEBRI`.
- **No Marcduino action text may be a prefix of another.** `Marcduino::processCommand` runs every
  action whose text prefixes the line, and only the last-registered one survives, so a clash drops a
  command silently. `test/host/prefix_guard.cpp` fails on any prefix pair except the three stock ones
  (`@0T1`→`@0T11`, `@1T1`→`@1T11`, `@2T1`→`@2T11`, which work because the longer text is registered
  later), and on any command-library example line that would run `#LEBRI` without being
  `rseries.cfg.brightness`.
- **The photosensitivity limits are safety limits, not tuning knobs,** and they live in `contract_core.h`, once,
  for all three forks: `STROBE_MIN_STATE_MS = 170`, the 340 ms floor between accents and between pump swings
  (`beatRateDivisor`), the 170 ms hold between live look changes, the tempo range. The core's show reset keeps
  the cool-down timers, so a reset is not a way through them.
- **`native:<code>` is an allow-list** (`nativeAllowedLogics()` in the core). Code 25 indexes one past the
  engine's effect table (`LogicEngine.h:2733`); it and every other code off the list drop the whole line.
- **The Serial1 reader is `ContractSerial`, with 128-byte lines.** The library's reader takes one byte a pass,
  which is slower than 9600 baud once `FastLED.show()` is counted, and processes an over-long line truncated.
  The longest line Studio can emit is 102 characters; `test/host/run.sh` stage 3 recomputes that and checks the
  serial and WiFi widths. The reader never waits on the pass-through stream (`contractDrainBudget()`).
- **A WiFi line goes through `contractWifiLine()` first.** The library's receiver frames on CR alone and cuts a
  long line off silently; the helper skips the LF of a CR LF host and drops a contract line that was cut off.
- **A contract line runs in the reader, not on the animation player.** `Marcduino::processCommand` queues one
  action with one command pointer, so of several lines handled in one pass only the last would run.
