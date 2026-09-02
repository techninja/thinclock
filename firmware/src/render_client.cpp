#include "render_client.h"

// Forward declarations from main.cpp
extern void resetState(ScreenState& state);
extern void initScreenState(ScreenState& state, Screen& scr);
extern void renderScreen(Screen& scr, ScreenState& state, const JsonDocument& data);

static RenderClient* _instance = nullptr;

static void wsEventHandler(WStype_t type, uint8_t* payload, size_t length) {
    if (_instance) _instance->onEvent(type, payload, length);
}

void RenderClient::begin(const String& serverHost, uint16_t serverPort) {
    _instance = this;
    _ws.begin(serverHost.c_str(), serverPort, "/ws/device");
    _ws.onEvent(wsEventHandler);
    _ws.setReconnectInterval(5000);
}

void RenderClient::loop() {
    _ws.loop();
}

void RenderClient::onEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            _connected = true;
            Serial.println("[ws] Connected to server");
            _ws.sendTXT("{\"type\":\"hello\",\"device\":\"thinclock\"}");
            break;
        case WStype_DISCONNECTED:
            _connected = false;
            _rendering = false;
            Serial.println("[ws] Disconnected");
            break;
        case WStype_TEXT:
            // Render command received — store for processing in tick()
            if (!_rendering) {
                _rendering = true;
                // Parse will happen in tick() with access to configMgr
                // Store raw payload temporarily
                _data.clear();
                deserializeJson(_data, payload, length);
                _framesDone = 0;
                _framesTotal = 0; // signal to startJob
            } else {
                _ws.sendTXT("{\"type\":\"busy\"}");
            }
            break;
        default:
            break;
    }
}

void RenderClient::startJob(uint8_t* payload, size_t length, ConfigManager& configMgr, Config& config) {
    _framesTotal = _data["frames"] | 30;
    _frameDelayMs = _data["frame_ms"] | 66;
    if (_framesTotal > 120) _framesTotal = 120;

    _screen.layers.clear();
    _screen.duration = 0;
    _screen.data_url = _data["data_url"] | "";

    for (JsonObject l : _data["layers"].as<JsonArray>()) {
        _screen.layers.push_back(configMgr.parseLayer(l, config.scroll_speed));
    }
    if (_data["icons"].is<JsonObject>()) {
        configMgr.parseIcons(_data["icons"].as<JsonObject>(), config.icons);
    }

    resetState(_state);
    initScreenState(_state, _screen);

    // Fetch data if needed
    if (!_screen.data_url.isEmpty() && !_data["data"].is<JsonObject>()) {
        JsonDocument fetchedData;
        configMgr.fetchData(_screen.data_url, fetchedData);
        _data["data"] = fetchedData;
    }

    _framesDone = 0;
    _lastFrameTime = millis();

    // Send job started
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"type\":\"started\",\"frames\":%d}", _framesTotal);
    _ws.sendTXT(msg);
}

void RenderClient::tick(Display& display, ConfigManager& configMgr, Config& config) {
    if (!_connected) return;

    if (_rendering) {
        // Initialize job on first tick
        if (_framesTotal == 0) {
            startJob(nullptr, 0, configMgr, config);
            if (_screen.layers.empty()) {
                _rendering = false;
                _ws.sendTXT("{\"type\":\"error\",\"msg\":\"no layers\"}");
                return;
            }
        }

        // Render one frame per tick, respecting frame delay
        uint32_t now = millis();
        if (now - _lastFrameTime >= _frameDelayMs) {
            _lastFrameTime = now;

            // Save buffer, render, restore
            static CRGB savedBuf[NUM_LEDS];
            const uint8_t* fb = display.getFramebuffer();
            memcpy(savedBuf, fb, sizeof(savedBuf));

            display.clear();
            renderScreen(_screen, _state, _data["data"]);

            // Unzigzag and send frame
            sendFrame(display);

            // Restore live display
            memcpy(const_cast<uint8_t*>(fb), savedBuf, sizeof(savedBuf));

            _framesDone++;
            if (_framesDone >= _framesTotal) {
                _rendering = false;
                _ws.sendTXT("{\"type\":\"done\"}");
            }
        }
    } else {
        // Stream live framebuffer at 10fps when idle
        uint32_t now = millis();
        if (now - _lastLiveFrame >= LIVE_INTERVAL_MS) {
            _lastLiveFrame = now;
            sendFrame(display);
        }
    }
}

void RenderClient::sendFrame(Display& display) {
    const uint8_t* fb = display.getFramebuffer();
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
    _ws.sendBIN(linear, NUM_LEDS * 3);
}
