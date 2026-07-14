# RSeries Logics Connect-Else-AP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make an RSeries logic board join a saved WiFi network (home, else the Pi4 droidnet AP) and only host its own AP when none is reachable — without ever freezing or stuttering the light animations.

**Architecture:** Extract all connect-else-AP decision logic into a pure, host-testable finite state machine (`src/wifi/wifi_connect_core.h`, mirroring the existing `contract_core.h` pattern — no Arduino/WiFi deps, time passed in as a parameter). Rewrite the vendored `WifiAccess::animate()` (which runs inside the render `AnimatedEvent` chain) into a thin, **non-blocking** adapter that reads the WiFi driver, calls the FSM once per tick, and executes the returned action. Delete the ~15–17 s of blocking scans/delays/spins that caused the freeze, delete a second blocking diagnostic scan in `loop()`, and move the render task off core 0 (shared with the WiFi stack) to core 1.

**Tech Stack:** C++17, PlatformIO (arduino-esp32, platform-espressif32 v5.2.0), FastLED, ReelTwo library, ArduinoJson, NVS (Preferences). Host tests via `clang++ -std=c++17` driven by `test/host/run.sh`.

## Global Constraints

- **No blocking calls reachable from `AnimatedEvent::process()`** — no `delay()`, no synchronous `WiFi.scanNetworks(false, …)`, no `while`/`for` spin on `WiFi.status()`. The connection logic must be a tick-poll state machine that kicks an action and returns immediately.
- **Pure core is dependency-free:** `src/wifi/wifi_connect_core.h` includes only C-stdlib headers (`<stdint.h>`, `<stddef.h>`), defines its own POD types, uses `inline` free functions, and **never calls `millis()`** — "now" is always a parameter. No `<Arduino.h>`/`<WiFi.h>`/FastLED/ReelTwo.
- **C++ standard is `-std=c++17`.** Designated initializers (`.field =`) are C++20 — do NOT use them; use positional aggregate init or explicit field assignment.
- **Success gate for a client connection is `associated AND has an IP`** (`WiFi.isConnected() && WiFi.localIP() != 0.0.0.0`). Associated-without-DHCP counts as failure.
- **The AP lifeline is sacred:** a bounded global connect budget must always guarantee the `192.168.4.1` AP comes up. Never weaken it.
- **Preserve the notify→`wifiActive` wiring:** `notifyConnected()` must fire when entering client-up **and** when entering AP; `notifyDisconnected()` when leaving either. `main.cpp:956-960/1004-1005` set the global `wifiActive` (which gates web/OTA in `loop()`) from these callbacks.
- **Host tests are hard-registered** in `test/host/run.sh` (no auto-discovery). A new test file must be added as a stage there.
- **Keep the LGPL-2.1 modified-file notice** at the top of `lib/Reeltwo/src/wifi/WifiAccess.h`.
- **Board targets:** `wlogicengine32` (esp32dev, default, locally cross-compilable) and `wlogicengine32s3` (esp32-s3, **not** locally cross-compilable — upstream toolchain package unresolvable; on-device only). Both are dual-core. There is a single shared `xTaskCreatePinnedToCore` for both.

---

### Task 1: Pure connection FSM + host tests

**Files:**
- Create: `src/wifi/wifi_connect_core.h`
- Create: `test/host/test_wifi_connect_core.cpp`
- Modify: `test/host/run.sh` (add a stage)

