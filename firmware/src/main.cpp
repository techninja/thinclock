#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <time.h>
#include <algorithm>
#include <Preferences.h>
#include "thinclock.h"
#include "display.h"
#include "config_manager.h"
#include "sensors.h"
#include "gauge.h"
#include "particles.h"
#include "gif_encoder.h"
#include "render_client.h"
#include "screen_state.h"

Display display;
ConfigManager configMgr;
Config config;
Preferences prefs;
Sensors sensors;
WebServer httpServer(80);
RenderClient renderClient;

String wifiSSID, wifiPass, configURL;
uint32_t lastConfigFetch = 0, lastDataFetch = 0, lastSensorRead = 0;
uint32_t lastScreenSwitch = 0, lastButtonCheck = 0;
int currentScreen = 0;
JsonDocument screenData;

ScreenState currentState;
ScreenState prevState;
int prevScreenIdx = -1;
JsonDocument prevScreenData;

// Transition
uint16_t transitionProgress = 255;
bool transitioning = false;

// Notifications
Notification notifications[MAX_NOTIFICATIONS];
int notifCount = 0;
bool notifViewerOpen = false;
int notifViewerIdx = 0;
int16_t notifSlideY = -8;
ScreenState notifState;
JsonDocument notifData;
int16_t notifScrollX = 0;
uint32_t notifLastScroll = 0;
uint32_t notifOpenTime = 0;
#define NOTIF_TIMEOUT_MS 60000
#define NOTIF_SCROLL_SPEED 80

// Timer
Timer timer = {0, 0, 0x00AAFF, false, false};
uint32_t timerBreathPhase = 0;

// Buttons
bool btnLeftLast = HIGH, btnMidLast = HIGH, btnRightLast = HIGH;
uint32_t btnLeftDown = 0, btnMidDown = 0, btnRightDown = 0;
bool btnLeftLongFired = false, btnMidLongFired = false, btnRightLongFired = false;
uint32_t lastNavTime = 0;
#define NAV_COOLDOWN_MS 500
#define LONG_PRESS_MS 500
#define BEEP_DOWN_FREQ 1800
#define BEEP_DOWN_DUR 20
#define BEEP_SHORT_FREQ 2200
#define BEEP_SHORT_DUR 30
#define BEEP_LONG_FREQ 1200
#define BEEP_LONG_DUR 50

// LDR "button" (cover sensor to trigger)
bool ldrCovered = false;
uint32_t ldrCoverStart = 0;
#define LDR_COVER_THRESHOLD 50   // raw analog value considered "covered"
#define LDR_COVER_MIN_MS 200     // must be covered at least this long
#define LDR_COOLDOWN_MS 1000     // cooldown after trigger
uint32_t lastLdrTrigger = 0;
bool timerPaused = false;
uint32_t timerPausedRemaining = 0;

#define SENSOR_READ_MS 2000
#define BUTTON_CHECK_MS 50

// --- Buzzer ---
void beepOnce(uint16_t freq = 2000, uint16_t duration = 80) {
    if (!config.allow_beep) return;
    ledcSetup(0, freq, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 128);
    delay(duration);
    ledcWrite(0, 0);
    ledcDetachPin(BUZZER_PIN);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

void beepTriple() {
    if (!config.allow_beep) return;
    for (int i = 0; i < 3; i++) {
        beepOnce(2500, 60);
        if (i < 2) delay(80);
    }
}

// Forward declarations
void resetState(ScreenState& state);
void initScreenState(ScreenState& state, const Screen& scr);
void switchScreen();
void postEvent(const char* event);
void renderScreen(Screen& scr, ScreenState& state, const JsonDocument& data);

// --- Buttons ---
void navigatePrev() {
    if (config.screens.empty() || millis() - lastNavTime < NAV_COOLDOWN_MS) return;
    lastNavTime = millis();
    transitioning = false;
    prevScreenIdx = -1;
    currentScreen = (currentScreen - 1 + config.screens.size()) % config.screens.size();
    lastScreenSwitch = millis();
    screenData.clear();
    lastDataFetch = 0;
    resetState(currentState);
}

void navigateNext() {
    if (config.screens.empty() || millis() - lastNavTime < NAV_COOLDOWN_MS) return;
    lastNavTime = millis();
    switchScreen();
}

// --- LDR "button" ---
void checkLDR() {
    uint16_t ldr = analogRead(LDR_PIN);
    uint32_t now = millis();

    if (ldr < LDR_COVER_THRESHOLD) {
        if (!ldrCovered) {
            ldrCovered = true;
            ldrCoverStart = now;
        } else if (now - ldrCoverStart >= LDR_COVER_MIN_MS && now - lastLdrTrigger >= LDR_COOLDOWN_MS) {
            // Trigger! Pause/resume timer
            lastLdrTrigger = now;
            if (timer.active && !timer.fired) {
                if (timerPaused) {
                    // Resume: set new endTime based on remaining
                    timer.endTime = millis() + timerPausedRemaining;
                    timerPaused = false;
                    beepOnce(1800, 40);
                } else {
                    // Pause: save remaining
                    timerPausedRemaining = timer.endTime - millis();
                    timerPaused = true;
                    beepOnce(1200, 40);
                }
            }
            postEvent("ldr_cover");
        }
    } else {
        ldrCovered = false;
    }
}

void postEvent(const char* event) {
    if (config.event_url.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(config.event_url);
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/json");
    http.POST("{\"event\":\"" + String(event) + "\",\"screen\":" + currentScreen + "}");
    http.end();
}

void checkButtons() {
    bool l = digitalRead(BUTTON_LEFT);
    bool m = digitalRead(BUTTON_MID);
    bool r = digitalRead(BUTTON_RIGHT);
    uint32_t now = millis();

    // --- LEFT ---
    if (l == LOW && btnLeftLast == HIGH) {
        btnLeftDown = now; btnLeftLongFired = false;
        beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR);
    }
    if (l == LOW && !btnLeftLongFired && now - btnLeftDown >= LONG_PRESS_MS) {
        btnLeftLongFired = true;
        beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR);
        // Long left: (reserved for future use)
        postEvent("left_long");
    }
    if (l == HIGH && btnLeftLast == LOW && !btnLeftLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        // Short left
        if (notifViewerOpen) {
            if (notifViewerIdx > 0) {
                notifViewerIdx--;
            } else if (notifViewerIdx == 0 && timer.active) {
                notifViewerIdx = -1;
            } else if (notifViewerIdx == -1 && timer.fired) {
                timer.active = false;
                timer.fired = false;
                notifViewerOpen = false;
            }
            notifSlideY = -8;
            notifScrollX = 0;
            notifOpenTime = now;
        } else {
            if (config.buttons == "navigate") navigatePrev();
        }
        postEvent("left");
    }

    // --- MIDDLE ---
    if (m == LOW && btnMidLast == HIGH) {
        btnMidDown = now; btnMidLongFired = false;
        beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR);
    }
    if (m == LOW && !btnMidLongFired && now - btnMidDown >= LONG_PRESS_MS) {
        btnMidLongFired = true;
        beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR);
        // Long middle: context action OR cancel timer if in viewer
        if (notifViewerOpen && notifViewerIdx == -1 && timer.active) {
            // Cancel timer
            timer.active = false;
            timer.fired = false;
            timerPaused = false;
            notifViewerOpen = false;
            notifSlideY = -8;
            beepOnce(1000, 60); delay(80); beepOnce(600, 80);
        } else {
            timerPaused = false;
            postEvent("select_long");
            lastConfigFetch = 0; // force config refresh
        }
    }
    if (m == HIGH && btnMidLast == LOW && !btnMidLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        // Short middle
        if (notifViewerOpen) {
            // Just close viewer
            notifViewerOpen = false;
            notifSlideY = -8;
        } else if (notifCount > 0 || timer.active) {
            // Dot showing: open viewer
            notifViewerOpen = true;
            notifViewerIdx = timer.active ? -1 : 0;
            notifSlideY = -8;
            notifScrollX = 0;
            notifOpenTime = now;
        } else {
            // No dots: context action
            timerPaused = false;
            postEvent("select");
            lastConfigFetch = 0; // force config refresh
        }
    }

    // --- RIGHT ---
    if (r == LOW && btnRightLast == HIGH) {
        btnRightDown = now; btnRightLongFired = false;
        beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR);
    }
    if (r == LOW && !btnRightLongFired && now - btnRightDown >= LONG_PRESS_MS) {
        btnRightLongFired = true;
        beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR);
        // Long right: (reserved for future use)
        postEvent("right_long");
    }
    if (r == HIGH && btnRightLast == LOW && !btnRightLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        // Short right
        if (notifViewerOpen) {
            if (notifViewerIdx == -1) {
                if (timer.fired) {
                    timer.active = false;
                    timer.fired = false;
                }
                if (notifCount > 0) {
                    notifViewerIdx = 0;
                } else {
                    notifViewerOpen = false;
                }
            } else {
                notifViewerIdx++;
                if (notifViewerIdx >= notifCount) {
                    notifViewerOpen = false;
                    notifCount = 0;
                    for (auto& n : notifications) n.active = false;
                }
            }
            notifSlideY = -8;
            notifScrollX = 0;
            notifOpenTime = now;
        } else {
            if (config.buttons == "navigate") navigateNext();
        }
        postEvent("right");
    }

    btnLeftLast = l; btnMidLast = m; btnRightLast = r;
}

