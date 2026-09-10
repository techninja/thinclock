#pragma once
#include "thinclock.h"
#include "screen_state.h"
#include <ArduinoJson.h>

struct AppState {
    Config       config;
    int          currentScreen      = 0;
    uint32_t     lastScreenSwitch   = 0;
    uint32_t     lastConfigFetch    = 0;
    uint32_t     lastDataFetch      = 0;
    JsonDocument screenData;

    ScreenState  currentState;
    ScreenState  prevState;
    int          prevScreenIdx      = -1;
    JsonDocument prevScreenData;
    uint16_t     transitionProgress = 255;
    bool         transitioning      = false;

    Timer        timer;
    bool         timerPaused            = false;
    uint32_t     timerPausedRemaining   = 0;

    Notification notifications[MAX_NOTIFICATIONS];
    int          notifCount         = 0;
    bool         notifViewerOpen    = false;
    int          notifViewerIdx     = 0;
    int16_t      notifSlideY        = -8;
    int16_t      notifScrollX       = 0;
    uint32_t     notifLastScroll    = 0;
    uint32_t     notifOpenTime      = 0;

    String       lastButtonEvent;
};
