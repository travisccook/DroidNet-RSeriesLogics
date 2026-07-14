/* ==========================================================================
 * MODIFIED FROM UPSTREAM.
 *
 * Original: reeltwo/Reeltwo, src/wifi/WifiAccess.h, by Mimir Reynisson (Rimim).
 *           https://github.com/reeltwo/Reeltwo  — LGPL-2.1.
 *
 * This copy is NOT the upstream file. It was changed for the C2B5 droid build
 * (2024-2025, prior to this fork; first published here in 2026). The changes
 * replace upstream's single-network WifiAccess with a multi-network manager:
 *
 *   - saved-network list with priorities (addSavedNetwork / removeSavedNetwork /
 *     clearSavedNetworks / getSavedNetworks), plus a WifiNetwork struct;
 *   - network scanning (startScan / isScanComplete / getScannedNetworks /
 *     isScanInProgress), plus a ScannedNetwork struct;
 *   - explicit AP vs client mode handling, with a configurable AP-fallback
 *     timeout (setAPFallback / setAPFallbackTimeout) instead of upstream's
 *     unconditional softAP-or-join logic;
 *   - reworked animate(): a non-blocking, tick-polled adapter (DroidNet fork, 2026).
 *     Each call reads the WiFi driver (associated / hasIp / AP-client-count / background
 *     rejoin-scan result) and delegates the connect-or-AP decision to the pure, host-tested
 *     FSM in src/wifi/wifi_connect_core.h; animate() only executes the action the FSM
 *     returns. Connect attempts still walk the saved list and fall back to AP mode on a
 *     bounded budget, but every step now happens across ticks — all blocking scans and
 *     delay()-based waits from the earlier connection state machine have been removed.
 *
 * See README_C2B5.md at the repository root. Every other file under
 * lib/Reeltwo is unmodified upstream.
 *
 * This notice is required by LGPL-2.1 section 2(b).
 * ========================================================================== */
#ifndef WifiAccess_h
#define WifiAccess_h

#include "ReelTwo.h"
#include "core/AnimatedEvent.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <vector>
#include <algorithm>

// DroidNet fork: pure, host-tested connect-else-AP decision FSM. File-relative path (this is a
// vendored lib header; the quoted include resolves against this file's own directory, so it does
// not depend on any -I flag). lib/Reeltwo/src/wifi/ -> repo root is four levels up.
#include "../../../../src/wifi/wifi_connect_core.h"

/**
  * \ingroup wifi
  *
  * \class WifiWebServer
  *
  * \brief Enhanced WiFi manager with multi-network support
  *
  * \code
  * #include "wifi/WifiAccess.h"
  *
  * WifiAccess wifiAccess("MyAccessPoint", "MyPassword");
  * 
  * \endcode
  *
  */
class WifiAccess : public AnimatedEvent
{
public:
    struct WifiNetwork
    {
        String ssid;
        String password;
        int priority;
        
        WifiNetwork() : priority(0) {}
        WifiNetwork(const String& s, const String& p, int pr = 0) : 
            ssid(s), password(p), priority(pr) {}
    };
    
    struct ScannedNetwork
    {
        String ssid;
        int32_t rssi;
        bool encrypted;
        
        ScannedNetwork() : rssi(0), encrypted(false) {}
        ScannedNetwork(const String& s, int32_t r, bool e) : 
            ssid(s), rssi(r), encrypted(e) {}
    };
    /** \brief Constructor
      *
      * \param interval the number of milliseconds before attempting to reconnect
      */
    WifiAccess(uint32_t interval = 30000) :
        fWifiAP(""),
        fWifiPassword(""),
        fWifiInterval(interval),
        fWifiSoftAP(false),
        fWifiEnabled(false),
        fWifiAttemptReconnect(true),
        fWifiActive(false),
        fAPFallbackTimeout(60000),  // 1 minute fallback timeout
        fConnectionAttemptStart(0),
        fCurrentNetworkIndex(0),
        fFallbackToAP(true),
        fScanInProgress(false)
    {
    }

