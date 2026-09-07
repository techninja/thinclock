#include <WiFi.h>
#include "http_routes.h"
#include "screens.h"
#include "gif_encoder.h"
#include "thinclock.h"
#include <ArduinoJson.h>

extern Display      display;
extern ConfigManager configMgr;

// -----------------------------------------------------------------------
// Shared helper
// -----------------------------------------------------------------------

void unzigzag(const uint8_t* fb, uint8_t* out) {
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
// Binary output handlers
// -----------------------------------------------------------------------

void handleFramebuffer(AppState& /*state*/) {
    static uint8_t linear[NUM_LEDS * 3];
    unzigzag(display.getFramebuffer(), linear);
    WiFiClient client = httpServer.client();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n");
    client.printf("Content-Length: %d\r\n", NUM_LEDS * 3);
    client.print("Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");
    client.write(linear, NUM_LEDS * 3);
}

void handlePreview(AppState& state) {
    if (!state.config.valid || state.config.screens.empty()) { httpServer.send(400, "application/json", "{\"error\":\"no config\"}"); return; }
    int screenIdx = httpServer.arg("screen").toInt();
    int frames    = httpServer.arg("frames").toInt();
    if (screenIdx < 0 || screenIdx >= (int)state.config.screens.size()) { httpServer.send(400, "application/json", "{\"error\":\"invalid screen\"}"); return; }
    frames = constrain(frames, 1, 120);

    WiFiClient client = httpServer.client();
    client.printf("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %d\r\n"
                  "X-Frames: %d\r\nX-Frame-Ms: 20\r\nCache-Control: public, max-age=60\r\n"
                  "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n", NUM_LEDS * 3 * frames, frames);

    static CRGB savedBuf[NUM_LEDS];
    memcpy(savedBuf, display.getFramebuffer(), sizeof(savedBuf));

    Screen& scr = state.config.screens[screenIdx];
    ScreenState st; resetState(st); initScreenState(st, scr);
    JsonDocument data;
    if (!scr.data_url.isEmpty()) configMgr.fetchData(scr.data_url, data);

    static uint8_t linear[NUM_LEDS * 3];
    for (int f = 0; f < frames; f++) {
        renderScreen(state, scr, st, data);
        unzigzag(display.getFramebuffer(), linear);
        client.write(linear, NUM_LEDS * 3); yield();
    }
    memcpy(const_cast<uint8_t*>(display.getFramebuffer()), savedBuf, sizeof(savedBuf));
}

void handleRender(AppState& state) {
    JsonDocument doc;
    if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
    int  frames       = constrain((int)(doc["frames"] | 1), 1, 120);
    bool showOnDevice = doc["display"] | false;

    Screen tmp; tmp.duration = 0; tmp.data_url = doc["data_url"] | "";
    for (JsonObject l : doc["layers"].as<JsonArray>()) tmp.layers.push_back(configMgr.parseLayer(l, state.config.scroll_speed));
    if (doc["icons"].is<JsonObject>()) configMgr.parseIcons(doc["icons"].as<JsonObject>(), state.config.icons);

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
        renderScreen(state, tmp, st, data);
        unzigzag(display.getFramebuffer(), linear);
        client.write(linear, NUM_LEDS * 3);
        if (showOnDevice) display.show();
        yield();
    }
    if (!showOnDevice) memcpy(const_cast<uint8_t*>(display.getFramebuffer()), savedBuf, sizeof(savedBuf));
}

// -----------------------------------------------------------------------
// GIF handlers
// -----------------------------------------------------------------------

static void renderGif(AppState& state, Screen& scr, ScreenState& st, const JsonDocument& data,
                      int frames, uint8_t scale, uint8_t gap, uint8_t gamma) {
    static CRGB savedBuf[NUM_LEDS];
    memcpy(savedBuf, display.getFramebuffer(), sizeof(savedBuf));

    WiFiClient client = httpServer.client();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\n"
                 "Cache-Control: public, max-age=60\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");

    GifEncoder gif; gif.begin(client, 66, scale, gap, gamma);
    static uint8_t linear[NUM_LEDS * 3];
    for (int f = 0; f < frames; f++) {
        renderScreen(state, scr, st, data);
        memset(linear, 0, sizeof(linear));
        unzigzag(display.getFramebuffer(), linear);
        gif.addFrame(linear); delay(66); yield();
    }
    gif.end();
    memcpy(const_cast<uint8_t*>(display.getFramebuffer()), savedBuf, sizeof(savedBuf));
}

void handleGifGet(AppState& state) {
    if (!state.config.valid || state.config.screens.empty()) { httpServer.send(400, "application/json", "{\"error\":\"no config\"}"); return; }
    int screenIdx = httpServer.arg("screen").toInt();
    if (screenIdx < 0 || screenIdx >= (int)state.config.screens.size()) { httpServer.send(400, "application/json", "{\"error\":\"invalid screen\"}"); return; }
    int     seconds = constrain(httpServer.arg("seconds").toInt(), 1, 10); if (seconds < 1) seconds = 2;
    uint8_t scale   = max((uint8_t)1, (uint8_t)httpServer.arg("scale").toInt());
    uint8_t gap     = httpServer.arg("gap").toInt();
    uint8_t gamma   = httpServer.arg("gamma").toInt(); if (gamma < 10) gamma = 18;

    Screen& scr = state.config.screens[screenIdx];
    ScreenState st; resetState(st); initScreenState(st, scr);
    JsonDocument data;
    if (!scr.data_url.isEmpty()) configMgr.fetchData(scr.data_url, data);
    renderGif(state, scr, st, data, seconds * 15, scale, gap, gamma);
}

void handleGifPost(AppState& state) {
    JsonDocument doc;
    if (deserializeJson(doc, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"parse\"}"); return; }
    int     seconds = constrain((int)(doc["seconds"] | 2), 1, 10);
    uint8_t scale   = max((uint8_t)1, (uint8_t)(doc["scale"] | 1));
    uint8_t gap     = doc["gap"] | 0;
    uint8_t gamma   = doc["gamma"] | 18; if (gamma < 10) gamma = 18;

    Screen tmp; tmp.duration = 0; tmp.data_url = doc["data_url"] | "";
    for (JsonObject l : doc["layers"].as<JsonArray>()) tmp.layers.push_back(configMgr.parseLayer(l, state.config.scroll_speed));
    if (doc["icons"].is<JsonObject>()) configMgr.parseIcons(doc["icons"].as<JsonObject>(), state.config.icons);

    ScreenState st; resetState(st); initScreenState(st, tmp);
    JsonDocument data;
    if      (doc["data"].is<JsonObject>())    data = doc["data"];
    else if (!tmp.data_url.isEmpty())         configMgr.fetchData(tmp.data_url, data);
    renderGif(state, tmp, st, data, seconds * 15, scale, gap, gamma);
}