// --- HTTP ---
void handleSensors() {
    JsonDocument doc;
    doc["temperature"] = round(sensors.data.temperature * 10.0) / 10.0;
    doc["humidity"] = round(sensors.data.humidity * 10.0) / 10.0;
    doc["light"] = (int)sensors.data.lightPct;
    doc["light_raw"] = (int)sensors.data.light;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

void handleStatus() {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["wifi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["screen"] = currentScreen;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

void handleNotify() {
    if (httpServer.method() == HTTP_POST) {
        // Add notification: POST /notify {"color":"FF0000","layers":[...]}
        if (notifCount >= MAX_NOTIFICATIONS) {
            httpServer.send(429, "application/json", "{\"error\":\"full\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, httpServer.arg("plain"));
        if (err) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }

        Notification& n = notifications[notifCount];
        n.active = true;
        n.color = strtoul((doc["color"] | "FFAA00"), NULL, 16);
        n.icon_name = doc["icon"] | "";
        n.layers.clear();
        // Simple text notification shorthand
        if (doc["text"].is<const char*>()) {
            Layer l;
            l.type = LAYER_TEXT;
            l.label = doc["text"].as<const char*>();
            l.x = 0; l.y = 0;
            l.color = strtoul((doc["text_color"] | "FFFFFF"), NULL, 16);
            l.scroll = SCROLL_AUTO;
            l.scroll_speed = 50;
            l.fade_edge = 2;
            l.opacity = 255;
            n.layers.push_back(l);
        }

        // Beep config
        const char* beepStr = doc["beep"] | "single";
        if (strcmp(beepStr, "none") == 0 || strcmp(beepStr, "false") == 0) n.beep = 0;
        else if (strcmp(beepStr, "alert") == 0) n.beep = 2;
        else n.beep = 1;  // default: single
        n.alertInterval = doc["alert_interval"] | 30000;  // default 30s
        n.lastBeep = 0;

        notifCount++;
        Serial.printf("[notif] added #%d beep=%d\n", notifCount, n.beep);

        // Immediate beep on receive
        if (n.beep == 1) beepOnce();
        else if (n.beep == 2) beepTriple();

        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else if (httpServer.method() == HTTP_DELETE) {
        // Clear all
        notifCount = 0;
        notifViewerOpen = false;
        for (auto& n : notifications) n.active = false;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else {
        // GET: return count
        String out = "{\"count\":" + String(notifCount) + "}";
        httpServer.send(200, "application/json", out);
    }
}

void handleTimer() {
    if (httpServer.method() == HTTP_POST) {
        // Start timer: POST /timer {"duration":1500000,"color":"00AAFF"}
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, httpServer.arg("plain"));
        if (err) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }

        timer.duration = doc["duration"] | 60000;
        timer.endTime = millis() + timer.duration;
        timer.color = strtoul((doc["color"] | "00AAFF"), NULL, 16);
        timer.active = true;
        timer.fired = false;
        beepOnce(1500, 50);
        Serial.printf("[timer] started %ums\n", (unsigned)timer.duration);
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else if (httpServer.method() == HTTP_DELETE) {
        // Cancel timer
        timer.active = false;
        timer.fired = false;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else {
        // GET: return status
        JsonDocument doc;
        doc["active"] = timer.active;
        if (timer.active) {
            int32_t remaining = (int32_t)(timer.endTime - millis());
            doc["remaining"] = remaining > 0 ? remaining : 0;
            doc["duration"] = timer.duration;
        }
        String out; serializeJson(doc, out);
        httpServer.send(200, "application/json", out);
    }
}

// --- WiFi & Serial ---

// Show scrolling text on the display (blocking, for status messages)
void scrollText(const String& text, uint32_t color = 0x00AAFF) {
    int16_t textW = display.nativeTextWidth(text, 1, false);
    for (int16_t x = MATRIX_WIDTH; x > -textW; x--) {
        display.clear();
        display.drawNativeText(text, x, 1, color, 1, false);
        display.show();
        delay(80);
    }
}

void startAPMode() {
    Serial.println("[wifi] Starting AP: thinclock-setup");
    display.clear();
    display.drawNativeText("SETUP", 1, 1, 0xFF8800, 1, false);
    display.show();

    WiFi.mode(WIFI_AP);
    WiFi.softAP("thinclock-setup", "thinclock");
    Serial.printf("[wifi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // Serve setup page
    httpServer.on("/", HTTP_GET, []() {
        httpServer.send(200, "text/html; charset=utf-8",
            "<html><body style='font-family:sans-serif;max-width:400px;margin:2em auto'>" \
            "<h2>thinclock setup</h2>" \
            "<form method='POST' action='/setup'>" \
            "<label>WiFi SSID<br><input name='ssid' style='width:100%'></label><br><br>" \
            "<label>WiFi Password<br><input name='pass' type='password' style='width:100%'></label><br><br>" \
            "<label>Config URL<br><input name='config_url' style='width:100%' placeholder='http://192.168.x.x:3232/api/config'></label><br><br>" \
            "<button type='submit' style='padding:8px 16px'>Save &amp; Reboot</button>" \
            "</form></body></html>"
        );
    });
    httpServer.on("/setup", HTTP_POST, []() {
        String ssid = httpServer.arg("ssid");
        String pass = httpServer.arg("pass");
        String url  = httpServer.arg("config_url");
        if (ssid.isEmpty()) { httpServer.send(400, "text/plain", "SSID required"); return; }
        prefs.begin("thinclock", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        if (!url.isEmpty()) prefs.putString("config_url", url);
        prefs.end();
        httpServer.send(200, "text/html; charset=utf-8", "<html><body><h2>Saved! Rebooting...</h2></body></html>");
        delay(1000);
        ESP.restart();
    });
    httpServer.begin();
}

void setupWiFi() {
    prefs.begin("thinclock", true);
    wifiSSID = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    configURL = prefs.getString("config_url", "");
    prefs.end();

    if (wifiSSID.isEmpty()) {
        Serial.println("No WiFi credentials — starting AP mode");
        startAPMode();
        return;
    }
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    Serial.printf("Connecting to %s", wifiSSID.c_str());
    display.clear();
    display.drawNativeText("WIFI", 1, 1, 0x0044FF, 1, false);
    display.show();
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) { delay(250); Serial.print("."); }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" FAIL — starting AP mode");
        startAPMode();
        return;
    }
    Serial.printf(" OK\nIP: %s\n", WiFi.localIP().toString().c_str());
    // Scroll IP on display so user can find the device
    scrollText("IP " + WiFi.localIP().toString(), 0x00FF44);
    // Advertise via mDNS so HA integration can auto-discover
    if (MDNS.begin("thinclock")) {
        MDNS.addService("_thinclock", "_tcp", 80);
        MDNS.addServiceTxt("_thinclock", "_tcp", "version", "0.9.0");
        MDNS.addServiceTxt("_thinclock", "_tcp", "ip", WiFi.localIP().toString());
        Serial.println("[mdns] thinclock.local");
    }
    configTzTime("UTC0", "pool.ntp.org");
    httpServer.on("/sensors", handleSensors);
    httpServer.on("/status", handleStatus);
    httpServer.on("/notify", handleNotify);
    httpServer.on("/timer", handleTimer);
        httpServer.on("/beep", HTTP_POST, []() {
            // POST /beep {"pattern":[[freq, duration, pause], ...]}
            // or shorthand: {"type":"single|double|triple|alarm"}
            JsonDocument doc;
            deserializeJson(doc, httpServer.arg("plain"));
            const char* type = doc["type"] | "";
            if (strcmp(type, "single") == 0) { beepOnce(); }
            else if (strcmp(type, "double") == 0) { beepOnce(1500, 60); delay(80); beepOnce(1500, 60); }
            else if (strcmp(type, "triple") == 0) { beepTriple(); }
            else if (strcmp(type, "alarm") == 0) { for(int i=0;i<5;i++){beepOnce(2500,40);delay(60);} }
            else if (doc["pattern"].is<JsonArray>()) {
                for (JsonArray note : doc["pattern"].as<JsonArray>()) {
                    uint16_t freq = note[0] | 2000;
                    uint16_t dur = note[1] | 80;
                    uint16_t pause = note[2] | 0;
                    beepOnce(freq, dur);
                    if (pause > 0) delay(pause);
                }
            }
            httpServer.send(200, "application/json", "{\"ok\":true}");
        });
        // Direct display control: push layers to render immediately
        httpServer.on("/display", HTTP_POST, []() {
            // POST /display {"layers":[...], "duration": 5000}
            // Temporarily overrides current screen with provided layers
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, httpServer.arg("plain"));
            if (err) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
            // TODO: parse layers and render as temporary override screen
            httpServer.send(200, "application/json", "{\"ok\":true}");
        });

        // Device info
        httpServer.on("/", HTTP_GET, []() {
            String ip = WiFi.localIP().toString();
            String cfg = config.valid ? "<span style='color:#4c4'>&#x2714; connected</span>" : "<span style='color:#c44'>&#x2718; not connected</span>";
            httpServer.send(200, "text/html; charset=utf-8",
                "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>" \
                "<style>body{font-family:sans-serif;max-width:420px;margin:2em auto;padding:0 1em}" \
                "input{width:100%;box-sizing:border-box;padding:6px;margin-top:4px}" \
                "button{padding:8px 16px;margin-top:8px;cursor:pointer}</style></head><body>" \
                "<h2>&#x1F551; thinclock</h2>" \
                "<p>IP: <b>" + ip + "</b> &nbsp; Config: " + cfg + "</p>" \
                "<form method='POST' action='/setup'>" \
                "<label>WiFi SSID<br><input name='ssid' value='" + wifiSSID + "'></label><br><br>" \
                "<label>WiFi Password<br><input name='pass' type='password' placeholder='(unchanged)'></label><br><br>" \
                "<label>Config URL<br><input name='config_url' value='" + configURL + "'></label><br>" \
                "<button type='submit'>Save &amp; Reboot</button>" \
                "</form>" \
                "<hr><p style='font-size:0.85em'><a href='/info'>info</a> &middot; <a href='/sensors'>sensors</a> &middot; <a href='/status'>status</a></p>" \
                "</body></html>"
            );
        });
        httpServer.on("/setup", HTTP_POST, []() {
            String ssid = httpServer.arg("ssid");
            String pass = httpServer.arg("pass");
            String url  = httpServer.arg("config_url");
            if (ssid.isEmpty()) { httpServer.send(400, "text/plain", "SSID required"); return; }
            prefs.begin("thinclock", false);
            prefs.putString("ssid", ssid);
            if (!pass.isEmpty()) prefs.putString("pass", pass);
            prefs.putString("config_url", url);
            prefs.end();
            httpServer.send(200, "text/html; charset=utf-8", "<html><body><h2>Saved! Rebooting...</h2></body></html>");
            delay(1000);
            ESP.restart();
        });
        httpServer.on("/info", HTTP_GET, []() {
            JsonDocument doc;
            doc["firmware"] = "thinclock";
            doc["version"] = "0.9.0";
            doc["build"] = __DATE__ " " __TIME__;
            doc["chip"] = ESP.getChipModel();
            doc["flash"] = ESP.getFlashChipSize();
            doc["free_heap"] = ESP.getFreeHeap();
            doc["uptime"] = millis() / 1000;
            doc["wifi_ssid"] = WiFi.SSID();
            doc["ip"] = WiFi.localIP().toString();
            doc["rssi"] = WiFi.RSSI();
            doc["config_url"] = configURL;
            String out; serializeJson(doc, out);
            httpServer.send(200, "application/json", out);
        });

        // Raw framebuffer: 768 bytes RGB (32x8x3), logical pixel order
        httpServer.on("/framebuffer", HTTP_GET, []() {
            const uint8_t* fb = display.getFramebuffer();
            // Unzigzag: convert physical LED order to logical x,y order
            static uint8_t linear[NUM_LEDS * 3];
            for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
                for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
                    uint8_t physX = (y % 2 == 0) ? x : (MATRIX_WIDTH - 1 - x);
                    uint16_t src = (y * MATRIX_WIDTH + physX) * 3;
                    uint16_t dst = (y * MATRIX_WIDTH + x) * 3;
                    linear[dst] = fb[src];
                    linear[dst + 1] = fb[src + 1];
                    linear[dst + 2] = fb[src + 2];
                }
            }
            WiFiClient client = httpServer.client();
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: application/octet-stream\r\n");
            client.printf("Content-Length: %d\r\n", NUM_LEDS * 3);
            client.print("Cache-Control: no-store\r\n");
            client.print("Access-Control-Allow-Origin: *\r\n");
            client.print("Connection: close\r\n\r\n");
            client.write(linear, NUM_LEDS * 3);
        });

        // Preview: render N frames of a screen, stream as raw RGB
        // GET /preview?screen=0&frames=30
        httpServer.on("/preview", HTTP_GET, []() {
            if (!config.valid || config.screens.empty()) {
                httpServer.send(400, "application/json", "{\"error\":\"no config\"}");
                return;
            }
            int screenIdx = httpServer.arg("screen").toInt();
            int frames = httpServer.arg("frames").toInt();
            if (screenIdx < 0 || screenIdx >= (int)config.screens.size()) {
                httpServer.send(400, "application/json", "{\"error\":\"invalid screen\"}");
                return;
            }
            if (frames < 1) frames = 1;
            if (frames > 120) frames = 120;

            uint32_t totalBytes = NUM_LEDS * 3 * frames;
            WiFiClient client = httpServer.client();
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: application/octet-stream\r\n");
            client.printf("Content-Length: %d\r\n", totalBytes);
            client.printf("X-Frames: %d\r\n", frames);
            client.printf("X-Frame-Ms: %d\r\n", 20);
            client.print("Cache-Control: public, max-age=60\r\n");
            client.print("Access-Control-Allow-Origin: *\r\n");
            client.print("Connection: close\r\n\r\n");

                // Save current render buffer so live display isn't corrupted
            static CRGB savedBuf[NUM_LEDS];
            const uint8_t* fbSave = display.getFramebuffer();
            memcpy(savedBuf, fbSave, sizeof(savedBuf));

            // Render frames using isolated state
            Screen& scr = config.screens[screenIdx];
            ScreenState previewState;
            resetState(previewState);
            initScreenState(previewState, scr);
            JsonDocument previewData;
            if (!scr.data_url.isEmpty()) {
                configMgr.fetchData(scr.data_url, previewData);
            }

            static uint8_t linear[NUM_LEDS * 3];
            for (int f = 0; f < frames; f++) {
                display.clear();
                renderScreen(scr, previewState, previewData);
                const uint8_t* fb = display.getFramebuffer();
                // Unzigzag
                for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
                    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
                        uint8_t physX = (y % 2 == 0) ? x : (MATRIX_WIDTH - 1 - x);
                        uint16_t src = (y * MATRIX_WIDTH + physX) * 3;
                        uint16_t dst = (y * MATRIX_WIDTH + x) * 3;
                        linear[dst] = fb[src];
                        linear[dst + 1] = fb[src + 1];
                        linear[dst + 2] = fb[src + 2];
                    }
                }
                client.write(linear, NUM_LEDS * 3);
                yield();
            }

            memcpy(const_cast<uint8_t*>(fbSave), savedBuf, sizeof(savedBuf));
        });

        // Render arbitrary layers: POST /render {"layers":[...], "frames":30, "display":false}
        httpServer.on("/render", HTTP_POST, []() {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, httpServer.arg("plain"));
            if (err) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }

            int frames = doc["frames"] | 1;
            if (frames < 1) frames = 1;
            if (frames > 120) frames = 120;
            bool showOnDevice = doc["display"] | false;

            // Parse into a temporary Screen
            Screen tmpScreen;
            tmpScreen.duration = 0;
            tmpScreen.data_url = doc["data_url"] | "";
            for (JsonObject l : doc["layers"].as<JsonArray>()) {
                tmpScreen.layers.push_back(configMgr.parseLayer(l, config.scroll_speed));
            }
            // Merge any provided icons into config
            if (doc["icons"].is<JsonObject>()) {
                configMgr.parseIcons(doc["icons"].as<JsonObject>(), config.icons);
            }

            // Save render buffer
            static CRGB savedBuf[NUM_LEDS];
            const uint8_t* fbSave = display.getFramebuffer();
            memcpy(savedBuf, fbSave, sizeof(savedBuf));

            // Init state
            ScreenState renderState;
            resetState(renderState);
            initScreenState(renderState, tmpScreen);

            // Use inline data if provided
            JsonDocument renderData;
            if (doc["data"].is<JsonObject>()) {
                renderData = doc["data"];
            }

            uint32_t totalBytes = NUM_LEDS * 3 * frames;
            WiFiClient client = httpServer.client();
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: application/octet-stream\r\n");
            client.printf("Content-Length: %d\r\n", totalBytes);
            client.printf("X-Frames: %d\r\n", frames);
            client.printf("X-Frame-Ms: %d\r\n", 20);
            client.print("Access-Control-Allow-Origin: *\r\n");
            client.print("Connection: close\r\n\r\n");

            static uint8_t linear[NUM_LEDS * 3];
            for (int f = 0; f < frames; f++) {
                display.clear();
                renderScreen(tmpScreen, renderState, renderData);
                const uint8_t* fb = display.getFramebuffer();
                // Unzigzag
                for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
                    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
                        uint8_t physX = (y % 2 == 0) ? x : (MATRIX_WIDTH - 1 - x);
                        uint16_t src = (y * MATRIX_WIDTH + physX) * 3;
                        uint16_t dst = (y * MATRIX_WIDTH + x) * 3;
                        linear[dst] = fb[src];
                        linear[dst + 1] = fb[src + 1];
                        linear[dst + 2] = fb[src + 2];
                    }
                }
                client.write(linear, NUM_LEDS * 3);
                if (showOnDevice) display.show();
                yield();
            }

            if (!showOnDevice) {
                memcpy(const_cast<uint8_t*>(fbSave), savedBuf, sizeof(savedBuf));
            }
        });

        // GIF preview: GET /gif?screen=0&seconds=2
        // Returns animated GIF with frame deduplication
        httpServer.on("/gif", HTTP_GET, []() {
            if (!config.valid || config.screens.empty()) {
                httpServer.send(400, "application/json", "{\"error\":\"no config\"}");
                return;
            }
            int screenIdx = httpServer.arg("screen").toInt();
            int seconds = httpServer.arg("seconds").toInt();
            if (screenIdx < 0 || screenIdx >= (int)config.screens.size()) {
                httpServer.send(400, "application/json", "{\"error\":\"invalid screen\"}");
                return;
            }
            if (seconds < 1) seconds = 1;
            if (seconds > 10) seconds = 10;
            int frames = seconds * 15;
            uint8_t scale = httpServer.arg("scale").toInt();
            uint8_t gap = httpServer.arg("gap").toInt();
            uint8_t gamma = httpServer.arg("gamma").toInt();
            if (scale < 1) scale = 1;
            if (gamma < 10) gamma = 18; // default 1.8

            // Save render buffer
            static CRGB savedBuf[NUM_LEDS];
            const uint8_t* fbSave = display.getFramebuffer();
            memcpy(savedBuf, fbSave, sizeof(savedBuf));

            Screen& scr = config.screens[screenIdx];
            ScreenState gifState;
            resetState(gifState);
            initScreenState(gifState, scr);
            JsonDocument gifData;
            if (!scr.data_url.isEmpty()) {
                configMgr.fetchData(scr.data_url, gifData);
            }

            WiFiClient client = httpServer.client();
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: image/gif\r\n");
            client.print("Cache-Control: public, max-age=60\r\n");
            client.print("Access-Control-Allow-Origin: *\r\n");
            client.print("Connection: close\r\n\r\n");

            GifEncoder gif;
            gif.begin(client, 66, scale, gap, gamma);

            static uint8_t linear[NUM_LEDS * 3];
            for (int f = 0; f < frames; f++) {
                display.clear();
                renderScreen(scr, gifState, gifData);
                const uint8_t* fb = display.getFramebuffer();
                memset(linear, 0, sizeof(linear));
                for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
                    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
                        uint8_t physX = (y % 2 == 0) ? x : (MATRIX_WIDTH - 1 - x);
                        uint16_t src = (y * MATRIX_WIDTH + physX) * 3;
                        uint16_t dst = (y * MATRIX_WIDTH + x) * 3;
                        linear[dst] = fb[src];
                        linear[dst + 1] = fb[src + 1];
                        linear[dst + 2] = fb[src + 2];
                    }
                }
                gif.addFrame(linear);
                delay(66); // advance real time so particles/tweens progress
                yield();
            }
            gif.end();
            memcpy(const_cast<uint8_t*>(fbSave), savedBuf, sizeof(savedBuf));
        });

        // POST /gif — render arbitrary layers as animated GIF
        httpServer.on("/gif", HTTP_POST, []() {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, httpServer.arg("plain"));
            if (err) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }

            int seconds = doc["seconds"] | 2;
            if (seconds < 1) seconds = 1;
            if (seconds > 10) seconds = 10;
            int frames = seconds * 15;
            uint8_t scale = doc["scale"] | 1;
            uint8_t gap = doc["gap"] | 0;
            uint8_t gamma = doc["gamma"] | 18;
            if (scale < 1) scale = 1;
            if (gamma < 10) gamma = 18;

            Screen tmpScreen;
            tmpScreen.duration = 0;
            tmpScreen.data_url = doc["data_url"] | "";
            for (JsonObject l : doc["layers"].as<JsonArray>()) {
                tmpScreen.layers.push_back(configMgr.parseLayer(l, config.scroll_speed));
            }
            if (doc["icons"].is<JsonObject>()) {
                configMgr.parseIcons(doc["icons"].as<JsonObject>(), config.icons);
            }

            static CRGB savedBuf[NUM_LEDS];
            const uint8_t* fbSave = display.getFramebuffer();
            memcpy(savedBuf, fbSave, sizeof(savedBuf));

            ScreenState renderState;
            resetState(renderState);
            initScreenState(renderState, tmpScreen);
            JsonDocument renderData;
            if (doc["data"].is<JsonObject>()) {
                renderData = doc["data"];
            } else if (!tmpScreen.data_url.isEmpty()) {
                configMgr.fetchData(tmpScreen.data_url, renderData);
            }

            WiFiClient client = httpServer.client();
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: image/gif\r\n");
            client.print("Cache-Control: public, max-age=60\r\n");
            client.print("Access-Control-Allow-Origin: *\r\n");
            client.print("Connection: close\r\n\r\n");

            GifEncoder gif;
            gif.begin(client, 66, scale, gap, gamma);
            static uint8_t linear[NUM_LEDS * 3];
            for (int f = 0; f < frames; f++) {
                display.clear();
                renderScreen(tmpScreen, renderState, renderData);
                const uint8_t* fb = display.getFramebuffer();
                memset(linear, 0, sizeof(linear));
                for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
                    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
                        uint8_t physX = (y % 2 == 0) ? x : (MATRIX_WIDTH - 1 - x);
                        uint16_t src = (y * MATRIX_WIDTH + physX) * 3;
                        uint16_t dst = (y * MATRIX_WIDTH + x) * 3;
                        linear[dst] = fb[src];
                        linear[dst + 1] = fb[src + 1];
                        linear[dst + 2] = fb[src + 2];
                    }
                }
                gif.addFrame(linear);
                delay(66);
                yield();
            }
            gif.end();
            memcpy(const_cast<uint8_t*>(fbSave), savedBuf, sizeof(savedBuf));
        });

        // CORS for browser UI access
        httpServer.enableCORS(true);
        httpServer.begin();

        // Start WebSocket render client — connect back to config server
        if (!configURL.isEmpty()) {
            // Extract host and port from config URL (http://host:port/path)
            String url = configURL;
            url.replace("http://", "");
            int slashIdx = url.indexOf('/');
            if (slashIdx > 0) url = url.substring(0, slashIdx);
            int colonIdx = url.indexOf(':');
            String host = colonIdx > 0 ? url.substring(0, colonIdx) : url;
            uint16_t port = colonIdx > 0 ? url.substring(colonIdx + 1).toInt() : 80;
            renderClient.begin(host, port);
            Serial.printf("[ws] Connecting to %s:%d\n", host.c_str(), port);
        }
}

void handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n'); line.trim();
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
        delay(500); ESP.restart();
    }
}

// --- Layer Rendering ---

void initScreenState(ScreenState& state, const Screen& scr) {
    state.textStates.clear();
    state.iconStates.clear();
    state.particleSystems.clear();
    state.lastTick = millis();
    state.startTime = millis();
    state.inited = true;

    for (const auto& layer : scr.layers) {
        if (layer.type == LAYER_TEXT || layer.type == LAYER_CLOCK) {
            state.textStates.push_back(TextState());
        }
        if (layer.type == LAYER_ICON) {
            state.iconStates.push_back(IconState());
        }
        if (layer.type == LAYER_PARTICLES) {
            ParticleSystem ps;
            ps.init(layer.particles);
            state.particleSystems.push_back(ps);
        }
    }
}

// --- Tween engine ---
static float tweenEase(float t, const String& easing) {
    if (easing == "sine") return 0.5f - 0.5f * cos(t * 3.14159f);
    if (easing == "ease_in") return t * t;
    if (easing == "ease_out") return 1.0f - (1.0f - t) * (1.0f - t);
    if (easing == "ease_in_out") return t < 0.5f ? 2*t*t : 1 - 2*(1-t)*(1-t);
    return t; // linear
}

static float evaluateTween(const Layer::Tween& tw, uint32_t elapsed) {
    if (elapsed < tw.delay) return tw.from;
    uint32_t active = elapsed - tw.delay;

    float progress;
    if (tw.loop == "repeat") {
        progress = (float)(active % tw.duration) / tw.duration;
    } else if (tw.loop == "pingpong") {
        uint32_t cycle = active % (tw.duration * 2);
        progress = (float)cycle / tw.duration;
        if (progress > 1.0f) progress = 2.0f - progress;
    } else {
        progress = (float)active / tw.duration;
        if (progress > 1.0f) progress = 1.0f;
    }

    float eased = tweenEase(progress, tw.easing);
    return tw.from + (tw.to - tw.from) * eased;
}

