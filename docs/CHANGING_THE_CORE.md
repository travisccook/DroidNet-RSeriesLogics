# Changing the shared contract core

Seven files are byte-identical in the three DroidNet board forks (RSeries Logics, PSI Pro, Flthy HPs). They are
listed in `test/host/core-files.txt`:

- `src/contract/contract_core.h`: the wire parser, the beat clock, the safety limits, the score, the effect math.
- `test/host/test_contract_core.cpp`, `test/host/test_contract_v13.cpp`, `test/host/fuzz_parsers.cpp`,
  `test/host/test_fx_regions.cpp`: its tests.
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

## Core revision CA1.5 (2026-11-16): the revision all three forks and Studio adopt

### Everything that changed, in one place

For the Flthy fork and Studio, coming from CA1.2, and for the PSI fork, coming from CA1.3. A row marked CA1.3 is
already in the PSI's copy. "Shape" means a group's `args` or `out` changed; "rows" means only its values did.

**API.**

| Since | Was | Is |
| ----- | --- | --- |
| CA1.3 | `BeatPos.phase` (`float`) | `BeatPos.phase16` (`uint16_t`, Q16) |
| CA1.3 | `float beatSectionProgress(beat, start, end)` | `uint32_t beatSectionProgress16(beat, start, end)` (Q16, 0 .. 65536) |
| CA1.3 | `beatAccentAmount(am, bp, m, float progress, bc, every)` | `beatAccentAmount(am, bp, m, uint32_t progress16, bc, every)` |
| CA1.3 | `#include <math.h>` in the core | gone; new `BEAT_UNITS`, `_beatSplit()` |
| CA1.4 | `contractFieldStep(ceil)` = `ceil / 4 + 1` | `ceil / 10 + 1`, never under 2 |
| CA1.4 | one measure of a frame | two: level and luminance. New `contractLuminance()`, `contractFieldAsk()`, `contractFieldTell()`, `contractFieldScale()`; `contractFieldGate()`, `contractFieldAllowed()`, `contractFieldForced()` and `contractFieldMayLight()` stay |
| CA1.4 | no look gate | `contractLookSig()`, `contractLookSigNative()`, `contractLookAllowed()`, `contractLookGate()`, `contractLookForced()`, `contractLookRestart()`, `contractSectionMayApply()` |
| CA1.4 | a native look told with `contractFieldGate()` | `contractNativeMayStart()`, `contractNativeStarted()`, `contractNativeLevel()`, `contractNativeEnded()` |
| CA1.4 | no list of kinds | `nativeDimsLogics()`, `nativeGatedLogics()`, `nativeGatedPsi()`, `nativeGatedFlthy()` |
| CA1.4 | `ContractUnitState` holds its anchors among its fields | `struct ContractAnchors` is its base; four new anchors: `fieldRefY`, `fieldLitY`, `lookSig`, `lookMs` |
| CA1.5 | a live `A` or a section always restarts the look | `contractLookSame()`: the same look applied again does not (S11) |
| CA1.5 | `fxChaseLit(p, elapsed, speed)` | `fxChaseLit(p, elapsed, speed, N)`: a constant lit count, `ceil(N / 3)` |
| CA1.5 | a wipe step is `fxStepMs(speed)` | `fxWipeStepMs(speed, N)`: a cycle of `2 * N` steps is never under `FX_CYCLE_MIN_MS` |
| CA1.5 | `fxPulsePeriodMs()` runs 2000 .. 340 ms | 2000 .. 680 ms (`FX_CYCLE_MIN_MS`) |
| CA1.5 | `fxFlashHalfMs()` runs 425 .. 170 ms | 445 .. 190 ms (`FX_FLASH_MIN_STATE_MS`) |
| CA1.5 | no speed for a native look | `nativeSpeedLogics()`; `nativeSteadyLogics()` |

**The native lists.**

| | CA1.2 and CA1.3 | CA1.4 | CA1.5 |
| --- | --- | --- | --- |
| logics | 0 to 2, 4 to 14, 19 to 24, 101 to 105 | 0, 1, 4, 5, 7 to 14, 19 to 21, 23, 24, 101, 105 | 0 to 2, 4 to 14, 19 to 24, 101, 105 |
| PSI | 0, 1, 4, 6 to 22, 92 | the same | the same |
| Flthy | 1, 6 | the same | the same |

**Constants in the vector file.** New since CA1.2 and CA1.3: `NATIVE_DIMS_LOGICS`, `NATIVE_GATED_LOGICS`,
`NATIVE_GATED_PSI`, `NATIVE_GATED_FLTHY`, `FIELD_STEP_DIV`, `FIELD_STEP_MIN` (CA1.4); `NATIVE_SPEED_LOGICS`,
`FX_CYCLE_MIN_MS`, `FX_FLASH_MIN_STATE_MS` (CA1.5). Changed: `NATIVE_ALLOW_LOGICS`.
`NATIVE_SPEED_LOGICS` is in the order of `NATIVE_ALLOW_LOGICS`: its n-th number is the speed of the n-th code.

**Vector groups.**

| Group | Since CA1.2 | Since CA1.3 |
| ----- | ----------- | ----------- |
| `beatPosAt`, `beatPosAtReseed` | shape: `phase_e6` is now `phase16` | the same as the PSI has |
| `accentAtEvery` | rows (7 of 1800 moved by 1) | the same |
| `beatPosAtLong`, `beatSectionProgress16`, `beatAccentAmount`, `beatClockSeedKind`, `accentEdgeAfterSeed` | new | the same |
| `contractFieldGate` | new (added after CA1.2), stateful | rows: the tenth step |
| `contractFrameGate` | new, stateful | new |
| `fxChaseLit` | shape: `N` is a fourth argument | shape |
| `fxWipeLit`, `fxPulseBright`, `fxFlashOn` | rows | rows |

Nothing else in the file changed: the `lines` and every other group are as they were at CA1.2.

### What CA1.5 itself is

