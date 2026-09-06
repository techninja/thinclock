#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>
#include "setup_page.h"

// Populated by setupWiFi(), read by http_routes and main
extern String wifiSSID, wifiPass, configURL;
extern bool   wifiBadPassword;
extern std::vector<ScannedNet> scannedNets;

// Shared objects owned by main.cpp
extern DNSServer   dnsServer;
extern Preferences prefs;

void setupWiFi();
void loopWiFi();
void scrollText(const String& text, uint32_t color = 0x00AAFF);