    /** \brief Constructor
      *
      * \param interval the number of milliseconds before attempting to reconnect
      */
    WifiAccess(const String& wifiAP, const String& wifiPassword, bool accessPoint = true, bool enabled = true, uint32_t interval = 30000) :
        fWifiAP(wifiAP),
        fWifiPassword(wifiPassword),
        fWifiInterval(interval),
        fWifiSoftAP(accessPoint),
        fWifiEnabled(enabled),
        fWifiAttemptReconnect(true),
        fWifiActive(false),
        fAPFallbackTimeout(60000),  // 1 minute fallback timeout
        fConnectionAttemptStart(0),
        fCurrentNetworkIndex(0),
        fFallbackToAP(true),
        fScanInProgress(false)
    {
    }

    void setNetworkCredentials(const String& wifiAP, const String& wifiPassword, bool accessPoint, bool enabled)
    {
        fWifiAP = wifiAP;
        fWifiPassword = wifiPassword;
        fWifiSoftAP = accessPoint;
        fWifiEnabled = enabled;
    }

    bool isConnected()
    {
    	bool connected = false;
        if (fWifiActive)
        {
        	connected = (fWifiSoftAP) ? true : WiFi.isConnected();
        }
        return connected;
    }

    IPAddress getIPAddress()
    {
        if (isConnected())
        {
        	return (fWifiSoftAP) ? WiFi.softAPIP() : WiFi.localIP();
        }
        return IPAddress();
    }

    String getMacAddress()
    {
    	return WiFi.macAddress();
    }

    String getWifiAP()
    {
    	return fWifiAP;
    }

    String getWifiPassword()
    {
    	return fWifiPassword;
    }

    bool isSoftAP()
    {
    	return fWifiSoftAP;
    }

    void setReconnect(bool reconnect)
    {
    	fWifiAttemptReconnect = reconnect;
    }

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

    inline bool enabled()
    {
    	return fWifiEnabled;
    }

    void setEnabled(bool enabled = true)
    {
    	fWifiEnabled = enabled;
    }
    
    // Multi-network management methods
    void addSavedNetwork(const String& ssid, const String& password, int priority = 0)
    {
        // Check if network already exists
        for (auto& net : fSavedNetworks)
        {
            if (net.ssid == ssid)
            {
                net.password = password;
                net.priority = priority;
                sortNetworksByPriority();
                return;
            }
        }
        fSavedNetworks.push_back(WifiNetwork(ssid, password, priority));
        sortNetworksByPriority();
    }
    
    void removeSavedNetwork(const String& ssid)
    {
        fSavedNetworks.erase(
            std::remove_if(fSavedNetworks.begin(), fSavedNetworks.end(),
                [&ssid](const WifiNetwork& net) { return net.ssid == ssid; }),
            fSavedNetworks.end()
        );
    }
    
    void clearSavedNetworks()
    {
        fSavedNetworks.clear();
    }
    
    const std::vector<WifiNetwork>& getSavedNetworks() const
    {
        return fSavedNetworks;
    }
    
    void setAPFallback(const String& apSSID, const String& apPassword)
    {
        fWifiAP = apSSID;
        fWifiPassword = apPassword;
    }
    
    void setAPFallbackTimeout(uint32_t timeout)
    {
        fAPFallbackTimeout = timeout;      // retained for API compatibility
        fWcCfg.bootBudgetMs = timeout;     // this is now the total STA connect budget
    }

    void setPerNetworkTimeout(uint32_t ms) { fWcCfg.perNetworkTimeoutMs = ms; }
    void setRescanInterval(uint32_t ms)    { fWcCfg.rescanIntervalMs = ms; }
    void setFlapThreshold(uint8_t n)       { fWcCfg.flapThreshold = n; }
    void setForceAPMode(bool on)           { fForceApMode = on; }