1. **S10 by onsets.** The test for a native look is S9's own measure: no two onsets closer than 340 ms, by
   brightness and by luminance, over the whole field and over any region of a quarter of it or more. On the logics
   that puts failure, flash colour and fire back on the list. Where a stock sequence's rate follows the engine's
   speed digit, the contract starts it at the fastest speed that passes: `nativeSpeedLogics()` (march 1, roaming
   pixel 1, horizontal scan line 2, fire 1, the rest 0). `test/host/test_native_s10.cpp` (stage 1i) is the
   classification kept as a test, on a model of the stock code; `DROIDNET_FORK.md` has its table.
2. **A look whose own light moves.** `nativeSteadyLogics()` lists the codes whose whole field stays within a
   tenth of the level the gate is told. After any other look the layer passes `known = false` to
   `contractNativeEnded()`, and it gives such a look a brighter `B` or `b=` only when the look starts; a dimmer one
   is at once. A fork with native looks of its own needs the same two decisions for each.
3. **S11: the same look applied again does not restart.** Before a live `A` or a section is written into the
   unit, `contractLookSame(state, effect, nativeCode, colour, speed)`. When it says no, the fork sets the look's
   start time and calls `contractLookRestart()`; when it says yes, it does neither.
4. **Chase** lights `ceil(N / 3)` positions at every step, on any `N`: every third position of a ring of `N`. A
   fork passes its strand length as the fourth argument. Where `N` is a multiple of 3 nothing changed.
5. **Pulse, wipe and flash** have longer shortest cycles, so that the S9 gate lets them through as drawn:
   680 ms for pulse and for a wipe's fill and drain (`FX_CYCLE_MIN_MS`), and 190 ms a state for flash
   (`FX_FLASH_MIN_STATE_MS`). The gate lets a field light again 340 ms after it last got brighter, which is a
   cycle's peak, not its start. And a look that lights exactly every 340 ms has no room for a late frame: one
   held edge moves the next later still, until a whole flash is lost.

### What the gate costs a look, after CA1.5

`test/host/gate_held_table.cpp`, 60 s a look, on the logics mock, with frames 10 ms apart and with frames 10 to
15 ms apart. One look is still touched:

| Look | Held back |
| ---- | --------- |
| solid, flash, pulse, rainbow, scan, meter, comet, chase, wipe, gradient, colorcycle, twinkle | never, at any speed |
| the beat pump on a solid look (`am=1`, `m=` 64 to 255, 60 to 240 BPM) | never |
| sparkle | 0.1 to 1.7 % of frames from `s=` 128 up, by 5 to 6 % of the frame's level |

With frames up to 40 ms apart the table is the same (sparkle: up to 1.9 %). How the three cycles were chosen, at
`s=255`:

| Shortest cycle | Frames 10 ms | 10 to 15 ms | 10 to 20 ms | 10 to 40 ms |
| -------------- | ------------ | ----------- | ----------- | ----------- |
| pulse, wipe: 600 ms | 0 % | 0.7 % | | |
| pulse, wipe: 620 ms | 0 % | 0.1 % (wipe) | | |
| pulse, wipe: 640 ms | 0 % | 0 % | 0 % | 0.2 % (wipe) |
| pulse, wipe: 680 ms | 0 % | 0 % | 0 % | 0 % |
| flash: 170 ms a state | 0 % | 25 % | 23 % | 23 % |
| flash: 180 ms a state | 0 % | 0 % | 0 % | 0.7 % |
| flash: 190 ms a state | 0 % | 0 % | 0 % | 0 % |

### What it costs in flash

The probe programs (avr-gcc 7.3, atmega32u4, link-time optimised). CA1.5 adds nothing to the three images of the
CA1.4 table: 8,700 B for a single-colour layer, 9,098 B and 9,958 B for the colour fork's first and last rows. A
fork that draws chase and wipe pays for the ring and for the stretched step: a probe that draws chase, wipe, pulse
and flash on a strand is 924 B at CA1.4 and 1,018 B at CA1.5. Doing that arithmetic in 16 bits made it 1,030 B,
and reading the 0 to 255 value once in `_applyParam` made one image 78 B smaller and two others 48 B and 106 B
larger; neither was kept.

## Core revision CA1.6 (2026-11-16): coloured fades, and a part of the field

### What a fork changes

Re-copy the six shared files. After that:

