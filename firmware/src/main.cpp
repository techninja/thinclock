#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <Preferences.h>
#include "thinclock.h"
#include "display.h"
#include "config_manager.h"
#include "sensors.h"

Display display;
ConfigManager configMgr;
Config config;
Preferences prefs;
Sensors sensors;
WebServer httpServer(80);

String wifiSSID, wifiPass, configURL;
uint32_t lastConfigFetch = 0, lastDataFetch = 0, lastSensorRead = 0;
uint32_t lastScreenSwitch = 0, lastButtonCheck = 0;
int currentScreen = 0;
JsonDocument screenData;

// Scroll state (reusable for both outgoing and incoming)
struct ScrollState {
    int16_t offset = 0;
    int8_t dir = 1;
    uint32_t pauseUntil = 0;
    uint32_t lastStep = 0;
    ScrollMode mode = SCROLL_NONE;
    int16_t textW = 0;
    bool completedOnce = false;
    String resolved;
    // Icon animation
    uint8_t iconFrame = 0;
    uint32_t lastIconStep = 0;
};

ScrollState scroll;       // current/incoming screen
ScrollState prevScroll;   // outgoing screen (during transition)
int prevScreenIdx = -1;
JsonDocument prevScreenData;

// Transition
uint16_t transitionProgress = 255;
bool transitioning = false;

// Buttons
bool btnLeftLast = HIGH, btnMidLast = HIGH, btnRightLast = HIGH;

#define SENSOR_READ_MS 2000
#define BUTTON_CHECK_MS 50

// --- Buttons ---

void postEvent(const char* event) {
    if (config.event_url.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(config.event_url);
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/json");
    String body = "{\"event\":\"" + String(event) + "\",\"screen\":" + currentScreen + "}";
    http.POST(body);
    http.end();
}

void checkButtons() {
    bool l = digitalRead(BUTTON_LEFT);
    bool m = digitalRead(BUTTON_MID);
    bool r = digitalRead(BUTTON_RIGHT);
    if (l == LOW && btnLeftLast == HIGH)  postEvent("left");
    if (m == LOW && btnMidLast == HIGH)   postEvent("select");
    if (r == LOW && btnRightLast == HIGH) postEvent("right");
    btnLeftLast = l; btnMidLast = m; btnRightLast = r;
}

// --- HTTP endpoints ---

void handleSensors() {
    JsonDocument doc;
    doc["temperature"] = round(sensors.data.temperature * 10.0) / 10.0;
    doc["humidity"] = round(sensors.data.humidity * 10.0) / 10.0;
    doc["light"] = (int)sensors.data.lightPct;
    doc["light_raw"] = (int)sensors.data.light;
    doc["sensor"] = sensors.type != SENSOR_NONE;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

void handleStatus() {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["wifi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["config_valid"] = config.valid;
    doc["screen"] = currentScreen;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// --- WiFi & Serial ---

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
        delay(250); Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FAIL");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        configTzTime("UTC0", "pool.ntp.org");
        httpServer.on("/sensors", handleSensors);
        httpServer.on("/status", handleStatus);
        httpServer.begin();
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

// --- Rendering ---

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
        if (config.time_format == "12h") {
            int h = t.tm_hour % 12; if (h == 0) h = 12;
            snprintf(buf, sizeof(buf), "%2d:%02d", h, t.tm_min);
        } else {
            snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        }
        display.drawText(buf, 2, 0, 0x00AAFF);
    }
    display.show();
}

// Render a screen with given scroll state (advances scroll each call)
void renderScreenWithState(Screen& scr, ScrollState& ss, const JsonDocument& data) {
    String text = scr.label;
    if (!scr.data_url.isEmpty() && !data.isNull()) {
        text = configMgr.resolvePlaceholders(scr.label, data);
    }

    // Icon lookup
    Icon* icon = nullptr;
    int16_t iconW = 0;
    if (!scr.icon.isEmpty() && config.icons.count(scr.icon)) {
        icon = &config.icons[scr.icon];
        iconW = icon->width + 1; // +1 pixel gap
    }

    // Text changed — reinit scroll
    if (text != ss.resolved) {
        ss.resolved = text;
        ss.textW = display.textWidth(ss.resolved);
        ss.offset = 0;
        ss.dir = 1;
        ss.pauseUntil = 0;
        ss.completedOnce = false;
        ss.lastStep = millis();

        int16_t startX = scr.text_x >= 0 ? scr.text_x : iconW;
        bool needsScroll = (ss.textW + startX) > MATRIX_WIDTH;

        if (scr.scroll == SCROLL_NONE) ss.mode = SCROLL_NONE;
        else if (scr.scroll == SCROLL_LEFT || scr.scroll == SCROLL_BOUNCE) ss.mode = scr.scroll;
        else ss.mode = needsScroll ? SCROLL_BOUNCE : SCROLL_NONE;
    }

    int16_t textX = scr.text_x >= 0 ? scr.text_x : iconW;
    int16_t y = scr.text_y >= 0 ? scr.text_y : 0;
    uint32_t now = millis();

    display.clear();

    // 1) Draw text first
    if (ss.mode == SCROLL_NONE) {
        display.drawText(ss.resolved, textX, y, scr.color);

    } else if (ss.mode == SCROLL_BOUNCE) {
        display.drawText(ss.resolved, textX - ss.offset, y, scr.color);
        display.applyEdgeFade(scr.fade_edge);

        if (now >= ss.pauseUntil && now - ss.lastStep >= scr.scroll_speed) {
            ss.offset += ss.dir;
            ss.lastStep = now;
            int16_t maxOff = ss.textW + textX - MATRIX_WIDTH;
            if (maxOff < 0) maxOff = 0;
            if (ss.dir == 1 && ss.offset >= maxOff) {
                ss.offset = maxOff; ss.dir = -1; ss.pauseUntil = now + 800;
            } else if (ss.dir == -1 && ss.offset <= 0) {
                ss.offset = 0; ss.dir = 1; ss.pauseUntil = now + 800;
                ss.completedOnce = true;
            }
        }

    } else { // SCROLL_LEFT
        // Start text just off the right edge of the visible area
        int16_t entryX = MATRIX_WIDTH;
        int16_t drawX = entryX - ss.offset;
        display.drawText(ss.resolved, drawX, y, scr.color);
        display.applyEdgeFade(scr.fade_edge);

        if (now - ss.lastStep >= scr.scroll_speed) {
            ss.offset++;
            ss.lastStep = now;
            // Fully exited left (past icon zone)
            int16_t exitX = icon ? -(int16_t)icon->width : 0;
            if (drawX + ss.textW < exitX) {
                ss.offset = 0;
                ss.completedOnce = true;
            }
        }
    }

    // 2) Clear icon zone and draw icon on top of text
    if (icon && !icon->frames.empty()) {
        display.clearRect(0, 0, icon->width, icon->height);

        if (icon->fps > 0 && icon->frames.size() > 1) {
            uint32_t frameMs = 1000 / icon->fps;
            if (now - ss.lastIconStep >= frameMs) {
                ss.iconFrame = (ss.iconFrame + 1) % icon->frames.size();
                ss.lastIconStep = now;
            }
        }
        uint8_t fi = ss.iconFrame % icon->frames.size();
        display.drawSprite(icon->frames[fi].data(), icon->width, icon->height, 0, 0);
    }
}

void resetScroll(ScrollState& ss) {
    ss.offset = 0; ss.dir = 1; ss.pauseUntil = 0; ss.lastStep = 0;
    ss.mode = SCROLL_NONE; ss.textW = 0; ss.completedOnce = false;
    ss.resolved = ""; ss.iconFrame = 0; ss.lastIconStep = 0;
}

void switchScreen() {
    // Save outgoing state
    prevScroll = scroll;
    prevScreenIdx = currentScreen;
    prevScreenData = screenData;

    // Advance
    currentScreen = (currentScreen + 1) % config.screens.size();
    lastScreenSwitch = millis();
    screenData.clear();
    lastDataFetch = 0;
    resetScroll(scroll);

    // Start transition (skip crossfade for banner screens - they start empty)
    Screen& nextScr = config.screens[currentScreen];
    if (nextScr.scroll == SCROLL_LEFT) {
        transitioning = false;
        transitionProgress = 255;
    } else {
        transitioning = true;
        transitionProgress = 0;
    }
}

// --- Main ---

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[thinclock]");

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUTTON_LEFT, INPUT_PULLUP);
    pinMode(BUTTON_MID, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT, INPUT_PULLUP);

    display.begin();
    display.clear();
    display.drawText("BOOT", 1, 0, 0x004400);
    display.show();

    sensors.begin();
    setupWiFi();
    config.valid = false;
    config.transition_ms = 8;
    config.time_format = "24h";
    config.temp_unit = "C";
}