static void applyTweens(Layer& layer, uint32_t elapsed) {
    for (const auto& tw : layer.tweens) {
        if (tw.prop == "x") layer.x = (int16_t)val;
        else if (tw.prop == "y") layer.y = (int16_t)val;
        else if (tw.prop == "opacity") layer.opacity = (uint8_t)constrain((int)val, 0, 255);
    }
}

void renderScreen(Screen& scr, ScreenState& state, const JsonDocument& data) {
    if (!state.inited) initScreenState(state, scr);

    uint32_t now = millis();
    uint32_t dt = now - state.lastTick;
    state.lastTick = now;

    int textIdx = 0, iconIdx = 0, particleIdx = 0;

    display.clear();

    for (auto& layer : scr.layers) {
        // Apply tweens to layer properties
        if (!layer.tweens.empty()) {
            applyTweens(layer, now - state.startTime);
        }

        // Snapshot before layer for opacity/blend
        bool needsBlend = (layer.opacity < 255 || layer.blend == "add");
        if (needsBlend) display.snapshotLayer();

        switch (layer.type) {

        case LAYER_PARTICLES: {
            if (particleIdx < (int)state.particleSystems.size()) {
                auto& ps = state.particleSystems[particleIdx];
                ps.tick(dt);
                ps.render(display);
                particleIdx++;
            }
            break;
        }

        case LAYER_ICON: {
            if (layer.icon_name.isEmpty() || !config.icons.count(layer.icon_name)) break;
            Icon& icon = config.icons[layer.icon_name];
            if (icon.frames.empty()) break;

            IconState& is = state.iconStates[iconIdx++];
            if (icon.fps > 0 && icon.frames.size() > 1) {
                uint32_t frameMs = 1000 / icon.fps;
                if (now - is.lastStep >= frameMs) {
                    is.frame = (is.frame + 1) % icon.frames.size();
                    is.lastStep = now;
                }
            }
            uint8_t fi = is.frame % icon.frames.size();

            if (icon.remap_key != 0 && !icon.remap_range.stops.empty() && !icon.remap_value_key.isEmpty()) {
                float val = data[icon.remap_value_key.c_str()].as<float>();
                uint32_t newColor = colorFromRange(icon.remap_range, val);
                std::vector<uint8_t> remapped = icon.frames[fi];
                remapIconColor(remapped, icon.remap_key, newColor);
                display.drawSprite(remapped.data(), icon.width, icon.height, layer.x, layer.y);
            } else {
                display.drawSprite(icon.frames[fi].data(), icon.width, icon.height, layer.x, layer.y);
            }
            break;
        }

        case LAYER_TEXT: {
            TextState& ts = state.textStates[textIdx++];
            String text = layer.label;
            if (!layer.data_url.isEmpty() && !data.isNull()) {
                text = configMgr.resolvePlaceholders(layer.label, data);
            } else if (!scr.data_url.isEmpty() && !data.isNull()) {
                text = configMgr.resolvePlaceholders(layer.label, data);
            }

            if (text != ts.resolved) {
                ts.resolved = text;
                ts.textW = display.textWidth(ts.resolved);
                ts.offset = 0; ts.dir = 1; ts.pauseUntil = 0;
                ts.completedOnce = false; ts.lastStep = now;
                bool needsScroll = (ts.textW + layer.x) > MATRIX_WIDTH;
                if (layer.scroll == SCROLL_NONE) ts.mode = SCROLL_NONE;
                else if (layer.scroll == SCROLL_LEFT || layer.scroll == SCROLL_BOUNCE) ts.mode = layer.scroll;
                else ts.mode = needsScroll ? SCROLL_BOUNCE : SCROLL_NONE;
            }

            if (ts.mode == SCROLL_NONE) {
                display.drawText(ts.resolved, layer.x, layer.y, layer.color);
            } else if (ts.mode == SCROLL_BOUNCE) {
                display.drawText(ts.resolved, layer.x - ts.offset, layer.y, layer.color);
                display.applyEdgeFade(layer.fade_edge);
                if (now >= ts.pauseUntil && now - ts.lastStep >= layer.scroll_speed) {
                    ts.offset += ts.dir; ts.lastStep = now;
                    int16_t maxOff = ts.textW + layer.x - MATRIX_WIDTH;
                    if (maxOff < 0) maxOff = 0;
                    if (ts.dir == 1 && ts.offset >= maxOff) { ts.offset = maxOff; ts.dir = -1; ts.pauseUntil = now + 800; }
                    else if (ts.dir == -1 && ts.offset <= 0) { ts.offset = 0; ts.dir = 1; ts.pauseUntil = now + 800; ts.completedOnce = true; }
                }
            } else { // SCROLL_LEFT
                int16_t drawX = MATRIX_WIDTH - ts.offset;
                display.drawText(ts.resolved, drawX, layer.y, layer.color);
                display.applyEdgeFade(layer.fade_edge);
                if (now - ts.lastStep >= layer.scroll_speed) {
                    ts.offset++; ts.lastStep = now;
                    if (drawX + ts.textW < 0) { ts.offset = 0; ts.completedOnce = true; }
                }
            }
            break;
        }

        case LAYER_CLOCK: {
            TextState& ts = state.textStates[textIdx++];
            char buf[6] = "??:??";
            if (layer.clock_format == "timer") {
                // Render device's internal timer countdown
                if (timer.active) {
                    int32_t remaining;
                    if (timerPaused) {
                        remaining = timerPausedRemaining;
                    } else {
                        remaining = (int32_t)(timer.endTime - millis());
                        if (remaining < 0) remaining = 0;
                    }
                    int mins = remaining / 60000;
                    int secs = (remaining / 1000) % 60;
                    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                } else {
                    snprintf(buf, sizeof(buf), "--:--");
                }
            } else {
                struct tm t;
                if (getLocalTime(&t)) {
                    if (layer.clock_format == "12h") {
                        int h = t.tm_hour % 12; if (h == 0) h = 12;
                        // Blink colon: space instead of colon on odd seconds
                        char sep = (t.tm_sec % 2 == 0) ? ':' : ' ';
                        snprintf(buf, sizeof(buf), "%02d%c%02d", h, sep, t.tm_min);
                    } else {
                        char sep = (t.tm_sec % 2 == 0) ? ':' : ' ';
                        snprintf(buf, sizeof(buf), "%02d%c%02d", t.tm_hour, sep, t.tm_min);
                    }
                }
            }
            ts.resolved = buf;
            int16_t clockX = layer.x;
            if (layer.align == "center") {
                int16_t tw = display.nativeTextWidth(ts.resolved, layer.native_spacing, layer.native_large);
                int16_t area = layer.align_width > 0 ? layer.align_width : (MATRIX_WIDTH - layer.x);
                clockX = layer.x + (area - tw) / 2;
            } else if (layer.align == "right") {
                int16_t tw = display.nativeTextWidth(ts.resolved, layer.native_spacing, layer.native_large);
                int16_t area = layer.align_width > 0 ? layer.align_width : (MATRIX_WIDTH - layer.x);
                clockX = layer.x + area - tw;
            }
            display.drawNativeText(ts.resolved, clockX, layer.y, layer.color, layer.native_spacing, layer.native_large);
            // PM indicator dot for 12h format
            if (layer.clock_format == "12h") {
                struct tm t2;
                if (getLocalTime(&t2) && t2.tm_hour >= 12) {
                    // Small dot at bottom-right of clock area
                    int16_t dotX = layer.x + (layer.native_large ? 26 : 18);
                    int16_t dotY = layer.y + (layer.native_large ? 6 : 4);
                    display.drawPixel(dotX, dotY, layer.color);
                }
            }
            break;
        }

        case LAYER_NATIVE: {
            String text = layer.label;
            if (!layer.data_url.isEmpty() && !data.isNull()) {
                text = configMgr.resolvePlaceholders(layer.label, data);
            } else if (!scr.data_url.isEmpty() && !data.isNull()) {
                text = configMgr.resolvePlaceholders(layer.label, data);
            }
            int16_t natX = layer.x;
            if (layer.align == "center") {
                int16_t tw = display.nativeTextWidth(text, layer.native_spacing, layer.native_large);
                int16_t area = layer.align_width > 0 ? layer.align_width : (MATRIX_WIDTH - layer.x);
                natX = layer.x + (area - tw) / 2;
            } else if (layer.align == "right") {
                int16_t tw = display.nativeTextWidth(text, layer.native_spacing, layer.native_large);
                int16_t area = layer.align_width > 0 ? layer.align_width : (MATRIX_WIDTH - layer.x);
                natX = layer.x + area - tw;
            }
            display.drawNativeText(text, natX, layer.y, layer.color, layer.native_spacing, layer.native_large);
            break;
        }

        case LAYER_GAUGE: {
            float val = 0;
            if (!layer.value_key.isEmpty()) val = data[layer.value_key.c_str()].as<float>();
            drawGauge(display, layer.gauge, layer.gauge_w, layer.gauge_h, layer.range, val, layer.x, layer.y);
            break;
        }

        case LAYER_PIXELS: {
            if (layer.pixels_pattern == "week_dots") {
                // 7 day indicators, 2px each + 1px gap
                int dayOfWeek = 0;
                if (!layer.pixels_data_key.isEmpty()) {
                    dayOfWeek = data[layer.pixels_data_key.c_str()].as<int>();
                } else {
                    struct tm t;
                    if (getLocalTime(&t)) dayOfWeek = t.tm_wday; // 0=Sun
                }
                int16_t startX = layer.x;
                for (int d = 0; d < 7; d++) {
                    uint32_t c = (d == dayOfWeek) ? layer.pixels_color : layer.pixels_dim_color;
                    int16_t dx = startX + d * 3;
                    display.drawPixel(dx, layer.y, c);
                    display.drawPixel(dx + 1, layer.y, c);
                }
            } else if (layer.pixels_pattern == "vline") {
                for (int16_t row = layer.y; row < MATRIX_HEIGHT; row++) {
                    display.drawPixel(layer.x, row, layer.pixels_color);
                }
            } else if (layer.pixels_pattern == "dots" && !layer.pixels_points.empty()) {
                for (const auto& pt : layer.pixels_points) {
                    display.drawPixel(layer.x + pt.first, layer.y + pt.second, layer.pixels_color);
                }
            }
            break;
        }

        case LAYER_GRADIENT: {
            uint8_t gw = layer.grad_w > 0 ? layer.grad_w : MATRIX_WIDTH;
            uint8_t gh = layer.grad_h > 0 ? layer.grad_h : MATRIX_HEIGHT;
            for (uint8_t gy = 0; gy < gh; gy++) {
                for (uint8_t gx = 0; gx < gw; gx++) {
                    float t;
                    if (layer.grad_direction == "vertical") {
                        t = (float)gy / (gh - 1);
                    } else if (layer.grad_direction == "diagonal") {
                        t = ((float)gx / (gw - 1) + (float)gy / (gh - 1)) * 0.5f;
                    } else { // horizontal
                        t = (float)gx / (gw - 1);
                    }
                    float val = layer.grad_colors.min_val + t * (layer.grad_colors.max_val - layer.grad_colors.min_val);
                    uint32_t c = colorFromRange(layer.grad_colors, val);
                    display.drawPixel(layer.x + gx, layer.y + gy, c);
                }
            }
            break;
        }

        } // switch

        // Apply blend mode after layer rendered
        if (layer.blend == "add") {
            display.applyLayerAdditive();
        } else if (layer.opacity < 255) {
            display.applyLayerOpacity(layer.opacity);
        }
    } // for layers
}