**Interfaces:**
- Consumes: nothing (pure, leaf module).
- Produces (used by Task 2's adapter):
  - `enum WcPhase : uint8_t { WC_IDLE, WC_CONNECTING, WC_CLIENT_UP, WC_AP };`
  - `enum WcAction : uint8_t { WCA_NONE, WCA_BEGIN_STA, WCA_START_AP, WCA_START_SCAN };`
  - `struct WcConfig { uint32_t perNetworkTimeoutMs, bootBudgetMs, rescanIntervalMs; uint8_t flapThreshold; };`
  - `struct WcInput { uint32_t nowMs; bool enabled, associated, hasIp; size_t savedCount; bool apClientConnected, rejoinCandidateSeen, forceApMode; };`
  - `struct WcState { WcPhase phase; size_t currentIndex; uint32_t attemptStartMs, budgetStartMs, lastScanMs; uint8_t failCount; bool initialized; };`
  - `struct WcDecision { WcAction action; size_t networkIndex; bool becameConnected, becameDisconnected; };`
  - `WcConfig wcDefaultConfig();`
  - `void wcInit(WcState&);`
  - `WcDecision wcStep(WcState&, const WcInput&, const WcConfig&);`

- [ ] **Step 1: Write the pure FSM header**

Create `src/wifi/wifi_connect_core.h`:

```cpp
// src/wifi/wifi_connect_core.h — pure, dependency-free WiFi connect-else-AP decision logic.
// Host-compilable (no Arduino/WiFi/Reeltwo). The firmware adapter (lib/Reeltwo/src/wifi/
// WifiAccess.h) includes this and EXECUTES the decisions against the real WiFi driver.
// "now" is passed in as a parameter and never read here — the same dependency-injection seam
// contract_core.h uses — so every transition is deterministically unit-testable off-hardware.
//
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only when
// distributed as part of this LGPL-2.1 fork.
#pragma once
#include <stdint.h>
#include <stddef.h>

// The board is always in exactly one of these phases.
enum WcPhase : uint8_t {
    WC_IDLE = 0,   // nothing started yet (pre-first-tick)
    WC_CONNECTING, // STA: walking the saved list, trying to associate + get an IP
    WC_CLIENT_UP,  // associated AND has an IP on a saved network (steady state)
    WC_AP,         // hosting our own AP lifeline; a slow background rejoin probe runs here
};

// The single imperative the adapter carries out THIS tick. Exactly one per tick; the FSM is
// polled continuously so multi-step sequences unfold across ticks.
enum WcAction : uint8_t {
    WCA_NONE = 0,  // do nothing
    WCA_BEGIN_STA, // WiFi.begin(savedNetworks[decision.networkIndex]) in STA mode
    WCA_START_AP,  // WiFi.softAP(apSsid, apPass) — bring up the lifeline
    WCA_START_SCAN,// kick an ASYNC scan (background rejoin probe while hosting AP)
};

// Tunable policy. Defaults from wcDefaultConfig().
struct WcConfig {
    uint32_t perNetworkTimeoutMs; // one saved network gets this long to associate + get IP
    uint32_t bootBudgetMs;        // total STA time before falling back to the AP lifeline
    uint32_t rescanIntervalMs;    // how often to probe for a saved network while hosting AP
    uint8_t  flapThreshold;       // consecutive down-polls before dropping a live client link
};

inline WcConfig wcDefaultConfig() {
    WcConfig c;
    c.perNetworkTimeoutMs = 9000;  // ~assoc + DHCP; fail fast to the next
    c.bootBudgetMs        = 30000; // bounded → AP lifeline is guaranteed
    c.rescanIntervalMs    = 30000; // slow/cheap background rejoin probe
    c.flapThreshold       = 3;     // one RSSI blip must not bounce a working client
    return c;
}

// What the FSM observes from the outside world this tick (adapter fills from the WiFi driver).
struct WcInput {
    uint32_t nowMs;               // millis()
    bool     enabled;             // WiFi feature enabled at all
    bool     associated;          // STA associated (WiFi.isConnected())
    bool     hasIp;               // got an IP (WiFi.localIP() != 0.0.0.0)
    size_t   savedCount;          // number of saved networks
    bool     apClientConnected;   // a station is joined to OUR AP (pause background rescan)
    bool     rejoinCandidateSeen; // a background scan saw one of our saved SSIDs
    bool     forceApMode;         // boot pinned AP-only (respected only at WC_IDLE)
};

// Persistent FSM state. Zero-initialize (WcState s{}) — wcStep() calls wcInit() on first tick.
struct WcState {
    WcPhase  phase;
    size_t   currentIndex;   // which saved network we're currently attempting
    uint32_t attemptStartMs; // when the current single-network attempt began
    uint32_t budgetStartMs;  // when the current STA connect budget began
    uint32_t lastScanMs;     // when we last kicked a background rejoin scan (WC_AP)
    uint8_t  failCount;      // consecutive down-polls while WC_CLIENT_UP
    bool     initialized;
};

// The decision returned each tick.
struct WcDecision {
    WcAction action;
    size_t   networkIndex;       // valid when action == WCA_BEGIN_STA
    bool     becameConnected;    // edge: fire notifyConnected() (client-up OR ap-up)
    bool     becameDisconnected; // edge: fire notifyDisconnected()
};

inline void wcInit(WcState& s) {
    s.phase = WC_IDLE;
    s.currentIndex = 0;
    s.attemptStartMs = 0;
    s.budgetStartMs = 0;
    s.lastScanMs = 0;
    s.failCount = 0;
    s.initialized = true;
}

// Begin an STA connect cycle from network 0.
inline WcDecision _wcStartConnecting(WcState& s, uint32_t now, bool fireDisconnect) {
    s.phase = WC_CONNECTING;
    s.currentIndex = 0;
    s.budgetStartMs = now;
    s.attemptStartMs = now;
    s.failCount = 0;
    WcDecision d{ WCA_BEGIN_STA, 0, false, fireDisconnect };
    return d;
}

// Bring up the AP lifeline.
inline WcDecision _wcStartAP(WcState& s, uint32_t now) {
    s.phase = WC_AP;
    s.lastScanMs = now; // don't rescan the instant we come up
    WcDecision d{ WCA_START_AP, 0, true, false };
    return d;
}

// The pure step. Call once per tick; mutates state; returns the action to perform.
inline WcDecision wcStep(WcState& s, const WcInput& in, const WcConfig& cfg) {
    WcDecision d{ WCA_NONE, 0, false, false };
    if (!in.enabled)
        return d;
    if (!s.initialized)
        wcInit(s);

    switch (s.phase) {
    case WC_IDLE:
        if (in.forceApMode || in.savedCount == 0)
            return _wcStartAP(s, in.nowMs);            // pinned AP, or nothing to try → lifeline
        return _wcStartConnecting(s, in.nowMs, false);

    case WC_CONNECTING:
        if (in.associated && in.hasIp) {               // success gate: associated AND IP
            s.phase = WC_CLIENT_UP;
            s.failCount = 0;
            d.becameConnected = true;
            return d;
        }
        if (in.nowMs - s.budgetStartMs >= cfg.bootBudgetMs)
            return _wcStartAP(s, in.nowMs);            // budget spent → lifeline (guaranteed)
        if (in.nowMs - s.attemptStartMs >= cfg.perNetworkTimeoutMs) {
            s.currentIndex = (in.savedCount > 0) ? (s.currentIndex + 1) % in.savedCount : 0;
            s.attemptStartMs = in.nowMs;               // this network timed out → try the next
            d.action = WCA_BEGIN_STA;
            d.networkIndex = s.currentIndex;
            return d;
        }
        return d;                                      // still waiting

    case WC_CLIENT_UP:
        if (in.associated && in.hasIp) {
            s.failCount = 0;                           // healthy
            return d;
        }
        if (s.failCount < 0xFF)                        // a down-poll — tolerate transient blips
            s.failCount++;
        if (s.failCount >= cfg.flapThreshold)
            return _wcStartConnecting(s, in.nowMs, true); // really lost → reconnect cycle
        return d;

    case WC_AP:
        if (in.rejoinCandidateSeen)                    // a saved network reappeared
            return _wcStartConnecting(s, in.nowMs, true);
        if (!in.apClientConnected &&
            in.nowMs - s.lastScanMs >= cfg.rescanIntervalMs) {
            s.lastScanMs = in.nowMs;                   // kick a cheap async probe
            d.action = WCA_START_SCAN;
            return d;
        }
        return d;                                      // hosting AP, nothing to do
    }
    return d;
}
```

- [ ] **Step 2: Write the failing host test**

Create `test/host/test_wifi_connect_core.cpp`:

```cpp
// Host unit tests for wifi_connect_core.h — no external deps, tiny assert harness.
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
#include "../../src/wifi/wifi_connect_core.h"
#include <cstdio>

static int g_fail = 0, g_total = 0;
#define CHECK(cond) do { g_total++; if(!(cond)){ g_fail++; \
  printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} } while(0)

// Build an input with sane defaults; callers tweak the fields they care about.
static WcInput mkInput(uint32_t now, size_t saved, bool assoc, bool ip) {
  WcInput in;
  in.nowMs = now; in.enabled = true; in.associated = assoc; in.hasIp = ip;
  in.savedCount = saved; in.apClientConnected = false;
  in.rejoinCandidateSeen = false; in.forceApMode = false;
  return in;
}

static void test_disabled_does_nothing() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  WcInput in = mkInput(1000, 2, false, false); in.enabled = false;
  WcDecision d = wcStep(s, in, c);
  CHECK(d.action == WCA_NONE && s.phase == WC_IDLE);
}

static void test_no_saved_goes_ap() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  WcDecision d = wcStep(s, mkInput(1000, 0, false, false), c);
  CHECK(d.action == WCA_START_AP && d.becameConnected && s.phase == WC_AP);
}

static void test_force_ap_mode_with_saved() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  WcInput in = mkInput(1000, 2, false, false); in.forceApMode = true;
  WcDecision d = wcStep(s, in, c);
  CHECK(d.action == WCA_START_AP && s.phase == WC_AP);
}

static void test_saved_begins_sta_then_connects() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  WcDecision d = wcStep(s, mkInput(1000, 2, false, false), c);
  CHECK(d.action == WCA_BEGIN_STA && d.networkIndex == 0 && s.phase == WC_CONNECTING);
  // still waiting a moment later
  d = wcStep(s, mkInput(1500, 2, false, false), c);
  CHECK(d.action == WCA_NONE && s.phase == WC_CONNECTING);
  // associated AND ip -> client up
  d = wcStep(s, mkInput(2000, 2, true, true), c);
  CHECK(d.action == WCA_NONE && d.becameConnected && s.phase == WC_CLIENT_UP);
}

static void test_associated_without_ip_is_failure() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 2, false, false), c);              // BEGIN index 0
  // associated but NO ip: not success, and before per-network timeout -> keep waiting
  WcDecision d = wcStep(s, mkInput(5000, 2, true, false), c);
  CHECK(d.action == WCA_NONE && s.phase == WC_CONNECTING);
  // per-network timeout (9s) with still no ip -> advance to next network
  d = wcStep(s, mkInput(1000 + 9000, 2, true, false), c);
  CHECK(d.action == WCA_BEGIN_STA && d.networkIndex == 1);
}

static void test_per_network_timeout_wraps() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 2, false, false), c);              // BEGIN 0, attemptStart=1000
  WcDecision d = wcStep(s, mkInput(10001, 2, false, false), c);
  CHECK(d.action == WCA_BEGIN_STA && d.networkIndex == 1);   // advance 0->1
  d = wcStep(s, mkInput(19002, 2, false, false), c);
  CHECK(d.action == WCA_BEGIN_STA && d.networkIndex == 0);   // wrap 1->0
}

static void test_boot_budget_falls_back_to_ap() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 2, false, false), c);              // budgetStart=1000
  WcDecision d = wcStep(s, mkInput(1000 + 30000, 2, false, false), c);
  CHECK(d.action == WCA_START_AP && d.becameConnected && s.phase == WC_AP);
}

static void test_client_flap_tolerated_then_dropped() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 2, false, false), c);
  wcStep(s, mkInput(2000, 2, true, true), c);                // -> WC_CLIENT_UP
  WcDecision d = wcStep(s, mkInput(3000, 2, false, false), c); // down 1
  CHECK(d.action == WCA_NONE && !d.becameDisconnected && s.phase == WC_CLIENT_UP);
  d = wcStep(s, mkInput(4000, 2, false, false), c);          // down 2
  CHECK(d.action == WCA_NONE && !d.becameDisconnected && s.phase == WC_CLIENT_UP);
  d = wcStep(s, mkInput(5000, 2, false, false), c);          // down 3 -> drop
  CHECK(d.action == WCA_BEGIN_STA && d.becameDisconnected && s.phase == WC_CONNECTING);
}

static void test_client_recovers_resets_failcount() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 2, false, false), c);
  wcStep(s, mkInput(2000, 2, true, true), c);                // WC_CLIENT_UP
  wcStep(s, mkInput(3000, 2, false, false), c);              // down 1
  wcStep(s, mkInput(4000, 2, true, true), c);                // recover -> failCount 0
  WcDecision d = wcStep(s, mkInput(5000, 2, false, false), c); // down 1 again, still tolerated
  CHECK(d.action == WCA_NONE && s.phase == WC_CLIENT_UP && s.failCount == 1);
}

static void test_ap_rescans_on_interval() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 0, false, false), c);              // -> WC_AP, lastScan=1000
  WcDecision d = wcStep(s, mkInput(1000 + 15000, 0, false, false), c);
  CHECK(d.action == WCA_NONE);                               // before rescanInterval (30s)
  d = wcStep(s, mkInput(1000 + 30000, 0, false, false), c);
  CHECK(d.action == WCA_START_SCAN);                         // interval elapsed
}

static void test_ap_pauses_rescan_with_client() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 0, false, false), c);              // -> WC_AP
  WcInput in = mkInput(1000 + 60000, 0, false, false); in.apClientConnected = true;
  WcDecision d = wcStep(s, in, c);
  CHECK(d.action == WCA_NONE);                               // paused while a client is on the AP
}

static void test_ap_rejoin_candidate_reconnects() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 0, false, false), c);              // -> WC_AP
  WcInput in = mkInput(2000, 1, false, false); in.rejoinCandidateSeen = true;
  WcDecision d = wcStep(s, in, c);
  CHECK(d.action == WCA_BEGIN_STA && d.becameDisconnected && s.phase == WC_CONNECTING);
}

int main() {
  test_disabled_does_nothing();
  test_no_saved_goes_ap();
  test_force_ap_mode_with_saved();
  test_saved_begins_sta_then_connects();
  test_associated_without_ip_is_failure();
  test_per_network_timeout_wraps();
  test_boot_budget_falls_back_to_ap();
  test_client_flap_tolerated_then_dropped();
  test_client_recovers_resets_failcount();
  test_ap_rescans_on_interval();
  test_ap_pauses_rescan_with_client();
  test_ap_rejoin_candidate_reconnects();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
```

- [ ] **Step 3: Run the test to verify it fails (header not yet present → compile error, then passes once Step 1 is in place)**

Run (from repo root):
```bash
clang++ -std=c++17 -Wall -Wextra -O0 test/host/test_wifi_connect_core.cpp -o /tmp/rseries_wifi_test && /tmp/rseries_wifi_test
```
Expected BEFORE Step 1's header exists: FAIL to compile (`file not found: ../../src/wifi/wifi_connect_core.h`). After Step 1 exists: `OK  <N>/<N> checks passed`, exit 0. (If any `CHECK` fails it prints `FAIL <file>:<line> <expr>` and the summary line starts with `FAILURES`.)

- [ ] **Step 4: Register the test as a stage in `test/host/run.sh`**

In `test/host/run.sh`, after the stage-1 block (the two lines that compile+run `test_contract_core.cpp`, currently lines 14-16), insert a new stage. Match the existing style exactly:

```bash
echo "[1b/4] wifi_connect_core FSM unit tests"
clang++ -std=c++17 -Wall -Wextra -O0 test_wifi_connect_core.cpp -o /tmp/rseries_wifi_test
/tmp/rseries_wifi_test
```
(`run.sh` `cd`s into `test/host/` first, so the filename is bare. There is no auto-discovery — this line is what makes the test run under `bash test/host/run.sh`.)

- [ ] **Step 5: Run the full host suite to verify green**

Run (from repo root):
```bash
bash test/host/run.sh
```
Expected: stage `[1b/4]` prints `OK  <N>/<N> checks passed`; the whole script proceeds (stage 4 cross-compile is optional and SKIPs cleanly if PlatformIO is absent). `set -e` means any failing stage aborts — so reaching the end is the pass signal.

- [ ] **Step 6: Commit**

```bash
git add src/wifi/wifi_connect_core.h test/host/test_wifi_connect_core.cpp test/host/run.sh
git commit -m "feat(wifi): pure connect-else-AP FSM + host tests"
```

---

### Task 2: Rewrite `WifiAccess::animate()` as the non-blocking adapter

**Files:**
- Modify: `lib/Reeltwo/src/wifi/WifiAccess.h` (add include + members + setters; replace `animate()` body at `:168-475`)

**Interfaces:**
- Consumes (from Task 1): `WcState`, `WcConfig`, `WcInput`, `WcDecision`, `wcDefaultConfig()`, `wcStep()`, and the `WcAction`/`WcPhase` enums.
- Produces: `WifiAccess` retains its existing public surface (`isConnected()`, `isSoftAP()`, `getIPAddress()`, `addSavedNetwork()`, `getSavedNetworks()`, `setAPFallback()`, `notifyWifiConnected/Disconnected()`), plus new setters `setPerNetworkTimeout(uint32_t)`, `setRescanInterval(uint32_t)`, `setFlapThreshold(uint8_t)`, `setForceAPMode(bool)`. `setAPFallbackTimeout(uint32_t)` now also sets the STA connect budget.

- [ ] **Step 1: Add the pure-core include**

In `lib/Reeltwo/src/wifi/WifiAccess.h`, after the existing includes (after line 35, `#include <algorithm>`), add:

```cpp
// DroidNet fork: pure, host-tested connect-else-AP decision FSM. File-relative path (this is a
// vendored lib header; the quoted include resolves against this file's own directory, so it does
// not depend on any -I flag). lib/Reeltwo/src/wifi/ -> repo root is four levels up.
#include "../../../../src/wifi/wifi_connect_core.h"
```

- [ ] **Step 2: Add the FSM members and config setters**

In the `protected:` section, after the existing multi-network members (after line 625, `bool fScanInProgress;`), add:

```cpp
    // DroidNet fork: non-blocking connect-else-AP FSM state + tunable policy. The FSM
    // (wifi_connect_core.h) owns all connection decisions; this class only reads the WiFi
    // driver and executes them. Zero-initialized WcState -> wcStep() inits it on first tick.
    WcState  fWc{};
    WcConfig fWcCfg = wcDefaultConfig();
    bool     fRejoinCandidate = false; // a completed background scan saw a saved SSID
    bool     fForceApMode = false;     // boot pinned AP-only (honored only at WC_IDLE)
```

In the `public:` section, near `setAPFallbackTimeout` (line 530), add the new setters and update `setAPFallbackTimeout`:

```cpp
    void setAPFallbackTimeout(uint32_t timeout)
    {
        fAPFallbackTimeout = timeout;      // retained for API compatibility
        fWcCfg.bootBudgetMs = timeout;     // this is now the total STA connect budget
    }

    void setPerNetworkTimeout(uint32_t ms) { fWcCfg.perNetworkTimeoutMs = ms; }
    void setRescanInterval(uint32_t ms)    { fWcCfg.rescanIntervalMs = ms; }
    void setFlapThreshold(uint8_t n)       { fWcCfg.flapThreshold = n; }
    void setForceAPMode(bool on)           { fForceApMode = on; }
```
(Delete the old `setAPFallbackTimeout` at line 530-533 — it is replaced by the version above.)

- [ ] **Step 3: Replace the blocking `animate()` body**

Replace the entire `virtual void animate()` method (lines 168-475 — from `virtual void animate()` through its closing `}`) with:

```cpp
    virtual void animate()
    {
        if (!fWifiEnabled)
            return;

        // Non-blocking connect-else-AP. All decisions come from the pure, host-tested FSM in
        // wifi_connect_core.h; this method only READS the WiFi driver and EXECUTES the returned
        // action. This runs in the render AnimatedEvent chain and MUST NOT block: no delay(), no
        // synchronous scan, no status spin.
        updateRejoinCandidate();   // consume any completed background scan (async, non-blocking)

        WcInput in;
        in.nowMs = millis();
        in.enabled = fWifiEnabled;
        in.associated = WiFi.isConnected();
        in.hasIp = (WiFi.localIP() != IPAddress((uint32_t)0));
        in.savedCount = fSavedNetworks.size();
        in.apClientConnected = (fWifiSoftAP && WiFi.softAPgetStationNum() > 0);
        in.rejoinCandidateSeen = fRejoinCandidate;
        in.forceApMode = fForceApMode;

        WcDecision d = wcStep(fWc, in, fWcCfg);

        switch (d.action)
        {
        case WCA_BEGIN_STA:
            fWifiSoftAP = false;
            fRejoinCandidate = false;
            if (WiFi.getMode() != WIFI_STA)
                WiFi.mode(WIFI_STA);
            if (d.networkIndex < fSavedNetworks.size())
            {
                const WifiNetwork& net = fSavedNetworks[d.networkIndex];
                WiFi.begin(net.ssid.c_str(), net.password.c_str());
            }
            else if (!fWifiAP.isEmpty())
            {
                WiFi.begin(fWifiAP.c_str(), fWifiPassword.c_str());
            }
            break;
        case WCA_START_AP:
            fWifiSoftAP = true;
            fRejoinCandidate = false;
            WiFi.mode(WIFI_AP_STA); // AP + STA so the background rejoin scan can still run
            WiFi.softAP(fWifiAP.c_str(), fWifiPassword.c_str());
            break;
        case WCA_START_SCAN:
            startScan();            // existing helper: async WiFi.scanNetworks(true)
            break;
        case WCA_NONE:
        default:
            break;
        }

        if (d.becameConnected)
        {
            fWifiActive = true;
            notifyConnected();
        }
        if (d.becameDisconnected)
        {
            fWifiActive = false;
            notifyDisconnected();
        }
    }

    // Consume a completed background scan (started by WCA_START_SCAN) and set fRejoinCandidate
    // if any saved SSID is currently visible. Non-blocking: isScanComplete() only reports true
    // once the async scan engine has already finished; it never waits.
    void updateRejoinCandidate()
    {
        if (fRejoinCandidate)
            return;
        if (isScanComplete())
        {
            for (const auto& scanned : fScannedNetworks)
            {
                for (const auto& saved : fSavedNetworks)
                {
                    if (scanned.ssid == saved.ssid)
                    {
                        fRejoinCandidate = true;
                        return;
                    }
                }
            }
        }
    }
```

- [ ] **Step 4: Verify the firmware compiles for the real MCU**

Run (from repo root; needs PlatformIO — install with `pip install platformio` if absent, since firmware changes cannot otherwise be compile-checked):
```bash
pio run -e wlogicengine32
```
Expected: `SUCCESS` link (a `.pio/build/wlogicengine32/firmware.bin` is produced). This is the same stage-4 check `run.sh` runs; it validates the file-relative include resolves and the adapter uses the WiFi/ReelTwo API correctly. (The `wlogicengine32s3` env cannot be built locally — upstream toolchain package — so it is validated only on-device in Task 7.)

- [ ] **Step 5: Confirm the notify→`wifiActive` invariant is intact**

Run:
```bash
grep -n "wifiActive = true" src/main.cpp && grep -n "notifyWifiConnected" src/main.cpp
```
Expected: `main.cpp:960: wifiActive = true;` inside the `wifiAccess.notifyWifiConnected(...)` lambda (line 956) is unchanged. The rewritten `animate()` still calls `notifyConnected()` on both client-up and AP-up, so web/OTA (gated on `wifiActive` in `loop()`) keeps working.

- [ ] **Step 6: Commit**

```bash
git add lib/Reeltwo/src/wifi/WifiAccess.h
git commit -m "feat(wifi): non-blocking WifiAccess adapter driving the connect FSM"
```

---

### Task 3: Boot — auto-connect-else-AP instead of AP-first

**Files:**
- Modify: `src/main.cpp:886-939` (the `else if (wifiEnabled)` boot block)

**Interfaces:**
- Consumes: `WifiAccess::setForceAPMode()`, `setEnabled()`, `getSavedNetworks()` from Task 2.
- Produces: on boot, the board attempts saved networks whenever any exist (unless the user explicitly pinned AP-only), else hosts AP.

**Context:** Today `WIFI_ACCESS_POINT` defaults to `true` (`main.cpp:76`), so a board comes up as its own AP unless the user pressed "Connect" (`wifi_force_sta`) — i.e. joining is manual. The feature requires the board to try saved networks automatically. The AP lifeline is preserved by the FSM's bounded budget (Task 1/2).

- [ ] **Step 1: Rewrite the mode-decision block**

In `src/main.cpp`, within `#ifdef USE_WIFI_WEB` (lines 889-939), replace from `// Check if we should start in AP mode` (line 889) through the end of the `if (startInAPMode) { … } else { … }` block (line 939) with:

```cpp
        // Auto connect-else-AP: attempt saved networks whenever we have them, and only host our
        // own AP when there are none — UNLESS the user explicitly pinned AP-only. The old model
        // required pressing "Connect" (wifi_force_sta) to join; the FSM now decides.
        bool forceSTAMode = preferences.getBool("wifi_force_sta", false);
        if (forceSTAMode) {
            preferences.remove("wifi_force_sta"); // one-shot flag, clear it
            DEBUG_PRINTLN("Forcing STA mode due to Connect button");
        }

        // Load saved WiFi networks from NVS
        loadWifiNetworks(wifiAccess);

        // The user has pinned AP-only iff they explicitly set PREFERENCE_WIFI_AP to true.
        bool apPinnedByUser = preferences.isKey(PREFERENCE_WIFI_AP) &&
                              preferences.getBool(PREFERENCE_WIFI_AP, WIFI_ACCESS_POINT);
        bool haveSaved = !wifiAccess.getSavedNetworks().empty();
        bool startInAPMode = !forceSTAMode && (apPinnedByUser || !haveSaved);

        // AP fallback credentials (the lifeline) — used whenever the FSM hosts its own AP.
        wifiAccess.setAPFallback(
            preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME),
            preferences.getString(PREFERENCE_WIFI_PASS, WIFI_AP_PASSPHRASE));

        // Connect policy. bootBudget comes from the existing PREFERENCE_WIFI_TIMEOUT (default
        // lowered from 60s to 30s per the design — lights render fine throughout, so this only
        // bounds time-to-reachability; auto-rejoin means AP fallback is never permanent).
        uint32_t wifiTimeout = preferences.getInt(PREFERENCE_WIFI_TIMEOUT, 30);
        wifiAccess.setAPFallbackTimeout(wifiTimeout * 1000);
        wifiAccess.setForceAPMode(startInAPMode);
        wifiAccess.setEnabled(true);
        DEBUG_PRINTLN(startInAPMode ? "WiFi: hosting AP (no saved networks or AP pinned)"
                                    : "WiFi: attempting saved networks (auto connect-else-AP)");
```

(Note: the FSM handles the actual AP-vs-STA start on the first `animate()` tick — boot no longer calls `setNetworkCredentials(...,accessPoint=true,...)`. `setForceAPMode(true)` makes the FSM go straight to AP; `false` makes it try saved networks then fall back within the budget.)

- [ ] **Step 2: Verify the firmware still compiles**

Run:
```bash
pio run -e wlogicengine32
```
Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat(wifi): boot auto-connects to saved networks, AP only as fallback"
```

---

### Task 4: Delete the blocking diagnostic scan in `loop()`

**Files:**
- Modify: `src/main.cpp:1358-1369` (inside the 30 s `[C2B5_DEBUG]` block in `loop()`)

**Context:** `loop()` (the Arduino task, currently core 1) runs a `WiFi.scanNetworks(false, false, false, 100)` (~1.4 s, blocking) every 30 s while disconnected. It is an independent hitch, and — critically — a **prerequisite for Task 5**: once rendering moves to core 1, this blocking scan would land on the same core as rendering and reintroduce a freeze. Delete the scan; keep the rest of the debug print.

- [ ] **Step 1: Remove the scan, keep the surrounding debug**

In `src/main.cpp`, in the `else` branch of the 30 s debug block (currently lines 1353-1370), replace:

```cpp
            } else {
                Serial.println("[C2B5_DEBUG] Not connected!");
                Serial.print("[C2B5_DEBUG] Saved networks: ");
                Serial.println(wifiAccess.getSavedNetworks().size());
                
                // Quick scan to see what networks are visible
                Serial.println("[C2B5_DEBUG] Performing quick scan...");
                int n = WiFi.scanNetworks(false, false, false, 100); // Quick scan, 100ms per channel
                Serial.print("[C2B5_DEBUG] Networks found: ");
                Serial.println(n);
                for (int i = 0; i < n && i < 5; i++) {
                    Serial.print("[C2B5_DEBUG]   ");
                    Serial.print(WiFi.SSID(i));
                    Serial.print(" (");
                    Serial.print(WiFi.RSSI(i));
                    Serial.println(" dBm)");
                }
            }
