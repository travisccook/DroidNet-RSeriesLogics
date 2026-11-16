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
- contract S9 and S4 as a property (stage 2e): hostile scripts and random line streams, with random button
  presses, go through the real layer, a quarter of them at the loop's own pace of 1 to 4 ms a pass; every frame
  is rendered and its pixels measured by two photometers. Asserted: no two dark-to-lit onsets of a display less
  than 340 ms apart, in the field's level (seven readings of "dark" and "lit", four of them an eighth of the
  brightness wide) and in its Rec.709 luminance (three readings); no two changes of the field's colour less than
  170 ms apart (five readings); no pixel above the owner's brightness; the native look after idle or the
  watchdog; dark after `X`; a button's sequence left running. The mock engine draws a frame every 10 ms at most
  and has the timing of the two stock sequences that flash by themselves (FLASHCOLOR and FAILURE);
- the Serial1 reader, including a 400-byte burst of back-to-back lines, with and without the pass-through;
- the real ESP32 cross-compile (`pio run -e wlogicengine32`), which is a link, not a run.

Run the suite for the current numbers; this file does not repeat them.

Fork head: `main` in this repository. The baseline import is commit `6bba8b1`; every commit after it
is the DroidNet layer (`git log --oneline 6bba8b1..HEAD`).

### What is in (contract v1.3, R1)

- **Verbs.** `A` (animate: live, or scored with `at=`), `P` (pulse: the accent overlay), `C` (beat clock),
  `B` (brightness), `L` (level), `X` (stop), `M` (mode: `show` / `idle`), `V` (verify the load), `Q` (query).
- **Looks.** `off`, `solid`, `flash`, `pulse`, `rainbow`, `scan`, `sparkle`, `meter`, `comet`, `chase`, `wipe`,
  `gradient`, `colorcycle`, `twinkle`, and `native:<code>` for the codes on the allow-list: 0, 1, 4, 5, 7 to 14,
  19 to 21, 23, 24, 101 and 105 (see "Native sequences" below for what is off it, and why).
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
  `contract_core.h`. On top of those, every frame the contract draws goes through two gates, whatever lines
  arrive and in whatever order. S9, the field gate: a display does not go from dark to lit less than 340 ms after
  it last lit or got brighter. A rise of more than a tenth of the display's brightness (at least 2 levels) is such
  an onset, and it is measured twice, by the brightest channel and by luminance, so white after blue counts
  though no channel got brighter. S4, the look gate: a frame that would show another look or colour than the
  field last showed is drawn 170 ms after the field's last change at the earliest, whichever path asked for it (a
  live `A`, a section reached by time or by a `C` seek, an accent, a native sequence, a look given back); the
  frame that is up stays up meanwhile. A blackout is never held back; what follows it is. `b=` and `B` are
  fractions of the display's own brightness, as its owner set it, and a brightness the owner sets during a show
  is the display's brightness from then on. `B` is honoured only in show mode, and a reset clears it. `b=` on a
  `P` is that pulse's alone.
