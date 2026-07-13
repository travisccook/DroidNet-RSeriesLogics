# RSeries Logics — Freeze-Free Connect-Else-AP (Design)

**Date:** 2026-07-13
**Status:** Approved design, pre-implementation
**Repo:** `DroidNet-RSeriesLogics` (public fork of the RSeries Logic Engine firmware)
**Branch:** `feature/wifi-connect-else-ap`

## 1. Goal

Make a DroidNet RSeries logic board **join existing infrastructure** — the home WiFi, or
(when that is down) the Pi 4 "droidnet" AP — and **only host its own access point when no
saved network is reachable**, exactly like the droidnet Pi does. Do this **without ever
freezing or stuttering the light animations.**

A prior attempt (the abandoned `C2B5/RSeriesLogic` fork) produced "a lot of random light
freezing" and was not carried forward. This design fixes the actual cause rather than
re-attempting the feature from scratch.

## 2. Key finding: this is not greenfield

The current fork **already implements** connect-else-AP. `src/main.cpp` boots WiFi in
`setup()` (compile gate `#define USE_WIFI`, runtime NVS key `"wifi"` default-on), loads a
priority-ordered list of saved networks stored as a single JSON blob in NVS
(`loadWifiNetworks`/`saveWifiNetworks`, ~2 KB `DynamicJsonDocument`), and delegates
connection management to a materially forked `lib/Reeltwo/src/wifi/WifiAccess.h`, which
falls back to hosting `RSeries`/`Astromech` at `192.168.4.1` after a 60 s timeout.

**The policy exists. What is broken is *how* it connects.**

## 3. Root cause of the freezing (verified against source)

### 3.1 Primary — a ~15–17 s blocking sequence inside the render task

`WifiAccess` is an `AnimatedEvent` (`WifiAccess.h:52`). Its `animate()` (`:168`) runs in the
**same sequential `AnimatedEvent::process()` chain as every LogicEngine renderer**, and
`FastLED.show()` only fires at the end of a full pass. `animate()` still contains blocking
calls on the connect path:

- two synchronous `WiFi.scanNetworks(false, …)` — `:285`, `:370` (~3–5 s combined)
- `delay(2000)` — `:409`; `delay(100)` — `:361`, `:414`; `delay(500)` — `:434`
- a `20 × delay(500)` = up to 10 s status-poll spin — `:433`

While `animate()` blocks, the pixels hold their last frame. The **failure path — no saved
network reachable, which is exactly this feature's target scenario (home WiFi down, Pi 4
not up yet)** — blocks the full ~15 s every attempt. This is *the* freeze. The old
`C2B5/RSeriesLogic` fork contains the same code — it was never fixed, only carried forward.

### 3.2 Contributing — the freeze re-arms every 30 s while disconnected

The blocking path is gated by `now - fWifiPrevMillis >= fWifiInterval`, `fWifiInterval =
30000` (`:215`, `:83`). Disconnected → ~15 s frozen, ~15 s animating, repeat. Once
associated or hosting an active SoftAP, `animate()` early-returns (`:176`–`:190`). The
symptom is therefore **connection-state-dependent, not random.**

### 3.3 Architectural amplifier — rendering shares core 0 with the WiFi stack

`xTaskCreatePinnedToCore(eventLoopTask, "Events", 5000, NULL, 1, &eventTask, 0)`
(`main.cpp:1109`–`1116`) pins **rendering to core 0 at priority 1** — the same core the
ESP32 hard-pins the WiFi MAC/PHY and lwIP to. Even with zero blocking calls, every
association / DHCP / beacon burst preempts the priority-1 render task → sub-frame
micro-stutter. This is why *any* WiFi work on this board is inherently fragile, independent
of the blocking bug.

### 3.4 Secondary — a blocking diagnostic scan in `loop()`

`WiFi.scanNetworks(false, false, false, 100)` at `main.cpp:1360` (~1.4 s across 14
channels) is a second, independent periodic hitch. Delete it.

### 3.5 Low / to rule out — WiFi-TX brownout

`WiFi.setSleep(false)` plus sustained-TX scan bursts pull 250–350 mA; with an untuned
brownout detector and no in-tree decoupling/TX-power config, a power dip is *possible*. The
reported fingerprint (a clean multi-second hold, not flicker/reset) points at §3.1, not
power — but a boot-time `esp_reset_reason()` check will rule it out.

## 4. Decisions (agreed)

| Decision | Choice | Rationale |
| --- | --- | --- |
| **Self-heal** | **Safe auto-rejoin** — keep retrying saved networks in the background while hosting AP: slow, async, non-blocking, flap-protected | A field droid must rejoin the control network hands-off (e.g. when the Pi 4 boots *after* the logics), without anyone physically touching it. This is the freeze-prone path in today's code, so it is the part we most carefully re-engineer. |
| **Provisioning** | **Web-provision → NVS** (no baked secrets) | Fork is a **public** repo. Creds are entered once via the board's web UI and persist in NVS across app reflash *and* OTA (dedicated `nvs` partition in `min_spiffs.csv`; OTA writes the inactive app slot; `pio upload` performs no erase). An empty-NVS board naturally hosts its AP, which *is* the provisioning entry point. Re-provision only needed after a deliberate `erase_flash` or on a new chip. |
| **Isolation scope** | **Middle: de-block + move render to core 1 now**; defer full network-task separation | Kills the freeze *and* the micro-stutter for everything done today with a small, contained change. Full separation (dedicated core-0 network task) is only needed for the future dome light-show *streaming* (continuous WiFi data) and will be done as part of that feature. |

## 5. Design

### 5.1 Behavior

