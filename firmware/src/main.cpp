#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "thinclock.h"
#include "display.h"
#include "config_manager.h"
#include "sensors.h"
#include "render_client.h"
#include "screen_state.h"
#include "tc_wifi.h"
#include "buttons.h"
#include "screens.h"
#include "http_routes.h"

// --- Globals ---
Display      display;
ConfigManager configMgr;
Config       config;
Preferences  prefs;
Sensors      sensors;
WebServer    httpServer(80);
DNSServer    dnsServer;
RenderClient renderClient;

String wifiSSID, wifiPass, configURL;
bool   wifiBadPassword = false;
std::vector<ScannedNet> scannedNets;

uint32_t lastConfigFetch = 0, lastDataFetch = 0, lastSensorRead = 0;
uint32_t lastScreenSwitch = 0, lastButtonCheck = 0;
int      currentScreen = 0;
JsonDocument screenData;

ScreenState  currentState, prevState;
int          prevScreenIdx = -1;
JsonDocument prevScreenData;
uint16_t     transitionProgress = 255;
bool         transitioning = false;

Notification notifications[MAX_NOTIFICATIONS];
int      notifCount = 0;
bool     notifViewerOpen = false;
int      notifViewerIdx = 0;
int16_t  notifSlideY = -8;
int16_t  notifScrollX = 0;
uint32_t notifLastScroll = 0;
uint32_t notifOpenTime = 0;
#define NOTIF_TIMEOUT_MS  60000
#define NOTIF_SCROLL_SPEED 80

Timer    timer;
bool     timerPaused = false;
uint32_t timerPausedRemaining = 0;

#define SENSOR_READ_MS  2000
#define BUTTON_CHECK_MS 50

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
    config.valid = false; config.transition_ms = 8; config.time_format = "24h";
}

