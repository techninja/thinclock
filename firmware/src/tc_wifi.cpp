#include <WiFi.h>
#include "tc_wifi.h"
#include "display.h"
#include "thinclock.h"
#include <ESPmDNS.h>
#include <time.h>

extern Display display;

void scrollText(const String& text, uint32_t color) {
    int16_t textW = display.nativeTextWidth(text, 1, false);
    for (int16_t x = MATRIX_WIDTH; x > -textW; x--) {
        display.clear();
        display.drawNativeText(text, x, 1, color, 1, false);
        display.show();
        delay(80);
    }
}

enum class WifiState { IDLE, CONNECTING, CONNECTED, FAILED };
static WifiState wifiState        = WifiState::IDLE;
static uint32_t  wifiConnectStart = 0;

void setupWiFi() {
    prefs.begin("thinclock", false);
    wifiSSID        = prefs.getString("ssid", "");
    wifiPass        = prefs.getString("pass", "");
    configURL       = prefs.getString("config_url", "");
    bool staPending   = prefs.getBool("sta_pending", false);
    bool staConnected  = prefs.getBool("sta_connected", false);
    prefs.end();

    // If we previously connected successfully, auto-set sta_pending so a
    // normal reboot goes straight back to STA without user intervention.
    if (!wifiSSID.isEmpty() && staConnected && !staPending) {
        prefs.begin("thinclock", false);
        prefs.putBool("sta_pending", true);
        prefs.end();
        staPending = true;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);

    // Blocking scan in STA-only mode BEFORE AP/DNS are up — no UDP stack to corrupt.
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false, false); // blocking, no hidden
    scannedNets.clear();
    for (int i = 0; i < n && i < 20; i++)
        scannedNets.push_back({ WiFi.SSID(i), (int)WiFi.RSSI(i) });
    WiFi.scanDelete();
    Serial.printf("[wifi] scan: %d networks\n", (int)scannedNets.size());

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("thinclock-setup", "thinclock", 6, 0, 4);
    IPAddress apIP(192, 168, 4, 1);
    IPAddress apGW(192, 168, 4, 1);
    IPAddress apSN(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apGW, apSN);
    Serial.printf("[wifi] AP up: %s\n", WiFi.softAPIP().toString().c_str());

    dnsServer.start(53, "*", apIP);
    Serial.println("[wifi] DNS server started");

    httpServer.begin();
    Serial.println("[http] server started");

    // If no credentials, or STA was not requested this boot: AP-only.
    // The portal runs cleanly with a fully working DNS stack.
    if (wifiSSID.isEmpty() || !staPending) {
        if (!wifiSSID.isEmpty())
            Serial.println("[wifi] Credentials present but sta_pending=false — AP-only this boot");
        else
            Serial.println("[wifi] No credentials — AP-only mode");
        display.clear();
        display.drawNativeText("SETUP", 1, 1, 0xFF8800, 1, false);
        display.show();
        return;
    }

    // sta_pending=true: this boot is dedicated to the STA attempt.
    // Clear the flag now — if we crash or fail, next boot goes back to AP-only.
    prefs.begin("thinclock", false);
    prefs.putBool("sta_pending", false);
    prefs.end();

    Serial.printf("[wifi] Connecting to %s\n", wifiSSID.c_str());
    wifiBadPassword = false;
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED &&
            info.wifi_sta_disconnected.reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT)
            wifiBadPassword = true;
    });
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    display.clear();
    display.drawNativeText("WIFI", 1, 1, 0x0044FF, 1, false);
    display.show();
    wifiState = WifiState::CONNECTING;
    wifiConnectStart = millis();
}

void loopWiFi() {
    if (wifiState != WifiState::CONNECTING) return;

    if (WiFi.status() == WL_CONNECTED) {
        wifiState = WifiState::CONNECTED;
        prefs.begin("thinclock", false);
        prefs.putBool("sta_connected", true);
        prefs.end();
        Serial.printf("[wifi] Connected: %s\n", WiFi.localIP().toString().c_str());
        if (MDNS.begin("thinclock")) {
            MDNS.addService("thinclock", "_tcp", 80);
            MDNS.addServiceTxt("thinclock", "_tcp", "version", "0.9.0");
            MDNS.addServiceTxt("thinclock", "_tcp", "ip", WiFi.localIP().toString());
            Serial.println("[mdns] thinclock.local announced _thinclock._tcp");
        }
        configTzTime("UTC0", "pool.ntp.org");
        return;
    }

    if (millis() - wifiConnectStart > 15000) {
        wifiState = WifiState::FAILED;
        Serial.printf("[wifi] FAIL (%s) — rebooting to clean AP\n",
                      wifiBadPassword ? "bad password" : "no connection");
        // STA failure corrupts the lwIP UDP stack; DNS cannot recover in-place.
        // Reboot to get a clean AP — sta_pending is already false so next boot
        // goes straight to AP-only mode with a fully working portal.
        if (wifiBadPassword) {
            prefs.begin("thinclock", false);
            prefs.putBool("sta_connected", false);
            prefs.end();
            scrollText("BAD PASS", 0xFF2200);
        } else scrollText("NO WIFI", 0xFF4400);
        delay(500);
        ESP.restart();
    }
}