```
with:
```cpp
            } else {
                Serial.println("[C2B5_DEBUG] Not connected!");
                Serial.print("[C2B5_DEBUG] Saved networks: ");
                Serial.println(wifiAccess.getSavedNetworks().size());
                // NOTE: the old blocking WiFi.scanNetworks(false,...) here was removed — it froze
                // the render task once rendering moved to core 1, and duplicated the FSM's async
                // rejoin scan. Visibility of nearby networks is available via the async scan path.
            }
```

- [ ] **Step 2: Verify the firmware compiles**

Run:
```bash
pio run -e wlogicengine32
```
Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "fix(wifi): remove blocking diagnostic scan from loop() (freeze source)"
```

---

### Task 5: Move the render task to core 1

**Files:**
- Modify: `src/main.cpp:1109-1116` (the `xTaskCreatePinnedToCore` for `eventLoopTask`)

**Context:** `eventLoopTask` runs `mainLoop()` → `AnimatedEvent::process()` (all LED rendering + the WiFi FSM `animate()`). It is pinned to **core 0**, the same core the ESP32 hard-pins the WiFi MAC/PHY to. Moving it to **core 1** shields rendering from WiFi preemption. This is safe because there is no `xPortGetCoreID`/core-affinity assumption in app code, and Task 4 already removed the only blocking call that ran on core 1.

