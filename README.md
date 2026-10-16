# DroidNet-RSeriesLogics

Part of [DroidNet](https://github.com/travisccook/DroidNetSignalBooster-releases), the Signal Booster for droids. This is one of its board firmwares.

An additive fork of **[WLogicEngine32](https://github.com/reeltwo/WLogicEngine32)** — the RSeries
Logic Engine firmware, originally written by **Joymonkey** (Paul Murphy) and updated and maintained
by **Rimim** (Mimir Reynisson).

Almost everything in this repository is their work. This fork adds one small layer (about 700 lines
across two headers plus a host test harness) so an external tool can drive the logic displays in
time with music, plus one native command, `#LEBRI`, that sets the logic displays' brightness, and
an announce line that tells the WCB on Serial1 what this board is.
Everything the firmware already did, it still does, unchanged.

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

## Status

**Work in progress.** This DroidNet build is provided as is, without warranty of any kind. Use it at your own risk. To go back, flash the upstream firmware ([README_UPSTREAM.md](README_UPSTREAM.md)).

The ESP32 build runs on the author's droid (C2-B5) and has since 2026-10-16; it was flashed over WiFi through the board's own Reflash page. The ESP32-S3 build is not supported yet: it compiles, but its hardware config is unconfirmed (see below).

**Testing.** It cross-compiles for the real microcontroller. `pio run` builds a complete
ESP32 firmware image with the real xtensa toolchain, and the real linker signs off on it:
**1,129,009 B of flash (57.4%)** and 63,532 B of SRAM (19.4%). Plus `bash test/host/run.sh` — 341
checks against the wire parser, beat clock and effect math, plus a type-check of the render layer
against a mock of the engine API, and it now finishes by doing that cross-compile itself.

This used to say the code had never been compiled for the ESP32 at all, and that was the honest
state. It is no longer. It is worth saying why it matters: the first real build immediately caught a
bug no mock could see — FastLED declares `RGB` as an *enumerator* of `fl::EOrder`, which name-hides a
`struct RGB`, and our shared contract core defined exactly that struct. It could never have compiled
on this board. The mock did not model `EOrder`, so the type-check passed happily for months.

**Both environments now build.** `wlogicengine32s3` (ESP32-S3) previously could not be built at all,
and an earlier revision of this README explained that with a claim that was simply **wrong** — it said
the platform pin was shared with the working ESP32 env, so recovering the S3 would drag that env's
toolchain along with it. It is not shared. `platform =` is declared *per-environment* (there is no
`[env]` section and `[common]` is empty), so the two are independent, coincidentally-identical lines.
The S3 could always have been re-pinned at zero risk. Correcting the record.

What was actually broken was two things, one hiding under the other:

1. `platform-espressif32` v5.2.0 makes `toolchain-riscv32-esp` **non-optional** for the S3 (it is the
   RISC-V ULP *coprocessor* toolchain) and pins it to `8.4.0+2021r2-patch3` — which the PlatformIO
   registry has since unpublished. Only that exact patch level was pruned; `patch5` is still served,
   so the env now pins `patch5` via `platform_packages`, in the S3 env alone. The plain-ESP32 env
   never requests riscv32, which is exactly why it kept building. The ULP toolchain is inert here
   anyway: there is no `ulp/` source and every object is compiled by `xtensa-esp32s3-elf-g++`.
2. Underneath that, the S3 env pulled **FastLED from unpinned git master**. It now uses the vendored
   `lib/FastLED` 3.5.0, same as the ESP32 env.

`wlogicengine32` was provably unaffected: it linked at exactly the same byte count before and after
the change. The platform stays on v5.2.0 — no bump, no new toolchain, nothing to re-validate.

**A compile is not a run, and the S3 env is not supported yet.** It is a fork-local
hand-translation of upstream's `make TARGET=ESP32S3` (upstream has no S3 PlatformIO env at all), and
it carries at least three hardware-config guesses a green build cannot catch: `ARDUINO_USB_CDC_ON_BOOT`
is **not** set, so with the board's `ARDUINO_USB_MODE=1` the `Serial` console maps to UART0 rather than
the USB port — on a board whose only console is USB, this image would very likely just look **silent**;
`BOARD_HAS_PSRAM` is declared on a devkit profile that has none; and `board = esp32-s3-devkitc-1` is a
generic devkit standing in for a WReactor32-2023. Treat the S3 target as "it compiles", nothing more.

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
a rewrite. If you never send a `!` or `#LEBRI` command, this firmware behaves like the stock
firmware.

## One native command: `#LEBRI` brightness

```text
#LEBRI<level>     both logic displays    e.g. #LEBRI128
#LEBRIF<level>    front (FLD) only       e.g. #LEBRIF255
#LEBRIR<level>    rear (RLD) only        e.g. #LEBRIR1

level   1-255, one to three digits (leading zeros are accepted; the library never sends them)
```

- **Linear, and 255 is the hardware maximum.** Each accepted command sets that display's FastLED
  colour correction to (L, L, L), which FastLED applies at output time. So it dims every look on the
  next frame: the palette effects, text, the custom effects 100-105 and the contract looks. It is a
  master dimmer over contract shows too.
- **The first command for a display raises its baseline once.** It sets that display's RAM default
  `fBri` and its live `fBri` to 255, so effect changes do not snap it back. Today's Normal look sits at
  about 39% (front) and 30% (rear) of the new scale, and 100% is about 2.54× (front) and 3.3× (rear)
  today's brightness. No power limiter is configured, and current draw at 255 has not been measured.
- **RAM only.** Nothing is written to Preferences/NVS. After a power-on the look is stock until the
  first command. (On an ESP32-S3 build the on-board controller restores its own saved `fBri` at
  setup, and its trimpot and web sliders compete with the command.)
- **Strict.** Anything else is ignored silently, with no clamp and no reply: a bare `#LEBRI`, 0,
  256-999, four or more digits, lowercase `f`/`r`, a sign, spaces or trailing text.
- A display whose init did not add exactly one FastLED controller (for example no rear display
  fitted) is ignored by the command. Its dither is switched off only when it is captured.
- A later contract `B` verb (`!L*B`) lowers the live `fBri` on top of the correction until the next
  effect change.
- On the WiFi Marcduino port 2000 a burst of several lines runs only the last line's action. That is
  pre-existing for every action on that port; Serial1 is not affected.

Its DroidNet command library id is `rseries.cfg.brightness` (board `droidnet-rseries-logic`, library
4.4.0). The code is `src/native/native_brightness_parse.h` (the pure parser),
`src/native/NativeBrightness.h` (the action and the controller capture) and five marked lines in
`src/main.cpp`. It does not touch the contract layer, effect 106 or the contract `B` verb.

## WDP device announce

The board tells the WCB it is wired to what it is. Greg Hulette's WCB firmware (7.0.1) learns what
sits on each of its serial ports from one line the device sends every 25-30 seconds (WDP device
announce, `docs/WDP_DEVICE_ANNOUNCE.md` in greghulette/Wireless_Communication_Board-WCB):

```text
@WDP1 {"type":"R-Series Logics","fw":"<commit>","caps":["droidnet.contract","led.logic"]}
```

- **On Serial1**, the Marcduino serial port the WCB is wired to, at that port's baud (default 9600),
  board TX to WCB RX. Nothing else in the firmware writes to Serial1. With Marcduino serial switched
  off, Serial1 never starts and nothing is sent.
- **When:** first 0.5-3 s after the port starts at boot, then every 25-30 s, each interval picked
  afresh. The jitter is seeded from the hardware RNG mixed with the chip's factory MAC, so boards
  that power up together do not announce in lockstep.
- **`fw` is the build's commit:** `git rev-parse --short=7 HEAD` when it was built
  (`scripts/pre_extra.py`), the same sha the prebuilt images in `firmware/` are named by. It gets
  `-dirty` when the tree had changes outside `firmware/`, and is `unknown` when git could not say.
  The line is 89 bytes with a clean commit, 95 with `-dirty`.
- **It never blocks.** `mainLoop()` writes only as much of the line as the UART reports room for
  (`availableForWrite()`, 128 bytes on an idle port), so a whole line normally goes in one write.
- The WCB consumes the line and never forwards it, so no other board sees it. `?WDP,DA` on the WCB
  shows what each port announced. `R-Series Logics` is the name the WCB Wizard itself uses for this
  board (`Wizard/device-labels.js`).

The code is `src/wdp/wdp_announce_core.h` (the line and its schedule, pure C++, shared byte for byte
with the PSI Pro and FlthyHPs forks), `src/wdp/wdp_fork.h` (this board's identity and the two calls)
and three marked lines in `src/main.cpp`.

---

## What is theirs and what is ours

Three layers, in the order they were written. The middle one matters: some of what is in this tree
is neither upstream's nor part of the contract layer.

**Theirs** — the firmware itself, and the great majority of the code here:

- `src/main.cpp` (1,415 lines — this is upstream's `WLogicEngine32.ino`, relocated), `WebPages.h`,
  `MarcduinoLogics.h`, `logic-sequences.h`, `pin-config.h`, `Screens.h`, `web-images.h`,
  `effects/`, `menus/`, `Makefile`, `platformio.ini` — Joymonkey and Rimim, with contributions from
  IOIIOOO, Curiousmarc, Mowee, Flthymcnasty and others.
- `lib/Reeltwo` — Rimim.
- `lib/FastLED`, `lib/Adafruit_NeoPixel` — their respective authors.
- `README_UPSTREAM.md` — upstream's own README, preserved here.

A few of those files carry in-place edits made downstream (see the next block); the code in them is
upstream's.

**Earlier C2B5 droid customization** (Travis Cook, pre-dating this fork) — downstream work that came
with the tree this fork was seeded from. It is *not* upstream's:

| Path | What it is |
| --- | --- |
| `data/modern/` | The C2B5 web UI — `index.html` and `wifi.html`, the only tracked files under `data/`. |
| `scripts/` | PlatformIO build hooks, the HTML-to-header compressor that embeds that UI, and a JavaScript validator. |
| `visualizer/` | A browser LED-matrix preview tool for the FLD/RLD displays (`"author": "C2B5 Project"`). Not firmware; not upstream. |
| `README_C2B5.md`, `docs/C2B5_CUSTOMIZATIONS.md`, `wapi_test_routes.h`, `wapi_debugging_guide.md`, root `package.json`, `.clang-tidy` | Notes, debug routes and tooling from the same work. |
| in-place edits | `src/main.cpp` (multi-network WiFi with priorities, `RSeries.local` mDNS, SSID URL-decoding, C2B5 debug output), `WebPages.h`, `platformio.ini`, and the two `lib/Reeltwo/src/wifi/` headers (`WifiAccess.h`, `WifiWebServer.h`), which carry an LGPL-2.1 §2(b) modified-file notice at the top. |

None of that is upstream's, and none of it is part of the contract layer. It is described in
[README_C2B5.md](README_C2B5.md) and [docs/C2B5_CUSTOMIZATIONS.md](docs/C2B5_CUSTOMIZATIONS.md).

**Ours** (Travis Cook, 2026) — the fork layer this repository exists for:

| Path | What it is | Lines |
| --- | --- | --- |
| `src/contract/contract_core.h` | Pure, dependency-free C++: wire parser, beat clock, effect math, score table. No Arduino, no FastLED, no Reeltwo. Byte-identical across the three DroidNet board forks. | 362 |
| `src/contract/ContractLogics.h` | The RSeries render layer: maps the contract onto this board's LED primitives, registers as custom effect id 106. | 337 |
| `test/host/*` | The host test harness (parser tests, engine mock, type-check). | 506 |
| `src/main.cpp` | **Four added lines and one bounds-check fix.** An `#include`, `ContractRender` appended to the custom-effect table, `contractSetup()` in `setup()`, `contractTick()` in `mainLoop()`. | +7 / −2 |
| `src/native/native_brightness_parse.h` | The `#LEBRI` parser. Pure C++, no Arduino, FastLED or Reeltwo. | 62 |
| `src/native/NativeBrightness.h` | The `#LEBRI` Marcduino action and the FastLED controller capture. Not part of the contract layer. | 87 |
| `src/main.cpp` (native brightness) | One `#include` and the controller capture around `FLD_init()` and `RLD_init()`, each line marked `DroidNet native brightness`. | +5 |
| `src/wdp/wdp_announce_core.h` | The WDP announce line and its schedule. Pure C++, byte-identical across the three DroidNet board forks. | 158 |
| `src/wdp/wdp_fork.h` | This board's WDP identity and the Serial1 adapter. | 54 |
| `src/main.cpp` (WDP announce) | One `#include`, `wdpForkInit()` after `Serial1.begin()`, `wdpForkTick()` in `mainLoop()`. | +3 |
| `scripts/pre_extra.py` (WDP announce) | Defines `WDP_FW`, the build's commit. | +26 |
| `DROIDNET_FORK.md`, this README | Fork docs. | — |

The bounds-check fix in `main.cpp` is a one-character change to a pre-existing off-by-one in the
custom-effect selector (`selectSequence-100 <= SizeOfArray(...)` read one slot past the array). It
was latent upstream because nothing ever selected the last id; adding effect 106 would have hit it.
That is the only line of upstream code this fork changes.

Run `git diff 6bba8b1..HEAD --stat` if you want the exact split (`6bba8b1` is the baseline import):
**13 files, 1,757 insertions, 183 deletions** (the deletions are almost entirely the documentation
rewrite; only two are code, both in `src/main.cpp`). Nine of those files are code — the six new ones above, the
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
   under `lib/Reeltwo` carry a "modified from upstream" notice. That collection is private and stays
   private; it is not a repository you can visit, and nothing here depends on it.
3. **This repository:** that tree, plus the DroidNet contract layer. This is the first public
   distribution of any of it.

---

## Building and testing

```sh
bash test/host/run.sh
# [1/9]  contract_core parser unit tests               -> OK  341/341 checks passed
# [1b/9] wifi_connect_core FSM unit tests              -> OK  20/20 checks passed
# [1c/9] wdp_announce_core WDP announce unit tests     -> OK  37506/37506 checks passed
# [1d/9] wdp_fork RSeries WDP identity + adapter       -> OK  19/19 checks passed
# [2/9]  ContractLogics.h firmware type-check          -> OK
# [3/9]  Marcduino command-buffer width guard          -> OK
# [4/9]  parser fuzz + differential (ASan/UBSan)       -> OK  1554686/1554686 checks passed
# [5/9]  native #LEBRI brightness parser unit tests    -> OK  3907/3907 checks passed
# [6/9]  native #LEBRI brightness action (mock)        -> OK  33/33 checks passed
# [7/9]  Marcduino prefix-collision guard              -> OK  43 actions, 635 catalog lines
# [8/9]  native #LEBRI brightness parser fuzz          -> OK  1070735/1070735 checks passed
# [9/9]  REAL cross-compile (ESP32)                    -> OK   <- needs PlatformIO; skips cleanly without it
```

Stage 7 reads `test/host/catalog_example_lines.tsv`, a checked-in snapshot of every example line in
the DroidNet command library. Regenerate it with
`python3 test/host/refresh_catalog_examples.py <library checkout>`.

Stages 1-8 need no hardware and no toolchain beyond clang. Stage 9 is the real cross-compile: it is what makes
"it compiles" a claim about the firmware rather than a claim about our mock. If PlatformIO is not
installed it skips cleanly and tells you what you did not check.

Firmware (PlatformIO): `pio run` builds both envs — `wlogicengine32` (ESP32, 1,129,009 B / 57.4%)
and `wlogicengine32s3` (ESP32-S3, 1,038,257 B / 52.8%). The S3 env used to be unbuildable; see the
Status and S3 notes above for what was wrong and for the hardware-config caveats that still apply to it.
See [README_UPSTREAM.md](README_UPSTREAM.md) for the real build and flashing instructions — they are
otherwise unchanged.

One fork-owned change to `platformio.ini`: `build_src_filter` gains `-<test/>`. `src_dir` is the repo
root here and that filter *replaces* PlatformIO's default, so the default's implicit `-<test/>` was
gone — meaning this fork's host suite (clang, C++17, its own `main()`) was being swept into the ESP32
firmware image.

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

**LGPL-2.1.** The upstream firmware and the vendored `lib/Reeltwo` are LGPL-2.1 (Joymonkey, Rimim);
the root [LICENSE](LICENSE) file is upstream's own, byte-identical. The DroidNet contract layer
(`src/contract/*`, `test/host/*`), the native brightness command (`src/native/*`) and the WDP
announce (`src/wdp/*`) are Copyright
(c) 2026 Travis Cook, LGPL-2.1-only as distributed here. Also vendored: `lib/FastLED` (MIT) and `lib/Adafruit_NeoPixel` (LGPL-3.0).