void loop() {
    handleSerial();
    loopWiFi();
    dnsServer.processNextRequest();
    httpServer.handleClient();
    renderClient.loop();
    renderClient.tick(display, configMgr, config);
    uint32_t now = millis();

    // Collect async scan results once ready
    if (WiFi.scanComplete() >= 0 && scannedNets.empty()) {
        int scanN = WiFi.scanComplete();
        for (int i = 0; i < scanN; i++)
            scannedNets.push_back(ScannedNet{ WiFi.SSID(i), WiFi.RSSI(i) });
        WiFi.scanDelete();
        Serial.printf("[wifi] scan done: %d networks\n", scanN);
    }

    if (now - lastButtonCheck > BUTTON_CHECK_MS) { checkButtons(); checkLDR(); lastButtonCheck = now; now = millis(); }
    if (now - lastSensorRead  > SENSOR_READ_MS)  { sensors.read(); lastSensorRead = now; }

    // Config fetch with exponential backoff
    static uint32_t configBackoff = CONFIG_POLL_MS;
    static uint8_t  failCount = 0;
    if (!configURL.isEmpty() && WiFi.status() == WL_CONNECTED) {
        if (!config.valid || now - lastConfigFetch > configBackoff) {
            Config newCfg;
            if (configMgr.fetchConfig(configURL, newCfg)) {
                configBackoff = CONFIG_POLL_MS; failCount = 0;
                display.setBrightness(newCfg.brightness);
                char tz[16]; int off = -newCfg.timezone_offset;
                snprintf(tz, sizeof(tz), "UTC%+d", off); configTzTime(tz, "pool.ntp.org");
                bool wasInvalid = !config.valid; config = newCfg;
                if (currentScreen >= (int)config.screens.size() || wasInvalid) {
                    currentScreen = 0; lastScreenSwitch = now; resetState(currentState);
                }
            }
            lastConfigFetch = now;
            if (!config.valid) {
                configBackoff = min((uint32_t)300000, configBackoff * 2);
                if (++failCount >= 5) {
                    Serial.println("[config] too many failures — clearing URL");
                    configURL = ""; prefs.begin("thinclock", false); prefs.remove("config_url"); prefs.end();
                    failCount = 0; configBackoff = CONFIG_POLL_MS; ESP.restart();
                }
            }
        }
    }

    // AP setup mode — keep pumping DNS/HTTP, never block
    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t lastApScroll = 0;
        if (now - lastApScroll > 15000) {
            lastApScroll = now;
            // Scroll on display but pump server between frames
            auto scrollAP = [](const String& text, uint32_t color) {
                extern Display display;
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
            if (digitalRead(BUTTON_MID) == LOW) {
                lastApScroll = 0; // force re-scroll with IP
            }
        }
        return;
    }

    if (!config.valid || config.screens.empty()) {
        showClock();
        if (digitalRead(BUTTON_MID) == LOW) { delay(50); if (digitalRead(BUTTON_MID) == LOW) {
            scrollText("IP " + WiFi.localIP().toString(), 0x00FF44);
            if (!configURL.isEmpty()) scrollText(configURL, 0xFF8800);
        }}
        delay(500); return;
    }

    // Data fetch
    Screen& scr = config.screens[currentScreen];
    String dataUrl = scr.data_url;
    if (dataUrl.isEmpty()) for (auto& l : scr.layers) if (l.type == LAYER_TEXT && !l.data_url.isEmpty()) { dataUrl = l.data_url; break; }
    if (!dataUrl.isEmpty() && now - lastDataFetch > DATA_POLL_MS) { configMgr.fetchData(dataUrl, screenData); lastDataFetch = now; }

    // Render + transition
    if (transitioning && prevScreenIdx >= 0) {
        if (!dataUrl.isEmpty() && screenData.isNull()) {
            display.renderToMain(); renderScreen(config.screens[prevScreenIdx], prevState, prevScreenData);
        } else {
            display.renderToPrev(); renderScreen(config.screens[prevScreenIdx], prevState, prevScreenData);
            display.renderToMain(); renderScreen(scr, currentState, screenData);
            transitionProgress += config.transition_ms;
            if (transitionProgress >= 255) { transitioning = false; prevScreenIdx = -1; }
            else display.crossfade((uint8_t)transitionProgress);
        }
    } else renderScreen(scr, currentState, screenData);

    // Notification overlay
    if (notifViewerOpen) {
        if (now - notifOpenTime > NOTIF_TIMEOUT_MS) { notifViewerOpen = false; notifSlideY = -8; }
        else {
            display.fadeAll(25);
            if (notifSlideY < 0) notifSlideY += 1;
            if (notifViewerIdx == -1 && timer.active) {
                for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) display.drawPixel(bx, max((int16_t)0, notifSlideY), timer.color);
                if (notifSlideY >= 0) {
                    int32_t rem = timerPaused ? timerPausedRemaining : (int32_t)(timer.endTime - millis());
                    if (rem < 0) rem = 0;
                    char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", rem / 60000, (rem / 1000) % 60);
                    display.drawNativeText(buf, 8, notifSlideY + 2, timer.color, 1, false);
                }
            } else if (notifViewerIdx >= 0 && notifViewerIdx < notifCount) {
                Notification& n = notifications[notifViewerIdx];
                for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) display.drawPixel(bx, max((int16_t)0, notifSlideY), n.color);
                if (notifSlideY >= 0) {
                    int16_t textY = notifSlideY + 2, iconW = 0;
                    if (!n.icon_name.isEmpty() && config.icons.count(n.icon_name)) {
                        Icon& ic = config.icons[n.icon_name];
                        if (!ic.frames.empty()) { display.drawSprite(ic.frames[0].data(), ic.width, min((uint8_t)6, ic.height), 0, textY - 1); iconW = ic.width + 1; }
                    }
                    for (const auto& l : n.layers) {
                        if (l.type != LAYER_TEXT) continue;
                        int16_t tw = display.nativeTextWidth(l.label), avail = MATRIX_WIDTH - iconW;
                        if (tw <= avail) { display.drawNativeText(l.label, iconW + (avail - tw) / 2, textY, l.color, 1, false); }
                        else {
                            display.drawNativeText(l.label, MATRIX_WIDTH - notifScrollX, textY, l.color, 1, false);
                            if (iconW > 0) {
                                display.clearRect(0, textY - 1, iconW, 7);
                                Icon& ic = config.icons[n.icon_name];
                                if (!ic.frames.empty()) display.drawSprite(ic.frames[0].data(), ic.width, min((uint8_t)6, ic.height), 0, textY - 1);
                            }
                            if (now - notifLastScroll >= NOTIF_SCROLL_SPEED) {
                                notifScrollX++; notifLastScroll = now;
                                if ((MATRIX_WIDTH - notifScrollX) + tw < iconW) notifScrollX = 0;
                            }
                        }
                    }
                }
            } else { notifViewerOpen = false; notifSlideY = -8; }
        }
    } else if (notifCount > 0 || timer.active) {
        if (timer.active) {
            if (!timerPaused) {
                int32_t rem = (int32_t)(timer.endTime - millis()); if (rem < 0) rem = 0;
                float prog = 1.0f - (float)rem / timer.duration;
                uint16_t cyc = max((uint16_t)800, (uint16_t)(6000 - prog * prog * 5200));
                float breath = 0.25f + ((sin(millis() * 6.2832f / cyc) + 1.0f) * 0.5f) * 0.75f;
                display.drawPixel(MATRIX_WIDTH - 1, 0,
                    ((uint32_t)(uint8_t)(((timer.color >> 16) & 0xFF) * breath) << 16) |
                    ((uint32_t)(uint8_t)(((timer.color >>  8) & 0xFF) * breath) <<  8) |
                    (uint8_t)((timer.color & 0xFF) * breath));
            } else {
                display.drawPixel(MATRIX_WIDTH - 1, 0,
                    (((timer.color >> 16) & 0xFF) >> 2) << 16 | (((timer.color >> 8) & 0xFF) >> 2) << 8 | ((timer.color & 0xFF) >> 2));
            }
        }
        int dotOff = timer.active ? 2 : 0;
        for (int i = 0; i < notifCount && i < 3; i++) display.drawPixel(MATRIX_WIDTH - 1 - dotOff - (i * 2), 0, notifications[i].color);
    }

    display.show();

    // Timer completion
    if (timer.active && !timer.fired && !timerPaused && millis() >= timer.endTime) { timer.fired = true; beepTriple(); }

    // Alert beeps
    if (!notifViewerOpen) {
        if (timer.active && timer.fired) { static uint32_t lastTimerBeep = 0; if (now - lastTimerBeep >= 15000) { beepTriple(); lastTimerBeep = now; } }
        for (int i = 0; i < notifCount; i++)
            if (notifications[i].beep == 2 && notifications[i].active && now - notifications[i].lastBeep >= notifications[i].alertInterval)
                { beepTriple(); notifications[i].lastBeep = now; }
    }

    // Screen cycling
    if (!transitioning && now - lastScreenSwitch > scr.duration && !screenHasScrolling(currentState)) { switchScreen(); postEvent("screen_changed"); }

    delay(20);
}
