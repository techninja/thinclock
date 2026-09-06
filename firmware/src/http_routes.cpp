#include <WiFi.h>
#include "http_routes.h"
#include "setup_page.h"
#include "screens.h"
#include "sensors.h"
#include "gif_encoder.h"
#include "thinclock.h"
#include "buttons.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

extern Config       config;
extern ConfigManager configMgr;
extern Display      display;
extern Sensors      sensors;
extern Preferences  prefs;
extern String       wifiSSID, configURL;
extern bool         wifiBadPassword;
extern std::vector<ScannedNet> scannedNets;
extern int          currentScreen;
extern uint32_t     lastConfigFetch;
extern String       lastButtonEvent;
extern Notification notifications[];
extern int          notifCount;
extern bool         notifViewerOpen;
extern Timer        timer;
extern bool         timerPaused;
extern uint32_t     timerPausedRemaining;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static void unzigzag(const uint8_t* fb, uint8_t* out) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
        for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t physX = (y % 2 == 0) ? x : (MATRIX_WIDTH - 1 - x);
            uint16_t src = (y * MATRIX_WIDTH + physX) * 3;
            uint16_t dst = (y * MATRIX_WIDTH + x) * 3;
            out[dst] = fb[src]; out[dst+1] = fb[src+1]; out[dst+2] = fb[src+2];
        }
    }
}

// -----------------------------------------------------------------------
// Route handlers
// -----------------------------------------------------------------------

