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
  Every value is an integer, and since CA1.3 every one is exact: the core has no float, so a mirror is compared
  with no tolerance. `phase16` and `progress16` are Q16 (a fraction times 65536, rounded down).

Groups are append-only: a later change adds a group, it never reshapes one. There has been one exception: at
CA1.3 the last `out` column of `beatPosAt` and `beatPosAtReseed` changed from `phase_e6` (a float, times a million)
to `phase16`, when the phase itself stopped being a float.

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
never delayed, a slow rise, a trough that is not dark); the step from both sides; power-on;
the `millis()` wrap; resets inside the window, one with a new ceiling; the same script at twelve ceilings from 0
to 255; and three long walks from a fixed generator. A later stateful group follows the same shape: `seq` first,
rows in order, the state after each call in `out`.

CA1.4 changed this group's rows, not its shape: the step is a tenth of the ceiling where it was a quarter, the
hand-written levels around the step moved with it, and three ceilings (9, 10, 19) joined the twelve.

`contractFrameGate` (CA1.4) is the second stateful group: one frame through the look gate and the two-measure
field gate, the way a fork that draws colour runs them.

- `args`: `seq`, `ceil`, `restart`, `nowMs`, `effect`, `rgb`, `level`, `lum`. `out`: `drawn`, `scale`, `lit`, `ref`,
  `litY`, `refY`, `onsetMs`, `lookMs`.
- Each row is one frame. `effect` is the `ContractEffect` number and `rgb` the look's colour as one number
  (`0xRRGGBB`); `level` and `lum` are the frame's two measures as drawn. `effect` 0 or `level` 0 is a black frame.
- `restart` = 1: `contractLookRestart(state)` runs first.
- `drawn = (effect == 0 or level == 0) or contractLookGate(state, contractLookSig(effect, rgb), nowMs)`.
  A frame that is not drawn changes nothing more, and its `scale` is 255.
- A drawn frame: `scale = contractFieldScale(state, level, lum, nowMs)`.
- The rest of `out` is the state after the row: `fieldLit`, `fieldRef`, `fieldLitY`, `fieldRefY`, `fieldOnsetMs`,
  `lookMs`.

Its sequences: white and blue by turns at one level; red and blue by turns; a look that fades in; a black frame
between two looks; the same look applied again; a frame one measure holds down; three long walks.

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

## Core revision CA1.3 (2026-11-16): the beat clock in integers

There is no float left in `contract_core.h`. The reason is the 8-bit forks: one float expression links the
compiler's soft-float library, about 1.3 KB of a 28 KB part. On the probe programs (avr-gcc 7.3, atmega32u4, the
whole core in one image) CA1.3 is 1,448 B of flash smaller than CA1.2, with the same static RAM. Stage 1h of the
host suite fails if a float comes back into the core or into the logics layer.

### What changed in the API

| Was | Is |
| --- | --- |
| `BeatPos.phase` (`float`, 0 to 1) | `BeatPos.phase16` (`uint16_t`, Q16: 0 at the beat, 65535 just before the next) |
| `float beatSectionProgress(beat, start, end)` (the raw ratio) | `uint32_t beatSectionProgress16(beat, start, end)` (Q16, clamped to 0 .. 65536) |
| `beatAccentAmount(am, bp, m, float progress, bc, every)` | `beatAccentAmount(am, bp, m, uint32_t progress16, bc, every)` |
| `#include <math.h>` in the core | gone |

`beatPosAt()`, `beatClockSeed()`, `accentEdgeAfterSeed()`, `beatAccentFires()` and `envBright()` keep their
signatures. New: `BEAT_UNITS` (60000) and `_beatSplit(bc, nowMs, unitsInto)`. The field and the function were
renamed on purpose: a layer that still reads `bp.phase` or holds the progress in a `float` stops compiling, instead
of compiling and keeping the soft-float library linked.

A fork's layer changes two things: it passes `beatSectionProgress16(...)` to `beatAccentAmount()`, and anything of
its own that read `bp.phase` reads `bp.phase16` (a fraction `f` of a beat is `phase16 >= f * 65536`).

### The definitions, for a mirror

All values are integers; `floor(a / b)` rounds towards minus infinity; `>> 16` is `floor(x / 65536)`. No
intermediate value needs more than 32 bits.

**The clock.** One beat is 60000 units, where a unit is one millisecond times the tempo (`ms * bpm`).