- **Boot:** load saved networks from NVS → attempt them **in saved priority order** (the
  stored list order; `WiFi.begin(ssid, pass)` scans internally per SSID, so no separate scan
  is needed) → on success run as a **client** on home WiFi or the Pi 4 droidnet AP. If none
  reachable within a bounded budget, **host `RSeries` AP at `192.168.4.1`** (the lifeline —
  preserved and hardened; never weakened).
  - *Ordering note:* priority order is deliberate — it avoids an up-front scan (a freeze
    risk we are eliminating). RSSI-based reordering (one async boot scan, then order
    candidates by signal) is a possible future refinement, not a goal here.
- **Self-heal:** while hosting AP, retry saved networks in the background — async,
  non-blocking, flap-protected — and hand-off rejoin the instant one appears.
- **Provisioning:** web UI writes creds → NVS; persists across reflash + OTA.

### 5.2 Engine fix — non-blocking connection state machine (kills §3.1, §3.2, §3.4)

Replace the blocking body of `WifiAccess::animate()` with a **tick-poll / event-driven state
machine** driven by `WiFi.onEvent()` callbacks + `millis()` deadlines.

- **Delete:** both synchronous `scanNetworks(false, …)` (`:285`, `:370`); all `delay()`
  (`:361`, `:409`, `:414`, `:434`); the 20× status-poll spin (`:433`); and the diagnostic
  scan at `main.cpp:1360`.
- **Phases:** `IDLE → BEGIN` (kick `WiFi.begin`, stamp time, return) `→ WAIT` (poll status
  each tick against a deadline) `→` next-network / `AP_FALLBACK` / `CONNECTED`. Every phase
  returns immediately — honors the `AnimatedEvent` "subclasses should not call delay() or
  otherwise block" contract.
- **Success gate:** *associated **AND** got an IP.* (An ESP32 can associate but never get
  DHCP; that must count as failure.)
- **Scans:** async-only (`scanNetworks(true)`), and only when genuinely needed —
  `WiFi.begin(ssid, pass)` already scans internally to associate, so the explicit pre-scans
  are pure waste.

### 5.3 Isolation fix — render on core 1 (kills §3.3)

Move the render task to the quiet core:
`xTaskCreatePinnedToCore(eventLoopTask, …, 0)` → **core `1`** (`main.cpp:1116`). WiFi stack +
web + OTA stay on core 0. Rendering — the same single sequential task — is then shielded
from WiFi preemption. Requires one bench pass to confirm no code assumed core-0 affinity.

Rendering is already single-core (one task); this changes *which* core it runs on, not how
much compute it gets. A ~240 MHz core computing per-pixel animation for a few hundred
WS2812/SK6812 LEDs plus the RMT `FastLED.show()` has large headroom.

### 5.4 Policy values (initial; tunable)

| Knob | Value | Rationale |
| --- | --- | --- |
| Per-network connect timeout | **8–10 s** | associate + DHCP; fail fast to the next |
| Boot budget before AP fallback | try each saved net once, cap **~25–30 s** | bounded → AP lifeline guaranteed. Lights render normally throughout (no freeze), so this delays only *reachability*, not the show. |
| Background rescan while in AP | async attempt every **~30 s** | slow enough to be power/radio-cheap; use APSTA mode |
| Flap protection | drop a link only after **3 consecutive** failed polls | one RSSI blip must not bounce a working client mid-show |
| AP-client coexistence | **pause** background rescan while a client is on the AP config page | a rescan channel-hops and would blip an active config session |

## 6. Non-goals (YAGNI)

- No dedicated core-0 network task / full render separation yet (deferred to the dome
  light-show streaming feature).
- No baked-in credentials / build-time secret injection.
- No new provisioning UX beyond the existing web UI + NVS.
- No change to the AP SSID/IP identity, mDNS (`RSeries.local`), OTA, or Marcduino behavior.
- No change to the LED driver, LogicEngine, or the cantina contract.

## 7. Validation (nothing here has run on hardware)

1. **Repro first:** on current firmware, confirm the freeze with the target network absent
   (the exact field scenario).
2. **After the fix:**
   - target absent → lights stay smooth, no multi-second freeze;
   - no micro-stutter during connect with render on core 1;
   - auto-rejoin fires when the Pi 4 appears late;
   - empty-NVS board → AP bootstrap + web-provision works;
   - `esp_reset_reason()` boot check rules out TX-brownout (§3.5).
3. Preserve the LGPL-2.1 modified-file notice on `WifiAccess.h`.

## 8. Open items

- **Which board are the deployed logic units** — `esp32dev` (WROVER) or
  `esp32-s3-devkitc-1`? Both are dual-core (so the core-1 move is available); needed only to
  pick the build/test target for the bench pass.

## 9. Reference (grounding file:line)

- Blocking connect path — `lib/Reeltwo/src/wifi/WifiAccess.h:168-457` (scans `:285`/`:370`;
  delays `:361`/`:409`/`:414`/`:434`; spin `:433`; 30 s re-arm `:215`; 60 s AP fallback
  `:88`/`:235`).
- Render task pinned to core 0 — `src/main.cpp:1109-1116`.
- Diagnostic scan in `loop()` — `src/main.cpp:1360`.
- Boot decision + NVS saved list — `src/main.cpp:553-635`, `:886-939`.
- Partition scheme — `platformio.ini` (`board_build.partitions = min_spiffs.csv`).
- droidnet connect-else-AP contract (behavioral reference) —
  `~/Repos/DroidNetSignalBooster/scripts/network-controller.sh`.
- Old-fork forensic twin (same blocking pattern) —
  `~/Repos/C2B5/RSeriesLogic/lib/Reeltwo/src/wifi/WifiAccess.h`.
