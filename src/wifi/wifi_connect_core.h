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