```text
delta   = (nowMs - anchorMs) as a signed 32-bit number      (the uint32 difference, wrapped)
minutes = floor(delta / 60000)
ms      = delta - minutes * 60000                           0 .. 59999
t       = ms * bpm
beats   = minutes * bpm + floor(t / 60000)
units   = t mod 60000                                       0 .. 59999

beatIndex = beatOffset + beats
phase16   = floor(units * 65536 / 60000)                    0 .. 65535
barPos    = beatIndex mod bpb, floored                      0 .. bpb - 1
```

This is exact for any `nowMs` within 2^31 ms (24.8 days) of the anchor, either way and across the `millis()`
wrap. Beat `k` begins at the first whole millisecond at or after its true instant: never early, under a
millisecond late, at ten minutes, at an hour and at 24 days alike.

**Nudge or seek** (a bare `C` on a running clock). With `(beatsA, unitsA)` the position before the line and
`(beatsB, unitsB)` after it, both at the moment the line arrived, and `d = beatIndexB - beatIndexA`: it is a nudge
when `d == 0`, or `d == 1 and unitsB < unitsA`, or `d == -1 and unitsB > unitsA`. Otherwise a seek.

**The accent-edge guard** (C1). The current beat counts as already accented when
`units >= (ACCENT_EDGE_LATE_MS + 1) * bpm`, that is `units >= 61 * bpm`: more than 60 whole milliseconds into it.

**The section progress.**

```text
if sectionEnd <= sectionStart or beatIndex <= sectionStart:  progress16 = 0
else if beatIndex >= sectionEnd:                             progress16 = 65536
else:
    n = beatIndex - sectionStart,  d = sectionEnd - sectionStart
    while d > 65535:  n = floor(n / 2),  d = floor(d / 2)
    progress16 = floor(n * 65536 / d)
```

**The envelope** (`beatAccentAmount`, when the beat fires and `m > 0`), with `p = phase16`:

```text
env = 65536 + ((p * p) >> 16) - 2 * p                        this is floor((65536 - p)^2 / 65536)
if barPos == 0:   env = min(65536, env + ((env * 19661) >> 16))
if am == 3 and progress16 < 65536:   env = (env * progress16) >> 16
amount = (env * m) >> 16
```

`envBright()` was already integer and is unchanged. Against the float envelope it replaces, at the same phase,
the amount is never more than 1 away, and it is the same value 999 times in 1000.

### The vector file

`beatPosAt` and `beatPosAtReseed` carry `phase16`. Five groups are new, one for each step above, so a mirror can
be pinned piece by piece: `beatPosAtLong` (up to 24 days after the seed and across the wrap, with `units`),
`beatSectionProgress16`, `beatAccentAmount` (the envelope from its integer inputs alone), `beatClockSeedKind`
(1 a seek, 2 a nudge) and `accentEdgeAfterSeed` (`counted`, and the beat). `accentAt` did not change by a single
value; `accentAtEvery` changed by 1 in 7 of its 1800 rows.

## Core revision CA1.4 (2026-11-16): colour, every path, and native looks

CA1.4 closes the second review of R1 and takes in two spec changes: the gate's step is a tenth of the ceiling
(S9), and native looks have a rule of their own (S10). Unlike CA1.2 and CA1.3 it gives a fork work to do.

### What changed

| Was | Is |
| --- | --- |
| `contractFieldStep(ceil)` = `ceil / 4 + 1` | `ceil / 10 + 1`, and never under 2 |
| one measure: the field level | two: the level, and the luminance (`contractLuminance(r, g, b)` per LED, then the mean) |
| `contractFieldGate(state, level, now)` for every frame | still there, for one measure; a colour fork uses `contractFieldAsk()` / `contractFieldTell()`, or `contractFieldScale()` |
| S4 held live `A` lines only | `contractLookGate()` at the frame, `contractSectionMayApply()` for sections, `contractLookRestart()` |
| a native look told with `contractFieldGate()` | `contractNativeMayStart()`, `contractNativeStarted()`, `contractNativeLevel()`, `contractNativeEnded()` |
| the safety anchors listed one by one in `contractShowReset()` | `struct ContractAnchors`, the base of `ContractUnitState`: the reset keeps it as one piece |
| logics allow-list 0 to 2, 4 to 14, 19 to 24, 101 to 105 | 0, 1, 4, 5, 7 to 14, 19 to 21, 23, 24, 101, 105 |

New in the vector file's constants: `NATIVE_DIMS_LOGICS` (the logics codes `b=` and `B` reach),
`NATIVE_GATED_LOGICS`, `NATIVE_GATED_PSI`, `NATIVE_GATED_FLTHY` (S10: which codes a fork gates frame by frame),
`FIELD_STEP_DIV` and `FIELD_STEP_MIN`. New group: `contractFrameGate` (above).

