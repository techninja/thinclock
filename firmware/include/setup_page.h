#pragma once
#include <Arduino.h>
#include <vector>

// Forward declarations — avoids pulling WebServer.h before WiFi.h
class WebServer;
class Preferences;

struct ScannedNet {
    String ssid;
    int32_t rssi;
    explicit ScannedNet(String s = "", int32_t r = 0) : ssid(s), rssi(r) {}
};

String setupPageHTML(const String& ssid, const String& cfgURL, bool apMode,
                     bool badPassword, const std::vector<ScannedNet>& scannedNets);
void   handleSetupPost(WebServer& server, Preferences& prefs);