// --- Screen management ---

void resetState(ScreenState& state) {
    state.inited = false;
    state.textStates.clear();
    state.iconStates.clear();
    state.particleSystems.clear();
}

bool screenHasScrolling(const ScreenState& state) {
    return std::any_of(state.textStates.begin(), state.textStates.end(),
        [](const TextState& ts) { return (ts.mode == SCROLL_LEFT || ts.mode == SCROLL_BOUNCE) && !ts.completedOnce; });
}

void switchScreen() {
    prevState = currentState;
    prevScreenIdx = currentScreen;
    prevScreenData = screenData;

    currentScreen = (currentScreen + 1) % config.screens.size();
    lastScreenSwitch = millis();
    screenData.clear();
    lastDataFetch = 0;
    resetState(currentState);

    transitioning = true;
    transitionProgress = 0;
}

// --- Fallback clock ---
void showClock() {
    struct tm t;
    display.clear();
    char buf[6] = "00:00";
    if (getLocalTime(&t)) snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    else { uint32_t s = millis()/1000; snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)((s/60)%100), (unsigned)(s%60)); }
    display.drawText(buf, 2, 0, 0x00AAFF);
    display.show();
}

// --- Main ---
void setup() {
    Serial.begin(115200); delay(500);
    Serial.println("\n[thinclock]");
    pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUTTON_LEFT, INPUT_PULLUP);
    pinMode(BUTTON_MID, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT, INPUT_PULLUP);

    display.begin();
    display.clear();
    display.drawText("BOOT", 1, 0, 0x004400);
    display.show();

    sensors.begin(); sensors.read();
    setupWiFi();
    config.valid = false;
    config.transition_ms = 8;
    config.time_format = "24h";
}