- [ ] **Step 1: Change the core affinity from 0 to 1**

In `src/main.cpp`, in the `xTaskCreatePinnedToCore` call (lines 1109-1116), change the final argument (the core id) from `0` to `1`:

```cpp
    xTaskCreatePinnedToCore(
          eventLoopTask,
          "Events",
          5000,    // shrink stack size?
          NULL,
          1,
          &eventTask,
          1);      // render on core 1 — off core 0 where the WiFi MAC/PHY + lwIP live
```

- [ ] **Step 2: Verify the firmware compiles**

Run:
```bash
pio run -e wlogicengine32
```
Expected: `SUCCESS`. (Runtime correctness of the core move is a bench check — Task 7.)

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "perf(wifi): render on core 1 to shield LEDs from the WiFi stack"
```

---

### Task 6: Log the reset reason at boot (brownout rule-out)

**Files:**
- Modify: `src/main.cpp` (early in `setup()`, right after `Serial.begin`)

**Context:** The design's low-probability suspect (#3.5) is a WiFi-TX brownout. A one-line boot log of `esp_reset_reason()` lets the bench check (Task 7) rule it out by observing whether the board ever resets with `ESP_RST_BROWNOUT` during a connect burst.

- [ ] **Step 1: Find the Serial init in setup()**

Run:
```bash
grep -n "Serial.begin" src/main.cpp | head -1
```
Note the line number (call it `L`).

- [ ] **Step 2: Add the reset-reason log immediately after `Serial.begin(...)`**

Insert directly after that `Serial.begin(...)` line:

```cpp
    // DroidNet fork: log the reset cause at boot. If WiFi-TX current spikes ever brown out the
    // 3V3 rail during a connect burst, this prints ESP_RST_BROWNOUT (== 9) and points at power,
    // not firmware. Header <esp_system.h> is already pulled in transitively by the arduino-esp32
    // core; include it explicitly to be safe.
    {
        esp_reset_reason_t _rr = esp_reset_reason();
        Serial.print("[C2B5_DEBUG] Reset reason: ");
        Serial.println((int)_rr);
        if (_rr == ESP_RST_BROWNOUT)
            Serial.println("[C2B5_DEBUG] *** LAST RESET WAS A BROWNOUT (check 3V3 power) ***");
    }