- **A button always works (L6).** A native sequence started on a display the contract holds (`@nT...`, the web
  page, the engine's own commands) ends the show on that display, as `M:v=idle` would, and runs. The other
  display plays on. The layer never selects over a sequence that is waiting for the engine's next frame, so a
  button pressed in the same 10 ms as a change of the contract's own is not lost; only `X` does, and `X` always
  lands.
- **`L` feeds the contract's own `meter` look and nothing else.** No native sequence is driven by it.
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
- `native:2`, `6`, `22`, `102`, `103` and `104` (failure, flash colour, fire, metaballs, fractal, fade and
  scroll). Their whole field swings faster than S10 allows and the engine draws them, so the layer cannot gate
  them. They still run from a button.
- `b=` and `B` on `native:19` to `24`, `101` and `105`. Those sequences write their pixels at a level of their
  own; `NATIVE_DIMS_LOGICS` in the vector file lists the codes that do take the brightness.
- One limit for the two displays together. Each display has its own gates, so the front and the rear can light
  by turns, each 340 ms apart: 5.9 onsets a second between them. Not limited, and a bench item.
- A limit on `#LEBRI`. It scales everything the board puts out, at once and as often as it is sent, outside the
  contract and outside both gates. An owner item.
- L6 on a show that is armed but has not begun. The display is the engine's own then, and the engine starts
  sequences there by itself (RANDOM does, every few seconds), so a button cannot be told from that: it plays, and
  the show's first `C`, `A` or `P` takes the display as usual. Likewise a button for the very sequence that is
  already running changes nothing and cancels nothing.
- A load longer than about 1.2 KB, sent back to back, while the serial pass-through runs slower than the input
  (the stock Serial2 pass-through is 2400 baud). The reader never waits on the pass-through, so it can take bytes
  no faster than the pass-through sends them, and the UART drops what its 1024-byte buffer cannot hold. `V`
  reports such a load bad. Pace the load, raise the pass-through's baud, or turn the pass-through off.
- A change of the whole field of a tenth of the display's brightness or less is a look's own texture to the
  S9 gate and is not rate-limited.
- A photometer that reads luminance in floating point, or on the field's mean colour, can differ from the
  gate's whole-number reading by up to 2 levels. A look that drifts across such an observer's threshold inside
  those 2 levels reads as lighting a little later than the gate timed it.

### Native sequences (S10)

The engine draws its own sequences, so the layer cannot pass their frames through the gate: on this board every
allow-listed code runs as stock, and a sequence whose whole field swings too fast is off the list. The test
applied here: as the contract selects the sequence (default colour, speed 0, no microphone source wired), the
mean of the whole field must not swing by more than a tenth of its own peak more than three times in any
second, in level or in luminance. A swing is counted the way the gate counts: a move of more than a tenth from
the last peak or trough, up or down.

The figures are from a model of the stock code, not from a display: the palette as `calculateAllColors()` bakes
it, every LED walking it, FastLED's `hsv2rgb_rainbow`, and a change of hue or scale taken as reaching all LEDs at
once (the real display smears it over a few frames, so this is the worst case). The custom effects and fire were
run frame by frame from their source. Nothing here is measured on hardware.

| Code | Sequence | Whole field, as the contract selects it | On the list |
| ---- | -------- | --------------------------------------- | ----------- |
| 0 | normal | steady texture | yes |
| 1 | alarm | steady: with the default colour both halves of its 250 ms flip are palette 2 at hue 0 | yes |
| 2 | failure | 1.8 to 6 s: the hue turns once in 2.56 s and the level swings by a third (front) with it, the closest two swings 310 ms apart, four inside 980 ms; at 6 s it jumps to 2.2 (front) or 3.2 (rear) times the normal level, then fades out in 2 s; dark to 18 s | no: at the limit |
| 3 | Leia | sets its own brightness, 50 to 255, every frame | no (S6) |
| 4 | march | steady in the mean; each half goes 2 to 100 % every 150 ms, the halves in turn | yes, see below |
| 5 | solid colour | steady | yes |
| 6 | flash colour | 2 to 100 % every 250 ms: 4 swings a second | no |
| 7, 8 | flip flop | steady in the mean; each half goes 2 to 100 % every 200 ms, the halves in turn | yes, see below |
| 9 | colour swap | the hue jumps by half a turn every 350 ms: level 69 to 100 %, luminance 40 to 100 %, 2.9 swings a second, never more than 3 in one | yes, by 10 ms |
| 10 | rainbow | the hue steps by 20 every 200 ms: level 52 to 100 %, 2.3 swings a second, the closest two 200 ms apart, never more than 3 in one; luminance 10 to 100 %, swings 1 s apart | yes |
| 11 | red alert | steady (no microphone source: it never leaves its colour) | yes |
| 12, 13 | mic bright, mic rainbow | steady (no microphone source) | yes |
| 14 | lights out | dark | yes |
| 15 to 18 | text | need a message | no |
| 19 | roaming pixel | one row fills, a pixel every 20 ms, and is cleared: at most 9 % of the front display lit, 24 % of a 24 by 4 rear display | yes: a small lit area |
| 20, 21 | scan lines | one row or one column lit, moving: steady in the mean (10 % front, 25 % rear for a row) | yes |
| 22 | fire | the stock code never resets its blend counter, so after the first second the whole field is rolled again every 200 ms: mean 37 to 100 % of its peak, 1.4 swings a second, up to 5 in one second | no |
| 23 | PSI colour wipe | a column every 50 ms, then 1 to 3 s still: one colour change a wipe | yes |
| 24 | pulse | a box grows from the centre and shrinks, a step every 140 ms: 1 to 100 % and back in 1.4 s (front) or 3.6 s (a 24 by 4 rear) | yes |
| 101 | plasma | level steady; luminance 16 to 100 %, swings 1.5 s apart | yes |
| 102 | metaballs | the colour is rolled again every 270 ms and the balls cross the display in 20 to 100 ms: 30 to 45 swings a second | no |
| 103 | fractal | level steady; luminance 9 to 100 %, 1.9 swings a second, up to 6 in one second | no |
| 104 | fade and scroll | random at every start; with all pixels in phase and one colour the field goes 0 to 100 % and back in 520 ms (4 swings a second) at its fastest three speeds of ten | no |
| 105 | colour wheel | steady texture | yes |

Three things are the owner's to decide, because the test above is about the whole field's mean:

- **March and the flip flops (4, 7, 8)** pass it only because their two halves swing against each other. Each
  half of the display goes dim and bright every 150 ms (march; 175 ms in its last 3 s) or 200 ms. If half a
  display counts, they come off: one line in `nativeAllowedLogics()` and a regenerated vector file.
- **Failure (2)** fails by 20 ms on the front display in a worst-case model and passes on the rear. A bench
  measurement could put it back.
- **Rainbow (10)** passes by the count of three in a second, and by luminance. Its level swings are 200 ms
  apart at their closest.

A microphone source wired to the engine (R2's level feed) changes 11, 12 and 13: they then follow the room, and
this table has to be done again for them.

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
- **S9: ask, then tell.** A frame effect 106 draws is measured (level and luminance), the gate is asked
  (`contractFieldAsk()`), a frame it holds down is drawn again scaled and measured again, and the gate is told
  what is on the LEDs (`contractFieldTell()`). Light the layer does not draw (a native look, at the brightness
  the contract gives it) is ASKED for before the hand-over and TOLD to the gate at the frame the engine starts
  it. The gate is never told about light that did not reach the LEDs.
- **S4 at the output.** `ContractRender` passes every lit frame's look (effect and colour) through
  `contractLookGate()` and leaves the frame that is up when the answer is no. A live `A` or a section restarts
  the look's timeline, so both call `contractLookRestart()`: the next lit frame is a change of look even when
  the look has the same name.
- **S10: a native sequence runs as stock.** Its frames are never held. The layer asks before it hands the
  display over (`contractNativeMayStart()`: yes only when an onset is allowed, whether the display is lit or dark
  just then, because a stock sequence may clear the display and light it at once; the same for the hand-back to
  the display's own look), tells the gate at the frame the engine starts the sequence
  (`contractNativeStarted()`, with the level it runs at: the engine's brightness, 0 for lights-out, full scale for
  a sequence that draws at a level of its own) and tells it when effect 106 takes the display back
  (`contractNativeEnded()`). A sequence that is not on the allow-list, or that draws at its own level, leaves
  the gate knowing nothing about the field: it is then taken as dark and just lit, so the contract's next light
  waits 340 ms, and a native look that is to follow it comes through effect 106, which holds the last frame
  meanwhile. The owner's own sequences (not on the list) keep the owner's brightness; `B` does not reach them.
- `setPixelRGB` bypasses the engine's `fBri`, so brightness and the beat envelope are applied inside the layer.
- **Brightness.** The ceiling is the display's own `fBri` (the RAM default object, read when a show arms or the
  first cue arrives). `b=`/`B` are fractions of it. For native looks the level is written to that default object,
  because the engine copies it back on every effect change. The owner's value goes back when the show no longer
  needs it: at once while effect 106 is drawing (nobody sees the engine's brightness then) and at the frame a
  button's sequence starts, through the S9 gate when a native look of the contract's is up. A value the owner
  sets meanwhile (the slider, the trimpot, `#LEBRI`) is noticed every pass and becomes the ceiling.
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
  A code goes on the list only when it passes the S10 test below.
- **The Serial1 reader is `ContractSerial`, with 128-byte lines.** The library's reader takes one byte a pass,
  which is slower than 9600 baud once `FastLED.show()` is counted, and processes an over-long line truncated.
  The longest line Studio can emit is 102 characters; `test/host/run.sh` stage 3 recomputes that and checks the
  serial and WiFi widths. The reader never waits on the pass-through stream (`contractDrainBudget()`).
- **A WiFi line goes through `contractWifiLine()` first.** The library's receiver frames on CR alone and cuts a
  long line off silently; the helper skips the LF of a CR LF host and drops a contract line that was cut off.
- **A contract line runs in the reader, not on the animation player.** `Marcduino::processCommand` queues one
  action with one command pointer, so of several lines handled in one pass only the last would run.
