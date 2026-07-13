# DroidNet-RSeriesLogics

An additive fork of **[WLogicEngine32](https://github.com/reeltwo/WLogicEngine32)** — the RSeries
Logic Engine firmware, originally written by **Joymonkey** (Paul Murphy) and updated and maintained
by **Rimim** (Mimir Reynisson).

Almost everything in this repository is their work. This fork adds one small layer (about 700 lines
across two headers plus a host test harness) so an external tool can drive the logic displays in
time with music. Everything the firmware already did, it still does, unchanged.

---

## The original project

[reeltwo/WLogicEngine32](https://github.com/reeltwo/WLogicEngine32) is the firmware for the
WReactor32 board (ESP32 / Lolin D32 Pro) that runs the **RSeries Logic Engine** dome lighting kit —
the front and rear logic displays on an astromech droid. It came out of the
[astromech.net parts runs](https://astromech.net/forums/showthread.php?40649-RSeries-Logic-Engine-dome-lighting-kit-250-(Jan-2021)-Open),
and it descends from Joymonkey's original [logicengine](https://github.com/joymonkey/logicengine)
project, which started this whole lineage.

If you use this fork, go star and support the original. Some specific things in it that are worth
your attention:

- **It is built on [Reeltwo](https://github.com/reeltwo/Reeltwo)** (also by Rimim), which is a
  genuinely good piece of embedded C++: a whole device and animation framework with a clean
  renderer abstraction (`LogicEngineRenderer`), a sequence system, Marcduino/JawaLite command
  dispatch, and a settings/menu stack. It is vendored here in `lib/Reeltwo`.

- **The extension seam is why this fork is so small.** Reeltwo exposes
  `CustomLogicEffectSelector`: any sequence id at or above 100 is handed off to firmware-supplied
  effect functions. Rimim designed a hook for exactly this kind of addition years ago, so our whole
  render layer plugs in as effect id 106 without modifying a single line of the vendored library.
  That is good API design paying off later for a stranger, and it is the nicest thing in this fork.

- **Firmware updates without a toolchain.** Drop `LEngine.bin` on an SD card, power up, and the
  board flashes itself (or update over the air through the built-in web interface). The average
  builder never needs to install Arduino or PlatformIO to take an update. That is real
  thoughtfulness about the actual person holding the droid.

- **Configuration on the board itself.** Colours, brightness and pattern speeds are adjustable with
  the buttons and trimpot, no computer required.

The upstream project's own README is preserved here as **[README_UPSTREAM.md](README_UPSTREAM.md)** —
that is where the real firmware documentation lives (Marcduino commands, `~RTLE` sequence codes,
WiFi setup, colour tables). Read that first. This README only covers what the fork adds.

---

## NEVER RUN THIS ON HARDWARE YET

**No code in this fork has ever run on a real board. Not once.**

Verification is host-side only:

- `bash test/host/run.sh` — 154 checks against the wire parser, beat clock and effect math, plus a
  type-check of the render layer against a mock of the engine API.
- That is it. No bench flash, no scope, no LEDs, no droid.

Nothing here has been compiled by PlatformIO for the ESP32, let alone flashed. Before this goes
anywhere near a droid: build it, bench it on a bare board with the display panels disconnected from
anything you care about, and check current draw. The stock firmware from upstream is the safe,
proven thing; this is not, yet.

---

## What this fork adds

A **Driveable-Animation Contract**: a small wire grammar so a host (Cantina Studio — a music
analysis and dome lighting authoring tool) can drive the logic displays in time with a song.

```text
!<cls><unit><verb>[:k=v,k=v,...]

cls    L = logic displays   P = PSI   H = holoprojectors   * = all
unit   F = front   R = rear   T = top/HP   * = all
verb   A = animate (set the base look)      P = pulse (a beat accent)
       C = beat-clock seed                  B = brightness
       L = level (energy 0-255)             X = stop
       M = mode (show / idle)               Q = query

example:  !LFA:i=comet,c=ff8800,s=200,m=64,am=1
          front logics -> comet, orange, fast, beat-modulated
```

The `!` prefix was chosen **because every stock board parser already ignores it.** That is the whole
trick, and it is what makes this layer additive rather than invasive.

What it buys:

- **Arbitrary RGB** (`c=ff8800`), not just the palette indices the native commands take.
- **Beat-locked accents** — the host seeds a beat clock (`!**C:bpm=,ph=,bpb=`) once and the board
  stays in phase with the music on its own.
- **Autonomous section playback** — the host pushes a compact "score" (looks scheduled at beat
  indices), then the board plays the whole song by itself with no further traffic.
- **Six new parametric effects** rendered along a per-board 1-D strand: `comet`, `chase`, `wipe`,
  `gradient`, `colorcycle`, `twinkle` — alongside `solid`, `flash`, `rainbow`, `scan`, `sparkle`,
  `meter`, and a `native:<code>` escape hatch that hands a section straight back to an iconic stock
  sequence.

**Every native command still works exactly as before.** `~RTLE…`, `@nT…`, `#LE…`, JawaLite, the web
interface, the SD-card updater, the menus — all untouched. This is backward-compatible and it is not
a rewrite. If you never send a `!` command, this firmware behaves like the stock firmware.

---

## What is theirs and what is ours

Three layers, in the order they were written. The middle one matters: some of what is in this tree
is neither upstream's nor part of the contract layer, and upstream should not be credited — or
blamed — for it.

**Theirs** — the firmware itself, and the great majority of the code here:

- `src/main.cpp` (1,415 lines — this is upstream's `WLogicEngine32.ino`, relocated), `WebPages.h`,
  `MarcduinoLogics.h`, `logic-sequences.h`, `pin-config.h`, `Screens.h`, `web-images.h`,
  `effects/`, `menus/`, `Makefile`, `platformio.ini` — Joymonkey and Rimim, with contributions from
  IOIIOOO, Curiousmarc, Mowee, Flthymcnasty and others.
- `lib/Reeltwo` — Rimim.
- `lib/FastLED`, `lib/Adafruit_NeoPixel` — their respective authors.
- `README_UPSTREAM.md` — upstream's own README, preserved here.

A few of those files carry in-place edits made downstream (see the next block, and the licence
section); the code in them is upstream's.

**Earlier C2B5 droid customization** (Travis Cook, pre-dating this fork) — downstream work that came
with the tree this fork was seeded from. It is *not* upstream's:

| Path | What it is |
| --- | --- |
| `data/modern/` | The C2B5 web UI — `index.html` and `wifi.html`, the only tracked files under `data/`. |
| `scripts/` | PlatformIO build hooks, the HTML-to-header compressor that embeds that UI, and a JavaScript validator. |
| `visualizer/` | A browser LED-matrix preview tool for the FLD/RLD displays (`"author": "C2B5 Project"`). Not firmware; not upstream. |
| `README_C2B5.md`, `docs/C2B5_CUSTOMIZATIONS.md`, `wapi_test_routes.h`, `wapi_debugging_guide.md`, root `package.json`, `.clang-tidy` | Notes, debug routes and tooling from the same work. |
| in-place edits | `src/main.cpp` (multi-network WiFi with priorities, `RSeries.local` mDNS, SSID URL-decoding, C2B5 debug output), `WebPages.h`, `platformio.ini`, and the two `lib/Reeltwo/src/wifi/` headers named in the licence section. |

None of that is upstream's, and none of it is part of the contract layer. It is described in
[README_C2B5.md](README_C2B5.md) and [docs/C2B5_CUSTOMIZATIONS.md](docs/C2B5_CUSTOMIZATIONS.md).

**Ours** (Travis Cook, 2026) — the fork layer this repository exists for:

| Path | What it is | Lines |
| --- | --- | --- |
| `src/contract/contract_core.h` | Pure, dependency-free C++: wire parser, beat clock, effect math, score table. No Arduino, no FastLED, no Reeltwo. Byte-identical across the three DroidNet board forks. | 362 |
| `src/contract/ContractLogics.h` | The RSeries render layer: maps the contract onto this board's LED primitives, registers as custom effect id 106. | 337 |
| `test/host/*` | The host test harness (parser tests, engine mock, type-check). | 506 |
| `src/main.cpp` | **Four added lines and one bounds-check fix.** An `#include`, `ContractRender` appended to the custom-effect table, `contractSetup()` in `setup()`, `contractTick()` in `mainLoop()`. | +7 / −2 |
| `DROIDNET_FORK.md`, this README | Fork docs. | — |

The bounds-check fix in `main.cpp` is a one-character change to a pre-existing off-by-one in the
custom-effect selector (`selectSequence-100 <= SizeOfArray(...)` read one slot past the array). It
was latent upstream because nothing ever selected the last id; adding effect 106 would have hit it.
That is the only line of upstream code this fork changes.

Run `git diff b47a5f9..HEAD --stat` if you want the exact split (`b47a5f9` is the baseline import):
**13 files, 1,754 insertions.** Nine of those files are code — the six new ones above, the
seven-line edit to `src/main.cpp`, and comment-only LGPL §2(b) notices on the two vendored Reeltwo
headers. The other four are documentation (this README, `README_UPSTREAM.md`, `DROIDNET_FORK.md`,
`README_C2B5.md`).

---

## Where this came from

1. **Upstream:** `reeltwo/WLogicEngine32`, by Joymonkey and Rimim, on top of Rimim's Reeltwo library.
   LGPL-2.1.
2. **A private working collection** in which this firmware was customized for one specific droid
   (call sign C2B5): better WiFi and AP-mode handling, multi-network support with priorities, an
   `RSeries.local` mDNS hostname, SSID URL-decoding, and a modern web UI. Those changes are
   documented in **[README_C2B5.md](README_C2B5.md)** and
   **[docs/C2B5_CUSTOMIZATIONS.md](docs/C2B5_CUSTOMIZATIONS.md)**, and they are the reason two files
   under `lib/Reeltwo` carry a "modified from upstream" notice (see the licence section). That
   collection is private and stays private; it is not a repository you can visit, and nothing here
   depends on it.
3. **This repository:** that tree, plus the DroidNet contract layer. This is the first public
   distribution of any of it.

---

## Building and testing

Host tests (no hardware, no ESP32 toolchain):

```sh
bash test/host/run.sh
# [1/2] contract_core parser unit tests -> OK  154/154 checks passed
# [2/2] ContractLogics.h firmware type-check -> OK
```

Firmware (PlatformIO): `wlogicengine32` (ESP32) and `wlogicengine32s3` (ESP32-S3). See
[README_UPSTREAM.md](README_UPSTREAM.md) for the real build and flashing instructions — they are
unchanged.

---

## Credits

The firmware, and the great majority of the code here:

- **Joymonkey (Paul Murphy)** — original Logic Engine and WLogicEngine32.
- **Rimim (Mimir Reynisson)** — WLogicEngine32 updates and maintenance, and the Reeltwo library.
- **IOIIOOO, Curiousmarc, Mowee, Flthymcnasty**, and others — named in upstream's own README as the
  source of portions of the code and inspiration.
- The **astromech.net** community and the RSeries Logic Engine parts runs.
- **FastLED** and **Adafruit** (Phil "Paint Your Dragon" Burgess and contributors) for the LED
  libraries vendored here.

The contract layer: **Travis Cook**, 2026.

---

## Licence

**This fork is LGPL-2.1, because upstream is.** WLogicEngine32 is licensed LGPL-2.1, the root
[LICENSE](LICENSE) file is upstream's own (byte-identical, untouched), and LGPL-2.1 is copyleft. The
contract layer is distributed as part of that work, so it carries **LGPL-2.1-only** headers
(`src/contract/*`, `test/host/*`). There is no "or later" grant anywhere upstream, so none is
claimed here.

The header in `src/contract/contract_core.h` is shared verbatim with the other two DroidNet board
forks and mentions a `LICENSE-DroidNet-Contract` file; that file lives in those (MIT-licensed) forks.
In *this* repository the file is LGPL-2.1-only, per the root LICENSE. Travis Cook holds the copyright
in the files he added and grants both, which is his to do.

**Modified upstream files.** LGPL-2.1 §2(b) requires modified files to say so. Two vendored Reeltwo
files were changed in place during the earlier C2B5 customization work:

- `lib/Reeltwo/src/wifi/WifiAccess.h`
- `lib/Reeltwo/src/wifi/WifiWebServer.h`

Both now carry a "MODIFIED FROM UPSTREAM" notice at the top stating what changed and when. No other
file under `lib/` has been touched by this fork.

**Other licences in the tree.** This repository also vendors:

- `lib/Reeltwo` — LGPL-2.1 (Rimim).
- `lib/FastLED` — MIT.
- `lib/Adafruit_NeoPixel` — LGPL-3.0. It is required, not vestigial: `LEDPixelEngine.h` includes it
  on the `USE_LEDLIB == 0` path alongside FastLED, and it is not in `platformio.ini`'s `lib_deps`,
  so the vendored copy is what satisfies the include.

This LGPL-2.1 / LGPL-3.0 mix is upstream's pre-existing structure. This fork did not introduce it and
has not tried to resolve it; it is stated here as fact so that anyone redistributing knows what is in
the box.
