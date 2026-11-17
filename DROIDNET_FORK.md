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
  brightness wide) and in its Rec.709 luminance (seven readings); no two changes of the field's colour less than
  170 ms apart (six readings, one of them from 4 levels up); no pixel above the owner's brightness; the native
  look after idle or the watchdog; dark after `X`; a button's sequence left running. The mock engine draws a
  frame every 10 ms at most and has the timing of the two stock sequences that flash by themselves (FLASHCOLOR
  and FAILURE). The engine draws at most one frame of a display in a pass and the pixels are read after every
  pass, so every frame is read;
- a part of the display, in the same stage: the whole field, its halves, its quarters and four bands of columns
  and of rows are watched on the pixels, with the rear display at five sizes by turns. A region may go from dark
  to lit three times in any one second. Asserted on every look by itself and on scripts made for it (a meter
  under `L`, a comet, looks by turns); in the random streams it is measured and printed. Every look by
  itself is held to an observer as fine as the gate (a tenth of the brightness, every band of a quarter of the
  columns and of the rows, by level and by luminance). Six scripts that alternate two looks are known failures:
  see "What is not in";
- the core's own looks on strands of 6 to 28 positions (stage 1j, pure core): no run of a quarter of a strand or
  more lights more than three times a second, at any speed;
- the frame fixtures Studio's preview is checked against (stage 2f, `test/host/frames/`) are what the layer
  draws on the mock engine;
- contract S10 as a test (stage 1i): a model of the stock sequences, on every display size the engine knows,
  counts each allow-listed code's onsets over the whole field and over every region of a quarter or more, at the
  speed the core starts it with. It is a model of the stock code, not a measurement of a display;
- the Serial1 reader, including a 400-byte burst of back-to-back lines, with and without the pass-through;
- the real ESP32 cross-compile (`pio run -e wlogicengine32`), which is a link, not a run.

Run the suite for the current numbers; this file does not repeat them.

Fork head: `main` in this repository. The baseline import is commit `6bba8b1`; every commit after it
is the DroidNet layer (`git log --oneline 6bba8b1..HEAD`).

### What is in (contract v1.3, R1)

- **Verbs.** `A` (animate: live, or scored with `at=`), `P` (pulse: the accent overlay), `C` (beat clock),
  `B` (brightness), `L` (level), `X` (stop), `M` (mode: `show` / `idle`), `V` (verify the load), `Q` (query).