    // Network scanning
    bool startScan()
    {
        if (!fScanInProgress)
        {
            fScanInProgress = true;
            WiFi.scanNetworks(true); // Async scan
            return true;
        }
        return false;
    }
    
    bool isScanComplete()
    {
        if (fScanInProgress)
        {
            int n = WiFi.scanComplete();
            if (n >= 0)
            {
                fScanInProgress = false;
                fScannedNetworks.clear();
                
                for (int i = 0; i < n; i++)
                {
                    fScannedNetworks.push_back(
                        ScannedNetwork(WiFi.SSID(i), WiFi.RSSI(i), 
                                     WiFi.encryptionType(i) != WIFI_AUTH_OPEN)
                    );
                }
                WiFi.scanDelete();
                return true;
            }
        }
        return false;
    }
    
    const std::vector<ScannedNetwork>& getScannedNetworks() const
    {
        return fScannedNetworks;
    }
    
    bool isScanInProgress() const
    {
        return fScanInProgress;
    }

	class Notify
	{
	public:
		virtual void wifiConnected(WifiAccess &access) = 0;
		virtual void wifiDisconnected(WifiAccess &access) = 0;
	};

	void addNotify(WifiAccess::Notify* client)
	{
		fWifiNotify.push_back(client);
	}

	void notifyWifiConnected(void (*notify)(WifiAccess& wifi))
	{
		fNotifyConnectedCallback = notify;
	}

	void notifyWifiDisconnected(void (*notify)(WifiAccess& wifi))
	{
		fNotifyDisconnectedCallback = notify;
	}

protected:
    String fWifiAP;
    String fWifiPassword;
    uint32_t fWifiInterval;
    uint32_t fWifiPrevMillis = 0;
    bool fWifiSoftAP;
    bool fWifiStarted = false;
    bool fWifiEnabled = true;
	bool fWifiAttemptReconnect = true;
    bool fWifiActive = false;
    bool fWifiWasConnected = false;
    std::vector<Notify*> fWifiNotify;
    void (*fNotifyConnectedCallback)(WifiAccess& wifi) = nullptr;
    void (*fNotifyDisconnectedCallback)(WifiAccess& wifi) = nullptr;
    
    // Multi-network support
    std::vector<WifiNetwork> fSavedNetworks;
    std::vector<ScannedNetwork> fScannedNetworks;
    uint32_t fAPFallbackTimeout;
    uint32_t fConnectionAttemptStart;
    size_t fCurrentNetworkIndex;
    bool fFallbackToAP;
    bool fScanInProgress;

    // DroidNet fork: non-blocking connect-else-AP FSM state + tunable policy. The FSM
    // (wifi_connect_core.h) owns all connection decisions; this class only reads the WiFi
    // driver and executes them. Zero-initialized WcState -> wcStep() inits it on first tick.
    WcState  fWc{};
    WcConfig fWcCfg = wcDefaultConfig();
    bool     fRejoinCandidate = false; // a completed background scan saw a saved SSID
    bool     fForceApMode = false;     // boot pinned AP-only (honored only at WC_IDLE)

    void sortNetworksByPriority()
    {
        std::sort(fSavedNetworks.begin(), fSavedNetworks.end(),
            [](const WifiNetwork& a, const WifiNetwork& b) {
                return a.priority > b.priority; // Higher priority first
            });
    }

    void notifyConnected()
    {
    	for (auto client: fWifiNotify)
    	{
    		client->wifiConnected(*this);
    	}
    	if (fNotifyConnectedCallback != nullptr)
    		fNotifyConnectedCallback(*this);
    }

    void notifyDisconnected()
    {
    	if (fNotifyDisconnectedCallback != nullptr)
    		fNotifyDisconnectedCallback(*this);
    	for (auto client: fWifiNotify)
    	{
    		client->wifiDisconnected(*this);
    	}
    }
};

#endif
