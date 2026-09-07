# Firmware Refactor: AppState

## Goal

Eliminate the `extern` global soup. Currently ~20 shared globals are declared
in `main.cpp` and re-declared as `extern` in every file that needs them. This
makes files hard to split, creates implicit coupling, and is the main reason
C++ decomposition feels painful here.

## The pattern

Introduce an `AppState` struct that owns all shared mutable state, instantiated
once in `main.cpp` and passed by reference to every subsystem.

```cpp
// include/app_state.h
struct AppState {
    Config       config;
    int          currentScreen = 0;
    uint32_t     lastScreenSwitch = 0;
    uint32_t     lastConfigFetch = 0;
    uint32_t     lastDataFetch = 0;
    JsonDocument screenData;
    ScreenState  currentState, prevState;
    int          prevScreenIdx = -1;
    JsonDocument prevScreenData;
    uint16_t     transitionProgress = 255;
    bool         transitioning = false;
    Timer        timer;
    bool         timerPaused = false;
    uint32_t     timerPausedRemaining = 0;
    Notification notifications[MAX_NOTIFICATIONS];
    int          notifCount = 0;
    bool         notifViewerOpen = false;
    int          notifViewerIdx = 0;
    int16_t      notifSlideY = -8;
    int16_t      notifScrollX = 0;
    uint32_t     notifLastScroll = 0;
    uint32_t     notifOpenTime = 0;
    String       lastButtonEvent;
};
```

Functions that currently reach for globals become:

```cpp
// before
void switchScreen() { currentScreen = (currentScreen + 1) % config.screens.size(); ... }

// after
void switchScreen(AppState& state) { state.currentScreen = (state.currentScreen + 1) % state.config.screens.size(); ... }
```

## What stays as globals

Hardware singletons that are genuinely process-wide and never need to be
passed around — these are fine as globals:

- `Display display`
- `ConfigManager configMgr`
- `Sensors sensors`
- `WebServer httpServer`
- `Preferences prefs`
- `RenderClient renderClient`
- `String configURL`, `wifiSSID`, `wifiPass`

## Approach

1. Create `include/app_state.h` with the struct
2. Instantiate `AppState state;` in `main.cpp`, remove the individual globals
3. Update function signatures one file at a time — `buttons.cpp` first (smallest
   surface), then `screens.cpp`, `http_routes.cpp`, `render_routes.cpp`, `main.cpp`
4. Remove all `extern` declarations as each file is migrated
5. `buttons.h` extern list shrinks to nothing — becomes just function declarations

## Why now is a good time

- All files are already split into logical modules
- The 300-line limit means no file is a monolith
- Working firmware with good test coverage via cppcheck
- The pattern is clear and mechanical — no architectural decisions needed

## Known working state before this refactor

- Firmware: screen_changed events, button events, WebSocket live stream
- HA integration: screen select, button event entity, coordinator refresh on events
- Deploy: `npm run deploy local` / `npm run flash`
- All cppcheck warnings resolved