| | What | A fork has to |
| --- | --- | --- |
| 1 | One rise is one onset: a coloured look that fades in is no longer held when its luminance crosses the step after its brightest channel did | nothing. One more byte of state a unit (`fieldRise`) |
| 2 | Verb `L`: a higher level is taken at most once in 340 ms a unit | wire two calls and read one field (below). Six bytes of state a unit |
| 3 | A comet's lap is never shorter than 680 ms | nothing. On a strand under 23 positions the comet is slower at its top speeds |
| 4 | A frame whose brightest LED is under 4 levels is not a look | call `contractLookAllowedAt()` / `contractLookGateAt()` with the frame's brightest LED in place of the two calls without `At`. Optional: the old calls behave as before |
| 5 | A native look can be told with two figures | pass the look's luminance too where it is well under its level (`contractNativeStarted(s, code, level, lum, now)`, `contractNativeLevel(s, level, lum, now)`). Optional |
| 6 | The show reset assigns the show state and does not touch the anchors | nothing, unless it names `ContractUnitState`'s base: there are two now, `ContractAnchors` and `ContractShowState`. Every field is reached as before |
| 7 | A held frame is not shown (this fork's layer, not the core) | decide (below, "A held frame") |

**Verb L.** In the verb's handler, `contractLevelOffer(state, v)`. Once a pass, in the tick or just before the
meter is drawn, `contractLevelDue(state, now)`. The meter reads `state.level` in place of the fork's own copy of
`v`. `contractLevelDue()` returns true when `state.level` changed, for a fork that derives something from it (the
PSI's rows). A fork that wants a fall on the LEDs before its next pass calls it right after the offer as well, as
this fork does. The show reset clears the level and what the host last asked for; the time of the last rise is an
anchor.

### Everything that changed

**API.**

| Was | Is |
| --- | --- |
| the two measures of the field gate share one time, and each lights by itself | they still share it; `fieldRise` (an anchor): a frame in which a measure lights and none falls begins a rise, a fall ends it, and while it lasts the other measure may light without waiting. `contractFieldAsk()` reads it, `contractFieldTell()` keeps it, `contractNativeEnded(s, false, now)` clears it. `contractFieldAllowed()` does not read it: light a fork cannot hold back always waits for the window |
| the fork keeps the level verb `L` gave it | `contractLevelOffer()`, `contractLevelDue()`; `level`, `levelWant` in the unit, `levelMs` among the anchors |
| `fxHead()` steps every `fxStepMs(speed)` | every `fxCometStepMs(speed, N)`: a lap is never under `FX_CYCLE_MIN_MS` |
| every lit frame is a look | `LOOK_MIN_LEVEL` (4), `contractLookAllowedAt()`, `contractLookGateAt()` |
| `contractNativeStarted()` and `contractNativeLevel()` take one figure | they also take two (level, luminance) |
| `ContractUnitState : ContractAnchors` | `ContractUnitState : ContractAnchors, ContractShowState` |

**Constants in the vector file.** New: `NATIVE_STEADY_LOGICS` (the logics codes whose own light does not move:
Studio need not copy the list by hand any more).

**Vector groups.**

| Group | Change |
| ----- | ------ |
| `contractFrameGate` | shape: `rise` is a ninth output. New sequences 7 to 23: coloured fades from black and back at two ceilings, the same fade begun inside the window, white by a colour change and again after a blackout, a fall ending a rise, one measure lighting as the other falls. A frame under 4 levels is `drawn` without the look gate (`contractLookGateAt`) |
| `contractLevelHold` | new, stateful. One row is one pass of a loop: `reset` (the show reset first), `v` (an `L` line, or -1 for none), then `contractLevelDue()`. Outputs `changed`, `level`, `want`, `levelMs` |
| `fxHead` | rows: the comet's lap |

### One rise is one onset

The defect, as Studio's frame parity found it: `pulse` in `22c55e` stood at `05200f` from 180 ms to 500 ms and
then jumped to `116530`. A colour's brightest channel crosses the gate's step before its luminance does. The
first crossing was an onset and set the shared time; the second, a few frames later, was taken for a second onset
inside the first one's window. The held table measured in white, where the two cross together.

The rule now: a frame in which either measure lights, and neither falls, begins a rise. The rise lasts until a
measure falls (goes from lit to not lit). While it lasts, the measure that is not lit yet lights without waiting.
A frame in which one measure lights as the other falls begins no rise, or two looks trading places could hand it
to each other for ever.

Nothing changes for an observer of either measure: two of its onsets are never under 340 ms apart. Between two
onsets of one measure that measure fell, and the fall ended the rise. To light again inside the window it needs a
new rise, begun after the fall by an onset of the other measure that waited for the window itself: 340 ms after
the shared time, which is no earlier than the first measure's last peak. `test_contract_v13.cpp` holds it to
that on frames of ten colours fading at random rates, with every observer whose dark and lit are a step apart;
the property stage has narrow observers of luminance too, and mixed colours in its random lines.

A mirror keeps one more bit, `rise`. A unit that is lit in both measures needs no value for it: the first fall
clears it.

### What the gate costs a look, in five colours

`test/host/gate_held_table.cpp`, 60 s a look, both displays, frames 10 ms apart and 10 to 15 ms apart. The share
of frames held, at the look's slowest and fastest speed:

| Look | Colour | CA1.5 | CA1.6 |
| ---- | ------ | ----- | ----- |
| pulse | white, blue | 0 % | 0 % |
| pulse | red | 17 to 53 % | 0 % |
| pulse | green | 17 to 50 % | 0 % |
| pulse | `116530` | 17 to 52 % | 0 % |
| wipe | white, blue | 0 % | 0 % |
| wipe | red | 7 to 51 % | 0 % |
| wipe | green | 14 to 44 % | 0 % |
| wipe | `116530` | 4 to 21 % | 0 % |
| sparkle | white, red, blue | 0 to 1.7 % | 0 to 1.7 % |
| sparkle | green | 0.7 to 4.9 % | 0 to 1.7 % |
| sparkle | `116530` | 0 % | 0 % |
| solid, flash, rainbow, scan, meter, comet, chase, gradient, colorcycle, twinkle | all five | 0 % | 0 % |
| the beat pump on solid, white and `116530` | | 0 % | 0 % |

Blue never showed the defect: its luminance is under a step at any level.

### A part of the field

The field gate measures a unit's whole field. Three things move part of it while the whole stays legal.

**Verb L** (the PSI review's finding). `L:v=255` and `L:v=128` by turns lit the top half of a meter ten times a
second. `contractLevelOffer()` / `contractLevelDue()` take a higher level at most once in 340 ms; a level that is
no higher is taken at once.

**The comet.** Its lit head and tail lap the strand, and every part of the strand goes dark and lit once a lap.
On 8 columns at `s=255` a lap took 240 ms. A lap is now a cycle like a pulse's, 680 ms at the least
(`fxCometStepMs`). 340 ms keeps the comet alone to three a second; 680 ms leaves room for one hand-over in that
second.

**A held frame.** The gate's answer to a frame that may not light is a scale, and a frame scaled down keeps the
field's level under the step. It also lights every part of the field that the frame lights, dimly: a meter's top
half that was dark and is now at six tenths has lit. This fork's layer now does not show a held frame at all: it
measures the frame without writing a LED (the frame is drawn twice), asks, and draws only what the gate passes as
it is. The frame that is up stays up, as it does when the look gate holds one. The look gate is taken by a frame
that is shown, not by one that was asked about. A fork that scales held frames keeps the whole-field limit and
not this one. `contractFieldScale()` and the scale that `contractFieldAsk()` returns are unchanged.

The property stage watches regions of each display (the whole, the halves, the quarters, four bands of columns
and of rows a quarter of the field each) on the rendered pixels: a region is lit when its level reaches an
observer's "lit" and the pixels in it that are not dark number a quarter of the field or more, and it may light
three times in any one second. The rear display takes five sizes by turns (8 by 10, 28 by 4, 16 by 4, 32 by 8,
24 by 4). Asserted on every look by itself and on scripts made for it. The most often one region lit in a second,
each look by itself at `s=255`, front and rear (28 by 4):

| Look | CA1.5 | CA1.6 |
| ---- | ----- | ----- |
| flash | 3 / 3 | 3 / 3 |
| pulse, wipe | 2 / 2 | 2 / 2 |
| comet | 4 / 2 (two onsets 223 ms apart) | 2 / 2 |
| sparkle, meter, chase | 1 / 1 | 1 / 1 |
| scan | 0 / 0 (one column: under a quarter of the field) | 0 / 0 |

**Not fixed.** A comet and a full look by turns (`A:i=comet` for 510 ms, `A:i=solid` for 170 ms, again and
again) light a quarter of the field four times in a second: the comet passes it and the full look lights it
again, two onsets in one 680 ms cycle. The field's level only ever rises at the full look, from a field that is
lit, which no whole-field measure holds. The script is in the property stage as a known failure, and the random
streams, which mix it in, are measured for regions and not asserted (at most 4 of one region in a second).
Closing it takes a gate that knows regions, or a comet that never leaves a part of the field dark.

### Three notes from the Flthy fork

Its layer asks the gates where frames are latched, and a 36,000-stream soak on CA1.5 found these. All three are
in the core's comments now.

1. **A gated native look and the look gate.** A fork that asks the gates where its frames are latched does not
   call `contractLookForced()` when a gated native look is selected. It passes the look's signature
   (`contractLookSigNative(code)`) to `contractLookAllowedAt()` / `contractLookGateAt()` with the look's first lit
   frame, so the 170 ms run from the frame that is on the LEDs. `contractLookForced()` at selection is for a fork
   that cannot hold that first frame: called before the frame, it starts S4's window before the look can be seen.
2. **One figure or two.** `contractNativeStarted()` and `contractNativeLevel()` with one figure record it for both
   measures: right for white light, an upper bound for any other. CA1.6 adds the forms with two, and a fork with
   colour LEDs whose stock look has a luminance well under its level passes both from a frame it measured. This
   fork passes one: it does not know what colour the engine's sequences are, and the host mock draws them white.
   A contract look that follows a native one at the same level and a higher luminance is therefore no onset to
   the gate here. That is a bench item.
3. **The frame at which a look can be seen.** A look that comes up through a few levels has not changed the
   field's colour at its first frame. This fork had the fault too: a dim, slow pulse and another colour 171 ms
   after its first frame changed colour twice under 170 ms apart. The rule is in the core
   (`contractLookGateAt()`), by the frame's brightest LED and not by the field level: one full column of a
   28-column display is a field level of 3 and a colour anyone can see.

The soak's lesson for a harness: a gate that holds a frame, releases it and lets the look draw its next frame in
the same pass gives two frames one timestamp, and a photometer that samples once a pass misses one. Here the
engine draws at most one frame of a display in a pass (every 10 ms at most), and the property stage reads the
pixels after every pass, so it reads every frame.

### What it costs in flash

The probe programs (avr-gcc 7.3, atmega32u4, link-time optimised), CA1.5 and CA1.6:

| Probe | CA1.5 | CA1.6 |
| ----- | ----- | ----- |
| the whole core as a single-colour layer calls it | 8,700 B | 8,684 B |
| a colour fork, the CA1.2 calls | 9,098 B | 9,086 B |
| a colour fork, every gate call wired | 9,958 B | 9,966 B |
| the same with verb L wired (`contractLevelOffer`, `contractLevelDue`) | | + 108 B |
| a strand that draws a comet | 666 B | 728 B |

The rise costs 34 to 58 B by itself; the show reset that no longer saves the anchors gives 56 B back. The unit's
state is 86 bytes on AVR after the rise (85 before), and 92 with verb L's level and time.

### The frame fixtures

`test/host/export_logics_frames.cpp` writes the LED buffers Studio's dome preview is checked against, one file a
case, to `test/host/frames/`: the exporter Studio wrote against this fork's mock, with its format and file names
unchanged (the format is in the Signal Booster's `tests/fixtures/dome/frames/README.md`). Stage 2f builds it and
compares; `UPDATE_FRAMES=1 bash test/host/run.sh` writes the folder again, and Studio copies it. `source.commit`
holds the first twelve hex digits of the sha256 of `contract_core.h` followed by `ContractLogics.h`: a file cannot
name the commit that will contain it. Against the frames exported at CA1.5, three files moved: `pulse-s0`,
`pulse-s255` and `wipe`.

## Core revision CA1.7 (2026-11-16): a part of a strand, for every look by itself

### What a fork changes

Re-copy the shared files; there is one more, `test/host/test_fx_regions.cpp` (it is in `core-files.txt`), and
a fork's suite runs it. After that:

| | What | A fork has to |
| --- | --- | --- |
| 1 | A chase steps every 130 ms at the least (`fxChaseStepMs`) | nothing |
| 2 | A scan's lap is never under a second there and back, or 680 ms round a ring (`fxScanStepMs`) | nothing |
| 3 | A change of colour alone recolours a look in place | where it restarts a look itself: ask `contractLookContinues(state, effect, nativeCode, speed)` in place of `contractLookSame()`, and restart (the start time, `contractLookRestart()`) only on false |
| 4 | Verb `B`, and `b=` on a look, go up at most once in 340 ms, in the same window as verb `L` | write `contractBrightOffer(state, b)` wherever it wrote `state.bright = b` from a line (verb `B`, the `b=` of a live `A`, a section being applied), and call `contractHoldsDue(state, now)` once a pass in place of `contractLevelDue()` |

`contractLookSame()` is unchanged, for a fork that wants the stricter answer.

**Verb B.** From the PSI's re-verification: `A:i=meter`, `L:v=86`, then `B:b=0` and `B:b=200` every 30 ms lit
the meter's lower rows 17 times a second while the whole field stayed under the gate's step. It is verb L's hole
through another verb, and it has verb L's rule: a lower fraction at once, a higher one at most once in 340 ms,
the newest raise waiting for the window. The two verbs share ONE window (`levelMs`): a meter's rows light when
`L` goes up and when `B` does, and with a window each the two could take turns and light them twice in every
340 ms. `contractHoldsDue()` moves both in one call and returns what changed (bit 0 `state.level`, bit 1
`state.bright`); two raises due in the same pass are both taken, as one rise. `contractLevelDue()` and
`contractBrightDue()` are the same rule one at a time, for a fork that wants a fall on the LEDs before its next
pass. The show reset puts `bright` and `brightWant` back at the ceiling. A fork that held the `B` step itself
(the Flthy fork) can drop its own hold.

### Why

The third review ran the looks with no hostile line at all. A chase at 30 ms a step lights every position for
one step in three: each position, and each two side by side, blinked eleven times a second, and on the logics'
front display (8 columns) the left half swung between two and four lit columns 13 times a second. A scan at
30 ms a step took a quarter of that display from dark to half lit 6 times a second. The whole field's level
never moves in either, so no gate saw it; and the property stage's region observers, which allowed for a small
lit area and had wide bands, read 1 and 0.

`test/host/test_fx_regions.cpp` (stage 1j) is the test that keeps it from coming back. It is pure core: every
look the core times, alone, at every speed 0 to 255, on every strand of 6 to 28 positions, for 4 s; every run of
neighbouring positions a quarter of the strand or longer is a window; an onset is counted as the gate counts one
(a rise of more than a tenth of full scale above the last trough); no window may have more than three in any one
second. The most any window had, CA1.6 and CA1.7:

| Look | CA1.6 | CA1.7 |
| ---- | ----- | ----- |
| chase | 10 to 13 on every length | 3 |
| scan, there and back | 8 on 6 positions, 6 on 7 and 8, 4 or 5 on 9 to 15, 3 or fewer from 16 | 2 |
| scan, round a ring | 6 on 6 positions, 5 on 7 and 8, 4 on 9 to 11, 3 or fewer from 12 | 2 |
| comet, pulse | 2 | 2 |
| wipe | 2 (1 from 18 positions) | the same |
| flash, twinkle | 3 | 3 |
| gradient | 1 (by luminance) | 1 |
| colorcycle | 0 | 0 |

The steps at `s=255`, in ms:

| Positions | 6 | 7 | 8 | 10 | 12 | 16 | 18 | 24 | 28 |
| --------- | - | - | - | -- | -- | -- | -- | -- | -- |
| chase (was 30) | 130 | 130 | 130 | 130 | 130 | 130 | 130 | 130 | 130 |
| scan, there and back (was 30) | 100 | 84 | 72 | 56 | 46 | 34 | 30 | 30 | 30 |
| scan, round a ring (was 30) | 114 | 98 | 85 | 68 | 57 | 43 | 38 | 30 | 30 |
| comet (CA1.6) | 114 | 98 | 85 | 68 | 57 | 43 | 38 | 30 | 30 |

How the floors were worked out, rather than assumed:

- **Chase.** On a ring whose length is a multiple of 3 a window of two positions lights once in three steps, and
  112 ms a step keeps that to three a second. Where it is not, one gap of the ring is short, and the gap passes
  the window as well: a ring of 8 lights a quarter of itself three times in eight steps, so 125 ms a step is a
  second exactly. 130 leaves room for a frame that is drawn 15 ms late. One figure for every length: a chase is
  now 130 to 157 ms a step, and its speed setting does little.
- **Scan, there and back.** Every window is passed twice a lap, at uneven times unless it is in the middle, so
  four passes can fall inside little more than one lap. The least lap that keeps every window to three a second
  is 770 ms on 6 positions and up to 880 ms on 22; a second is the round figure above that. (680 ms, as for the
  comet, is not enough here.)
- **Scan, round a ring.** One pass a lap: 340 ms would do, and it gets the comet's 680 ms.

### A change of colour alone

`contractLookContinues()`: the same effect, native code and speed in another colour is the look that is up,
recoloured in place. Its timeline carries on, and the look gate times the change of colour as before (the colour
is part of `contractLookSig()`). A comet or a scan restarted by a change of one bit of its colour every 170 ms
passed the same quarter of the strand six times a second; it now laps as if nothing had been sent.

### What a fork's own region test should look like, and what stays open

This fork's property stage holds every look by itself to the fine observer on the rendered pixels (the whole
display, its halves and quarters, every band of a quarter of the columns and of the rows, by level and by
luminance), on five rear display sizes. Its own `sparkle` failed that (a new random pattern every 80 ms moved a
quarter of the display by more than a tenth five times a second) and now draws a new pattern every 350 to
477 ms.

Scripts that alternate two looks, each of which lights a part of the field, still exceed three a second in a
region while every whole-field limit holds. They are in the stage as known failures, with their figures:

| Script | A region lit, in one second |
| ------ | --------------------------- |
| a scan and a chase by turns every 170 ms | 6 times |
| a scan and a full look by turns every 170 ms | 6 times |
| `C` seeks across sections of solid, scan, comet and wipe | 5 times |
| a wipe with a wipe accent over it every 340 ms | 5 times |
| a comet and a full look by turns (510 ms and 170 ms) | 4 times |

A limit on them needs a gate with a state for each region.

### The vector file

New constants: `FX_CHASE_STEP_MIN_MS`, `FX_SCAN_LAP_MIN_MS`, `LOOK_MIN_LEVEL`. New group, stateful:
`contractHoldsDue` (one row is one pass: `reset`, an `L` line `v` or -1, a `B` line or `b=` as `b` or -1, then the
call; outputs `changed`, `level`, `levelWant`, `bright`, `brightWant`, `riseMs`). `contractLevelHold` is unchanged.
Groups: `fxChaseLit` and `fxScanPos` have new rows. `contractFrameGate` has a ninth argument, `peak` (the brightest
LED of the frame; equal to `level` in every row there was), and two new sequences, 24 and 25, where the two differ:
a field level of 3 with one LED at full is a look and is held; a field level of 3 with no LED over 3 is not.

### What it costs in flash

| Probe (avr-gcc 7.3, atmega32u4) | CA1.6 | CA1.7 |
| ------------------------------- | ----- | ----- |
| the whole core as a single-colour layer calls it (it draws a scan round a ring of 10) | 8,684 B | 8,716 B |
| a colour fork, every gate call wired, with a scan | 9,966 B | 9,998 B |
| a scan there and back on a strand of fixed length | 394 B | 412 B |
| the same with the length a variable | 428 B | 496 B |
| a strand that draws chase, wipe, pulse and flash | 1,018 B | 1,036 B |

A fork whose strand length is a constant pays 18 to 22 B for the scan's floor. Verb B's hold, on the colour
fork's probe with verb L wired already: 68 B more (three offers in place of three writes, and
`contractHoldsDue()` in place of `contractLevelDue()`); 154 B over a fork that has wired neither.

## Core revision CA2 (2026-11-21): contract v1.4, the ids `logic`, `projector` and `vu`

The core of contract v1.4 (spec `2026-11-21-driveable-animation-contract-v1.4-design.md` in the Signal Booster):
`CONTRACT_VERSION "1.4"`, `CONTRACT_RELEASE 2`, three effect ids and the math two of them need. No key, verb, accent,
score-entry field, `ContractParams` field or `ContractUnitState` field is new, and no existing function behaves
differently. Everything here is host-tested and cross-compiled; nothing has run on a board.

### What changed

| | What | Where |
| --- | --- | --- |
| 1 | `CE_LOGIC` (16), `CE_PROJECTOR` (17), `CE_VU` (18), appended, and their names in the effect table | `_effectFromName()` |
| 2 | none of the three is an accent: `ae=` naming one is no accent (the default of `accentEffectAllowed()`, now a test) | |
| 3 | `logic`'s colour (N1): `fxLogicOwn(c)`, `fxLogicHue(c)`; `FX_LOGIC_GREY` 32, `FX_LOGIC_HUE_BASE` 170 | |
| 4 | `logic`'s pace (N2): `fxLogicPace(s, x, floor)`; `FX_LOGIC_FADE_MIN` 1, `FX_LOGIC_DELAY_MIN` 10 | the region model kept them |
| 5 | `logic`'s pump: `fxLogicPumpFires()`, `fxLogicPumpAmount()`; `FX_LOGIC_PUMP_MIN_MS` 400 | not in the spec: below |
| 6 | `projector`'s row and its crumbling rule (N6, N7): `fxProjectorSlot(c, slot)`, `fxProjectorTake(cur, cand)`; `FX_PROJECTOR_STEP_MIN_MS` 50, `FX_PROJECTOR_STEP_MAX_MS` 149, `FX_PROJECTOR_BRIGHT_MS` 340 | |
| 7 | `vu` (N10): no function of its own. While pumped it offers `envBright(255, m, beatAccentAmount(...))` to the level hold | vector group `vuLevel` |
| 8 | the query reply says `ver=1.4,rel=2` | `contractFormatQuery()` |

Tests, first: `test_contract_v13.cpp` holds each function on its edges (a grey 31 and 32 apart, every anchor of the
hue wheel and one plain hue each side, `s` 0, 127, 128, 129 and 255 against fades 0, 1, 3, 255 and delays 0, 10, 40,
255, the ten slots of nine rows, `fxProjectorTake()` for every pair of the blue row, the pump against S2's choice at
every tempo), the three names and the new reply. The expected values were worked out from the spec's words by a model
written apart from the core. `fuzz_parsers.cpp`'s own name tables have the names, by hand. `test_fx_regions.cpp`
(stage 1j) holds `projector`'s step rule to three onsets a second in every window of a jewel (below). And a new
fork-local stage, 1k, `test/host/test_logic_regions.cpp`: the measure of the spec's section 9 for `logic`, on a model
of the walk.

### What a fork has to do

Re-copy the seven shared files (`core-files.txt`; `test_logic_regions.cpp` is not one of them). Then:

| Fork | What | A fork has to |
| --- | --- | --- |
| all | three new ids parse; a v1.3 line means what it meant | draw its own row of v1.4's table for each id (below), and say `contract.1.4` in its WDP caps (PSI: `contract.1.4s`) |
| PSI, Flthy | the stand-ins | PSI: `logic` and `projector` draw as `sparkle`, `vu` as `meter` wherever `CE_METER` is tested. Flthy: `logic` draws `twinkle`, `projector` its own look (Task F4; `sparkle` until then), `vu` `meter`; `FLTHY_FX_MAX` and the LED function range take the three codes |
| logics | `logic` and `vu` (Task F2) | pump `logic` with `fxLogicPumpAmount()` where every other look uses `beatAccentAmount()`; give the engine the level, not a fraction of 255 (below) |
| Flthy | `projector` (Task F4) | the step rule of N7 as `test_fx_regions.cpp` models it: the brightening step waits `FX_PROJECTOR_BRIGHT_MS` after the last one and after `levelMs`, and sets `levelMs` |
| all | the tests and scenario files that spell `ver=1.3,rel=1` or `contract.1.3` | spell `ver=1.4,rel=2` and `contract.1.4` |

This fork, at CA2, had the stand-ins only (`logic` drew `twinkle`, `projector` `sparkle`, `vu` `meter`) until Task F2
drew `logic` and `vu` itself; "The logics layer on CA2" at the end of this section says what it does now.

### Readings of the spec, and one rule it did not have

Where the spec could be read more than one way, or turned out short, the safe reading was taken. Each is written down
here, with its section.

1. **`logic`'s pump rises at most every 400 ms, not every 340 (v1.4 N3, N4 and section 9).** N4 says "the pump's own
   floor (S2) keeps its rises 340 ms apart". That is true of the level the layer asks for. It is not true of the light:
   N3 has the level reach each LED at that LED's next step, so the field takes each beat's level 0 to about 130 ms
   after the beat (a step of the walk is `fade + 1` frames; an LED in a key pause is later still), and by a different
   amount each beat. The model of the walk put four onsets into one second whenever the beats were 341 to about 380 ms
   apart: in a region by the fine reading on every display, and over the whole field by the gate's own count at the
   owner's brightness 255. It happened at every pace, the stock one included, and worse at the slowest, so section 9's
   lever, the floor of N2, cannot move it. The safe reading of section 9's rule ("three onsets or fewer") is to keep it:
   `fxLogicPumpFires()` is `beatAccentFires()` with the rate floor at `FX_LOGIC_PUMP_MIN_MS`. It never fires on a beat
   S2 would not, and in the tempo range 30 to 300 it differs only where a group is one beat (`am=2` with no `an=`,
   `am=3`, a downbeat mode with `bpb=1`) from 151 to 176 BPM, where it keeps every other beat. In a scan of 100 to
   300 BPM at both brightnesses and three paces, 360 ms failed and 380 ms passed (fades 1, 3 and 10); 400 ms passed
   that scan on four seeds, and with owners' fades up to 40 and delays up to 100. 400 leaves room for a frame drawn
   late. `fxLogicPumpAmount()` is `beatAccentAmount()` on the beats it fires, else 0.
   A consequence for the logics layer: a scored accent (`ae=`) over a `logic` section fires on S2's beats, the pump on
   these. From 151 to 176 BPM the accent can come on a beat the pump skips. The overlay is a look the layer draws and
   the gate measures, so this is a matter of looks, not of safety; the layer may choose to fire it on the pump's beats.
2. **"Measured the gate's way" (section 9), for a look drawn far under its ceiling.** The S9 gate counts a rise of a
   tenth of the ceiling. The stock blink at the front display's brightness 160 puts out no channel above 17 (the
   palette's value is scaled by the brightness when it is baked, again by `updateDisplay(fBri)`, and the rainbow
   conversion squares it), so by the gate's count it can never flash at all. The model holds `logic` to the gate's
   rule with the look's own full scale (its brightest LED, by each measure) in place of the ceiling, which is stricter,
   and to the gate's own count as well. A tenth of each series' own peak, stage 1i's reading, was tried and dropped: on
   a region whose peak is under two levels it counts quarter-level noise.
3. **`logic` reaches the engine as `updateDisplay(level)`, the level under the ceiling (N3).** The stock blink is
   `updateDisplay()`, which is `updateDisplay(fBri)`, so `logic` at `b=255` (level = ceiling = `fBri`) is the light
   of `native:0` exactly, as N1 and section 9 say. The plan's Task F2 has the engine's argument as `(level * 255) /
   ceil`, 255 at the ceiling: that draws the palette unscaled, brighter than `native:0` by the ratio of 255 to the
   display's brightness before the squaring, an onset at every hand-over from the idle blink. The model follows the
   spec; Task F2 should too.
   *Later (the 2026-11-21 review, R2-F2, and the owner's decision of 2026-11-24):* as first built, "the light of
   `native:0` exactly" held at `b=255` only. Under 255 `native:0` runs with the engine's brightness at the level,
   which bakes the palette at the level and scales it by the level again, while `logic` baked at the ceiling and
   scaled once (on the front at 160, LEDs summing to 159 against 28 at `b=128`). The layer now bakes `logic`'s
   palette at the level it draws, so the two are one light at every level, and **N4 holds as written**: going
   between `native:0` and `logic` at one level is a change of look and not an onset. Stage 1k bakes at the level too.
4. **`fxLogicHue()` of a colour with no spread (section 5).** The formula divides by `max - min`. Such a colour is the
   display's own (`fxLogicOwn()`), and a layer never asks its hue; the function gives 0 rather than divide by 0.
5. **A slot past 9 (N6)** is dark, like 7 to 9.
6. **`vuLevel`** is a plain group (the level offered, from the clock), as `accentAt` is. The hold it is offered to is
   pinned already by `contractHoldsDue` and `contractLevelHold`.

### The region model (stage 1k)

`test/host/test_logic_regions.cpp` is a model, not a measurement: the palette as `calculateAllColors()` bakes it, every
LED walking it with its pauses drawn from FastLED's `random8()`, `hsv2rgb_rainbow`, a frame every 10 ms, the pace from
`fxLogicPace()`, the level from the core's clock through `envBright()` and `fxLogicPumpAmount()`, each LED taking it at
its next step. Every display size the engine knows (front 8 by 10; rear 16 by 4, 24 by 4, 28 by 4, 32 by 8), at the
display's own brightness and at 255, the own pace, the slowest and the fastest, the own palette and two colours,
steady and pumped (`am=2`, `m=255`) at 30, 120, 176, 177 and 300 BPM, 60 s a case. It counts the worst second (the
most onsets inside any 1,000 ms) over the whole field and over every band of rows, every band of columns and the four
quarters, by brightness and by luminance, and fails when a pace on offer has more than three where the display's own
stock blink has fewer. It also runs 146 to 180 BPM with S2's floor alone and with `logic`'s own, and fails unless the
second has none over three. `DROIDNET_FORK.md` has its table. It follows `test_native_s10.cpp`'s FastLED (`scale8`
without `FASTLED_SCALE8_FIXED`); built with `-DLOGIC_SCALE8_FIXED`, as the vendored FastLED 3.5 is configured, every
verdict is the same.

What it found: steady, no pace on offer lights any part of any display more than twice in a second, so the floors of
N2 stay at 1 and 10 (the fastest pace is the front's own; on the rear it takes the delay to 20 and the fade to 1).
Pumped, the 400 ms floor above.

### `projector`'s step rule (stage 1j)

`test_fx_regions.cpp` runs N7 on a jewel of seven (the centre and a ring of six) with seeded dice, 30 minutes a case:
five colours, the level steady and under `B`/`b=` raised and dropped as fast as lines come (through the core's own
hold, sharing `levelMs` with the brightening steps). The windows are every pair of pixels, both halves of the ring,
the centre with each half, and the whole jewel, by brightness and by luminance, counted the gate's way. In every case
the most is three onsets in a second, the closest two brightening steps are 340 ms apart, and a crumbling step never
lights a pixel. Under the storms the brightening steps are rare (about one in three seconds), because each raise of
the level takes the window they share.

### The vector file

New constants: `FX_LOGIC_GREY`, `FX_LOGIC_HUE_BASE`, `FX_LOGIC_FADE_MIN`, `FX_LOGIC_DELAY_MIN`,
`FX_PROJECTOR_STEP_MIN_MS`, `FX_PROJECTOR_STEP_MAX_MS`, `FX_PROJECTOR_BRIGHT_MS`, `FX_LOGIC_PUMP_MIN_MS`. Changed:
`CONTRACT_RELEASE` (2) and `contract_version` (`1.4`). New lines, one for each id:
`LFA:i=logic,c=116530,s=200,at=16,am=2,m=255`, `HTA:i=projector,c=ff0000,b=200,at=32`, `PFA:i=vu,am=2,m=200,at=8`.
New groups, all pure:

| Group | args | out |
| ----- | ---- | --- |
| `fxLogicOwn` | `r`, `g`, `b` | `own` |
| `fxLogicHue` | `r`, `g`, `b` | `hue` |
| `fxLogicPace` | `s`, `x`, `floor` | `y` |
| `fxProjectorSlot` | `rgb`, `slot` | `r`, `g`, `b` |
| `fxProjectorTake` | `cur`, `cand` (each `0xRRGGBB`) | `take` |
| `vuLevel` | `bpm`, `ph`, `bpb`, `beat`, `seedMs`, `nowMs`, `am`, `m`, `an`, `ap` | `fires`, `amt`, `level` (= `envBright(255, m, amt)`) |
| `fxLogicPump` | the same as `vuLevel` | `fires` (`fxLogicPumpFires`), `amt` (`fxLogicPumpAmount`, progress from a section 0 to 16) |

No row of an existing group, no line and no constant but the release changed.

### What it costs

The ESP32 image of this fork (`pio run -e wlogicengine32`), CA1.7 and CA2 with the stand-ins: 1,136,881 B and
1,136,937 B of flash (+56 B), 64,948 B of static RAM both. On the PSI the plan's prototype priced the names and the
version at +16 B; the PSI's own figure is Task F3's to measure. The new functions are inline and used by no PSI code,
so they cost it nothing.

### The logics layer on CA2 (Task F2, and the 2026-11-21 review's fixes)

Nothing in the core changed for this; it is the RSeries layer only (`src/contract/ContractLogics.h`), recorded here
because it reads the spec, and because the review found that the gates alone are not enough for `logic`.

- **N3 and N4, as the layer reads them now.** The level reaches the engine as `updateDisplay(level)` with the palette
  baked at that level (`calculateAllColors(pal, level)` through `setBrightness(level)` and `setPaletteHue()`, whenever
  the level changes): as `native:0` at that level is drawn. So `logic` and `native:0` at one `b=` are one light, at
  every level (the review's R2-F2; the owner chose this over recording the difference). Stage 1k, the model the core's
  floors were decided on, now bakes at the level as well; its rule and its verdicts stand (`logic`'s own floor 0 of
  216 cases over; S2's alone 18 of 216).
- **The gates take `logic` unmeasured (N4), and its windows are the layer's.** The walk's light takes each level and
  colour LED by LED, 0 to about 130 ms late and up to a key pause (390 ms on the rear). The core's 340 ms window and
  the pump's own 400 ms floor alone let it break section 9 by stage 1k's count on the real layer (stage R2rev): a rise
  by a path the pump's floor does not see (`ap=` moved each beat, a `C` re-seed, the clock held and resumed, `B` and
  `b=`), and a recolour. So the layer keeps windows of its own, using only existing core calls:
  `FX_LOGIC_PUMP_MIN_MS` (400 ms since `fieldOnsetMs`) for an onset as the look enters, for a pump beat after a pump
  beat, and for the next light after the walk; and the WALK'S WINDOW for every other rise of the walk, of any size,
  and a recolour: `W = 2 × STROBE_MIN_STATE_MS + D`, D the longest an LED takes to step (`10 ms × (max(delay − 1,
  fade) + 1)`, the pause distribution of `updateMappedLED()`): a rise's light is on every LED within D, so rises W
  apart keep every part of the display's onsets 340 ms apart. 440 ms on the front at its own pace, 740 on the rear,
  1,940 on the rear at `s=0`. It replaced a fitted 500 ms, which the second review broke on a seventh walk seed;
  swept over 400 seeds on R2rev's paths it gives 0 of 11,200 over. Rises under the gate's step are held too, because
  the walk's light goes as about the fourth power of the level (R2-F5).
- **`native:0` is the same walk** and the same light, so its brightness rises (`_nativeBright()`) keep the same window
  at the engine's live pace (R2-F4), at the frame the engine restarts the idle blink too (`contractEngineLatched()`,
  R2-F8: it starts at the brightness it has and rises when the window opens). That path is R1's too; it was
  backported to `main` (branch `lighting-r1-walkrise`).
- **The hand-over with the idle blink is seamless when it changes nothing** (`logic` not pumped, in the display's own
  colours, at the idle blink's level): the same walk goes on, no LED moves, no hold. Any other hand-over, and every
  hand-over between the walk and other light, is unseen light (`contractNativeEnded(known=false)`), the frame that is
  up held 400 ms. Without that hold for the changing hand-overs too, a pumped `logic` and `native:0` by turns every
  300 ms at 255 lit the rear 16 by 4 four times a second by the gate's own count.
- **A recolour** of the walk that is up waits the window and starts it again (`contractNativeEnded(known=true)`).
- **`projector`'s stand-in** (`sparkle`) is not pumped (N8).
- The exact rules, in the order the layer applies them, are in `DROIDNET_FORK.md` ("What is in (contract v1.4, R2)",
  "Its windows"), for Studio's mirror. A fork that draws `logic` itself (none does) would need the same.
- Stage R2rev (the reviews' findings, R2-F1 to R2-F8) passes in full. R2-F7's check, as the third review restated
  it, runs the same lines from the same seed with and without a seamless `logic` and finds no LED of any frame apart
  over 600 ms; the first version compared the most light with the mean of the 100 ms before, which `native:0` alone
  fails too (its walk's own texture). Stage 2g holds the held frames to `native:0`'s frame for frame.

The ESP32 image with `logic` and `vu` drawn by the layer and the reviews' fixes: 1,140,141 B of flash and 64,964 B of
static RAM (F2 as first merged: 1,138,617 B and 64,948 B).