### The rule, for a mirror

The field gate keeps, for each of the two measures, `lit` and `ref` (lit: the brightest value since it lit; not
lit: the darkest since it fell), and one time for both, `onsetMs`. With `step = max(2, floor(ceil / 10) + 1)` and
`open = (now - onsetMs) >= 340`, decided once a frame:

```text
ask(lit, ref, x):   x                    if lit or open or x < ref or x - ref < step
                    ref + step - 1       otherwise (held)

scale = min(255, floor(ask(level) * 255 / level), floor(ask(lum) * 255 / lum))     (a measure at 0 does not limit)

tell(lit, ref, x), for what the unit SHOWS (level and lum as scaled):
    lit:      x > ref            -> ref = x, brighter
              ref - x >= step    -> not lit, ref = x
    not lit:  x < ref            -> ref = x
              x - ref >= step    -> lit, ref = x, brighter

brighter in either measure  ->  onsetMs = now  and  lookMs = now
```

`contractFieldScale()` asks, scales the two numbers (`floor(x * scale / 255)`) and tells. A fork that measures the
scaled frame again asks, scales, measures and tells: per-LED rounding can leave the frame a level under the
computed one, and the gate must hear what is on the LEDs. `contractFieldGate(state, level, now)` is the same rule
with `lum = level`, for a fork that then shows the allowed level itself.

The look gate keeps `lookSig` and `lookMs`. A look is `(effect << 24) | rgb`, or `(CE_NATIVE << 24) | code` for
a native one. A lit frame with another signature than `lookSig` is drawn only when `now - lookMs >= 170`; then
`lookSig` becomes its signature and `lookMs = now`. `contractLookRestart()` sets `lookSig` to 0, so the next lit
frame is a change whatever it shows. `lookMs` also moves whenever the field lit or got brighter (above): a look
that fades in is still arriving.

### What a fork has to do

1. **Measure twice, ask, tell.** Where the fork called `contractFieldGate()` on a frame it drew, it now measures
   the frame's level and its luminance and calls `contractFieldAsk()`; when the scale is under 255 it scales the
   frame; then `contractFieldTell()` with the two measures of what is on the LEDs. A fork with no spare pass to
   measure the scaled frame calls `contractFieldScale()` instead. A fork whose LEDs have one colour keeps
   `contractFieldGate()`.
2. **The look gate, at the frame.** Before a lit frame is drawn:
   `if (!contractLookGate(state, contractLookSig(effect, colour), now))` leave the frame that is up. Black frames
   are not asked: a blackout is never held. The accent overlay is a look like any other, with its own effect and
   colour.
3. **Restart.** Where the layer sets the look's start time (a live `A` applied, a section applied), it calls
   `contractLookRestart(state)`.
4. **Sections.** The tick applies a section only when `contractSectionMayApply(state, now)` says yes; a section
   that has to wait is taken by a later tick if the beat is still in it. (A `C` seek can move the beat between two
   sections every few milliseconds.)
5. **Native looks (S10).** Each allow-listed code is one of two kinds, and `nativeGated<Board>(code)` says which.
   - **Runs as stock.** The fork does not pass its frames through the gate, and does not report them: the
     gate is told once, when the look starts, the level it runs at. It calls
     `contractNativeMayStart(state, code, level, now)` before it hands the LEDs over and waits a pass on a no.
     The answer is yes only when an onset is allowed (340 ms after the field last lit or got brighter) and a
     change of look is (170 ms), whether the unit is lit or dark just then: a stock look may clear the unit and
     light it at once, or sit in a dark stretch of its own and relight in its own time, and to anyone watching
     that light follows the contract's last. The same call hands a unit back to the board's own idle look. Then
     `contractNativeStarted(state, code, level, now)` at the frame the look is on the LEDs;
     `contractNativeLevel(state, level, now)` when the fork moves the look's brightness (asking
     `contractFieldAllowed()` first for a rise it can hold back); and `contractNativeEnded(state, known, now)`
     when the fork's own renderer takes the LEDs back. `level` is 0 for a native blackout, the brightness the
     fork gives the look, or the ceiling. `known` is false for a look that is not on the allow-list (a button
     started it) or whose level the fork cannot say: the field is then taken as dark and just lit, and the fork's
     next light waits 340 ms. Do not call `contractNativeEnded()` for a steady idle look: a show starts from it.
   - **Gated.** The fork measures each frame of the look and passes it through the field gate like a look of
     the contract's. It still asks `contractNativeMayStart()` before and calls
     `contractLookForced(state, contractLookSigNative(code), now)` when the look starts.
   The core has one obvious call for each edge of the first kind; the second kind keeps the frame calls of item 1.