void loop() {
    handleSerial();
    httpServer.handleClient();
    renderClient.loop();
    renderClient.tick(display, configMgr, config);
    uint32_t now = millis();

    if (now - lastButtonCheck > BUTTON_CHECK_MS) { checkButtons(); checkLDR(); lastButtonCheck = now; now = millis(); }
    if (now - lastSensorRead > SENSOR_READ_MS) { sensors.read(); lastSensorRead = now; }

    // Config — with exponential backoff on failure
    static uint32_t configBackoff = CONFIG_POLL_MS; // cppcheck-suppress variableScope
    static bool lastFetchFailed = false;             // cppcheck-suppress variableScope
    if (!configURL.isEmpty() && WiFi.status() == WL_CONNECTED) {
        if (!config.valid || now - lastConfigFetch > configBackoff) {
            Config newCfg;
            if (configMgr.fetchConfig(configURL, newCfg)) {
                configBackoff = CONFIG_POLL_MS; // reset on success
                lastFetchFailed = false;
                display.setBrightness(newCfg.brightness);
                // Apply timezone
                char tz[16];
                int offset = -newCfg.timezone_offset; // POSIX is inverted
                snprintf(tz, sizeof(tz), "UTC%+d", offset);
                configTzTime(tz, "pool.ntp.org");
                bool wasInvalid = !config.valid;
                config = newCfg;
                // Clamp screen index if config shrank
                if (currentScreen >= (int)config.screens.size()) {
                    currentScreen = 0;
                    lastScreenSwitch = now;
                    resetState(currentState);
                } else if (wasInvalid) {
                    currentScreen = 0;
                    lastScreenSwitch = now;
                    resetState(currentState);
                }
            }
            lastConfigFetch = now;
            if (!config.valid) {
                // Back off: 30s → 60s → 120s → ... → 5min max
                configBackoff = min((uint32_t)300000, configBackoff * 2);
                lastFetchFailed = true;
            }
        }
    }

    if (!config.valid || config.screens.empty()) {
        showClock();
        // Middle button: scroll IP and config URL so user can find/fix the device
        if (digitalRead(BUTTON_MID) == LOW) {
            delay(50);
            if (digitalRead(BUTTON_MID) == LOW) {
                if (WiFi.status() == WL_CONNECTED) {
                    scrollText("IP " + WiFi.localIP().toString(), 0x00FF44);
                    if (!configURL.isEmpty()) scrollText(configURL, 0xFF8800);
                } else {
                    scrollText("NO WIFI", 0xFF0000);
                    scrollText(wifiSSID, 0xFF4400);
                }
            }
        }
        delay(500);
        return;
    }

    // Fetch data
    Screen& scr = config.screens[currentScreen];
    String dataUrl = scr.data_url;
    if (dataUrl.isEmpty()) {
        // Check if any text layer has its own data_url
        for (auto& l : scr.layers) {
            if (l.type == LAYER_TEXT && !l.data_url.isEmpty()) { dataUrl = l.data_url; break; }
        }
    }
    if (!dataUrl.isEmpty() && (now - lastDataFetch > DATA_POLL_MS)) {
        configMgr.fetchData(dataUrl, screenData);
        lastDataFetch = now;
    }

    // Render
    if (transitioning && prevScreenIdx >= 0) {
        if (!dataUrl.isEmpty() && screenData.isNull()) {
            display.renderToMain();
            renderScreen(config.screens[prevScreenIdx], prevState, prevScreenData);
        } else {
            display.renderToPrev();
            renderScreen(config.screens[prevScreenIdx], prevState, prevScreenData);
            display.renderToMain();
            renderScreen(scr, currentState, screenData);
            transitionProgress += config.transition_ms;
            if (transitionProgress >= 255) {
                transitioning = false; prevScreenIdx = -1;
            } else {
                display.crossfade((uint8_t)transitionProgress);
            }
        }
    } else {
        renderScreen(scr, currentState, screenData);
    }

    // --- Notification overlay (into render buffer before final show) ---
    if (notifViewerOpen) {
        if (now - notifOpenTime > NOTIF_TIMEOUT_MS) {
            notifViewerOpen = false;
            notifSlideY = -8;
        } else {
            display.fadeAll(25);  // dim background to ~10% for notification readability
            if (notifSlideY < 0) notifSlideY += 1;

            if (notifViewerIdx == -1 && timer.active) {
                // Show timer countdown
                uint32_t borderColor = timer.color;
                for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) {
                    display.drawPixel(bx, max((int16_t)0, notifSlideY), borderColor);
                }
                if (notifSlideY >= 0) {
                    int32_t remaining;
                    if (timerPaused) {
                        remaining = timerPausedRemaining;
                    } else {
                        remaining = (int32_t)(timer.endTime - millis());
                        if (remaining < 0) remaining = 0;
                    }
                    int mins = remaining / 60000;
                    int secs = (remaining / 1000) % 60;
                    char buf[6];
                    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                    int16_t textY = notifSlideY + 2;
                    display.drawNativeText(buf, 8, textY, timer.color, 1, false);
                }
            } else if (notifViewerIdx >= 0 && notifViewerIdx < notifCount) {
                // Show notification
                uint32_t borderColor = notifications[notifViewerIdx].color;
                for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) {
                    display.drawPixel(bx, max((int16_t)0, notifSlideY), borderColor);
                }
                if (notifSlideY >= 0) {
                    Notification& n = notifications[notifViewerIdx];
                    int16_t textY = notifSlideY + 2;
                    int16_t iconW = 0;

                    // Draw icon if present
                    if (!n.icon_name.isEmpty() && config.icons.count(n.icon_name)) {
                        Icon& icon = config.icons[n.icon_name];
                        if (!icon.frames.empty()) {
                            // Scale to fit: use 6px tall area (below border)
                            display.drawSprite(icon.frames[0].data(), icon.width, min((uint8_t)6, icon.height), 0, textY - 1);
                            iconW = icon.width + 1;
                        }
                    }

                    // Draw text (offset by icon width, clipped to not overlap icon)
                    for (const auto& l : n.layers) {
                        if (l.type == LAYER_TEXT) {
                            int16_t textW = display.nativeTextWidth(l.label);
                            int16_t availW = MATRIX_WIDTH - iconW;
                            if (textW <= availW) {
                                display.drawNativeText(l.label, iconW + (availW - textW) / 2, textY, l.color, 1, false);
                            } else {
                                int16_t drawX = MATRIX_WIDTH - notifScrollX;
                                display.drawNativeText(l.label, drawX, textY, l.color, 1, false);
                                // Clear icon zone so text doesn't bleed over it
                                if (iconW > 0) {
                                    display.clearRect(0, textY - 1, iconW, 7);
                                    // Redraw icon
                                    if (!n.icon_name.isEmpty() && config.icons.count(n.icon_name)) {
                                        Icon& ic = config.icons[n.icon_name];
                                        if (!ic.frames.empty()) {
                                            display.drawSprite(ic.frames[0].data(), ic.width, min((uint8_t)6, ic.height), 0, textY - 1);
                                        }
                                    }
                                }
                                if (now - notifLastScroll >= NOTIF_SCROLL_SPEED) {
                                    notifScrollX++;
                                    notifLastScroll = now;
                                    if (drawX + textW < iconW) notifScrollX = 0;
                                }
                            }
                        }
                    }
                }
            } else {
                // No more items, close
                notifViewerOpen = false;
                notifSlideY = -8;
            }
        }
    } else if ((notifCount > 0 || timer.active)) { // notifViewerOpen is false here by structure
        // Timer indicator dot
        if (timer.active && !timerPaused) {
            // Breathing dot — speed increases as time runs out
            int32_t remaining = (int32_t)(timer.endTime - millis());
            if (remaining < 0) remaining = 0;
            float progress = 1.0f - (float)remaining / timer.duration;
            uint16_t cycleMs = 6000 - (uint16_t)(progress * progress * 5200);
            if (cycleMs < 800) cycleMs = 800;
            float breath = (sin(millis() * 6.2832f / cycleMs) + 1.0f) * 0.5f;
            breath = 0.25f + breath * 0.75f;
            uint8_t r = ((timer.color >> 16) & 0xFF) * breath;
            uint8_t g = ((timer.color >> 8) & 0xFF) * breath;
            uint8_t b = (timer.color & 0xFF) * breath;
            display.drawPixel(MATRIX_WIDTH - 1, 0, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
        } else if (timer.active && timerPaused) {
            // Paused: static dim dot (no breathing)
            uint8_t r = ((timer.color >> 16) & 0xFF) >> 2;
            uint8_t g = ((timer.color >> 8) & 0xFF) >> 2;
            uint8_t b = (timer.color & 0xFF) >> 2;
            display.drawPixel(MATRIX_WIDTH - 1, 0, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
        }
        // Notification dots (offset if timer dot is showing)
        int dotOffset = timer.active ? 2 : 0;
        for (int i = 0; i < notifCount && i < 3; i++) {
            display.drawPixel(MATRIX_WIDTH - 1 - dotOffset - (i * 2), 0, notifications[i].color);
        }
    }

    // Single show per frame
    display.show();

    // Timer completion check
    if (timer.active && !timer.fired && !timerPaused && millis() >= timer.endTime) {
        timer.fired = true;
        beepTriple();
        // Keep timer active so user sees 00:00 and can dismiss
    }

    // Alert beep check (repeating notifications)
    if (!notifViewerOpen) {
        // Timer done — repeat beep every 15s until viewed
        if (timer.active && timer.fired) {
            static uint32_t lastTimerBeep = 0;
            if (now - lastTimerBeep >= 15000) {
                beepTriple();
                lastTimerBeep = now;
            }
        }
        for (int i = 0; i < notifCount; i++) {
            if (notifications[i].beep == 2 && notifications[i].active) {
                if (now - notifications[i].lastBeep >= notifications[i].alertInterval) {
                    beepTriple();
                    notifications[i].lastBeep = now;
                }
            }
        }
    }

    // Screen cycling (only when not transitioning)
    if (!transitioning && now - lastScreenSwitch > scr.duration) {
        if (!screenHasScrolling(currentState)) switchScreen();
    }

    delay(20);
}