void loop() {
    handleSerial();
    httpServer.handleClient();
    uint32_t now = millis();

    if (now - lastButtonCheck > BUTTON_CHECK_MS) { checkButtons(); lastButtonCheck = now; }
    if (now - lastSensorRead > SENSOR_READ_MS) { sensors.read(); lastSensorRead = now; }

    // Config fetch
    if (!configURL.isEmpty() && WiFi.status() == WL_CONNECTED) {
        if (!config.valid || now - lastConfigFetch > CONFIG_POLL_MS) {
            if (configMgr.fetchConfig(configURL, config)) {
                display.setBrightness(config.brightness);
                currentScreen = 0;
                lastScreenSwitch = now;
                resetScroll(scroll);
            }
            lastConfigFetch = now;
        }
    }

    // Fallback clock
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

    if (transitioning && prevScreenIdx >= 0) {
        // Render outgoing screen (still animating) into prev_frame
        display.renderToPrev();
        Screen& prevScr = config.screens[prevScreenIdx];
        renderScreenWithState(prevScr, prevScroll, prevScreenData);

        // Render incoming screen into render_buf
        display.renderToMain();
        renderScreenWithState(scr, scroll, screenData);

        // Blend and output
        transitionProgress += config.transition_ms;
        if (transitionProgress >= 255) {
            transitioning = false;
            prevScreenIdx = -1;
            display.show();
        } else {
            display.crossfade((uint8_t)transitionProgress);
        }
    } else {
        // Normal rendering
        renderScreenWithState(scr, scroll, screenData);
        display.show();
    }

    // Screen cycling
    if (now - lastScreenSwitch > scr.duration) {
        bool canSwitch = true;
        if (scroll.mode == SCROLL_LEFT || scroll.mode == SCROLL_BOUNCE) {
            canSwitch = scroll.completedOnce;
        }
        if (canSwitch) switchScreen();
    }

    delay(20);
}