- **Looks.** `off`, `solid`, `flash`, `pulse`, `rainbow`, `scan`, `sparkle`, `meter`, `comet`, `chase`, `wipe`,
  `gradient`, `colorcycle`, `twinkle`, and `native:<code>` for the codes on the allow-list: 0 to 2, 4 to 14,
  19 to 24, 101 and 105 (see "Native sequences" below for the test, the speeds and what is off the list).
  `flash`, `pulse`, `scan`, `chase` and `wipe` use the core's shared timing, the same on all three boards:
  `flash` is 190 to 445 ms a state; `pulse` is a breathe whose cycle is never shorter than 680 ms (and a steady
  fill moved by the beat pump when one is on); a `wipe` fills and drains in 680 ms at the least; `chase` lights
  three of the eight columns at every step, and a step is 130 ms at the least; `scan` is one column sweeping back
  and forth, there and back in a second at the least (72 ms a column on the front display's eight); a `comet`
  laps the display in 680 ms at the least (85 ms a column). `sparkle` draws a new pattern every 350 to 477 ms. A
  live `A` or a section that asks for the look that is already up (the same effect and speed) does not restart
  it, and neither does one that changes its colour alone: the look is recoloured in place.
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
  frame that is up stays up meanwhile. A frame either gate holds is not shown at all, dimmed or otherwise: the
  frame that is up stays up. A frame whose brightest LED is under 4 levels is not a look, and is never held. A
  blackout is never held back; what follows it is. `b=` and `B` are
  fractions of the display's own brightness, as its owner set it, and a brightness the owner sets during a show
  is the display's brightness from then on. `B` is honoured only in show mode, and a reset clears it. `b=` on a
  `P` is that pulse's alone.
- **A button always works (L6).** A native sequence started on a display the contract holds (`@nT...`, the web
  page, the engine's own commands) ends the show on that display, as `M:v=idle` would, and runs. The other
  display plays on. The layer never selects over a sequence that is waiting for the engine's next frame, so a
  button pressed in the same 10 ms as a change of the contract's own is not lost; only `X` does, and `X` always
  lands.
- **`L` feeds the contract's own `meter` look and nothing else.** No native sequence is driven by it. Each
  display keeps its own level. A higher level is taken at most once in 340 ms; a level that is no higher at
  once. A raise that comes sooner waits, the newest of them, and the show reset clears the level.
- **`B`, and `b=` on a look, go up at most once in 340 ms** too, in the same window as `L`: a lower brightness
  is taken at once, a higher one waits for the window, the newest of them. A look that lights part of a display
  (a meter with one row lit) under `B:b=0` and `B:b=200` by turns lit that part as often as the lines came.
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
- `native:102`, `103` and `104` (metaballs, fractal, fade and scroll). The first two fail S10's test and the
  engine draws them, so the layer cannot gate them; the third draws what it does at random when it starts. They
  still run from a button.
- `b=` and `B` on `native:19` to `24`, `101` and `105`. Those sequences write their pixels at a level of their
  own; `NATIVE_DIMS_LOGICS` in the vector file lists the codes that do take the brightness.
- A brighter `B` or `b=` on a native sequence whose own light moves (`native:2`, `6`, `9`, `10`) while it runs.
  Nothing can tell when such a sequence next lights by itself, so it is given a brighter level only when it
  starts. A dimmer one reaches it at once.
- A limit on a part of a display for every order of lines. No look by itself, and no meter under `L`, lights a
  region of a quarter of a display or more than three times in a second; the core's own test asserts that for
  the looks it times, on every strand length (stage 1j), and the property stage for every look on the rendered
  pixels, by an observer as fine as the gate. Two looks by turns do, five scripts in all, which the stage lists
  as known failures with their figures (a scan and a chase every 170 ms: six times a second). The first found
  was a comet and a full look: the comet passes a region and the full look lights it again, four onsets in one
  second (`A:i=comet` for 510 ms, `A:i=solid` for 170 ms, repeated). The whole-field gates cannot see it. It is
  in the property stage as a known failure, and it is an owner item: a gate that knows regions, or a comet
  that never leaves a part of the display dark.
- The luminance of a native sequence. The layer tells the gate one figure for a native look, its level, and the
  gate takes its luminance to be the same. A contract look that follows at the same level and a higher
  luminance (white after the blue of a front logic) is no onset to the gate. A bench item.
- Two roundings the third review measured and that are left as they are. The C1 guard ("more than 60 ms into
  the beat") differs from the words only between 60 and 61 ms. Section progress, for the build ramp, is coarse
  by 2 to 3 parts in 65,536 on a section longer than 65,535 beats.
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
- "By luminance" is exact in the gate's own measure: Rec.709 in whole numbers, `(54 r + 183 g + 19 b) >> 8` per
  LED, then the mean in whole levels. The third review read it in floating point and dated an onset strictly:
  on a gradient or a colorcycle whose luminance creeps by one level before an `X` and a relight it found pairs
  155 to 315 ms apart. Dated from when the level came within 2 of the observer's threshold, every pair was
  340 ms apart. It is a rounding of the measure and not a flash anyone sees.
- A photometer that reads luminance in floating point, or on the field's mean colour, can differ from the
  gate's whole-number reading by up to 2 levels. A look that drifts across such an observer's threshold inside
  those 2 levels reads as lighting a little later than the gate timed it.

### Native sequences (S10)

The engine draws its own sequences, so the layer cannot pass their frames through the gate: on this board every
allow-listed code runs as stock, and a sequence that fails the test is off the list. The test is S9's own
measure, applied to the sequence as the contract starts it (default colour, the speed in the table, no microphone
source wired): by brightness and by luminance, no two onsets closer than 340 ms, over the whole field and over any
region of a quarter of the field or more. An onset is counted the way the gate counts: a rise of more than a
tenth above the last trough. A sequence whose lit area never reaches a quarter of the field passes without the
count.

The classification is kept as a test, `test/host/test_native_s10.cpp` (stage 1i of the suite), on a model of
the stock code: the palette as `calculateAllColors()` bakes it, every LED walking it, FastLED's
`hsv2rgb_rainbow`, the engine's effect delay, and the sequences that draw pixel by pixel run frame by frame on
every display size the engine knows (front 8 by 10; rear 16 by 4, 24 by 4, 28 by 4, 32 by 8). A change of hue or
scale is taken as reaching all LEDs at once, which is the worst case. The test fails when a code on the core's
list does not pass at the speed the core gives it, or when a lower speed would pass too. Nothing here is measured
on hardware; the bench confirms it.

Closest two onsets, by the model:

| Code | Sequence | Kind | Speed | Whole field, level | Whole field, luminance | Any region of a quarter or more | On the list |
| ---- | -------- | ---- | ----- | ------------------ | ---------------------- | ------------------------------- | ----------- |
| 0 | normal | runs as stock | 0 | none | none | none | yes |
| 1 | alarm | runs as stock | 0 | none | none | none | yes |
| 2 | failure | runs as stock | 0 | 580 ms | 1,810 ms | none | yes |
| 3 | Leia | | | | | | no: sets its own brightness (S6) |
| 4 | march | runs as stock | 1 | none | none | 350 ms | yes |
| 5 | solid colour | runs as stock | 0 | none | none | none | yes |
| 6 | flash colour | runs as stock | 0 | 500 ms | 500 ms | none | yes |
| 7, 8 | flip flop | runs as stock | 0 | none | none | 400 ms | yes |
| 9 | colour swap | runs as stock | 0 | 700 ms | 700 ms | none | yes |
| 10 | rainbow | runs as stock | 0 | 600 ms | 2,000 ms | none | yes |
| 11 | red alert | runs as stock | 0 | none | none | none | yes |
| 12, 13 | mic bright, mic rainbow | runs as stock | 0 | none | none | none | yes |
| 14 | lights out | runs as stock | 0 | dark | dark | dark | yes |
| 15 to 18 | text | | | | | | no: they need a message |
| 19 | roaming pixel | runs as stock | 1 | 800 ms | 800 ms | 800 ms | yes |
| 20 | horizontal scan line | runs as stock | 2 | none | none | 450 ms | yes |
| 21 | vertical scan line | runs as stock | 0 | a small lit area on every display | | | yes |
| 22 | fire | runs as stock | 1 | 400 ms | 400 ms | 400 ms | yes |
| 23 | PSI colour wipe | runs as stock | 0 | none | none | none | yes |
| 24 | pulse | runs as stock | 0 | 700 ms | 700 ms | 700 ms | yes |
| 101 | plasma | runs as stock | 0 | none | 5,400 ms | 2,910 ms | yes |
| 102 | metaballs | | | 20 ms | 20 ms | 20 ms | no: fails |
| 103 | fractal | | | none | 250 ms | 40 ms | no: fails |
| 104 | fade and scroll | | | 510 ms | 510 ms | 510 ms | no: see below |
| 105 | colour wheel | runs as stock | 0 | none | none | none | yes |

What the speed column changes, and why:

- **March (4), speed 1.** At the engine's speed 0 its thirds flip every 150 ms, so each of them lights every
  300 ms. At speed 1 they flip every 300 ms. Its last 3 s run at 175 ms whatever the speed
  (`LogicEngine.h:2004`): 350 ms, which passes with 10 to 20 ms to spare by the third review's measurement.
  That is the narrowest margin in this table.
- **Roaming pixel (19), speed 1.** At speed 0 it adds a pixel every 10 ms and clears the row when it is full. On
  the front display a row is a tenth of the field; on a 4-row rear display it is a quarter, and it would fill
  every 160 to 290 ms. At speed 1 it is a pixel every 50 ms.
- **Horizontal scan line (20), speed 2.** One row at a time: a tenth of the front display, a quarter of a 4-row
  rear one, where an inner row is lit again 150 ms after it was at speed 0. At speed 2 the line moves every
  150 ms and the closest two are 450 ms apart.
- **Fire (22), speed 1.** The stock code never resets its blend counter, so after its first second the whole
  field is rolled again at every update: every 100 ms at speed 0, every 200 ms at speed 1.

Notes:

- **Alarm (1)** is steady only as the contract starts it: with the default colour both halves of its flip are
  the same palette. **Red alert and the mic effects (11, 12, 13)** are steady because no microphone source is
  wired to the engine. A source wired there (R2's level feed) changes all three, and this table has to be done
  again for them.
- **Fade and scroll (104)** passes the count in its worst case (one colour, every pixel in phase, its fastest
  speed: the field lights every 510 ms). It is off the list because its palette, pattern and speed are drawn at
  random each time it starts, and a host cannot ask for a particular one.
- **Which sequences are steady.** The whole field of 0, 1, 4, 5, 7, 8 and 11 to 14 stays within a tenth of the
  level the gate is told (`nativeSteadyLogics()`). After any other sequence the gate takes the field as dark and
  just lit, so a contract look that follows waits 340 ms, and a native look that follows comes through effect
  106, which holds the last frame meanwhile.

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
- **S9: measure, ask, draw, tell.** A frame effect 106 is about to draw is measured first, without writing a
  LED (`_draw()` runs twice a frame: level, luminance and the brightest LED), the gate is asked
  (`contractFieldAsk()`), and a frame it would scale is not drawn: the frame that is up stays. A frame scaled
  down keeps the field's level legal and still lights, dimly, every part of the display the frame lights. A
  frame the gate passes is drawn as it is, and the gate is told (`contractFieldTell()`). Light the layer does
  not draw (a native look, at the brightness
  the contract gives it) is ASKED for before the hand-over and TOLD to the gate at the frame the engine starts
  it. The gate is never told about light that did not reach the LEDs.
- **S4 at the output.** `ContractRender` asks for every frame's look (effect and colour) with
  `contractLookAllowedAt()`, by the frame's brightest LED (under 4 levels it is not a look), and leaves the frame
  that is up when the answer is no. The gate is taken (`contractLookGateAt()`) only by a frame that is shown,
  so its 170 ms run from the frame in which the look could be seen. A live `A` or a section that
  changes the look (`contractLookSame()` says no: another effect, native code, colour or speed) restarts the
  look's timeline and calls `contractLookRestart()`, so the next lit frame is a change of look. One that asks
  for the look that is up leaves its timeline alone (S11): the look goes on from where it was.
- **S10: a native sequence runs as stock.** Its frames are never held. The layer asks before it hands the
  display over (`contractNativeMayStart()`: yes only when an onset is allowed, whether the display is lit or dark
  just then, because a stock sequence may clear the display and light it at once; the same for the hand-back to
  the display's own look), tells the gate at the frame the engine starts the sequence
  (`contractNativeStarted()`, with the level it runs at: the engine's brightness, 0 for lights-out, full scale for
  a sequence that draws at a level of its own) and tells it when effect 106 takes the display back
  (`contractNativeEnded()`). A sequence whose light is not steady (`nativeSteadyLogics()` says no: it is off the
  allow-list, draws at a level of its own, or moves its own light) leaves the gate knowing nothing about the
  field: it is then taken as dark and just lit, so the contract's next light waits 340 ms, and a native look that
  is to follow it comes through effect 106, which holds the last frame meanwhile. A brighter level reaches such a
  sequence only when it next starts. The owner's own sequences (not on the list) keep the owner's brightness; `B`
  does not reach them.
- **The contract selects a native sequence at the core's speed** (`nativeSpeedLogics()`: 1 for march, roaming
  pixel and fire, 2 for the horizontal scan line, 0 for the rest). `_select()` is the one place that does it.
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
