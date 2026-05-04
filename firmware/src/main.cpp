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
#include "gauge.h"
#include "particles.h"

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

// --- Per-layer state ---
struct TextState {
    int16_t offset = 0;
    int8_t dir = 1;
    uint32_t pauseUntil = 0;
    uint32_t lastStep = 0;
    ScrollMode mode = SCROLL_NONE;
    int16_t textW = 0;
    bool completedOnce = false;
    String resolved;
};

struct IconState {
    uint8_t frame = 0;
    uint32_t lastStep = 0;
};

struct ScreenState {
    std::vector<TextState> textStates;
    std::vector<IconState> iconStates;
    std::vector<ParticleSystem> particleSystems;
    uint32_t lastTick = 0;
    uint32_t startTime = 0;
    bool inited = false;
};

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
#define NOTIF_SCROLL_SPEED 45

// Buttons
bool btnLeftLast = HIGH, btnMidLast = HIGH, btnRightLast = HIGH;
uint32_t lastNavTime = 0;
#define NAV_COOLDOWN_MS 500

#define SENSOR_READ_MS 2000
#define BUTTON_CHECK_MS 50

// Forward declarations
void resetState(ScreenState& state);
void switchScreen();

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

    if (l == LOW && btnLeftLast == HIGH) {
        if (notifViewerOpen) {
            // Previous notification
            if (notifViewerIdx > 0) {
                notifViewerIdx--;
                notifSlideY = -8;
                notifScrollX = 0;
                notifOpenTime = millis();
            }
        } else {
            if (config.buttons == "navigate") navigatePrev();
        }
        postEvent("left");
    }
    if (m == LOW && btnMidLast == HIGH) {
        if (notifViewerOpen) {
            // Close viewer
            notifViewerOpen = false;
            notifSlideY = -8;
        } else if (notifCount > 0) {
            // Open viewer
            notifViewerOpen = true;
            notifViewerIdx = 0;
            notifSlideY = -8;
            notifScrollX = 0;
            notifOpenTime = millis();
        }
        postEvent("select");
    }
    if (r == LOW && btnRightLast == HIGH) {
        if (notifViewerOpen) {
            // Next notification or close
            notifViewerIdx++;
            if (notifViewerIdx >= notifCount) {
                // Dismiss all and close
                notifViewerOpen = false;
                notifSlideY = -8;
                notifCount = 0;
                for (auto& n : notifications) n.active = false;
            } else {
                notifSlideY = -8;
                notifScrollX = 0;
                notifOpenTime = millis();
            }
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
        // Full layers array
        // (simplified: just support text for now)
        notifCount++;
        Serial.printf("[notif] added #%d\n", notifCount);
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
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) { delay(250); Serial.print("."); }
    Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FAIL");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        configTzTime("UTC0", "pool.ntp.org");
        httpServer.on("/sensors", handleSensors);
        httpServer.on("/status", handleStatus);
        httpServer.on("/notify", handleNotify);
        httpServer.begin();
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

void initScreenState(ScreenState& state, Screen& scr) {
    state.textStates.clear();
    state.iconStates.clear();
    state.particleSystems.clear();
    state.lastTick = millis();
    state.startTime = millis();
    state.inited = true;

    for (auto& layer : scr.layers) {
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
    for (auto& tw : layer.tweens) {
        float val = evaluateTween(tw, elapsed);
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

        // Snapshot before layer for opacity blending
        if (layer.opacity < 255) display.snapshotLayer();

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
            struct tm t;
            char buf[6] = "??:??";
            if (getLocalTime(&t)) {
                if (layer.clock_format == "12h") {
                    int h = t.tm_hour % 12; if (h == 0) h = 12;
                    snprintf(buf, sizeof(buf), "%02d:%02d", h, t.tm_min);
                } else {
                    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
                }
            }
            ts.resolved = buf;
            display.drawNativeText(ts.resolved, layer.x, layer.y, layer.color, layer.native_spacing, layer.native_large);
            break;
        }

        case LAYER_NATIVE: {
            String text = layer.label;
            if (!layer.data_url.isEmpty() && !data.isNull()) {
                text = configMgr.resolvePlaceholders(layer.label, data);
            } else if (!scr.data_url.isEmpty() && !data.isNull()) {
                text = configMgr.resolvePlaceholders(layer.label, data);
            }
            display.drawNativeText(text, layer.x, layer.y, layer.color, layer.native_spacing, layer.native_large);
            break;
        }

        case LAYER_GAUGE: {
            float val = 0;
            if (!layer.value_key.isEmpty()) {
                val = data[layer.value_key.c_str()].as<float>();
            }
            Icon gaugeIcon;
            gaugeIcon.width = layer.gauge_w;
            gaugeIcon.height = layer.gauge_h;
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

        // Apply opacity blending after layer rendered
        if (layer.opacity < 255) display.applyLayerOpacity(layer.opacity);
    } // for layers
}

// --- Screen management ---

void resetState(ScreenState& state) {
    state.inited = false;
    state.textStates.clear();
    state.iconStates.clear();
    state.particleSystems.clear();
}

bool screenHasScrolling(ScreenState& state) {
    for (auto& ts : state.textStates) {
        if ((ts.mode == SCROLL_LEFT || ts.mode == SCROLL_BOUNCE) && !ts.completedOnce)
            return true;
    }
    return false;
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
    else { uint32_t s = millis()/1000; snprintf(buf, sizeof(buf), "%02lu:%02lu", (s/60)%100, s%60); }
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
    uint32_t now = millis();

    if (now - lastButtonCheck > BUTTON_CHECK_MS) { checkButtons(); lastButtonCheck = now; now = millis(); }
    if (now - lastSensorRead > SENSOR_READ_MS) { sensors.read(); lastSensorRead = now; }

    // Config
    if (!configURL.isEmpty() && WiFi.status() == WL_CONNECTED) {
        if (!config.valid || now - lastConfigFetch > CONFIG_POLL_MS) {
            Config newCfg;
            if (configMgr.fetchConfig(configURL, newCfg)) {
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
        }
    }

    if (!config.valid || config.screens.empty()) { showClock(); delay(500); return; }

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
    if (notifViewerOpen && notifViewerIdx < notifCount) {
        if (now - notifOpenTime > NOTIF_TIMEOUT_MS) {
            notifViewerOpen = false;
            notifSlideY = -8;
        } else {
            display.fadeAll(50);
            if (notifSlideY < 0) notifSlideY += 1;
            uint32_t borderColor = notifications[notifViewerIdx].color;
            for (int16_t bx = 0; bx < MATRIX_WIDTH; bx++) {
                display.drawPixel(bx, max((int16_t)0, notifSlideY), borderColor);
            }
            if (notifSlideY >= 0) {
                Notification& n = notifications[notifViewerIdx];
                for (auto& l : n.layers) {
                    if (l.type == LAYER_TEXT) {
                        int16_t textW = display.nativeTextWidth(l.label);
                        int16_t textY = notifSlideY + 2;
                        if (textW <= MATRIX_WIDTH) {
                            display.drawNativeText(l.label, (MATRIX_WIDTH - textW) / 2, textY, l.color, 1, false);
                        } else {
                            int16_t drawX = MATRIX_WIDTH - notifScrollX;
                            display.drawNativeText(l.label, drawX, textY, l.color, 1, false);
                            if (now - notifLastScroll >= NOTIF_SCROLL_SPEED) {
                                notifScrollX++;
                                notifLastScroll = now;
                                if (drawX + textW < 0) notifScrollX = 0;
                            }
                        }
                    }
                }
            }
        }
    } else if (notifCount > 0 && !notifViewerOpen) {
        for (int i = 0; i < notifCount && i < 3; i++) {
            display.drawPixel(MATRIX_WIDTH - 1 - (i * 2), 0, notifications[i].color);
        }
    }

    // Single show per frame
    display.show();

    // Screen cycling (only when not transitioning)
    if (!transitioning && now - lastScreenSwitch > scr.duration) {
        if (!screenHasScrolling(currentState)) switchScreen();
    }

    delay(20);
}