static void handleSensors() {
    JsonDocument doc;
    float temp = sensors.data.temperature;
    if (config.temp_unit == "F") temp = temp * 9.0f / 5.0f + 32.0f;
    doc["temperature"] = round(temp * 10.0) / 10.0;
    doc["humidity"]    = round(sensors.data.humidity * 10.0) / 10.0;
    doc["light"]       = (int)sensors.data.lightPct;
    doc["light_raw"]   = (int)sensors.data.light;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

static void handleStatus() {
    JsonDocument doc;
    doc["uptime"]      = millis() / 1000;
    doc["wifi"]        = WiFi.RSSI();
    doc["ip"]          = WiFi.localIP().toString();
    doc["screen"]      = currentScreen;
    if (currentScreen >= 0 && currentScreen < (int)config.screens.size()) {
        doc["screen_id"]   = config.screens[currentScreen].id;
        doc["screen_name"] = config.screens[currentScreen].name;
    }
    doc["last_button"] = lastButtonEvent;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

static void handleInfo() {
    JsonDocument doc;
    doc["firmware"]   = "thinclock";
    doc["version"]    = "0.9.0";
    doc["build"]      = __DATE__ " " __TIME__;
    doc["chip"]       = ESP.getChipModel();
    doc["flash"]      = ESP.getFlashChipSize();
    doc["free_heap"]  = ESP.getFreeHeap();
    doc["uptime"]     = millis() / 1000;
    doc["wifi_ssid"]  = WiFi.SSID();
    doc["ip"]         = WiFi.localIP().toString();
    doc["rssi"]       = WiFi.RSSI();
    doc["config_url"] = configURL;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

static void handleNotify() {
    if (httpServer.method() == HTTP_POST) {
        if (notifCount >= MAX_NOTIFICATIONS) { httpServer.send(429, "application/json", "{\"error\":\"full\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
        Notification& n = notifications[notifCount];
        n.active    = true;
        n.color     = strtoul((doc["color"] | "FFAA00"), NULL, 16);
        n.icon_name = doc["icon"] | "";
        n.layers.clear();
        if (doc["text"].is<const char*>()) {
            Layer l; l.type = LAYER_TEXT; l.label = doc["text"].as<const char*>();
            l.x = 0; l.y = 0; l.color = strtoul((doc["text_color"] | "FFFFFF"), NULL, 16);
            l.scroll = SCROLL_AUTO; l.scroll_speed = 50; l.fade_edge = 2; l.opacity = 255;
            n.layers.push_back(l);
        }
        const char* beepStr = doc["beep"] | "single";
        if      (strcmp(beepStr, "none") == 0 || strcmp(beepStr, "false") == 0) n.beep = 0;
        else if (strcmp(beepStr, "alert") == 0) n.beep = 2;
        else n.beep = 1;
        n.alertInterval = doc["alert_interval"] | 30000;
        n.lastBeep = 0;
        notifCount++;
        if (n.beep == 1) beepOnce(); else if (n.beep == 2) beepTriple();
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else if (httpServer.method() == HTTP_DELETE) {
        notifCount = 0; notifViewerOpen = false;
        for (int i = 0; i < MAX_NOTIFICATIONS; i++) notifications[i].active = false;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else {
        httpServer.send(200, "application/json", "{\"count\":" + String(notifCount) + "}");
    }
}

static void handleTimer() {
    if (httpServer.method() == HTTP_POST) {
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
        timer.duration = doc["duration"] | 60000;
        timer.endTime  = millis() + timer.duration;
        timer.color    = strtoul((doc["color"] | "00AAFF"), NULL, 16);
        timer.active   = true; timer.fired = false;
        beepOnce(1500, 50);
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else if (httpServer.method() == HTTP_DELETE) {
        timer.active = false; timer.fired = false;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else {
        JsonDocument doc;
        doc["active"] = timer.active;
        if (timer.active) {
            int32_t rem = (int32_t)(timer.endTime - millis());
            doc["remaining"] = rem > 0 ? rem : 0;
            doc["duration"]  = timer.duration;
        }
        String out; serializeJson(doc, out);
        httpServer.send(200, "application/json", out);
    }
}

static void handleBeep() {
    JsonDocument doc; deserializeJson(doc, httpServer.arg("plain"));
    const char* type = doc["type"] | "";
    if      (strcmp(type, "single") == 0) beepOnce();
    else if (strcmp(type, "double") == 0) { beepOnce(1500, 60); delay(80); beepOnce(1500, 60); }
    else if (strcmp(type, "triple") == 0) beepTriple();
    else if (strcmp(type, "alarm")  == 0) { for (int i = 0; i < 5; i++) { beepOnce(2500, 40); delay(60); } }
    else if (doc["pattern"].is<JsonArray>()) {
        for (JsonArray note : doc["pattern"].as<JsonArray>()) {
            beepOnce(note[0] | 2000, note[1] | 80);
            if ((uint16_t)(note[2] | 0) > 0) delay(note[2].as<uint16_t>());
        }
    }
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleFramebuffer() {
    static uint8_t linear[NUM_LEDS * 3];
    unzigzag(display.getFramebuffer(), linear);
    WiFiClient client = httpServer.client();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n");
    client.printf("Content-Length: %d\r\n", NUM_LEDS * 3);
    client.print("Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");
    client.write(linear, NUM_LEDS * 3);
}

static void handlePreview() {
    if (!config.valid || config.screens.empty()) { httpServer.send(400, "application/json", "{\"error\":\"no config\"}"); return; }
    int screenIdx = httpServer.arg("screen").toInt();
    int frames    = httpServer.arg("frames").toInt();
    if (screenIdx < 0 || screenIdx >= (int)config.screens.size()) { httpServer.send(400, "application/json", "{\"error\":\"invalid screen\"}"); return; }
    frames = constrain(frames, 1, 120);

    WiFiClient client = httpServer.client();
    client.printf("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %d\r\n"
                  "X-Frames: %d\r\nX-Frame-Ms: 20\r\nCache-Control: public, max-age=60\r\n"
                  "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n", NUM_LEDS * 3 * frames, frames);

    static CRGB savedBuf[NUM_LEDS];
    memcpy(savedBuf, display.getFramebuffer(), sizeof(savedBuf));

    Screen& scr = config.screens[screenIdx];
    ScreenState st; resetState(st); initScreenState(st, scr);
    JsonDocument data;
    if (!scr.data_url.isEmpty()) configMgr.fetchData(scr.data_url, data);

    static uint8_t linear[NUM_LEDS * 3];
    for (int f = 0; f < frames; f++) {
        display.clear(); renderScreen(scr, st, data);
        unzigzag(display.getFramebuffer(), linear);
        client.write(linear, NUM_LEDS * 3); yield();
    }
    memcpy(const_cast<uint8_t*>(display.getFramebuffer()), savedBuf, sizeof(savedBuf));
}

static void handleRender() {
    JsonDocument doc;
    if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
    int  frames       = constrain((int)(doc["frames"] | 1), 1, 120);
    bool showOnDevice = doc["display"] | false;

    Screen tmp; tmp.duration = 0; tmp.data_url = doc["data_url"] | "";
    for (JsonObject l : doc["layers"].as<JsonArray>()) tmp.layers.push_back(configMgr.parseLayer(l, config.scroll_speed));
    if (doc["icons"].is<JsonObject>()) configMgr.parseIcons(doc["icons"].as<JsonObject>(), config.icons);

    static CRGB savedBuf[NUM_LEDS];
    memcpy(savedBuf, display.getFramebuffer(), sizeof(savedBuf));

    ScreenState st; resetState(st); initScreenState(st, tmp);
    JsonDocument data;
    if (doc["data"].is<JsonObject>()) data = doc["data"];

    WiFiClient client = httpServer.client();
    client.printf("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %d\r\n"
                  "X-Frames: %d\r\nX-Frame-Ms: 20\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
                  NUM_LEDS * 3 * frames, frames);

    static uint8_t linear[NUM_LEDS * 3];
    for (int f = 0; f < frames; f++) {
        display.clear(); renderScreen(tmp, st, data);
        unzigzag(display.getFramebuffer(), linear);
        client.write(linear, NUM_LEDS * 3);
        if (showOnDevice) display.show();
        yield();
    }
    if (!showOnDevice) memcpy(const_cast<uint8_t*>(display.getFramebuffer()), savedBuf, sizeof(savedBuf));
}

static void renderGif(Screen& scr, ScreenState& st, JsonDocument& data, int frames,
                      uint8_t scale, uint8_t gap, uint8_t gamma) {
    static CRGB savedBuf[NUM_LEDS];
    memcpy(savedBuf, display.getFramebuffer(), sizeof(savedBuf));

    WiFiClient client = httpServer.client();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\n"
                 "Cache-Control: public, max-age=60\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");

    GifEncoder gif; gif.begin(client, 66, scale, gap, gamma);
    static uint8_t linear[NUM_LEDS * 3];
    for (int f = 0; f < frames; f++) {
        display.clear(); renderScreen(scr, st, data);
        memset(linear, 0, sizeof(linear));
        unzigzag(display.getFramebuffer(), linear);
        gif.addFrame(linear); delay(66); yield();
    }
    gif.end();
    memcpy(const_cast<uint8_t*>(display.getFramebuffer()), savedBuf, sizeof(savedBuf));
}

static void handleGifGet() {
    if (!config.valid || config.screens.empty()) { httpServer.send(400, "application/json", "{\"error\":\"no config\"}"); return; }
    int screenIdx = httpServer.arg("screen").toInt();
    if (screenIdx < 0 || screenIdx >= (int)config.screens.size()) { httpServer.send(400, "application/json", "{\"error\":\"invalid screen\"}"); return; }
    int     seconds = constrain(httpServer.arg("seconds").toInt(), 1, 10); if (seconds < 1) seconds = 2;
    uint8_t scale   = max((uint8_t)1, (uint8_t)httpServer.arg("scale").toInt());
    uint8_t gap     = httpServer.arg("gap").toInt();
    uint8_t gamma   = httpServer.arg("gamma").toInt(); if (gamma < 10) gamma = 18;

    Screen& scr = config.screens[screenIdx];
    ScreenState st; resetState(st); initScreenState(st, scr);
    JsonDocument data;
    if (!scr.data_url.isEmpty()) configMgr.fetchData(scr.data_url, data);
    renderGif(scr, st, data, seconds * 15, scale, gap, gamma);
}

static void handleGifPost() {
    JsonDocument doc;
    if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
    int     seconds = constrain((int)(doc["seconds"] | 2), 1, 10);
    uint8_t scale   = max((uint8_t)1, (uint8_t)(doc["scale"] | 1));
    uint8_t gap     = doc["gap"] | 0;
    uint8_t gamma   = doc["gamma"] | 18; if (gamma < 10) gamma = 18;

    Screen tmp; tmp.duration = 0; tmp.data_url = doc["data_url"] | "";
    for (JsonObject l : doc["layers"].as<JsonArray>()) tmp.layers.push_back(configMgr.parseLayer(l, config.scroll_speed));
    if (doc["icons"].is<JsonObject>()) configMgr.parseIcons(doc["icons"].as<JsonObject>(), config.icons);

    ScreenState st; resetState(st); initScreenState(st, tmp);
    JsonDocument data;
    if      (doc["data"].is<JsonObject>())    data = doc["data"];
    else if (!tmp.data_url.isEmpty())         configMgr.fetchData(tmp.data_url, data);
    renderGif(tmp, st, data, seconds * 15, scale, gap, gamma);
}

// -----------------------------------------------------------------------
// registerHttpRoutes — called from wifi.cpp before httpServer.begin()
// -----------------------------------------------------------------------

void registerHttpRoutes() {
    // Setup page (both AP and STA modes)
    httpServer.on("/", HTTP_GET, []() {
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });
    httpServer.on("/setup", HTTP_POST, []() { handleSetupPost(httpServer, prefs); });

    // Captive portal detection endpoints
    // Android probes /generate_204 and expects exactly 204 — a redirect breaks it.
    // iOS/macOS probe /hotspot-detect.html and expect a non-204 body response.
    // onNotFound catches everything else (DNS wildcard sends all hosts here).
    httpServer.on("/generate_204", HTTP_GET, []() {
        Serial.printf("[portal] /generate_204 at %lums\n", millis());
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });
    httpServer.on("/gen_204", HTTP_GET, []() {
        Serial.printf("[portal] /gen_204 at %lums\n", millis());
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });
    httpServer.on("/hotspot-detect.html",  HTTP_GET, []() {
        Serial.printf("[portal] /hotspot-detect.html at %lums\n", millis());
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });
    httpServer.on("/connecttest.txt", HTTP_GET, []() {
        Serial.printf("[portal] /connecttest.txt at %lums\n", millis());
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });
    httpServer.on("/ncsi.txt", HTTP_GET, []() {
        Serial.printf("[portal] /ncsi.txt at %lums\n", millis());
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });
    httpServer.onNotFound([]() {
        Serial.printf("[portal] 404->portal host=%s uri=%s at %lums\n",
            httpServer.hostHeader().c_str(), httpServer.uri().c_str(), millis());
        httpServer.send(200, "text/html; charset=utf-8",
            setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets));
    });

    // Config URL push from HA
    httpServer.on("/config_url", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain")) || !doc["config_url"].is<const char*>()) {
            httpServer.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
        }
        configURL = doc["config_url"].as<const char*>();
        prefs.begin("thinclock", false); prefs.putString("config_url", configURL); prefs.end();
        Serial.printf("[config] URL set: %s\n", configURL.c_str());
        lastConfigFetch = 0;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });

    // Device endpoints
    httpServer.on("/info",        HTTP_GET,  handleInfo);
    httpServer.on("/sensors",     HTTP_GET,  handleSensors);
    httpServer.on("/status",      HTTP_GET,  handleStatus);
    httpServer.on("/notify",                 handleNotify);
    httpServer.on("/timer",                  handleTimer);
    httpServer.on("/beep",        HTTP_POST, handleBeep);
    httpServer.on("/button",      HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain")) || !doc["button"].is<const char*>()) {
            httpServer.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
        }
        simulateButton(doc["button"].as<String>());
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });
    httpServer.on("/screen",      HTTP_POST, []() {
        JsonDocument doc;
        deserializeJson(doc, httpServer.arg("plain"));
        int idx = doc["index"] | -1;
        if (idx >= 0 && idx < (int)config.screens.size()) {
            currentScreen = idx;
            lastScreenSwitch = millis();
            resetState(currentState);
        }
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });
    httpServer.on("/display",     HTTP_POST, []() {
        JsonDocument doc;
        deserializeJson(doc, httpServer.arg("plain"));
        if (doc["brightness"].is<int>()) {
            config.brightness = constrain((int)doc["brightness"], 0, 100);
            display.setBrightness(config.brightness);
        }
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });
    httpServer.on("/framebuffer", HTTP_GET,  handleFramebuffer);
    httpServer.on("/preview",     HTTP_GET,  handlePreview);
    httpServer.on("/render",      HTTP_POST, handleRender);
    httpServer.on("/gif",         HTTP_GET,  handleGifGet);
    httpServer.on("/gif",         HTTP_POST, handleGifPost);

    httpServer.enableCORS(true);
}
