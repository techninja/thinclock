#pragma once
#include "thinclock.h"
#include "screen_state.h"
#include "display.h"
#include "config_manager.h"
#include <WebSocketsClient.h>

/**
 * WebSocket client for render jobs.
 * Connects to server, receives render commands, sends back raw RGB frames.
 * Also streams live framebuffer when no render job is active.
 */
class RenderClient {
public:
    void begin(const String& serverHost, uint16_t serverPort);
    void loop();

    // Call from main loop — does one frame of work if job active
    void tick(Display& display, ConfigManager& configMgr, Config& config);

    bool isConnected() { return _connected; }

    // Called by static WS event handler
    void onEvent(WStype_t type, uint8_t* payload, size_t length);

private:
    WebSocketsClient _ws;
    bool _connected = false;
    bool _rendering = false;

    // Render job state
    Screen _screen;
    ScreenState _state;
    JsonDocument _data;
    uint16_t _framesTotal = 0;
    uint16_t _framesDone = 0;
    uint32_t _lastFrameTime = 0;
    uint16_t _frameDelayMs = 66;

    // Live framebuffer streaming
    uint32_t _lastLiveFrame = 0;
    static const uint32_t LIVE_INTERVAL_MS = 100; // 10fps

    void startJob(uint8_t* payload, size_t length, ConfigManager& configMgr, Config& config);
    void sendFrame(Display& display);
};
