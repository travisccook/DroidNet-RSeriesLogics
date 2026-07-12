#ifndef WifiAccess_h
#define WifiAccess_h

#include "ReelTwo.h"
#include "core/AnimatedEvent.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <vector>
#include <algorithm>

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
            
        uint32_t now = millis();
        
        // If we're in soft AP mode and active, nothing to do
        if (fWifiSoftAP && fWifiActive)
            return;
            
        // Check if we're connected
        if (WiFi.isConnected())
        {
            if (!fWifiActive)
            {
                fWifiWasConnected = true;
                fWifiActive = true;
                fWifiSoftAP = false;
                fConnectionAttemptStart = 0; // Reset connection timer
                notifyConnected();
            }
            return;
        }
        
        // Not connected - handle disconnection if we were active
        if (fWifiActive)
        {
            notifyDisconnected();
            fWifiActive = false;
        }
        
        // If we're already in AP mode, start it
        if (fWifiSoftAP)
        {
            if (!fWifiActive)
            {
                DEBUG_PRINTLN("Starting WiFi Access Point: " + fWifiAP);
                WiFi.mode(WIFI_AP);
                WiFi.softAP(fWifiAP.c_str(), fWifiPassword.c_str());
                fWifiActive = true;
                notifyConnected();
            }
            return;
        }
        
        // Try to connect to saved networks
        if (!fWifiStarted || now - fWifiPrevMillis >= fWifiInterval)
        {
            // If this is our first attempt, record the start time
            if (fConnectionAttemptStart == 0)
            {
                // Check if we have any networks to try
                if (fSavedNetworks.empty() && fWifiAP.isEmpty())
                {
                    // No networks configured - go straight to AP mode
                    DEBUG_PRINTLN("No saved networks - starting AP mode immediately");
                    fWifiSoftAP = true;
                    return;
                }
                
                fConnectionAttemptStart = now;
                fCurrentNetworkIndex = 0;
                WiFi.mode(WIFI_STA);
            }
            
            // Check if we've exceeded the fallback timeout
            if (fFallbackToAP && fAPFallbackTimeout > 0 && 
                now - fConnectionAttemptStart >= fAPFallbackTimeout)
            {
                DEBUG_PRINT("Connection timeout after ");
                DEBUG_PRINT((now - fConnectionAttemptStart) / 1000);
                DEBUG_PRINTLN(" seconds - falling back to AP mode");
                fWifiSoftAP = true;
                fConnectionAttemptStart = 0;
                return;
            }
            
            // Try next network in the list
            if (!fSavedNetworks.empty() && fCurrentNetworkIndex < fSavedNetworks.size())
            {
                const WifiNetwork& network = fSavedNetworks[fCurrentNetworkIndex];
                DEBUG_PRINTLN("Attempting to connect to: " + network.ssid);
                DEBUG_PRINT("SSID: '");
                DEBUG_PRINT(network.ssid);
                DEBUG_PRINT("' (length: ");
                DEBUG_PRINT(network.ssid.length());
                DEBUG_PRINTLN(")");
                
                // Hex dump SSID to check for hidden characters
                DEBUG_PRINT("SSID hex: ");
                for (int i = 0; i < network.ssid.length(); i++) {
                    char c = network.ssid.charAt(i);
                    if (c < 16) DEBUG_PRINT("0");
                    DEBUG_PRINT(String(c, HEX));
                    DEBUG_PRINT(" ");
                }
                DEBUG_PRINTLN("");
                
                DEBUG_PRINT("Password: '");
                DEBUG_PRINT(network.password);
                DEBUG_PRINT("' (length: ");
                DEBUG_PRINT(network.password.length());
                DEBUG_PRINTLN(")");
                
                // Hex dump password to check for hidden characters
                DEBUG_PRINT("Password hex: ");
                for (int i = 0; i < network.password.length(); i++) {
                    char c = network.password.charAt(i);
                    if (c < 16) DEBUG_PRINT("0");
                    DEBUG_PRINT(String(c, HEX));
                    DEBUG_PRINT(" ");
                }
                DEBUG_PRINTLN("");
                
                // Quick scan to see current network details
                DEBUG_PRINTLN("Quick scan for target network...");
                int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
                DEBUG_PRINT("Scan returned: ");
                DEBUG_PRINTLN(n);
                
                bool networkFound = false;
                if (n == -2) {
                    DEBUG_PRINTLN("ERROR: Scan failed!");
                } else if (n == -1) {
                    DEBUG_PRINTLN("ERROR: Scan running!");
                } else if (n == 0) {
                    DEBUG_PRINTLN("WARNING: No networks found at all!");
                } else if (n > 0) {
                    DEBUG_PRINT("Scan found ");
                    DEBUG_PRINT(n);
                    DEBUG_PRINTLN(" networks:");
                    // Show ALL networks for debugging
                    for (int i = 0; i < n; i++) {
                        DEBUG_PRINT("  ");
                        if (WiFi.SSID(i).isEmpty()) {
                            DEBUG_PRINT("[HIDDEN] ");
                        } else {
                            DEBUG_PRINT("'");
                            DEBUG_PRINT(WiFi.SSID(i));
                            DEBUG_PRINT("'");
                        }
                        DEBUG_PRINT(" BSSID: ");
                        DEBUG_PRINT(WiFi.BSSIDstr(i));
                        DEBUG_PRINT(", Ch: ");
                        DEBUG_PRINT(WiFi.channel(i));
                        DEBUG_PRINT(", RSSI: ");
                        DEBUG_PRINT(WiFi.RSSI(i));
                        DEBUG_PRINT(", Enc: ");
                        wifi_auth_mode_t encType = WiFi.encryptionType(i);
                        DEBUG_PRINT(encType);
                        DEBUG_PRINT(" (");
                        switch(encType) {
                            case WIFI_AUTH_OPEN: DEBUG_PRINT("OPEN"); break;
                            case WIFI_AUTH_WEP: DEBUG_PRINT("WEP"); break;
                            case WIFI_AUTH_WPA_PSK: DEBUG_PRINT("WPA"); break;
                            case WIFI_AUTH_WPA2_PSK: DEBUG_PRINT("WPA2"); break;
                            case WIFI_AUTH_WPA_WPA2_PSK: DEBUG_PRINT("WPA/WPA2"); break;
                            case WIFI_AUTH_WPA2_ENTERPRISE: DEBUG_PRINT("WPA2_ENT"); break;
                            case WIFI_AUTH_WPA3_PSK: DEBUG_PRINT("WPA3"); break;
                            case WIFI_AUTH_WPA2_WPA3_PSK: DEBUG_PRINT("WPA2/WPA3"); break;
                            default: DEBUG_PRINT("UNKNOWN"); break;
                        }
                        DEBUG_PRINTLN(")");
                        
                        if (WiFi.SSID(i) == network.ssid) {
                            networkFound = true;
                            DEBUG_PRINTLN("  ^-- TARGET NETWORK FOUND!");
                        }
                    }
                    WiFi.scanDelete();
                }
                
                if (!networkFound && n > 0) {
                    DEBUG_PRINTLN("WARNING: Target network not visible in scan - it might be hidden");
                }
                
                // Show current WiFi settings
                DEBUG_PRINT("WiFi mode: ");
                DEBUG_PRINTLN(WiFi.getMode());
                DEBUG_PRINT("Auto reconnect: ");
                DEBUG_PRINTLN(WiFi.getAutoReconnect());
                DEBUG_PRINT("TX Power: ");
                DEBUG_PRINTLN(WiFi.getTxPower());
                
                DEBUG_PRINTLN("Calling WiFi.begin()...");
                
                // Try connecting with proper setup
                DEBUG_PRINTLN("Setting up WiFi for connection...");
                WiFi.disconnect(true, false);  // disconnect but keep WiFi on
                WiFi.mode(WIFI_STA);
                WiFi.setSleep(false);  // Disable power saving
                WiFi.setAutoReconnect(false);  // We'll handle reconnect ourselves
                delay(100);
                
                // Get BSSID from scan if available
                uint8_t* targetBSSID = NULL;
                uint8_t bssid[6];
                int targetChannel = 0;
                wifi_auth_mode_t targetEncryption = WIFI_AUTH_OPEN;
                
                // Re-scan to get fresh BSSID
                int scanResult = WiFi.scanNetworks(false, true);
                if (scanResult > 0) {
                    for (int i = 0; i < scanResult; i++) {
                        if (WiFi.SSID(i) == network.ssid) {
                            targetChannel = WiFi.channel(i);
                            targetEncryption = WiFi.encryptionType(i);
                            uint8_t* scanBSSID = WiFi.BSSID(i);
                            if (scanBSSID != NULL) {
                                memcpy(bssid, scanBSSID, 6);
                                targetBSSID = bssid;
                            }
                            DEBUG_PRINT("Found network on channel ");
                            DEBUG_PRINT(targetChannel);
                            DEBUG_PRINT(" with BSSID: ");
                            DEBUG_PRINT(WiFi.BSSIDstr(i));
                            DEBUG_PRINT(" Encryption: ");
                            DEBUG_PRINTLN(targetEncryption);
                            
                            // Warn about WPA vs WPA2 mismatch
                            if (targetEncryption == WIFI_AUTH_WPA_PSK) {
                                DEBUG_PRINTLN("WARNING: Network is using WPA (not WPA2). This may cause connection issues.");
                            }
                            break;
                        }
                    }
                    WiFi.scanDelete();
                }
                
                // Try connecting with BSSID if we found it
                wl_status_t status;
                if (targetBSSID != NULL) {
                    DEBUG_PRINTLN("Trying with specific BSSID and channel...");
                    
                    // First try: normal connection with BSSID
                    status = WiFi.begin(network.ssid.c_str(), network.password.c_str(), targetChannel, targetBSSID, false);
                    DEBUG_PRINT("First attempt status: ");
                    DEBUG_PRINTLN(status);
                    
                    // Wait a bit to see if it connects
                    delay(2000);
                    
                    if (WiFi.status() != WL_CONNECTED) {
                        DEBUG_PRINTLN("First attempt failed, trying without BSSID...");
                        WiFi.disconnect(true, false);
                        delay(100);
                        
                        // Second try: without BSSID (sometimes helps with WPA issues)
                        status = WiFi.begin(network.ssid.c_str(), network.password.c_str());
                        DEBUG_PRINT("Second attempt status: ");
                        DEBUG_PRINTLN(status);
                    }
                } else {
                    DEBUG_PRINTLN("Trying without BSSID (network not in scan)...");
                    status = WiFi.begin(network.ssid.c_str(), network.password.c_str());
                }
                
                DEBUG_PRINT("WiFi.begin() returned status: ");
                DEBUG_PRINTLN(status);
                DEBUG_PRINT("WiFi.status() is now: ");
                DEBUG_PRINTLN(WiFi.status());
                
                // Give it more time to connect
                DEBUG_PRINTLN("Waiting for connection...");
                for (int i = 0; i < 20; i++) {
                    delay(500);
                    wl_status_t currentStatus = WiFi.status();
                    if (currentStatus != WL_DISCONNECTED && currentStatus != WL_NO_SSID_AVAIL) {
                        DEBUG_PRINT("Status changed to: ");
                        DEBUG_PRINTLN(currentStatus);
                        break;
                    }
                    if (i % 4 == 0) {
                        DEBUG_PRINT(".");
                    }
                }
                DEBUG_PRINTLN("");
                DEBUG_PRINT("Final WiFi.status(): ");
                DEBUG_PRINTLN(WiFi.status());
                fWifiStarted = true;
                fWifiPrevMillis = now;
                
                // Move to next network for next attempt
                fCurrentNetworkIndex++;
                if (fCurrentNetworkIndex >= fSavedNetworks.size())
                {
                    fCurrentNetworkIndex = 0; // Wrap around to try all networks again
                    DEBUG_PRINTLN("Tried all networks, cycling back to first");
                }
            }
            else if (!fWifiAP.isEmpty())
            {
                // Fall back to legacy single network if no saved networks
                DEBUG_PRINTLN("Attempting to connect to: " + fWifiAP);
                WiFi.begin(fWifiAP.c_str(), fWifiPassword.c_str());
                fWifiStarted = true;
                fWifiPrevMillis = now;
            }
            else if (fFallbackToAP)
            {
                // No networks to try - go to AP mode
                DEBUG_PRINTLN("No saved networks - starting AP mode");
                fWifiSoftAP = true;
                fConnectionAttemptStart = 0;
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
        fAPFallbackTimeout = timeout;
    }
    
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
