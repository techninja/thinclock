#include <WiFi.h>
#include "http_routes.h"
#include "setup_page.h"
#include "screens.h"
#include "sensors.h"
#include "thinclock.h"
#include "buttons.h"
#include <ArduinoJson.h>
#include <Preferences.h>

extern ConfigManager configMgr;
extern Display      display;
extern Sensors      sensors;
extern Preferences  prefs;
extern String       wifiSSID, configURL;
extern bool         wifiBadPassword;
extern std::vector<ScannedNet> scannedNets;

// -----------------------------------------------------------------------
// JSON API handlers
// -----------------------------------------------------------------------

static void handleSensors(const AppState& state) {
    JsonDocument doc;
    float temp = sensors.data.temperature;
    if (state.config.temp_unit == "F") temp = temp * 9.0f / 5.0f + 32.0f;
    doc["temperature"] = round(temp * 10.0) / 10.0;
    doc["humidity"]    = round(sensors.data.humidity * 10.0) / 10.0;
    doc["light"]       = (int)sensors.data.lightPct;
    doc["light_raw"]   = (int)sensors.data.light;
    String out; serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

static void handleStatus(const AppState& state) {
    JsonDocument doc;
    doc["uptime"]      = millis() / 1000;
    doc["wifi"]        = WiFi.RSSI();
    doc["ip"]          = WiFi.localIP().toString();
    doc["screen"]      = state.currentScreen;
    if (state.currentScreen >= 0 && state.currentScreen < (int)state.config.screens.size()) {
        doc["screen_id"]   = state.config.screens[state.currentScreen].id;
        doc["screen_name"] = state.config.screens[state.currentScreen].name;
    }
    doc["last_button"] = state.lastButtonEvent;
    doc["brightness"]   = state.config.brightness;
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

static void handleNotify(AppState& state) {
    if (httpServer.method() == HTTP_POST) {
        if (state.notifCount >= MAX_NOTIFICATIONS) { httpServer.send(429, "application/json", "{\"error\":\"full\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
        Notification& n = state.notifications[state.notifCount];
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
        state.notifCount++;
        if (n.beep == 1) beepOnce(); else if (n.beep == 2) beepTriple();
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else if (httpServer.method() == HTTP_DELETE) {
        state.notifCount = 0; state.notifViewerOpen = false;
        for (int i = 0; i < MAX_NOTIFICATIONS; i++) state.notifications[i].active = false;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else {
        httpServer.send(200, "application/json", "{\"count\":" + String(state.notifCount) + "}");
    }
}

static void handleTimer(AppState& state) {
    if (httpServer.method() == HTTP_POST) {
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
        state.timer.duration = doc["duration"] | 60000;
        state.timer.endTime  = millis() + state.timer.duration;
        state.timer.color    = strtoul((doc["color"] | "00AAFF"), NULL, 16);
        state.timer.active   = true; state.timer.fired = false;
        beepOnce(1500, 50);
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else if (httpServer.method() == HTTP_DELETE) {
        state.timer.active = false; state.timer.fired = false;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    } else {
        JsonDocument doc;
        doc["active"] = state.timer.active;
        if (state.timer.active) {
            int32_t rem = (int32_t)(state.timer.endTime - millis());
            doc["remaining"] = rem > 0 ? rem : 0;
            doc["duration"]  = state.timer.duration;
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

// -----------------------------------------------------------------------
// registerHttpRoutes
// -----------------------------------------------------------------------

void registerHttpRoutes(AppState& state) {
    auto portalPage = []() {
        return setupPageHTML(wifiSSID, configURL, WiFi.status() != WL_CONNECTED, wifiBadPassword, scannedNets);
    };

    httpServer.on("/",                    HTTP_GET,  [portalPage]() { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });
    httpServer.on("/setup",               HTTP_POST, []() { handleSetupPost(httpServer, prefs); });
    httpServer.on("/generate_204",        HTTP_GET,  [portalPage]() { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });
    httpServer.on("/gen_204",             HTTP_GET,  [portalPage]() { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });
    httpServer.on("/hotspot-detect.html", HTTP_GET,  [portalPage]() { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });
    httpServer.on("/connecttest.txt",     HTTP_GET,  [portalPage]() { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });
    httpServer.on("/ncsi.txt",            HTTP_GET,  [portalPage]() { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });
    httpServer.onNotFound([portalPage]()  { httpServer.send(200, "text/html; charset=utf-8", portalPage()); });

    httpServer.on("/config_url", HTTP_POST, [&state]() {
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain")) || !doc["config_url"].is<const char*>()) {
            httpServer.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
        }
        configURL = doc["config_url"].as<const char*>();
        prefs.begin("thinclock", false); prefs.putString("config_url", configURL); prefs.end();
        state.lastConfigFetch = 0;
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });

    httpServer.on("/info",    HTTP_GET, handleInfo);
    httpServer.on("/sensors", HTTP_GET, [&state]() { handleSensors(state); });
    httpServer.on("/status",  HTTP_GET, [&state]() { handleStatus(state); });
    httpServer.on("/notify",            [&state]() { handleNotify(state); });
    httpServer.on("/timer",             [&state]() { handleTimer(state); });
    httpServer.on("/beep",    HTTP_POST, handleBeep);

    httpServer.on("/button", HTTP_POST, [&state]() {
        JsonDocument doc;
        if (deserializeJson(doc, httpServer.arg("plain")) || !doc["button"].is<const char*>()) {
            httpServer.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
        }
        simulateButton(state, doc["button"].as<String>());
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });

    httpServer.on("/screen", HTTP_POST, [&state]() {
        JsonDocument doc;
        deserializeJson(doc, httpServer.arg("plain"));
        int idx = doc["index"] | -1;
        if (idx >= 0 && idx < (int)state.config.screens.size()) {
            state.currentScreen = idx; state.lastScreenSwitch = millis(); resetState(state.currentState);
        }
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });

    httpServer.on("/display", HTTP_POST, [&state]() {
        JsonDocument doc;
        deserializeJson(doc, httpServer.arg("plain"));
        if (doc["brightness"].is<int>()) {
            state.config.brightness = constrain((int)doc["brightness"], 0, 100);
            display.setBrightness(state.config.brightness);
        }
        httpServer.send(200, "application/json", "{\"ok\":true}");
    });

    httpServer.on("/framebuffer", HTTP_GET,  [&state]() { handleFramebuffer(state); });
    httpServer.on("/preview",     HTTP_GET,  [&state]() { handlePreview(state); });
    httpServer.on("/render",      HTTP_POST, [&state]() { handleRender(state); });
    httpServer.on("/gif",         HTTP_GET,  [&state]() { handleGifGet(state); });
    httpServer.on("/gif",         HTTP_POST, [&state]() { handleGifPost(state); });

    httpServer.enableCORS(true);
}
