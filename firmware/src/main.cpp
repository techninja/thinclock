#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "thinclock.h"
#include "app_state.h"
#include "display.h"
#include "config_manager.h"
#include "sensors.h"
#include "render_client.h"
#include "screen_state.h"
#include "tc_wifi.h"
#include "buttons.h"
#include "screens.h"
#include "http_routes.h"

// --- Hardware singletons (genuinely process-wide) ---
Display       display;
ConfigManager configMgr;
Preferences   prefs;
Sensors       sensors;
WebServer     httpServer(80);
DNSServer     dnsServer;
RenderClient  renderClient;

String wifiSSID, wifiPass, configURL;
bool   wifiBadPassword = false;
std::vector<ScannedNet> scannedNets;

// --- Shared mutable state ---
AppState state;

#define SENSOR_READ_MS  2000
#define BUTTON_CHECK_MS 50
#define NOTIF_TIMEOUT_MS  60000
#define NOTIF_SCROLL_SPEED 80

// --- Serial provisioning ---
static void handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line.isEmpty()) return;
    JsonDocument doc;
    if (deserializeJson(doc, line)) return;
    if (doc["ssid"].is<const char*>()) {
        prefs.begin("thinclock", false);
        prefs.putString("ssid", doc["ssid"].as<const char*>());
        prefs.putString("pass", doc["pass"] | "");
        if (doc["config_url"].is<const char*>()) prefs.putString("config_url", doc["config_url"].as<const char*>());
        prefs.end();
        Serial.println("Saved. Rebooting..."); delay(500); ESP.restart();
    }
}

// --- setup / loop ---
void setup() {
    Serial.begin(115200); delay(500);
    Serial.println("\n[thinclock]");
    pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUTTON_LEFT,  INPUT_PULLUP);
    pinMode(BUTTON_MID,   INPUT_PULLUP);
    pinMode(BUTTON_RIGHT, INPUT_PULLUP);
    display.begin(); display.clear();
    display.drawText("BOOT", 1, 0, 0x004400); display.show();
    sensors.begin(); sensors.read();
    setupWiFi();
    state.config.valid = false;
    state.config.transition_ms = 8;
    state.config.time_format = "24h";
    registerHttpRoutes(state);
}

