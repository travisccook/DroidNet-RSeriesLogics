#ifdef USE_WIFI_WEB

#include "web-images.h"
#include "build/generated/modern_ui_html.h"
#include <algorithm>
#include <vector>
#include <cstdio>

// Forward declarations
void saveWifiNetworks(WifiAccess& wifi);

// Helper function to decode URL-encoded strings
String urlDecode(const String& encoded) {
    String decoded = "";
    char temp[] = "0x00";
    for (size_t i = 0; i < encoded.length(); i++) {
        if (encoded[i] == '%') {
            if (i + 2 < encoded.length()) {
                temp[2] = encoded[i + 1];
                temp[3] = encoded[i + 2];
                decoded += (char)strtol(temp, NULL, 16);
                i += 2;
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

////////////////////////////////
// List of available sequences by name and matching id
enum
{
    kMAX_FADE = 15,
    kMAX_DELAY = 500,
    kMIN_DELAY = 10,
    kMIN_BRI = 10,

    kMAX_ADJLOOP = 90000,
    kMIN_ADJLOOP = 500,
};

WMenuData mainMenu[] = {
    { "Logics", "/logics" },
    { "Setup", "/setup" },
    { "C2B5 UI", "/m/" }
};

WMenuData setupMenu[] = {
    { "Home", "/" },
    { "Logics", "/setuplogics" },
    { "Marcduino", "/marcduino" },
    { "WiFi", "/wifi" },
    { "Remote", "/remote" },
    { "Firmware", "/firmware" },
    { "Back", "/" }
};

WElement mainContents[] = {
    WVerticalMenu("menu", mainMenu, SizeOfArray(mainMenu)),
    rseriesSVG
};


WElement setupContents[] = {
    WVerticalMenu("setup", setupMenu, SizeOfArray(setupMenu)),
    rseriesSVG
};

String fldBoards[] = {
    "None",
    "RSeries 40 LED",
    "RSeries 40 LED Inverted",
    "C3PO 40 LED",
    "Naboo 48 LED",
    "RGBW 40 LED"
};

String rldBoards[] = {
    "None",
    "Imahara 112 LED",
    "Imahara 112 LED Inverted",
    "DeathStar 96 LED",
    "DeathStar 96 LED StaggerOdd",
    "DeathStar 96 LED Inverted",
    "DeathStar 96 LED Inverted StaggerOdd",
    "Kenny 96 LED",
    "Naboo 96 LED",
    "Super 256 LED",
};

String logicsSeq[] = {
#define LOGICENGINE_SEQ(nam, val) \
    BUILTIN_SEQ(nam, LogicEngineDefaults::val)
#define BUILTIN_SEQ(nam, val) \
    nam,

#include "logic-sequences.h"

#undef BUILTIN_SEQ
#undef LOGICENGINE_SEQ
};

unsigned logicsSeqNumber[] = {
#define LOGICENGINE_SEQ(nam, val) \
    BUILTIN_SEQ(nam, LogicEngineDefaults::val)
#define BUILTIN_SEQ(nam, val) \
    val,

#include "logic-sequences.h"

#undef BUILTIN_SEQ
#undef LOGICENGINE_SEQ
};

String logicsColors[] = {
    "Default",
    "Red",
    "Orange",
    "Yellow",
    "Green",
    "Cyan",
    "Blue",
    "Purple",
    "Magenta",
    "Pink"
};

bool sFLDChanged = true;
bool sRLDChanged = true;

int sFLDSequence;
int sRLDSequence;

String sFLDText = "";
String sRLDText = "";
String sFLDDisplayText;
String sRLDDisplayText;

int sFLDColor = LogicEngineRenderer::kDefault;
int sRLDColor = LogicEngineRenderer::kDefault;

int sFLDSpeedScale;
int sRLDSpeedScale;

int sFLDNumSeconds;
int sRLDNumSeconds;

/////////////////////////////////////////////////////////////////////////
// Web Interface for logic engine animation sequences
WElement logicsContents[] = {
    WSelect("Front Logic Sequence", "frontseq",
        logicsSeq, SizeOfArray(logicsSeq),
        []() { return sFLDSequence; },
        [](int val) { sFLDSequence = val; sFLDChanged = true; } ),
    WSelect("Front Color", "frontcolor",
        logicsColors, SizeOfArray(logicsColors),
        []() { return sFLDColor; },
        [](int val) { sFLDColor = val; sFLDChanged = true; } ),
    WSlider("Animation Speed", "fldspeed", 0, 9,
        []()->int { return sFLDSpeedScale; },
        [](int val) { sFLDSpeedScale = val; sFLDChanged = true; } ),
    WSlider("Number of seconds", "fldseconds", 0, 99,
        []()->int { return sFLDNumSeconds; },
        [](int val) { sFLDNumSeconds = val; sFLDChanged = true; } ),
    WTextField("Front Text:", "fronttext",
        []()->String { return sFLDText; },
        [](String val) { sFLDText = val; sFLDChanged = true; } ),
    WSelect("Rear Logic Sequence", "rearseq",
        logicsSeq, SizeOfArray(logicsSeq),
        []() { return sRLDSequence; },
        [](int val) { sRLDSequence = val; sRLDChanged = true; } ),
    WSelect("Rear Color", "rearcolor",
        logicsColors, SizeOfArray(logicsColors),
        []() { return sRLDColor; },
        [](int val) { sRLDColor = val; sRLDChanged = true; } ),
    WSlider("Animation Speed", "rldspeed", 0, 9,
        []()->int { return sRLDSpeedScale; },
        [](int val) { sRLDSpeedScale = val; sRLDChanged = true; } ),
    WSlider("Number of seconds", "rldseconds", 0, 99,
        []()->int { return sRLDNumSeconds; },
        [](int val) { sRLDNumSeconds = val; sRLDChanged = true; } ),
    WTextField("Rear Text:", "reartext",
        []()->String { return sRLDText; },
        [](String val) { sRLDText = val; sRLDChanged = true; } ),
    WButton("Run", "run", []() {
        if (sFLDChanged)
        {
            sFLDDisplayText = sFLDText;
            sFLDDisplayText.replace("\\n", "\n");
            if (FLD != nullptr)
                FLD->resetEffect();
            FLD_selectSequence(logicsSeqNumber[sFLDSequence], (LogicEngineRenderer::ColorVal)sFLDColor, sFLDSpeedScale, sFLDNumSeconds);
            FLD_setTextMessage(sFLDDisplayText.c_str());
            sFLDChanged = false;
        }
        if (sRLDChanged)
        {
            sRLDDisplayText = sRLDText;
            sRLDDisplayText.replace("\\n", "\n");
            if (RLD != nullptr)
                RLD->resetEffect();
            RLD_selectSequence(logicsSeqNumber[sRLDSequence], (LogicEngineRenderer::ColorVal)sRLDColor, sRLDSpeedScale, sRLDNumSeconds);
            RLD_setTextMessage(sRLDDisplayText.c_str());
            sRLDChanged = false;
        }
    }),
    WHorizontalAlign(),
    WButton("Back", "back", "/"),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    rseriesSVG
};

////////////////////////////////

String swBaudRates[] = {
    "2400",
    "9600",
    "57600",
    "115200",
};

int swBaudRatesInt[] = {
    2400,
    9600,
    57600,
    115200,
};

bool isValidBaudRate(int baudRate) {
    for (int i = 0; i < SizeOfArray(swBaudRatesInt); i++)
    {
        if (baudRate == swBaudRatesInt[i]) {
            return true;
        }
    }
    return false;
}

int swBaudRateToIndex(int baudRate)
{
    for (int i = 0; i < SizeOfArray(swBaudRatesInt); i++)
    {
        if (baudRate == swBaudRatesInt[i])
            return i;
    }
    return 0;
}

int marcSerial1Baud;
int marcSerial2Baud;
bool marcSerialPass;
bool marcSerialEnabled;
bool marcWifiEnabled;
bool marcWifiSerialPass;

WElement marcduinoContents[] = {
    WSelect("Serial1 Baud Rate", "serial1baud",
        swBaudRates, SizeOfArray(swBaudRates),
        []() { return (marcSerial1Baud = swBaudRateToIndex(preferences.getInt(PREFERENCE_MARCSERIAL1, MARC_SERIAL1_BAUD_RATE))); },
        [](int val) { marcSerial1Baud = swBaudRatesInt[val]; }),
    WVerticalAlign(),
    WSelect("Serial2 Baud Rate", "serial2baud",
        swBaudRates, SizeOfArray(swBaudRates),
        []() { return (marcSerial2Baud = swBaudRateToIndex(preferences.getInt(PREFERENCE_MARCSERIAL2, MARC_SERIAL2_BAUD_RATE))); },
        [](int val) { marcSerial2Baud = swBaudRatesInt[val]; }),
    WVerticalAlign(),
    WCheckbox("Serial1 pass-through to Serial2", "serialpass",
        []() { return (marcSerialPass = (preferences.getBool(PREFERENCE_MARCSERIAL_PASS, MARC_SERIAL_PASS))); },
        [](bool val) { marcSerialPass = val; } ),
    WVerticalAlign(),
    WCheckbox("Marcduino on Serial1", "enabled",
        []() { return (marcSerialEnabled = (preferences.getBool(PREFERENCE_MARCSERIAL_ENABLED, MARC_SERIAL_ENABLED))); },
        [](bool val) { marcSerialEnabled = val; } ),
    WVerticalAlign(),
    WCheckbox("Marcduino on Wifi (port 2000)", "wifienabled",
        []() { return (marcWifiEnabled = (preferences.getBool(PREFERENCE_MARCWIFI_ENABLED, MARC_WIFI_ENABLED))); },
        [](bool val) { marcWifiEnabled = val; } ),
    WVerticalAlign(),
    WCheckbox("Marcduino Wifi pass-through to Serial2", "wifipass",
        []() { return (marcWifiSerialPass = (preferences.getBool(PREFERENCE_MARCWIFI_SERIAL_PASS, MARC_WIFI_SERIAL_PASS))); },
        [](bool val) { marcWifiSerialPass = val; } ),
    WVerticalAlign(),
    WButton("Save", "save", []() {
        preferences.putInt(PREFERENCE_MARCSERIAL1, marcSerial1Baud);
        preferences.putInt(PREFERENCE_MARCSERIAL2, marcSerial2Baud);
        preferences.putBool(PREFERENCE_MARCSERIAL_PASS, marcSerialPass);
        preferences.putBool(PREFERENCE_MARCSERIAL_ENABLED, marcSerialEnabled);
        preferences.putBool(PREFERENCE_MARCWIFI_ENABLED, marcWifiEnabled);
        preferences.putBool(PREFERENCE_MARCWIFI_SERIAL_PASS, marcWifiSerialPass);
    }),
    WHorizontalAlign(),
    WButton("Back", "back", "/setup"),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

WElement setupLogicsContents[] = {
    WSelect("Front Logic Engine Type", "fld",
        fldBoards, SizeOfArray(fldBoards),
        []() { return (int)sFLDType; },
        [](int val) { FLD_selectType((LogicEngineFLDType)val); } ),
    WSelect("Rear Logic Engine Type", "rld",
        rldBoards, SizeOfArray(rldBoards),
        []() { return (int)sRLDType; },
        [](int val) { RLD_selectType((LogicEngineRLDType)val); } ),
#ifdef USE_CONTROLLER
    WSlider("Front Brightness", "fldbri", 0, 255,
        []()->int { return sController.getSettings(0).fBri; },
        [](int val) { sController.changeSetting(0, 1, val); } ),
    WSlider("Front Hue", "fldhue", 0, 255,
        []()->int { return sController.getSettings(0).fHue; },
        [](int val) { sController.changeSetting(0, 2, val); } ),
    WSlider("Front Fade", "fldfad", 0, 255,
        []()->int { return sController.getSettings(0).fFade; },
        [](int val) { sController.changeSetting(0, 3, val); } ),
    WSlider("Front Pause", "fldpau", 0, 255,
        []()->int { return sController.getSettings(0).fDelay; },
        [](int val) { sController.changeSetting(0, 4, val); } ),
    WSlider("Front Palette", "fldpal", 0, LogicEngineDefaults::PAL_COUNT,
        []()->int { return sController.getSettings(0).fPalNum; },
        [](int val) { sController.changeSetting(0, 5, val); } ),
    WSlider("Rear Brightness", "rearbri", 0, 255,
        []()->int { return sController.getSettings(1).fBri; },
        [](int val) { sController.changeSetting(1, 1, val); } ),
    WSlider("Rear Hue", "rearhue", 0, 255,
        []()->int { return sController.getSettings(1).fHue; },
        [](int val) { sController.changeSetting(1, 2, val); } ),
    WSlider("Rear Fade", "rearfae", 0, 255,
        []()->int { return sController.getSettings(1).fFade; },
        [](int val) { sController.changeSetting(1, 3, val); } ),
    WSlider("Rear Pause", "rearpau", 0, 255,
        []()->int { return sController.getSettings(1).fDelay; },
        [](int val) { sController.changeSetting(1, 4, val); } ),
    WSlider("Rear Palette", "rearpal", 0, LogicEngineDefaults::PAL_COUNT,
        []()->int { return sController.getSettings(1).fPalNum; },
        [](int val) { sController.changeSetting(1, 5, val); } ),
#endif
    WButton("Save", "save", []() {
        preferences.putUChar(PREFERENCE_FLD, sFLDType);
        preferences.putUChar(PREFERENCE_RLD, sRLDType);
    #ifdef USE_CONTROLLER
        sController.commit();
    #endif
    }),
    WHorizontalAlign(),
    WButtonReload("Defaults", "default", []() {
    #ifdef USE_CONTROLLER
        sController.restoreFactoryDefaults(0);
        sController.restoreFactoryDefaults(1);
    #endif
    }),
    WHorizontalAlign(),
    WButton("Back", "back", "/setup"),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

String wifiSSID;
String wifiPass;
bool wifiAP;
String mdnsHostname;
int wifiTimeout;

// Define W2 for subheadings
class W2 : public WElement
{
public:
    W2(String title)
    {
        appendBody("<h2>"+String(title)+"</h2>");
    }
};

// Display current saved networks - Dynamic version
class SavedNetworksDynamic : public WDynamic
{
public:
    void emitBody(Print& out) const override
    {
        out.print("<div class='savednetworks'>");
        out.print("<style>");
        out.print(".savednetworks ul { list-style: none; padding: 0; }");
        out.print(".savednetworks button { margin: 0 5px; padding: 5px 10px; border: none; border-radius: 3px; cursor: pointer; }");
        out.print(".savednetworks button:hover { opacity: 0.8; }");
        out.print(".savednetworks input[type='number'] { border: 1px solid #ccc; border-radius: 3px; padding: 3px; }");
        out.print("</style>");
        out.print("<h3>Saved Networks</h3>");
        out.print("<p>Networks are tried in priority order (highest first):</p>");
        out.print("<ul>");
        
        // Display saved networks
        auto savedNetworks = wifiAccess.getSavedNetworks();
        DEBUG_PRINTLN("SavedNetworksDynamic: Found " + String(savedNetworks.size()) + " saved networks");
        
        for (const auto& net : savedNetworks)
        {
            DEBUG_PRINTLN("Displaying network: " + net.ssid + " (Priority: " + String(net.priority) + ")");
            // HTML escape the SSID for display
            String displaySSID = "";
            for (int i = 0; i < net.ssid.length(); i++) {
                char c = net.ssid[i];
                if (c == '&') {
                    displaySSID += "&amp;";
                } else if (c == '<') {
                    displaySSID += "&lt;";
                } else if (c == '>') {
                    displaySSID += "&gt;";
                } else if (c == '"') {
                    displaySSID += "&quot;";
                } else if (c == '\'') {
                    displaySSID += "&#39;";
                } else {
                    displaySSID += c;
                }
            }
            // Escape SSID for JavaScript
            String jsEscapedSSID = "";
            for (int i = 0; i < net.ssid.length(); i++) {
                char c = net.ssid[i];
                if (c == '\\') {
                    jsEscapedSSID += "\\\\";
                } else if (c == '\'') {
                    jsEscapedSSID += "\\'";
                } else if (c == '\"') {
                    jsEscapedSSID += "\\\"";
                } else if (c == '\n') {
                    jsEscapedSSID += "\\n";
                } else if (c == '\r') {
                    jsEscapedSSID += "\\r";
                } else if (c == '\t') {
                    jsEscapedSSID += "\\t";
                } else if (c < 32 || c > 126) {
                    // Escape any other control characters or non-ASCII
                    char hexBuf[5];
                    sprintf(hexBuf, "\\x%02x", (unsigned char)c);
                    jsEscapedSSID += hexBuf;
                } else {
                    jsEscapedSSID += c;
                }
            }
            
            // Create a unique ID for this network entry
            String networkId = "net_" + String(savedNetworks.size() - (&net - &savedNetworks[0]) - 1);
            
            out.print("<li style='margin: 10px 0; padding: 10px; background: #f5f5f5; border-radius: 5px;'>");
            out.print("<div style='display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap;'>");
            out.print("<span style='font-weight: bold; margin-right: 10px;'>" + displaySSID + "</span>");
            out.print("<div style='display: flex; align-items: center; gap: 10px;'>");
            out.print("<span>Priority: </span>");
            out.print("<input type='number' id='priority_" + networkId + "' min='1' max='10' value='" + String(net.priority) + "' ");
            out.print("style='width: 50px;' onchange=\"updatePriority('" + jsEscapedSSID + "', this.value)\" />");
            out.print("<button onclick=\"connectToNetwork('" + jsEscapedSSID + "')\" style='background: #4CAF50; color: white; padding: 5px 10px;'>Connect</button>");
            out.print("<button onclick=\"removeNetwork('" + jsEscapedSSID + "')\" style='background: #f44336; color: white; padding: 5px 10px;'>Remove</button>");
            out.print("</div>");
            out.print("</div>");
            out.print("</li>");
        }
        
        if (savedNetworks.empty())
        {
            out.print("<li>No saved networks</li>");
        }
        
        out.print("</ul>");
        out.print("</div>");
    }
};

// Variables for new network form
String newNetworkSSID;
String newNetworkPassword;  
int newNetworkPriority = 5;

// Create an instance of the dynamic saved networks display
SavedNetworksDynamic savedNetworksDynamic;

WElement wifiContents[] = {
    W1("WiFi Network Manager"),
    WCheckbox("WiFi Enabled", "enabled",
        []() { return wifiEnabled; },
        [](bool val) { wifiEnabled = val; } ),
    WHR(),
    W2("WiFi Mode"),
    WHTML(F("<div style='margin: 10px 0;'>"
            "<label style='margin-right: 20px;'>"
            "<input type='radio' name='wifimode' id='mode_client' value='client' onchange='updateWifiMode(this.value)'>"
            " Client Mode (Connect to saved networks)"
            "</label>"
            "<label>"
            "<input type='radio' name='wifimode' id='mode_ap' value='ap' onchange='updateWifiMode(this.value)'>"
            " Access Point Mode (Create hotspot)"
            "</label>"
            "</div>"
            "<div id='mode_warning' style='color: #ff6666; display: none; margin: 10px 0;'>"
            "⚠️ Cannot use Client Mode without saved networks. Please add a network first."
            "</div>")),
    WHR(),
    W2("Access Point Settings"),
    WLabel("These settings are used when no saved networks are available", "aplabel"),
    WTextField("AP Name:", "apname",
        []()->String { return (wifiSSID = preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME)); },
        [](String val) { wifiSSID = val; } ),
    WPassword("AP Password:", "appass",
        []()->String { return (wifiPass = preferences.getString(PREFERENCE_WIFI_PASS, WIFI_AP_PASSPHRASE)); },
        [](String val) { wifiPass = val; } ),
    WTextFieldIntegerRange("Connection Timeout (seconds):", "timeout", 10, 300,
        []()->String { return String(wifiTimeout = preferences.getInt(PREFERENCE_WIFI_TIMEOUT, 60)); },
        [](String val) { wifiTimeout = val.toInt(); } ),
    WLabel("Time to try connecting to saved networks before switching to AP mode", "timeoutlabel"),
    WHR(),
    W2("mDNS Settings"),
    WTextField("Hostname:", "mdns",
        []()->String { 
            mdnsHostname = preferences.getString(PREFERENCE_MDNS_HOSTNAME, "");
            if (mdnsHostname.isEmpty()) {
                String mac = WiFi.macAddress();
                mdnsHostname = mac.substring(mac.length()-5, mac.length());
                mdnsHostname.remove(2, 1);
                mdnsHostname = String(WIFI_AP_NAME)+String("-")+mdnsHostname;
            }
            return mdnsHostname;
        },
        [](String val) { mdnsHostname = val; } ),
    WLabel("Access device at: http://[hostname].local", "mdnslabel"),
    WHR(),
    W2("WiFi Status"),
    WHTML(F("<div id='wifistatus' style='background:#f5f5f5; padding:10px; border-radius:5px; margin:10px 0;'>"
            "<p>Loading WiFi status...</p>"
            "</div>")),
    WHTML(F("<button onclick='refreshWifiStatus()' class='btn'>Refresh Status</button>")),
    WHR(),
    W2("Saved Networks"),
    WDynamicElement(savedNetworksDynamic),
    WHR(),
    W2("Add New Network"),
    WHTML(F("<button onclick=\"scanForNetworks()\" class=\"btn\">Scan for Networks</button>")),
    WHTML(F("<div id=\"scanresults\" style=\"margin: 10px 0;\"></div>")),
    WHR(),
    WTextField("Network SSID:", "newssid",
        []()->String { return newNetworkSSID; },
        [](String val) { 
            DEBUG_PRINTLN("SSID field updated: [" + val + "]");
            newNetworkSSID = val; 
        } ),
    WPassword("Password:", "newpass",
        []()->String { return newNetworkPassword; },
        [](String val) { 
            DEBUG_PRINTLN("Password field updated");
            newNetworkPassword = val; 
        } ),
    WSlider("Priority", "newpriority", 1, 10,
        []()->int { return newNetworkPriority; },
        [](int val) { newNetworkPriority = val; } ),
    WButtonReload("Add Network", "addnet", []() {
        DEBUG_PRINTLN("=== ADD NETWORK BUTTON CLICKED ===");
        DEBUG_PRINTLN("Raw SSID value: [" + newNetworkSSID + "]");
        DEBUG_PRINTLN("Raw Password value: " + String(newNetworkPassword.isEmpty() ? "[empty]" : "[set]"));
        DEBUG_PRINTLN("SSID length: " + String(newNetworkSSID.length()));
        DEBUG_PRINTLN("Priority: " + String(newNetworkPriority));
        
        if (!newNetworkSSID.isEmpty()) {
            // Trim whitespace from SSID but preserve internal spaces
            newNetworkSSID.trim();
            DEBUG_PRINTLN("Trimmed SSID: [" + newNetworkSSID + "]");
            DEBUG_PRINTLN("Adding network to wifiAccess...");
            
            // Before adding, let's see what networks are already saved
            DEBUG_PRINTLN("Current saved networks count: " + String(wifiAccess.getSavedNetworks().size()));
            
            wifiAccess.addSavedNetwork(newNetworkSSID, newNetworkPassword, newNetworkPriority);
            
            // Check if the network was actually added
            DEBUG_PRINTLN("After adding, saved networks count: " + String(wifiAccess.getSavedNetworks().size()));
            
            DEBUG_PRINTLN("Saving WiFi networks to preferences...");
            saveWifiNetworks(wifiAccess);
            DEBUG_PRINTLN("Network saved successfully");
            
            // Clear the form fields
            newNetworkSSID = "";
            newNetworkPassword = "";
            newNetworkPriority = 5;
        } else {
            DEBUG_PRINTLN("SSID is empty, not adding network");
        }
        DEBUG_PRINTLN("=== END ADD NETWORK ===");
    }),
    WHR(),
    WHTML(F("<input type='hidden' id='wifimode_hidden' value='ap'>")),
    WHTML(F("<button id='savesettings_btn' class='btn' onclick='saveWifiSettings()'>Save Settings</button>")),
    WHorizontalAlign(),
    WButtonReload("Clear All Networks", "clear", []() {
        wifiAccess.clearSavedNetworks();
        saveWifiNetworks(wifiAccess);
    }),
    WJavaScript(
        "var wifiModeSelection = 'ap';\n"
        "function updateWifiMode(mode) {\n"
        "  wifiModeSelection = mode;\n"
        "  document.getElementById('wifimode_hidden').value = mode;\n"
        "  console.log('WiFi mode selected:', mode);\n"
        "}\n"
        "function saveWifiSettings() {\n"
        "  // Check if we can use client mode\n"
        "  var hasSavedNetworks = document.querySelectorAll('.network-card').length > 0;\n"
        "  if (wifiModeSelection === 'client' && !hasSavedNetworks) {\n"
        "    alert('Cannot use Client Mode without saved networks. Please add a network first.');\n"
        "    return;\n"
        "  }\n"
        "  \n"
        "  // Trigger all field updates to sync with server\n"
        "  var fields = ['enabled', 'apname_fld', 'appass_fld', 'timeout_fld', 'mdns_fld'];\n"
        "  fields.forEach(function(id) {\n"
        "    var elem = document.getElementById(id);\n"
        "    if (elem) {\n"
        "      var event = new Event('change', { bubbles: true });\n"
        "      elem.dispatchEvent(event);\n"
        "    }\n"
        "  });\n"
        "  \n"
        "  // Send the mode selection\n"
        "  setTimeout(function() {\n"
        "    window.location.href = '/wifisavesettings?mode=' + wifiModeSelection;\n"
        "  }, 200);\n"
        "}\n"
        "var scanTimeout;\n"
        "function scanForNetworks() {\n"
        "  var resultsDiv = document.getElementById('scanresults');\n"
        "  resultsDiv.innerHTML = '<p>Starting WiFi scan...</p>';\n"
        "  startWifiScan();\n"
        "}\n"
        "function startWifiScan() {\n"
        "  clearTimeout(scanTimeout);\n"
        "  scanTimeout = setTimeout(function() {\n"
        "    var resultsDiv = document.getElementById('scanresults');\n"
        "    resultsDiv.innerHTML = '<p>Scan in progress... (this may take 5-10 seconds)</p>';\n"
        "    setTimeout(function() { window.location.href = '/wifiscan'; }, 3000);\n"
        "  }, 1000);\n"
        "}\n"
        "function removeNetwork(ssid) {\n"
        "  if (confirm('Remove network \"' + ssid + '\"?')) {\n"
        "    var xhr = new XMLHttpRequest();\n"
        "    xhr.open('GET', '/wifiremove?ssid=' + encodeURIComponent(ssid), true);\n"
        "    xhr.onload = function() {\n"
        "      if (xhr.status === 200) {\n"
        "        alert('Network removed successfully');\n"
        "        location.reload();\n"
        "      } else {\n"
        "        alert('Failed to remove network');\n"
        "      }\n"
        "    };\n"
        "    xhr.send();\n"
        "  }\n"
        "}\n"
        "function updatePriority(ssid, priority) {\n"
        "  var xhr = new XMLHttpRequest();\n"
        "  xhr.open('GET', '/wifiupdatepriority?ssid=' + encodeURIComponent(ssid) + '&priority=' + priority, true);\n"
        "  xhr.onload = function() {\n"
        "    if (xhr.status === 200) {\n"
        "      console.log('Priority updated successfully');\n"
        "    } else {\n"
        "      alert('Failed to update priority');\n"
        "    }\n"
        "  };\n"
        "  xhr.send();\n"
        "}\n"
        "function connectToNetwork(ssid) {\n"
        "  if (confirm('Connect to network \"' + ssid + '\"?')) {\n"
        "    var xhr = new XMLHttpRequest();\n"
        "    xhr.open('GET', '/wificonnect?ssid=' + encodeURIComponent(ssid), true);\n"
        "    xhr.onload = function() {\n"
        "      if (xhr.status === 200) {\n"
        "        alert('Attempting to connect to ' + ssid + '. The device will restart.');\n"
        "        setTimeout(function() { window.location.href = '/'; }, 3000);\n"
        "      } else {\n"
        "        alert('Failed to initiate connection');\n"
        "      }\n"
        "    };\n"
        "    xhr.send();\n"
        "  }\n"
        "}\n"
        "function refreshWifiStatus() {\n"
        "  var xhr = new XMLHttpRequest();\n"
        "  xhr.open('GET', '/wifistatus', true);\n"
        "  xhr.onload = function() {\n"
        "    if (xhr.status === 200) {\n"
        "      document.getElementById('wifistatus').innerHTML = xhr.responseText;\n"
        "    }\n"
        "  };\n"
        "  xhr.send();\n"
        "}\n"
        "// Custom function to handle Add Network button\n"
        "function addNetworkCustom() {\n"
        "  var ssidField = document.getElementById('newssid_fld');\n"
        "  var passField = document.getElementById('newpass_fld');\n"
        "  var prioritySlider = document.getElementById('newpriority_slider');\n"
        "  \n"
        "  console.log('Add Network Custom clicked');\n"
        "  console.log('SSID:', ssidField.value);\n"
        "  console.log('Password:', passField.value ? '[set]' : '[empty]');\n"
        "  console.log('Priority:', prioritySlider.value);\n"
        "  \n"
        "  // Trigger change events to ensure values are synced with server\n"
        "  var changeEvent = new Event('change', { bubbles: true });\n"
        "  ssidField.dispatchEvent(changeEvent);\n"
        "  passField.dispatchEvent(changeEvent);\n"
        "  prioritySlider.dispatchEvent(changeEvent);\n"
        "  \n"
        "  // Wait a bit for the values to be sent, then trigger the button action\n"
        "  setTimeout(function() {\n"
        "    console.log('Calling pressed_addnet()');\n"
        "    pressed_addnet();\n"
        "  }, 100);\n"
        "}\n"
        "// Override the button handler on load\n"
        "window.addEventListener('load', function() {\n"
        "  var addButton = document.getElementById('addnet_btn');\n"
        "  if (addButton) {\n"
        "    console.log('Overriding Add Network button handler');\n"
        "    addButton.onclick = addNetworkCustom;\n"
        "    // Reset button text in case it was changed\n"
        "    addButton.textContent = 'Add Network';\n"
        "  }\n"
        "  \n"
        "  // Initialize WiFi mode radio buttons\n"
        "  var hasSavedNetworks = document.querySelectorAll('.network-card').length > 0;\n"
        "  var isAPMode = " + String(preferences.getBool(PREFERENCE_WIFI_AP, WIFI_ACCESS_POINT) ? "true" : "false") + ";\n"
        "  \n"
        "  if (isAPMode || !hasSavedNetworks) {\n"
        "    document.getElementById('mode_ap').checked = true;\n"
        "    wifiModeSelection = 'ap';\n"
        "  } else {\n"
        "    document.getElementById('mode_client').checked = true;\n"
        "    wifiModeSelection = 'client';\n"
        "  }\n"
        "  \n"
        "  // Show warning if trying to use client mode without networks\n"
        "  document.getElementById('mode_client').addEventListener('change', function() {\n"
        "    if (this.checked && !hasSavedNetworks) {\n"
        "      document.getElementById('mode_warning').style.display = 'block';\n"
        "      document.getElementById('mode_ap').checked = true;\n"
        "      wifiModeSelection = 'ap';\n"
        "    } else {\n"
        "      document.getElementById('mode_warning').style.display = 'none';\n"
        "    }\n"
        "  });\n"
        "  \n"
        "  // Refresh WiFi status on page load\n"
        "  refreshWifiStatus();\n"
        "});\n"
        "// Check for selected network from scan page\n"
        "window.addEventListener('load', function() {\n"
        "  var selectedSSID = sessionStorage.getItem('selectedSSID');\n"
        "  var selectedPassword = sessionStorage.getItem('selectedPassword');\n"
        "  console.log('Page loaded - checking sessionStorage');\n"
        "  console.log('selectedSSID:', selectedSSID);\n"
        "  console.log('selectedPassword:', selectedPassword ? '[hidden]' : 'null');\n"
        "  if (selectedSSID) {\n"
        "    // Decode the SSID in case it was URL-encoded\n"
        "    try {\n"
        "      selectedSSID = decodeURIComponent(selectedSSID);\n"
        "      console.log('Decoded SSID:', selectedSSID);\n"
        "    } catch (e) {\n"
        "      console.log('SSID was not URL-encoded');\n"
        "    }\n"
        "    console.log('Setting SSID field to:', selectedSSID);\n"
        "    var ssidField = document.getElementById('newssid_fld');\n"
        "    var passField = document.getElementById('newpass_fld');\n"
        "    \n"
        "    // Set the field values\n"
        "    ssidField.value = selectedSSID;\n"
        "    if (selectedPassword) {\n"
        "      passField.value = selectedPassword;\n"
        "    }\n"
        "    \n"
        "    // Clear sessionStorage\n"
        "    sessionStorage.removeItem('selectedSSID');\n"
        "    sessionStorage.removeItem('selectedPassword');\n"
        "    \n"
        "    // Trigger the onchange event to update server-side values\n"
        "    // Create and dispatch change events\n"
        "    var changeEvent = new Event('change', { bubbles: true });\n"
        "    console.log('Triggering change event for SSID field');\n"
        "    ssidField.dispatchEvent(changeEvent);\n"
        "    \n"
        "    if (selectedPassword) {\n"
        "      console.log('Triggering change event for password field');\n"
        "      passField.dispatchEvent(changeEvent);\n"
        "    }\n"
        "    \n"
        "    // Focus on the Add Network button to make it clear the fields are populated\n"
        "    setTimeout(function() {\n"
        "      var addButton = document.getElementById('addnet_btn');\n"
        "      if (addButton) {\n"
        "        console.log('Focusing on Add Network button');\n"
        "        addButton.focus();\n"
        "        // Also update button text to indicate fields are populated\n"
        "        addButton.textContent = 'Add Network (' + selectedSSID + ')';\n"
        "      } else {\n"
        "        console.log('Add Network button not found');\n"
        "      }\n"
        "    }, 100);\n"
        "  } else {\n"
        "    console.log('No selected SSID in sessionStorage');\n"
        "  }\n"
        "});\n"
    ),
    WHorizontalAlign(),
    WButton("Back", "back", "/setup"),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG,
    WHR(),
    W2("Debug Information"),
    WHTML(F("<div id='debug' style='background: #333; padding: 10px; font-family: monospace; font-size: 12px; white-space: pre-wrap; max-height: 300px; overflow-y: auto;'></div>")),
    WHTML(F("<div id='jsource' style='background: #222; padding: 10px; font-family: monospace; font-size: 10px; white-space: pre-wrap; max-height: 400px; overflow-y: auto; display: none;'></div>")),
    WHTML(F("<button onclick='document.getElementById(\"jsource\").style.display=\"block\"; this.style.display=\"none\";' style='margin-top: 10px;'>Show Page Source</button>")),
    WHTML(F("<script>"
            "window.addEventListener('error', function(e) {"
            "  var debug = document.getElementById('debug');"
            "  debug.innerHTML += 'ERROR: ' + e.message + ' at line ' + e.lineno + ' col ' + e.colno + '\\n';"
            "  debug.innerHTML += 'File: ' + e.filename + '\\n\\n';"
            "});"
            "window.addEventListener('load', function() {"
            "  var debug = document.getElementById('debug');"
            "  debug.innerHTML += 'Page loaded successfully\\n';"
            "  debug.innerHTML += 'Total script tags: ' + document.getElementsByTagName('script').length + '\\n';"
            "  debug.innerHTML += 'Functions defined:\\n';"
            "  for (var prop in window) {"
            "    if (typeof window[prop] === 'function' && "
            "        (prop.startsWith('update') || prop.startsWith('pressed') || "
            "         prop === 'scanForNetworks' || prop === 'removeNetwork' || "
            "         prop === 'updatePriority' || prop === 'connectToNetwork' || "
            "         prop === 'addNetworkCustom' || prop === 'selectNetwork')) {"
            "      debug.innerHTML += '  - ' + prop + '\\n';"
            "    }"
            "  }"
            "  debug.innerHTML += '\\nDocument ready state: ' + document.readyState + '\\n';"
            "  debug.innerHTML += 'Current URL: ' + window.location.pathname + '\\n';"
            "  "
            "  var jsource = document.getElementById('jsource');"
            "  var scripts = document.getElementsByTagName('script');"
            "  var lineNum = 1;"
            "  var fullSource = '';"
            "  for (var i = 0; i < scripts.length; i++) {"
            "    if (scripts[i].innerHTML) {"
            "      var lines = scripts[i].innerHTML.split('\\n');"
            "      fullSource += '=== SCRIPT BLOCK ' + (i+1) + ' (starts at line ' + lineNum + ') ===\\n';"
            "      for (var j = 0; j < lines.length; j++) {"
            "        fullSource += (lineNum + j) + ': ' + lines[j].replace(/</g, '&lt;').replace(/>/g, '&gt;') + '\\n';"
            "      }"
            "      lineNum += lines.length;"
            "      fullSource += '\\n';"
            "    }"
            "  }"
            "  jsource.innerHTML = fullSource;"
            "  debug.innerHTML += '\\nCheck line 374 in the source view for the syntax error.\\n';"
            "});"
            "</script>"))
};

////////////////////////////////

// Custom dynamic class for WiFi scan results
class WifiScanDynamic : public WDynamic
{
public:
    void emitBody(Print& out) const override
    {
        // Emit the selectNetwork function first so it's available when buttons are rendered
        out.print("<script>");
        out.print("function selectNetwork(ssid) {");
        out.print("  console.log('selectNetwork called with SSID:', ssid);");
        out.print("  var pass = prompt('Enter password for ' + ssid + ':');");
        out.print("  if (pass !== null) {");
        out.print("    console.log('Password entered, storing SSID:', ssid);");
        out.print("    sessionStorage.setItem('selectedSSID', ssid);");
        out.print("    sessionStorage.setItem('selectedPassword', pass);");
        out.print("    window.location.href = '/wifi';");
        out.print("  } else {");
        out.print("    console.log('Password prompt cancelled');");
        out.print("  }");
        out.print("}");
        out.print("</script>");
        
        // Check if scan is complete
        if (!wifiAccess.isScanInProgress() || wifiAccess.isScanComplete()) {
            auto& networks = wifiAccess.getScannedNetworks();
            
            if (networks.size() > 0) {
                out.print("<table style='width:100%; border-collapse: collapse;'>");
                out.print("<tr style='background: #f0f0f0;'>");
                out.print("<th style='padding: 5px; text-align: left;'>Network Name</th>");
                out.print("<th style='padding: 5px;'>Signal</th>");
                out.print("<th style='padding: 5px;'>Security</th>");
                out.print("<th style='padding: 5px;'>Action</th></tr>");
                
                // Sort by signal strength
                std::vector<WifiAccess::ScannedNetwork> sortedNetworks = networks;
                std::sort(sortedNetworks.begin(), sortedNetworks.end(), 
                    [](const WifiAccess::ScannedNetwork& a, const WifiAccess::ScannedNetwork& b) {
                        return a.rssi > b.rssi;
                    });
                
                for (const auto& net : sortedNetworks) {
                    out.print("<tr style='border-bottom: 1px solid #ddd;'>");
                    // HTML escape the SSID for display
                    String displaySSID = "";
                    for (int i = 0; i < net.ssid.length(); i++) {
                        char c = net.ssid[i];
                        if (c == '&') {
                            displaySSID += "&amp;";
                        } else if (c == '<') {
                            displaySSID += "&lt;";
                        } else if (c == '>') {
                            displaySSID += "&gt;";
                        } else if (c == '"') {
                            displaySSID += "&quot;";
                        } else if (c == '\'') {
                            displaySSID += "&#39;";
                        } else {
                            displaySSID += c;
                        }
                    }
                    out.print("<td style='padding: 5px;'>" + displaySSID + "</td>");
                    
                    // Signal strength
                    out.print("<td style='padding: 5px; text-align: center;'>");
                    if (net.rssi > -50) out.print("████");
                    else if (net.rssi > -60) out.print("███░");
                    else if (net.rssi > -70) out.print("██░░");
                    else out.print("█░░░");
                    out.print(" (" + String(net.rssi) + ")</td>");
                    
                    // Security
                    out.print("<td style='padding: 5px; text-align: center;'>");
                    out.print(net.encrypted ? "🔒" : "Open");
                    out.print("</td>");
                    
                    // Action button
                    out.print("<td style='padding: 5px; text-align: center;'>");
                    // Properly escape the SSID for JavaScript
                    String escapedSSID = "";
                    for (int i = 0; i < net.ssid.length(); i++) {
                        char c = net.ssid[i];
                        if (c == '\\') {
                            escapedSSID += "\\\\";
                        } else if (c == '\'') {
                            escapedSSID += "\\'";
                        } else if (c == '\"') {
                            escapedSSID += "\\\"";
                        } else if (c == '\n') {
                            escapedSSID += "\\n";
                        } else if (c == '\r') {
                            escapedSSID += "\\r";
                        } else if (c == '\t') {
                            escapedSSID += "\\t";
                        } else if (c < 32 || c > 126) {
                            // Escape any other control characters or non-ASCII
                            char hexBuf[5];
                            sprintf(hexBuf, "\\x%02x", (unsigned char)c);
                            escapedSSID += hexBuf;
                        } else {
                            escapedSSID += c;
                        }
                    }
                    out.print("<button onclick=\"selectNetwork('" + escapedSSID + "')\">Select</button>");
                    out.print("</td>");
                    out.print("</tr>");
                }
                out.print("</table>");
                
                // Start a new scan automatically
                if (!wifiAccess.isScanInProgress()) {
                    wifiAccess.startScan();
                }
            } else {
                out.print("<p>No networks found. Scanning again...</p>");
                if (!wifiAccess.isScanInProgress()) {
                    wifiAccess.startScan();
                }
                out.print("<script>setTimeout(function() { location.reload(); }, 3000);</script>");
            }
        } else {
            out.print("<p>Scanning for networks...</p>");
            out.print("<script>setTimeout(function() { location.reload(); }, 2000);</script>");
        }
    }
};

// WiFi scan results page
WifiScanDynamic wifiScanDynamic;

WElement wifiScanContents[] = {
    W1("Available Networks"),
    WButton("Refresh Scan", "refresh", []() {
        wifiAccess.startScan();
    }),
    WHR(),
    WDynamicElement(wifiScanDynamic),
    WHR(),
    WButton("Back to WiFi Settings", "back", "/wifi"),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

String remoteHostName;
String remoteSecret;

WElement remoteContents[] = {
    W1("Droid Remote Setup"),
    WCheckbox("Droid Remote Enabled", "remoteenabled",
        []() { return remoteEnabled; },
        [](bool val) { remoteEnabled = val; if (remoteEnabled) wifiEnabled = false; } ),
    WLabel("Droid Remote Disables WiFi", "label1"),
    WHR(),
    WTextField("Device Name:", "hostname",
        []()->String { return (remoteHostName = preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME)); },
        [](String val) { remoteHostName = val; } ),
    WPassword("Secret:", "secret",
        []()->String { return (remoteSecret = preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET)); },
        [](String val) { remoteSecret = val; } ),
    WButton("Save", "save", []() {
        DEBUG_PRINTLN("Remote Changed");
        preferences.putBool(PREFERENCE_REMOTE_ENABLED, remoteEnabled);
        preferences.putBool(PREFERENCE_WIFI_ENABLED, wifiEnabled);
        preferences.putString(PREFERENCE_REMOTE_HOSTNAME, remoteHostName);
        preferences.putString(PREFERENCE_REMOTE_SECRET, remoteSecret);
        reboot();
    }),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

WElement firmwareContents[] = {
    W1("Firmware Setup"),
    WFirmwareFile("Firmware:", "firmware"),
    WFirmwareUpload("Reflash", "firmware"),
    WLabel("Current Firmware Build Date:", "label"),
    WLabel(__DATE__, "date"),
#ifdef BUILD_VERSION
    WHRef(BUILD_VERSION, "Sources"),
#endif
    WButton("Clear Prefs", "clear", []() {
        DEBUG_PRINTLN("Clear all preference settings");
        preferences.clear();
    }),
    WHorizontalAlign(),
    WButton("Reboot", "reboot", []() {
        reboot();
    }),
    WHorizontalAlign(),
    WButton("Back", "back", "/setup"),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

//////////////////////////////////////////////////////////////////

WPage pages[] = {
    // IMPORTANT: Routes are matched in order. Place catch-all routes like /m/ early
    // to ensure they're checked before more specific routes
    
    // Modern UI catch-all route - must be early in the list
    WAPI("/m/", [](Print& out, String queryString) {
        Serial.println("[MODERN_UI] /m/ route hit (from top of routes)");
        
        // Redirect to index.html for directory request
        out.print("HTTP/1.0 302 Found\r\n");
        out.print("Location: /m/index.html\r\n");
        out.print("Cache-Control: no-cache\r\n");
        out.print("\r\n");
    }),
    
    // Debug test endpoint
    WAPI("/api/test", [](Print& out, String queryString) {
        Serial.println("[API_TEST] /api/test endpoint hit");
        out.print("HTTP/1.0 200 OK\r\n");
        out.print("Content-Type: application/json\r\n");
        out.print("Access-Control-Allow-Origin: *\r\n");
        out.print("Connection: close\r\n");
        out.print("\r\n");
        out.print("{\"status\":\"ok\",\"message\":\"API is working\"}");
    }),
    
    WPage("/", mainContents, SizeOfArray(mainContents)),
      WPage("/logics", logicsContents, SizeOfArray(logicsContents)),
    WPage("/setup", setupContents, SizeOfArray(setupContents)),
      WPage("/setuplogics", setupLogicsContents, SizeOfArray(setupLogicsContents)),
      WPage("/marcduino", marcduinoContents, SizeOfArray(marcduinoContents)),
      WPage("/wifi", wifiContents, SizeOfArray(wifiContents)),
      WPage("/wifiscan", wifiScanContents, SizeOfArray(wifiScanContents)),
      WPage("/remote", remoteContents, SizeOfArray(remoteContents)),
      WPage("/firmware", firmwareContents, SizeOfArray(firmwareContents)),
        WUpload("/upload/firmware",
            [](Client& client)
            {
                if (Update.hasError())
                    client.println("HTTP/1.0 200 FAIL");
                else
                    client.println("HTTP/1.0 200 OK");
                client.println("Content-type:text/html");
                client.println("Vary: Accept-Encoding");
                client.println();
                client.println();
                client.stop();
                if (!Update.hasError())
                {
                    reboot();
                }
            #ifdef USE_LOGICS
                FLD_selectSequence(LogicEngineDefaults::FAILURE);
                FLD_setTextMessage("Flash Fail");
                FLD_selectSequence(LogicEngineDefaults::TEXTSCROLLLEFT, LogicEngineRenderer::kRed, 1, 0);
                FLD_setEffectWidthRange(1.0);
                FLD_setEffectWidthRange(1.0);
            #endif
                otaInProgress = false;
            },
            [](WUploader& upload)
            {
                if (upload.status == UPLOAD_FILE_START)
                {
                    otaInProgress = true;
                    unmountFileSystems();
                #ifdef USE_LOGICS
                    FLD_selectSequence(LogicEngineDefaults::NORMAL);
                    RLD_selectSequence(LogicEngineDefaults::NORMAL);
                    FLD_setEffectWidthRange(0);
                    RLD_setEffectWidthRange(0);
                #endif
                    Serial.printf("Update: %s\n", upload.filename.c_str());
                    if (!Update.begin(upload.fileSize))
                    {
                        //start with max available size
                        Update.printError(Serial);
                    }
                }
                else if (upload.status == UPLOAD_FILE_WRITE)
                {
                    float range = (float)upload.receivedSize / (float)upload.fileSize;
                    DEBUG_PRINTLN("Received: "+String(range*100)+"%");
                   /* flashing firmware to ESP*/
                    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                    {
                        Update.printError(Serial);
                    }
                #ifdef USE_LOGICS
                    FLD_setEffectWidthRange(range);
                    RLD_setEffectWidthRange(range);
                #endif
                }
                else if (upload.status == UPLOAD_FILE_END)
                {
                    DEBUG_PRINTLN("GAME OVER");
                    if (Update.end(true))
                    {
                        //true to set the size to the current progress
                        Serial.printf("Update Success: %u\nRebooting...\n", upload.receivedSize);
                    }
                    else
                    {
                        Update.printError(Serial);
                    }
                }
            }),
      // WiFi network management endpoints
      WAPI("/wifiremove", [](Print& out, String queryString) {
          // Parse SSID from query string
          int ssidIndex = queryString.indexOf("ssid=");
          if (ssidIndex >= 0) {
              String ssid = queryString.substring(ssidIndex + 5);
              int ampIndex = ssid.indexOf('&');
              if (ampIndex >= 0) {
                  ssid = ssid.substring(0, ampIndex);
              }
              // URL decode the SSID
              ssid = urlDecode(ssid);
              
              DEBUG_PRINTLN("Removing network: " + ssid);
              wifiAccess.removeSavedNetwork(ssid);
              saveWifiNetworks(wifiAccess);
              out.print("OK");
          } else {
              out.print("ERROR: No SSID provided");
          }
      }),
      WAPI("/wifiupdatepriority", [](Print& out, String queryString) {
          // Parse SSID and priority from query string
          int ssidIndex = queryString.indexOf("ssid=");
          int priorityIndex = queryString.indexOf("priority=");
          
          if (ssidIndex >= 0 && priorityIndex >= 0) {
              String ssid = queryString.substring(ssidIndex + 5);
              int ampIndex = ssid.indexOf('&');
              if (ampIndex >= 0) {
                  ssid = ssid.substring(0, ampIndex);
              }
              // URL decode the SSID
              ssid = urlDecode(ssid);
              
              String priorityStr = queryString.substring(priorityIndex + 9);
              ampIndex = priorityStr.indexOf('&');
              if (ampIndex >= 0) {
                  priorityStr = priorityStr.substring(0, ampIndex);
              }
              int priority = priorityStr.toInt();
              
              DEBUG_PRINTLN("Updating priority for network: " + ssid + " to " + String(priority));
              
              // Find the network and get its password
              const auto& networks = wifiAccess.getSavedNetworks();
              for (const auto& net : networks) {
                  if (net.ssid == ssid) {
                      // Use addSavedNetwork to update the priority
                      wifiAccess.addSavedNetwork(ssid, net.password, priority);
                      saveWifiNetworks(wifiAccess);
                      out.print("OK");
                      return;
                  }
              }
              out.print("ERROR: Network not found");
          } else {
              out.print("ERROR: Missing parameters");
          }
      }),
      WAPI("/wifisavesettings", [](Print& out, String queryString) {
          DEBUG_PRINTLN("WiFi Settings Changed");
          
          // Extract mode parameter
          int modeIdx = queryString.indexOf("mode=");
          bool apMode = true;  // Default to AP mode
          if (modeIdx >= 0) {
              String mode = queryString.substring(modeIdx + 5);
              int ampIdx = mode.indexOf('&');
              if (ampIdx >= 0) {
                  mode = mode.substring(0, ampIdx);
              }
              apMode = (mode == "ap");
              DEBUG_PRINT("Mode selected: ");
              DEBUG_PRINTLN(mode);
          }
          
          // Only allow client mode if there are saved networks
          if (!apMode && wifiAccess.getSavedNetworks().empty()) {
              DEBUG_PRINTLN("Cannot use client mode without saved networks, forcing AP mode");
              apMode = true;
          }
          
          // Save all settings
          preferences.putBool(PREFERENCE_WIFI_ENABLED, wifiEnabled);
          preferences.putString(PREFERENCE_WIFI_SSID, wifiSSID);
          preferences.putString(PREFERENCE_WIFI_PASS, wifiPass);
          preferences.putString(PREFERENCE_MDNS_HOSTNAME, mdnsHostname);
          preferences.putInt(PREFERENCE_WIFI_TIMEOUT, wifiTimeout);
          preferences.putBool(PREFERENCE_WIFI_AP, apMode);
          
          DEBUG_PRINT("Settings saved - AP Mode: ");
          DEBUG_PRINTLN(apMode ? "true" : "false");
          
          out.print("OK");
          delay(500);
          reboot();
      }),
      WAPI("/wificonnect", [](Print& out, String queryString) {
          // Parse SSID from query string
          int ssidIndex = queryString.indexOf("ssid=");
          if (ssidIndex >= 0) {
              String ssid = queryString.substring(ssidIndex + 5);
              int ampIndex = ssid.indexOf('&');
              if (ampIndex >= 0) {
                  ssid = ssid.substring(0, ampIndex);
              }
              // URL decode the SSID
              ssid = urlDecode(ssid);
              
              DEBUG_PRINTLN("Connecting to network: " + ssid);
              
              // Find the network and get its password and priority
              const auto& networks = wifiAccess.getSavedNetworks();
              for (const auto& net : networks) {
                  if (net.ssid == ssid) {
                      // Keep the existing priority when connecting
                      wifiAccess.addSavedNetwork(ssid, net.password, net.priority);
                      saveWifiNetworks(wifiAccess);
                      
                      // Set flag to connect to STA mode and reboot
                      preferences.putBool(PREFERENCE_WIFI_ENABLED, true);
                      preferences.putBool(PREFERENCE_WIFI_AP, false);  // Force client mode
                      
                      out.print("OK");
                      // Reboot after a short delay
                      delay(500);
                      reboot();
                      return;
                  }
              }
              out.print("ERROR: Network not found");
          } else {
              out.print("ERROR: No SSID provided");
          }
      }),
      WAPI("/wifistatus", [](Print& out, String queryString) {
          out.print("<div style='font-family: monospace;'>\n");
          out.print("<strong>Current Status:</strong><br/>\n");
          
          // WiFi Mode
          bool isAP = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
          bool isSTA = WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA;
          out.print("Mode: ");
          if (isAP && isSTA) out.print("AP+STA");
          else if (isAP) out.print("Access Point");
          else if (isSTA) out.print("Station (Client)");
          else out.print("Off");
          out.print("<br/>\n");
          
          // Connection Status
          if (isSTA) {
              out.print("Status: ");
              if (WiFi.isConnected()) {
                  out.print("<span style='color:green;'>Connected</span><br/>\n");
                  out.print("Network: " + WiFi.SSID() + "<br/>\n");
                  out.print("IP Address: " + WiFi.localIP().toString() + "<br/>\n");
                  out.print("Signal: " + String(WiFi.RSSI()) + " dBm<br/>\n");
              } else {
                  out.print("<span style='color:red;'>Not Connected</span><br/>\n");
                  out.print("Attempting to connect...<br/>\n");
              }
          }
          
          if (isAP) {
              out.print("<br/><strong>Access Point Info:</strong><br/>\n");
              out.print("SSID: " + preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME) + "<br/>\n");
              out.print("IP: " + WiFi.softAPIP().toString() + "<br/>\n");
              out.print("Clients: " + String(WiFi.softAPgetStationNum()) + "<br/>\n");
          }
          
          // Saved Networks Info
          out.print("<br/><strong>Saved Networks:</strong><br/>\n");
          auto networks = wifiAccess.getSavedNetworks();
          out.print("Count: " + String(networks.size()) + "<br/>\n");
          if (!networks.empty()) {
              out.print("<ul style='margin: 5px 0;'>\n");
              for (const auto& net : networks) {
                  out.print("<li>" + net.ssid + " (Priority: " + String(net.priority) + ")</li>\n");
              }
              out.print("</ul>\n");
          }
          
          out.print("</div>\n");
      }),
      // Modern UI route handler - MOVED TO TOP OF ROUTES FOR PROPER MATCHING
      /* Original /m/ handler moved to beginning of pages array
      WAPI("/m/", [](Print& out, String queryString) {
          Serial.println("[MODERN_UI] /m/ route hit");
          Serial.print("[MODERN_UI] queryString: ");
          Serial.println(queryString);
          
          // Extract the requested path
          String requestedFile = "";
          int pathStart = queryString.indexOf("/m/");
          if (pathStart >= 0) {
              requestedFile = queryString.substring(pathStart + 3);
              // Remove query parameters if present
              int queryStart = requestedFile.indexOf('?');
              if (queryStart > 0) {
                  requestedFile = requestedFile.substring(0, queryStart);
              }
          }
          
          Serial.print("[MODERN_UI] Requested file: ");
          Serial.println(requestedFile);
          
          // Default to index.html for directory requests
          if (requestedFile.isEmpty() || requestedFile.endsWith("/")) {
              requestedFile = "index.html";
              Serial.println("[MODERN_UI] Defaulting to index.html");
          }
          
          // Determine which compressed content to serve
          const uint8_t* gzData = nullptr;
          size_t gzSize = 0;
          size_t originalSize = 0;
          
          if (requestedFile == "index.html" || requestedFile == "index") {
              Serial.println("[MODERN_UI] Serving MODERN_INDEX_GZ");
              gzData = MODERN_INDEX_GZ;
              gzSize = MODERN_INDEX_GZ_SIZE;
              originalSize = MODERN_INDEX_ORIGINAL_SIZE;
              Serial.print("[MODERN_UI] Index size: ");
              Serial.print(gzSize);
              Serial.println(" bytes (compressed)");
          } else if (requestedFile == "wifi.html" || requestedFile == "wifi") {
              Serial.println("[MODERN_UI] Serving MODERN_WIFI_GZ");
              gzData = MODERN_WIFI_GZ;
              gzSize = MODERN_WIFI_GZ_SIZE;
              originalSize = MODERN_WIFI_ORIGINAL_SIZE;
              Serial.print("[MODERN_UI] WiFi size: ");
              Serial.print(gzSize);
              Serial.println(" bytes (compressed)");
          }
          
          if (gzData == nullptr) {
              Serial.println("[MODERN_UI] ERROR: No matching file found!");
              out.print("HTTP/1.0 404 Not Found\r\n");
              out.print("Content-Type: text/html\r\n\r\n");
              out.print("<h1>404 - Page Not Found</h1>");
              out.print("<p>The requested page was not found: " + requestedFile + "</p>");
              out.print("<a href='/m/'>Back to Modern UI Home</a>");
              return;
          }
          
          // Send response headers
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: text/html; charset=utf-8\r\n");
          out.print("Content-Encoding: gzip\r\n");
          out.print("Content-Length: " + String(gzSize) + "\r\n");
          out.print("Cache-Control: no-cache, no-store, must-revalidate\r\n");
          out.print("Pragma: no-cache\r\n");
          out.print("Expires: 0\r\n");
          out.print("Connection: close\r\n");
          out.print("\r\n");
          
          // Send compressed data from PROGMEM
          // Send in chunks to avoid memory issues
          const size_t chunkSize = 512;
          uint8_t chunk[chunkSize];
          
          for (size_t offset = 0; offset < gzSize; offset += chunkSize) {
              size_t bytesToRead = min(chunkSize, gzSize - offset);
              memcpy_P(chunk, gzData + offset, bytesToRead);
              out.write(chunk, bytesToRead);
          }
      }), */
      // Additional routes for modern UI pages
      WAPI("/m/index.html", [](Print& out, String queryString) {
          Serial.println("[MODERN_UI] Serving /m/index.html");
          // Serve compressed index.html
          const uint8_t* gzData = MODERN_INDEX_GZ;
          size_t gzSize = MODERN_INDEX_GZ_SIZE;
          Serial.print("[MODERN_UI] Index size: ");
          Serial.print(gzSize);
          Serial.print(" bytes, original size: ");
          Serial.println(MODERN_INDEX_ORIGINAL_SIZE);
          
          // Send response headers
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: text/html; charset=utf-8\r\n");
          out.print("Content-Encoding: gzip\r\n");
          out.print("Content-Length: " + String(gzSize) + "\r\n");
          out.print("Cache-Control: no-cache, no-store, must-revalidate\r\n");
          out.print("Pragma: no-cache\r\n");
          out.print("Expires: 0\r\n");
          out.print("Connection: close\r\n");
          out.print("\r\n");
          
          // Send compressed data from PROGMEM
          // Use smaller chunk size to avoid buffer issues
          const size_t chunkSize = 256;
          uint8_t chunk[chunkSize];
          
          for (size_t offset = 0; offset < gzSize; offset += chunkSize) {
              size_t bytesToRead = min(chunkSize, gzSize - offset);
              memcpy_P(chunk, gzData + offset, bytesToRead);
              out.write(chunk, bytesToRead);
          }
          // Final flush after all data is sent
          out.flush();
      }),
      WAPI("/m/wifi.html", [](Print& out, String queryString) {
          Serial.println("[MODERN_UI] Serving /m/wifi.html");
          // Serve compressed wifi.html
          const uint8_t* gzData = MODERN_WIFI_GZ;
          size_t gzSize = MODERN_WIFI_GZ_SIZE;
          Serial.print("[MODERN_UI] Compressed size: ");
          Serial.print(gzSize);
          Serial.println(" bytes");
          
          // Send response headers
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: text/html; charset=utf-8\r\n");
          out.print("Content-Encoding: gzip\r\n");
          out.print("Content-Length: " + String(gzSize) + "\r\n");
          out.print("Cache-Control: no-cache, no-store, must-revalidate\r\n");
          out.print("Pragma: no-cache\r\n");
          out.print("Expires: 0\r\n");
          out.print("Connection: close\r\n");
          out.print("\r\n");
          
          // Send compressed data from PROGMEM
          // Use smaller chunk size to avoid buffer issues
          const size_t chunkSize = 256;
          uint8_t chunk[chunkSize];
          
          size_t totalSent = 0;
          for (size_t offset = 0; offset < gzSize; offset += chunkSize) {
              size_t bytesToRead = min(chunkSize, gzSize - offset);
              memcpy_P(chunk, gzData + offset, bytesToRead);
              size_t written = out.write(chunk, bytesToRead);
              totalSent += written;
              if (written != bytesToRead) {
                  Serial.print("[MODERN_UI] Write error at offset ");
                  Serial.print(offset);
                  Serial.print(": expected ");
                  Serial.print(bytesToRead);
                  Serial.print(" got ");
                  Serial.println(written);
              }
          }
          // Final flush after all data is sent
          out.flush();
          Serial.print("[MODERN_UI] Total bytes sent: ");
          Serial.println(totalSent);
      }),
      WAPI("/m/wifi", [](Print& out, String queryString) {
          // Serve compressed wifi.html for /m/wifi (without .html)
          const uint8_t* gzData = MODERN_WIFI_GZ;
          size_t gzSize = MODERN_WIFI_GZ_SIZE;
          
          // Send response headers
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: text/html; charset=utf-8\r\n");
          out.print("Content-Encoding: gzip\r\n");
          out.print("Content-Length: " + String(gzSize) + "\r\n");
          out.print("Cache-Control: no-cache, no-store, must-revalidate\r\n");
          out.print("Pragma: no-cache\r\n");
          out.print("Expires: 0\r\n");
          out.print("Connection: close\r\n");
          out.print("\r\n");
          
          // Send compressed data from PROGMEM
          // Use smaller chunk size to avoid buffer issues
          const size_t chunkSize = 256;
          uint8_t chunk[chunkSize];
          
          for (size_t offset = 0; offset < gzSize; offset += chunkSize) {
              size_t bytesToRead = min(chunkSize, gzSize - offset);
              memcpy_P(chunk, gzData + offset, bytesToRead);
              out.write(chunk, bytesToRead);
          }
          // Final flush after all data is sent
          out.flush();
      }),
      // Modern API endpoints for WiFi management
      WAPI("/api/wifi/status", [](Print& out, String queryString) {
          Serial.println("[API] /api/wifi/status endpoint hit");
          DynamicJsonDocument doc(1024);
          
          // Current status
          doc["enabled"] = wifiEnabled;
          doc["mode"] = wifiAccess.isSoftAP() ? "ap" : "client";
          doc["connected"] = WiFi.isConnected();
          
          if (WiFi.isConnected()) {
              doc["ssid"] = WiFi.SSID();
              doc["ip"] = WiFi.localIP().toString();
              doc["rssi"] = WiFi.RSSI();
              doc["channel"] = WiFi.channel();
          } else if (wifiAccess.isSoftAP()) {
              doc["ssid"] = preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME);
              doc["ip"] = WiFi.softAPIP().toString();
              doc["clients"] = WiFi.softAPgetStationNum();
          }
          
          // AP settings
          JsonObject ap = doc.createNestedObject("ap");
          ap["ssid"] = preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME);
          ap["timeout"] = preferences.getInt(PREFERENCE_WIFI_TIMEOUT, 60);
          
          // Send response
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("Connection: close\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
          
          // Debug logging
          Serial.print("[API] Status response: ");
          serializeJson(doc, Serial);
          Serial.println();
      }),
      WAPI("/api/wifi/networks", [](Print& out, String queryString) {
          DynamicJsonDocument doc(2048);
          JsonArray networks = doc.to<JsonArray>();
          
          auto savedNetworks = wifiAccess.getSavedNetworks();
          for (const auto& net : savedNetworks) {
              JsonObject network = networks.createNestedObject();
              network["ssid"] = net.ssid;
              network["priority"] = net.priority;
              network["hasPassword"] = !net.password.isEmpty();
          }
          
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
      }),
      WAPI("/api/wifi/scan", [](Print& out, String queryString) {
          Serial.println("[API] /api/wifi/scan endpoint hit");
          wifiAccess.startScan();
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("\r\n");
          out.print("{\"status\":\"scanning\",\"message\":\"WiFi scan initiated\"}");
      }),
      WAPI("/api/wifi/scanresults", [](Print& out, String queryString) {
          Serial.println("[API] /api/wifi/scanresults endpoint hit");
          DynamicJsonDocument doc(4096);
          JsonObject response = doc.to<JsonObject>();
          
          // Check scan status ONCE and store the result
          bool scanComplete = wifiAccess.isScanComplete();
          bool scanInProgress = wifiAccess.isScanInProgress();
          
          response["scanning"] = scanInProgress;
          response["complete"] = scanComplete;
          
          Serial.print("[API] Scan in progress: ");
          Serial.println(scanInProgress);
          Serial.print("[API] Scan complete: ");
          Serial.println(scanComplete);
          
          JsonArray networks = response.createNestedArray("networks");
          
          // If scan just completed, get the networks
          if (scanComplete || (!scanInProgress && !scanComplete)) {
              auto& scannedNetworks = wifiAccess.getScannedNetworks();
              Serial.print("[API] Found ");
              Serial.print(scannedNetworks.size());
              Serial.println(" networks");
              for (const auto& net : scannedNetworks) {
                  JsonObject network = networks.createNestedObject();
                  network["ssid"] = net.ssid;
                  network["rssi"] = net.rssi;
                  network["encrypted"] = net.encrypted;
                  
                  // Signal strength category
                  if (net.rssi > -50) network["signal"] = "excellent";
                  else if (net.rssi > -60) network["signal"] = "good";
                  else if (net.rssi > -70) network["signal"] = "fair";
                  else network["signal"] = "poor";
              }
          }
          
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
      }),
      // Add network API endpoint
      WAPI("/api/wifi/add", [](Print& out, String queryString) {
          // Parse parameters from query string
          String ssid = "";
          String password = "";
          int priority = 5;
          
          // Extract SSID
          int ssidIdx = queryString.indexOf("ssid=");
          if (ssidIdx >= 0) {
              int endIdx = queryString.indexOf('&', ssidIdx);
              if (endIdx < 0) endIdx = queryString.length();
              ssid = urlDecode(queryString.substring(ssidIdx + 5, endIdx));
          }
          
          // Extract password
          int passIdx = queryString.indexOf("password=");
          if (passIdx >= 0) {
              int endIdx = queryString.indexOf('&', passIdx);
              if (endIdx < 0) endIdx = queryString.length();
              password = urlDecode(queryString.substring(passIdx + 9, endIdx));
          }
          
          // Extract priority
          int prioIdx = queryString.indexOf("priority=");
          if (prioIdx >= 0) {
              int endIdx = queryString.indexOf('&', prioIdx);
              if (endIdx < 0) endIdx = queryString.length();
              priority = queryString.substring(prioIdx + 9, endIdx).toInt();
              if (priority < 1) priority = 1;
              if (priority > 10) priority = 10;
          }
          
          DynamicJsonDocument doc(256);
          
          if (ssid.isEmpty()) {
              doc["success"] = false;
              doc["error"] = "SSID is required";
          } else {
              wifiAccess.addSavedNetwork(ssid, password, priority);
              saveWifiNetworks(wifiAccess);
              doc["success"] = true;
              doc["message"] = "Network added successfully";
          }
          
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
      }),
      // Update AP settings endpoint
      WAPI("/api/wifi/apsettings", [](Print& out, String queryString) {
          // Parse parameters from query string
          String ssid = "";
          String password = "";
          int timeout = 60;
          
          // Extract SSID
          int ssidIdx = queryString.indexOf("ssid=");
          if (ssidIdx >= 0) {
              int endIdx = queryString.indexOf('&', ssidIdx);
              if (endIdx < 0) endIdx = queryString.length();
              ssid = urlDecode(queryString.substring(ssidIdx + 5, endIdx));
          }
          
          // Extract password
          int passIdx = queryString.indexOf("password=");
          if (passIdx >= 0) {
              int endIdx = queryString.indexOf('&', passIdx);
              if (endIdx < 0) endIdx = queryString.length();
              password = urlDecode(queryString.substring(passIdx + 9, endIdx));
          }
          
          // Extract timeout
          int timeoutIdx = queryString.indexOf("timeout=");
          if (timeoutIdx >= 0) {
              int endIdx = queryString.indexOf('&', timeoutIdx);
              if (endIdx < 0) endIdx = queryString.length();
              timeout = queryString.substring(timeoutIdx + 8, endIdx).toInt();
              if (timeout < 10) timeout = 10;
              if (timeout > 300) timeout = 300;
          }
          
          DynamicJsonDocument doc(256);
          
          if (!ssid.isEmpty()) {
              preferences.putString(PREFERENCE_WIFI_SSID, ssid);
              preferences.putString(PREFERENCE_WIFI_PASS, password);
              preferences.putInt(PREFERENCE_WIFI_TIMEOUT, timeout);
              
              doc["success"] = true;
              doc["message"] = "AP settings saved successfully";
          } else {
              doc["success"] = false;
              doc["error"] = "SSID is required";
          }
          
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
      }),
      // Update network priority endpoint
      WAPI("/api/wifi/updatepriority", [](Print& out, String queryString) {
          Serial.println("[API] /api/wifi/updatepriority endpoint hit");
          
          // Parse parameters from query string
          String ssid = "";
          int priority = 5;
          
          // Extract SSID
          int ssidIdx = queryString.indexOf("ssid=");
          if (ssidIdx >= 0) {
              int endIdx = queryString.indexOf('&', ssidIdx);
              if (endIdx < 0) endIdx = queryString.length();
              ssid = urlDecode(queryString.substring(ssidIdx + 5, endIdx));
          }
          
          // Extract priority
          int priorityIdx = queryString.indexOf("priority=");
          if (priorityIdx >= 0) {
              int endIdx = queryString.indexOf('&', priorityIdx);
              if (endIdx < 0) endIdx = queryString.length();
              priority = queryString.substring(priorityIdx + 9, endIdx).toInt();
              if (priority < 1) priority = 1;
              if (priority > 10) priority = 10;
          }
          
          DynamicJsonDocument doc(256);
          
          if (!ssid.isEmpty()) {
              // Find the network and update its priority
              const auto& networks = wifiAccess.getSavedNetworks();
              bool found = false;
              
              for (const auto& net : networks) {
                  if (net.ssid == ssid) {
                      // Update the network with new priority
                      wifiAccess.addSavedNetwork(ssid, net.password, priority);
                      saveWifiNetworks(wifiAccess);
                      found = true;
                      break;
                  }
              }
              
              if (found) {
                  doc["success"] = true;
                  doc["message"] = "Priority updated successfully";
                  Serial.print("[API] Updated priority for ");
                  Serial.print(ssid);
                  Serial.print(" to ");
                  Serial.println(priority);
              } else {
                  doc["success"] = false;
                  doc["error"] = "Network not found";
              }
          } else {
              doc["success"] = false;
              doc["error"] = "SSID is required";
          }
          
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
      }),
      // Firmware version endpoint
      WAPI("/api/firmware/version", [](Print& out, String queryString) {
          Serial.println("[API] /api/firmware/version endpoint hit");
          DynamicJsonDocument doc(256);
          
          // Get build date and time
          String buildDate = __DATE__;
          String buildTime = __TIME__;
          
          // Get the filename from the pre_extra.py script if available
          #ifdef FIRMWARE_FILENAME
          doc["filename"] = FIRMWARE_FILENAME;
          doc["version"] = FIRMWARE_FILENAME;
          #else
          doc["filename"] = "RSeriesLogic_" + buildDate + "_" + buildTime + ".bin";
          doc["version"] = "RSeriesLogic_" + buildDate + "_" + buildTime + ".bin";
          #endif
          doc["buildDate"] = buildDate;
          doc["buildTime"] = buildTime;
          
          out.print("HTTP/1.0 200 OK\r\n");
          out.print("Content-Type: application/json\r\n");
          out.print("Access-Control-Allow-Origin: *\r\n");
          out.print("Connection: close\r\n");
          out.print("\r\n");
          serializeJson(doc, out);
          
          // Debug logging
          Serial.print("[API] Firmware version response: ");
          serializeJson(doc, Serial);
          Serial.println();
      })
};

WifiWebServer<25,SizeOfArray(pages)> webServer(pages, wifiAccess);
#endif