6. **A button after a wait.** A hand-over the gate held can come due in the pass after someone pressed a button.
   The logics layer does not select over a selection that is waiting in the engine; each fork needs the same care
   in its own terms. An `X` is the exception: it always lands.
   Two more things a fork's layer has to get right by itself, because the core cannot see them: verb `L` feeds
   the contract's own `meter` look and no native mode; and the hand-back to the board's own look goes through
   `contractNativeMayStart()` like any native start, from a lit unit as from a dark one.
7. **The property stage.** Each fork's copy of `test_contract_property.cpp` gains the second photometer
   (luminance as the gate defines it, and the colour observers), the colour-flip scripts, the 1 to 4 ms passes and
   whatever stock looks of the fork flash by themselves in its mock.

### What the gate costs a look

`test/host/gate_held_table.cpp` measures it: every look the contract draws, by itself for 60 s, at five speeds,
and the beat pump at four tempi. The share of frames the gate scaled down, and by how much on average:

```text
c++ -std=c++17 -O1 -I test/host test/host/gate_held_table.cpp -o /tmp/held && /tmp/held
```

On the logics mock (both displays 8 by 10 there), with the tenth step:

| Look | Held back |
| ---- | --------- |
| solid, flash, rainbow, scan, meter, comet, gradient, colorcycle, twinkle | never, at any speed |
| the beat pump on a solid look (`am=1`, `m=` 64 to 255, 60 to 240 BPM) | never |
| sparkle | 0.1 to 1.5 % of frames, by 4 to 12 % of the frame's level |
| chase, `s=` 96 and up | 33 to 55 % of frames, by 6 to 7 % |
| pulse, `s=` 224 / 240 / 255 | 6 % / 20 % / 44 % of frames, by 35 % / 61 % / 73 % |
| wipe, `s=` 240 / 255 | 1.4 % / 14.6 % of frames, by 20 % / 45 % |

With the quarter step the same run held nothing but the fastest pulse (35 % of frames) and the fastest wipe (2 %).
What the tenth step adds:

- **Chase.** On 8 columns the pattern lights 3, 3, then 2 of them, so the field mean moves by an eighth of the
  ceiling at every shift, which is more than a step. The gate takes about 7 % off the frames with 3 lit. On a
  rear display 24 columns wide the count is always 8 and nothing is held. The fix is in how chase is drawn (a lit
  count that does not change on this width), which is a change to a look and is proposed, not made.
- **Pulse and wipe at their top speeds** light the whole field again less than 340 ms after it last got
  brighter. That is S9 doing its work; the answer is a longer shortest cycle for both in the shared timing
  (`fxPulseBright`, `fxWipeLit`), also proposed, not made.
- **Sparkle** is touched on a frame in a hundred.

### What it costs in flash

The probe programs again (avr-gcc 7.3, atmega32u4, link-time optimised, the whole core in one image):

| Image | CA1.3 | CA1.4 |
| ----- | ----- | ----- |
| everything a single-colour layer calls (`contractFieldGate()` only) | 8,602 B | 8,700 B |
| `contractShowReset()` by itself | 322 B | 230 B (476 B with the nine anchors listed one by one) |

A colour fork's layer, shaped like the PSI's (the calls above plus its native asks and tells), step by step:

| Wired | Flash | Step |
| ----- | ----- | ---- |
| CA1.3 core | 8,984 B | |
| CA1.4 core, the same calls | 9,098 B | +114 B |
| two measures: `contractFieldAsk()` and `contractFieldTell()` for `contractFieldGate()` | 9,308 B | +210 B |
| the look gate at the frame | 9,464 B | +156 B |
| `contractSectionMayApply()` and `contractLookRestart()` | 9,710 B | +246 B |
| the four native calls for the fork's own | 9,958 B | +248 B |

974 B in all for a colour fork that wires everything, on a part with 28 KB. The step itself costs nothing. The
steps are what the linker made of each addition in this one program; a real layer will divide them differently.
Marking the gate's functions `noinline`, or sharing the ask between its two callers by hand, made every one of
these images larger: the compiler's own choices under `-Os -flto` are better. The larger functions left in the
core, should a fork need room: `_applyParam` 874 B, `beatAccentFires` 336 B, `_effectFromName` 324 B,
`contractFormatQuery` 306 B, `scoreInsertEx` 220 B.

Static RAM: 10 B more a unit (the luminance pair and the look gate's two words), 75 B to 85 B on that part.