```

If compilation reports `esp_reset_reason` undeclared, add `#include <esp_system.h>` near the top of `main.cpp` with the other includes.

- [ ] **Step 3: Verify the firmware compiles**

Run:
```bash
pio run -e wlogicengine32
```
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "chore(wifi): log reset reason at boot to rule out TX brownout"
```

---

### Task 7: On-device bench validation

**Files:** none (manual hardware validation — the gated final deliverable).

**Context:** Nothing in Tasks 1-6 has run on hardware. This task reproduces the original failure, then confirms the fix, on a real logic board. **Open item: confirm whether the deployed boards are `wlogicengine32` (esp32dev) or `wlogicengine32s3`** — the S3 cannot be cross-compiled locally, so if the fleet is S3, Tasks 2-6's `pio run` compile checks validate only the esp32dev image and the S3 build must be produced on/for the device by its normal deploy path.

- [ ] **Step 1: Reproduce the freeze on the pre-fix firmware.** Flash the current `main`-branch firmware to a board, power it with **no** saved network present (home WiFi off, Pi4 droidnet off). Confirm the reported symptom: lights hold/freeze for ~15 s, animate ~15 s, repeat. Record it (video/notes) as the baseline.

- [ ] **Step 2: Flash the fixed firmware** (this branch) to the same board, same conditions (no saved network reachable). **Expected:** lights run smoothly and continuously; within the connect budget (~30 s) the board comes up as its own `RSeries` AP at `192.168.4.1`; **no multi-second freeze at any point.**

- [ ] **Step 3: Micro-stutter check during connect.** With a saved network present, power the board and watch the LEDs during the association/DHCP window. **Expected:** no visible stutter (render is on core 1, isolated from the WiFi stack).

- [ ] **Step 4: Auto-rejoin (the headline behavior).** Start the board with the Pi4 droidnet **off** so it hosts its own AP. Then power the Pi4 droidnet on. **Expected:** within ~1-2 rescan intervals (~30-60 s) the board leaves its AP and joins droidnet, hands-off, with no freeze — and no client was on its AP during the switch.

- [ ] **Step 5: AP bootstrap on empty NVS.** On a board with no saved networks (fresh, or after `pio run -t erase`), confirm it hosts its `RSeries` AP so it can be web-provisioned; add a network via the web UI; power-cycle; confirm it now joins that network. Confirm creds survive an OTA update (no re-provision needed).

- [ ] **Step 6: Brownout rule-out.** Watch the serial log across several connect bursts. **Expected:** the `[C2B5_DEBUG] Reset reason:` line never reports `ESP_RST_BROWNOUT` (9). If it does, the fix is masking a power problem — escalate to the hardware/decoupling check in the design (§3.5).

- [ ] **Step 7: Record results and finish the branch.** Note pass/fail per step in the PR/commit description. If all pass, proceed to merge per the finishing-a-development-branch skill.

- [ ] **Step 8: Core-1 sharing under load.** With render now on core 1 alongside the Arduino `loop()` task that services `webServer.handle()`/`ArduinoOTA.handle()`, watch LED smoothness WHILE actively driving the web UI (loading/using the config page) AND during an OTA update. **Expected:** no visible stutter; if there is, web/OTA are not as near-idle as assumed and full core-0 isolation (§5.3) should be prioritized sooner.

- [ ] **Step 9: AP/STA flap check on an unreachable-but-visible saved SSID.** Arrange a saved SSID that is visible but not associable (wrong password, or the AP is up but its upstream is down) while the real target network is absent. **Expected/watch for:** the board may cycle ~30 s hosting its AP / ~30 s attempting STA, with no failed-rejoin backoff. Confirm this is either not occurring or is an acceptable amount of flapping.
