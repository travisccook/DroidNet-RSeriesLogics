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

static void test_ap_rejoin_paused_while_client_connected() {
  WcState s{}; WcConfig c = wcDefaultConfig();
  wcStep(s, mkInput(1000, 0, false, false), c);            // -> WC_AP
  WcInput in = mkInput(2000, 1, false, false);
  in.rejoinCandidateSeen = true; in.apClientConnected = true;
  WcDecision d = wcStep(s, in, c);
  CHECK(d.action == WCA_NONE && !d.becameDisconnected && s.phase == WC_AP); // did NOT drop the AP/client
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
  test_ap_rejoin_paused_while_client_connected();
  printf("%s  %d/%d checks passed\n", g_fail ? "FAILURES" : "OK", g_total - g_fail, g_total);
  return g_fail ? 1 : 0;
}