void loop() {
    handleSerial();
    loopWiFi();
    dnsServer.processNextRequest();
    httpServer.handleClient();
    renderClient.loop();
    renderClient.tick(display, configMgr, state);
    uint32_t now = millis();

    // Collect async scan results once ready
    if (WiFi.scanComplete() >= 0 && scannedNets.empty()) {
        int scanN = WiFi.scanComplete();
        for (int i = 0; i < scanN; i++)
            scannedNets.push_back(ScannedNet{ WiFi.SSID(i), WiFi.RSSI(i) });
        WiFi.scanDelete();
        Serial.printf("[wifi] scan done: %d networks\n", scanN);
    }

    static uint32_t lastSensorRead = 0, lastButtonCheck = 0;
    if (now - lastButtonCheck > BUTTON_CHECK_MS) { checkButtons(state); checkLDR(state); lastButtonCheck = now; now = millis(); }
    if (now - lastSensorRead  > SENSOR_READ_MS)  { sensors.read(); lastSensorRead = now; }

    // Config fetch with exponential backoff
    static uint32_t configBackoff = CONFIG_POLL_MS; // cppcheck-suppress variableScope
    static uint8_t  failCount = 0;                  // cppcheck-suppress variableScope
    if (!configURL.isEmpty() && WiFi.status() == WL_CONNECTED) {
        if (!state.config.valid || now - state.lastConfigFetch > configBackoff) {
            Config newCfg;
            if (configMgr.fetchConfig(configURL, newCfg)) {
                configBackoff = CONFIG_POLL_MS; failCount = 0;
                display.setBrightness(newCfg.brightness);
                char tz[16]; int off = -newCfg.timezone_offset;
                snprintf(tz, sizeof(tz), "UTC%+d", off); configTzTime(tz, "pool.ntp.org");
                bool wasInvalid = !state.config.valid; state.config = newCfg;
                if (state.currentScreen >= (int)state.config.screens.size() || wasInvalid) {
                    state.currentScreen = 0; state.lastScreenSwitch = now; resetState(state.currentState);
                }
                if (wasInvalid) {
                    String url = configURL;
                    url.replace("http://", ""); url.replace("https://", "");
                    int slash = url.indexOf('/');
                    if (slash > 0) url = url.substring(0, slash);
                    int colon = url.lastIndexOf(':');
                    String host = colon > 0 ? url.substring(0, colon) : url;
                    uint16_t port = colon > 0 ? url.substring(colon + 1).toInt() : 80;
                    renderClient.begin(host, port);
                    Serial.printf("[ws] connecting to %s:%d\n", host.c_str(), port);
                }
            }
            state.lastConfigFetch = now;
            if (!state.config.valid) {
                configBackoff = min((uint32_t)300000, configBackoff * 2);
                if (++failCount >= 5) {
                    Serial.println("[config] too many failures — clearing URL");
                    configURL = ""; prefs.begin("thinclock", false); prefs.remove("config_url"); prefs.end();
                    failCount = 0; configBackoff = CONFIG_POLL_MS; ESP.restart();
                }
            }
        }
    }

    // AP setup mode
    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t lastApScroll = 0;
        if (now - lastApScroll > 15000) {
            lastApScroll = now;
            auto scrollAP = [](const String& text, uint32_t color) {
                int16_t textW = display.nativeTextWidth(text, 1, false);
                for (int16_t x = MATRIX_WIDTH; x > -textW; x--) {
                    display.clear();
                    display.drawNativeText(text, x, 1, color, 1, false);
                    display.show();
                    dnsServer.processNextRequest();
                    httpServer.handleClient();
                    delay(80);
                }
            };
            scrollAP("SETUP", 0xFF8800);
            scrollAP("thinclock-setup", 0xFF8800);
            scrollAP("pw: thinclock", 0xFFAA00);
        }
        if (digitalRead(BUTTON_MID) == LOW) {
            delay(50);
            if (digitalRead(BUTTON_MID) == LOW) lastApScroll = 0;
        }
        return;
    }

    if (!state.config.valid || state.config.screens.empty()) {
        showClock();
        if (digitalRead(BUTTON_MID) == LOW) { delay(50); if (digitalRead(BUTTON_MID) == LOW) {
            scrollText("IP " + WiFi.localIP().toString(), 0x00FF44);
            if (!configURL.isEmpty()) scrollText(configURL, 0xFF8800);
        }}
        delay(500); return;
    }

    // Data fetch
    Screen& scr = state.config.screens[state.currentScreen];
    String dataUrl = scr.data_url;
    if (dataUrl.isEmpty()) for (auto& l : scr.layers) if (l.type == LAYER_TEXT && !l.data_url.isEmpty()) { dataUrl = l.data_url; break; }
    if (!dataUrl.isEmpty() && now - state.lastDataFetch > DATA_POLL_MS) { configMgr.fetchData(dataUrl, state.screenData); state.lastDataFetch = now; }

    // Render + transition
    if (state.transitioning && state.prevScreenIdx >= 0) {
        if (!dataUrl.isEmpty() && state.screenData.isNull()) {
            display.renderToMain(); renderScreen(state, state.config.screens[state.prevScreenIdx], state.prevState, state.prevScreenData);
        } else {
            display.renderToPrev(); renderScreen(state, state.config.screens[state.prevScreenIdx], state.prevState, state.prevScreenData);
            display.renderToMain(); renderScreen(state, scr, state.currentState, state.screenData);
            state.transitionProgress += state.config.transition_ms;
            if (state.transitionProgress >= 255) { state.transitioning = false; state.prevScreenIdx = -1; }
            else display.crossfade((uint8_t)state.transitionProgress);
        }
    } else renderScreen(state, scr, state.currentState, state.screenData);

    // Notification overlay
    if (state.notifViewerOpen) {
        if (now - state.notifOpenTime > NOTIF_TIMEOUT_MS) { state.notifViewerOpen = false; state.notifSlideY = -8; }
        else {
            display.fadeAll(25);
            if (state.notifSlideY < 0) state.notifSlideY += 1;
            if (state.notifViewerIdx == -1 && state.timer.active) {
                for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) display.drawPixel(bx, max((int16_t)0, state.notifSlideY), state.timer.color);
                if (state.notifSlideY >= 0) {
                    int32_t rem = state.timerPaused ? state.timerPausedRemaining : (int32_t)(state.timer.endTime - millis());
                    if (rem < 0) rem = 0;
                    char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", rem / 60000, (rem / 1000) % 60);
                    display.drawNativeText(buf, 8, state.notifSlideY + 2, state.timer.color, 1, false);
                }
            } else if (state.notifViewerIdx >= 0 && state.notifViewerIdx < state.notifCount) {
                Notification& n = state.notifications[state.notifViewerIdx];
                for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) display.drawPixel(bx, max((int16_t)0, state.notifSlideY), n.color);
                if (state.notifSlideY >= 0) {
                    int16_t textY = state.notifSlideY + 2, iconW = 0;
                    if (!n.icon_name.isEmpty() && state.config.icons.count(n.icon_name)) {
                        Icon& ic = state.config.icons[n.icon_name];
                        if (!ic.frames.empty()) { display.drawSprite(ic.frames[0].data(), ic.width, min((uint8_t)6, ic.height), 0, textY - 1); iconW = ic.width + 1; }
                    }
                    for (const auto& l : n.layers) {
                        if (l.type != LAYER_TEXT) continue;
                        int16_t tw = display.nativeTextWidth(l.label), avail = MATRIX_WIDTH - iconW;
                        if (tw <= avail) { display.drawNativeText(l.label, iconW + (avail - tw) / 2, textY, l.color, 1, false); }
                        else {
                            display.drawNativeText(l.label, MATRIX_WIDTH - state.notifScrollX, textY, l.color, 1, false);
                            if (iconW > 0) {
                                display.clearRect(0, textY - 1, iconW, 7);
                                Icon& ic = state.config.icons[n.icon_name];
                                if (!ic.frames.empty()) display.drawSprite(ic.frames[0].data(), ic.width, min((uint8_t)6, ic.height), 0, textY - 1);
                            }
                            if (now - state.notifLastScroll >= NOTIF_SCROLL_SPEED) {
                                state.notifScrollX++; state.notifLastScroll = now;
                                if ((MATRIX_WIDTH - state.notifScrollX) + tw < iconW) state.notifScrollX = 0;
                            }
                        }
                    }
                }
            } else { state.notifViewerOpen = false; state.notifSlideY = -8; }
        }
    } else if (state.notifCount > 0 || state.timer.active) {
        if (state.timer.active) {
            if (!state.timerPaused) {
                int32_t rem = (int32_t)(state.timer.endTime - millis()); if (rem < 0) rem = 0;
                float prog = 1.0f - (float)rem / state.timer.duration;
                uint16_t cyc = max((uint16_t)800, (uint16_t)(6000 - prog * prog * 5200));
                float breath = 0.25f + ((sin(millis() * 6.2832f / cyc) + 1.0f) * 0.5f) * 0.75f;
                display.drawPixel(MATRIX_WIDTH - 1, 0,
                    ((uint32_t)(uint8_t)(((state.timer.color >> 16) & 0xFF) * breath) << 16) |
                    ((uint32_t)(uint8_t)(((state.timer.color >>  8) & 0xFF) * breath) <<  8) |
                    (uint8_t)((state.timer.color & 0xFF) * breath));
            } else {
                display.drawPixel(MATRIX_WIDTH - 1, 0,
                    (((state.timer.color >> 16) & 0xFF) >> 2) << 16 |
                    (((state.timer.color >>  8) & 0xFF) >> 2) << 8  |
                    ((state.timer.color & 0xFF) >> 2));
            }
        }
        int dotOff = state.timer.active ? 2 : 0;
        for (int i = 0; i < state.notifCount && i < 3; i++)
            display.drawPixel(MATRIX_WIDTH - 1 - dotOff - (i * 2), 0, state.notifications[i].color);
    }

    display.show();

    // Timer completion
    if (state.timer.active && !state.timer.fired && !state.timerPaused && millis() >= state.timer.endTime)
        { state.timer.fired = true; beepTriple(); }

    // Alert beeps
    if (!state.notifViewerOpen) {
        if (state.timer.active && state.timer.fired) {
            static uint32_t lastTimerBeep = 0;
            if (now - lastTimerBeep >= 15000) { beepTriple(); lastTimerBeep = now; }
        }
        for (int i = 0; i < state.notifCount; i++)
            if (state.notifications[i].beep == 2 && state.notifications[i].active &&
                now - state.notifications[i].lastBeep >= state.notifications[i].alertInterval)
                { beepTriple(); state.notifications[i].lastBeep = now; }
    }

    // Screen cycling
    if (!state.transitioning && now - state.lastScreenSwitch > scr.duration && !screenHasScrolling(state.currentState))
        { switchScreen(state); postEvent(state, "screen_changed"); }

    delay(20);
}
