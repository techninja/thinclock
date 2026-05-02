#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "thinclock.h"
#include "display.h"
#include "config_manager.h"

Display display;
ConfigManager configMgr;
Config config;
Preferences prefs;

String wifiSSID;
String wifiPass;
String configURL;

uint32_t lastConfigFetch = 0;
uint32_t lastDataFetch = 0;
uint32_t lastScreenSwitch = 0;
uint32_t lastScrollStep = 0;
int currentScreen = 0;
int16_t scrollOffset = 0;
int8_t scrollDir = 1;          // 1 = left, -1 = right
uint32_t scrollPauseUntil = 0; // bounce pause timestamp
ScrollMode activeScrollMode = SCROLL_NONE;
int16_t scrollTextW = 0;
String resolvedText;
JsonDocument screenData;

void setupWiFi() {
    prefs.begin("thinclock", true);
    wifiSSID = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    configURL = prefs.getString("config_url", "");
    prefs.end();

    if (wifiSSID.isEmpty()) {
        Serial.println("No WiFi. Send: {\"ssid\":\"...\",\"pass\":\"...\",\"config_url\":\"...\"}");
        return;
    }

    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    Serial.printf("Connecting to %s", wifiSSID.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(250);
        Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FAIL");

    if (WiFi.status() == WL_CONNECTED) {
        configTzTime("UTC0", "pool.ntp.org");
    }
}

void handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) return;

    JsonDocument doc;
    if (deserializeJson(doc, line)) return;

    if (doc["ssid"].is<const char*>()) {
        prefs.begin("thinclock", false);
        prefs.putString("ssid", doc["ssid"].as<const char*>());
        prefs.putString("pass", doc["pass"] | "");
        if (doc["config_url"].is<const char*>())
            prefs.putString("config_url", doc["config_url"].as<const char*>());
        prefs.end();
        Serial.println("Saved. Rebooting...");
        delay(500);
        ESP.restart();
    }
}

void showClock() {
    struct tm t;
    display.clear();
    if (!getLocalTime(&t)) {
        uint32_t s = millis() / 1000;
        char buf[9];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", (s / 60) % 100, s % 60);
        display.drawText(buf, 2, 0, 0x00AAFF);
    } else {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        display.drawText(buf, 2, 0, 0x00AAFF);
    }
    display.show();
}

void resetScrollState() {
    scrollOffset = 0;
    scrollDir = 1;
    scrollPauseUntil = 0;
    activeScrollMode = SCROLL_NONE;
    scrollTextW = 0;
    resolvedText = "";
    lastScrollStep = 0;
}

void switchScreen() {
    currentScreen = (currentScreen + 1) % config.screens.size();
    lastScreenSwitch = millis();
    screenData.clear();
    lastDataFetch = 0;
    resetScrollState();
}

void showScreen(Screen& scr) {
    // Resolve text once per data fetch
    String text = scr.label;
    if (!scr.data_url.isEmpty() && !screenData.isNull()) {
        text = configMgr.resolvePlaceholders(scr.label, screenData);
    }

    // Detect if text changed (new data)
    if (text != resolvedText) {
        resolvedText = text;
        scrollTextW = display.textWidth(resolvedText);
        scrollOffset = 0;
        scrollDir = 1;
        scrollPauseUntil = 0;
        lastScrollStep = millis();

        int16_t startX = scr.text_x >= 0 ? scr.text_x : 0;
        bool needsScroll = (scrollTextW + startX) > MATRIX_WIDTH;

        if (scr.scroll == SCROLL_NONE) {
            activeScrollMode = SCROLL_NONE;
        } else if (scr.scroll == SCROLL_LEFT || scr.scroll == SCROLL_BOUNCE) {
            activeScrollMode = scr.scroll;
        } else { // SCROLL_AUTO
            activeScrollMode = needsScroll ? SCROLL_BOUNCE : SCROLL_NONE;
        }
    }

    int16_t x = scr.text_x >= 0 ? scr.text_x : 0;
    int16_t y = scr.text_y >= 0 ? scr.text_y : 0;
    uint32_t now = millis();

    display.clear();

    if (activeScrollMode == SCROLL_NONE) {
        display.drawText(resolvedText, x, y, scr.color);
    } else if (activeScrollMode == SCROLL_BOUNCE) {
        display.drawText(resolvedText, x - scrollOffset, y, scr.color);
        display.applyEdgeFade(scr.fade_edge);

        if (now >= scrollPauseUntil && now - lastScrollStep >= scr.scroll_speed) {
            scrollOffset += scrollDir;
            lastScrollStep = now;

            int16_t maxOffset = scrollTextW + x - MATRIX_WIDTH;
            if (maxOffset < 0) maxOffset = 0;

            if (scrollDir == 1 && scrollOffset >= maxOffset) {
                scrollOffset = maxOffset;
                scrollDir = -1;
                scrollPauseUntil = now + 800; // pause at end
            } else if (scrollDir == -1 && scrollOffset <= 0) {
                scrollOffset = 0;
                scrollDir = 1;
                scrollPauseUntil = now + 800; // pause at start
            }
        }
    } else { // SCROLL_LEFT (banner)
        int16_t drawX = MATRIX_WIDTH - scrollOffset;
        display.drawText(resolvedText, drawX, y, scr.color);
        display.applyEdgeFade(scr.fade_edge);

        if (now - lastScrollStep >= scr.scroll_speed) {
            scrollOffset++;
            lastScrollStep = now;

            // Text has fully exited left side
            if (drawX + scrollTextW < 0) {
                scrollOffset = 0; // re-enter from right
            }
        }
    }

    display.show();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[thinclock]");

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    display.begin();
    display.clear();
    display.drawText("BOOT", 1, 0, 0x004400);
    display.show();

    setupWiFi();
    config.valid = false;
}

void loop() {
    handleSerial();
    uint32_t now = millis();

    // Fetch config periodically
    if (!configURL.isEmpty() && WiFi.status() == WL_CONNECTED) {
        if (!config.valid || now - lastConfigFetch > CONFIG_POLL_MS) {
            if (configMgr.fetchConfig(configURL, config)) {
                display.setBrightness(config.brightness);
                currentScreen = 0;
                lastScreenSwitch = now;
                resetScrollState();
            }
            lastConfigFetch = now;
        }
    }

    // No valid config — show clock
    if (!config.valid || config.screens.empty()) {
        showClock();
        delay(500);
        return;
    }

    // Fetch data for current screen
    Screen& scr = config.screens[currentScreen];
    if (!scr.data_url.isEmpty() && (now - lastDataFetch > DATA_POLL_MS)) {
        configMgr.fetchData(scr.data_url, screenData);
        lastDataFetch = now;
    }

    showScreen(scr);

    // Cycle screens (but wait for scroll to complete at least once if scrolling)
    if (now - lastScreenSwitch > scr.duration) {
        bool canSwitch = true;
        if (activeScrollMode == SCROLL_LEFT) {
            // Wait for banner to fully exit
            int16_t drawX = MATRIX_WIDTH - scrollOffset;
            canSwitch = (drawX + scrollTextW < 0);
        } else if (activeScrollMode == SCROLL_BOUNCE) {
            // Switch at home position
            canSwitch = (scrollOffset == 0 && scrollDir == 1);
        }
        if (canSwitch) switchScreen();
    }

    delay(20); // ~50fps render loop
}
