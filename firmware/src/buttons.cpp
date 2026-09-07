#include <WiFi.h>
#include "buttons.h"
#include "screens.h"
#include "config_manager.h"
#include <HTTPClient.h>

#define NAV_COOLDOWN_MS  500
#define LONG_PRESS_MS    500
#define BEEP_DOWN_FREQ   1800
#define BEEP_DOWN_DUR    20
#define BEEP_SHORT_FREQ  2200
#define BEEP_SHORT_DUR   30
#define BEEP_LONG_FREQ   1200
#define BEEP_LONG_DUR    50

#define LDR_COVER_MIN_MS  200
#define LDR_COOLDOWN_MS   1000
#define LDR_AMBIENT_MIN   80
#define LDR_COVER_RATIO   0.35f

static bool     btnLeftLast = HIGH, btnMidLast = HIGH, btnRightLast = HIGH;
static uint32_t btnLeftDown = 0, btnMidDown = 0, btnRightDown = 0;
static bool     btnLeftLongFired = false, btnMidLongFired = false, btnRightLongFired = false;
static uint32_t lastNavTime = 0;

static bool     ldrCovered = false;
static uint32_t ldrCoverStart = 0;
static uint32_t lastLdrTrigger = 0;
static uint16_t ldrBaseline = 0;

// -----------------------------------------------------------------------

void beepOnce(uint16_t freq, uint16_t duration) {
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
    for (int i = 0; i < 3; i++) { beepOnce(2500, 60); if (i < 2) delay(80); }
}

void postEvent(AppState& state, const char* event) {
    if (strcmp(event, "screen_changed") != 0) state.lastButtonEvent = event;
    if (state.config.event_url.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(state.config.event_url);
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{\"event\":\"" + String(event) + "\",\"screen\":" + state.currentScreen + "}");
    Serial.printf("[event] %s → %s (%d)\n", event, state.config.event_url.c_str(), code);
    http.end();
}

void simulateButton(AppState& state, const String& btn) {
    if (btn == "left")        { beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR); navigatePrev(state); postEvent(state, "left"); }
    else if (btn == "right")  { beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR); navigateNext(state); postEvent(state, "right"); }
    else if (btn == "select") {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (state.notifCount > 0 || state.timer.active) {
            state.notifViewerOpen = !state.notifViewerOpen;
            if (state.notifViewerOpen) {
                state.notifViewerIdx = state.timer.active ? -1 : 0;
                state.notifSlideY = -8; state.notifScrollX = 0;
                state.notifOpenTime = millis();
            }
        } else { state.lastConfigFetch = 0; }
        postEvent(state, "select");
    }
}

void navigatePrev(AppState& state) {
    if (state.config.screens.empty() || millis() - lastNavTime < NAV_COOLDOWN_MS) return;
    lastNavTime = millis();
    resetCurrentScreen(state, true);
}

void navigateNext(AppState& state) {
    if (state.config.screens.empty() || millis() - lastNavTime < NAV_COOLDOWN_MS) return;
    lastNavTime = millis();
    switchScreen(state);
}

void checkLDR(AppState& state) {
    uint16_t ldr = analogRead(LDR_PIN);
    uint32_t now = millis();
    if (!ldrCovered) ldrBaseline = ldrBaseline ? (ldrBaseline * 15 + ldr) / 16 : ldr;
    bool armed     = ldrBaseline >= LDR_AMBIENT_MIN;
    bool isCovered = armed && ldr < (uint16_t)(ldrBaseline * LDR_COVER_RATIO);

    if (isCovered) {
        if (!ldrCovered) { ldrCovered = true; ldrCoverStart = now; }
        else if (now - ldrCoverStart >= LDR_COVER_MIN_MS && now - lastLdrTrigger >= LDR_COOLDOWN_MS) {
            lastLdrTrigger = now; ldrCovered = false;
            if (state.timer.active && !state.timer.fired) {
                if (state.timerPaused) {
                    state.timer.endTime = millis() + state.timerPausedRemaining;
                    state.timerPaused = false; beepOnce(1800, 40);
                } else {
                    state.timerPausedRemaining = state.timer.endTime - millis();
                    state.timerPaused = true; beepOnce(1200, 40);
                }
            }
            postEvent(state, "ldr_cover");
        }
    } else { ldrCovered = false; }
}

void checkButtons(AppState& state) {
    bool l = digitalRead(BUTTON_LEFT);
    bool m = digitalRead(BUTTON_MID);
    bool r = digitalRead(BUTTON_RIGHT);
    uint32_t now = millis();

    // --- LEFT ---
    if (l == LOW && btnLeftLast == HIGH) { btnLeftDown = now; btnLeftLongFired = false; beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR); }
    if (l == LOW && !btnLeftLongFired && now - btnLeftDown >= LONG_PRESS_MS) {
        btnLeftLongFired = true; beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR); postEvent(state, "left_long");
    }
    if (l == HIGH && btnLeftLast == LOW && !btnLeftLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (state.notifViewerOpen) {
            if (state.notifViewerIdx > 0) state.notifViewerIdx--;
            else if (state.notifViewerIdx == 0 && state.timer.active) state.notifViewerIdx = -1;
            else if (state.notifViewerIdx == -1 && state.timer.fired) { state.timer.active = false; state.timer.fired = false; state.notifViewerOpen = false; }
            state.notifSlideY = -8; state.notifScrollX = 0; state.notifOpenTime = now;
        } else if (state.config.buttons == "navigate") navigatePrev(state);
        postEvent(state, "left");
    }

    // --- MIDDLE ---
    if (m == LOW && btnMidLast == HIGH) { btnMidDown = now; btnMidLongFired = false; beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR); }
    if (m == LOW && !btnMidLongFired && now - btnMidDown >= LONG_PRESS_MS) {
        btnMidLongFired = true; beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR);
        if (state.notifViewerOpen && state.notifViewerIdx == -1 && state.timer.active) {
            state.timer.active = false; state.timer.fired = false; state.timerPaused = false;
            state.notifViewerOpen = false; state.notifSlideY = -8;
            beepOnce(1000, 60); delay(80); beepOnce(600, 80);
        } else { state.timerPaused = false; postEvent(state, "select_long"); state.lastConfigFetch = 0; }
    }
    if (m == HIGH && btnMidLast == LOW && !btnMidLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (state.notifViewerOpen) { state.notifViewerOpen = false; state.notifSlideY = -8; }
        else if (state.notifCount > 0 || state.timer.active) {
            state.notifViewerOpen = true; state.notifViewerIdx = state.timer.active ? -1 : 0;
            state.notifSlideY = -8; state.notifScrollX = 0; state.notifOpenTime = now;
        } else { state.timerPaused = false; postEvent(state, "select"); state.lastConfigFetch = 0; }
    }

    // --- RIGHT ---
    if (r == LOW && btnRightLast == HIGH) { btnRightDown = now; btnRightLongFired = false; beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR); }
    if (r == LOW && !btnRightLongFired && now - btnRightDown >= LONG_PRESS_MS) {
        btnRightLongFired = true; beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR); postEvent(state, "right_long");
    }
    if (r == HIGH && btnRightLast == LOW && !btnRightLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (state.notifViewerOpen) {
            if (state.notifViewerIdx == -1) {
                if (state.timer.fired) { state.timer.active = false; state.timer.fired = false; }
                if (state.notifCount > 0) state.notifViewerIdx = 0; else state.notifViewerOpen = false;
            } else {
                state.notifViewerIdx++;
                if (state.notifViewerIdx >= state.notifCount) {
                    state.notifViewerOpen = false; state.notifCount = 0;
                    for (int i = 0; i < MAX_NOTIFICATIONS; i++) state.notifications[i].active = false;
                }
            }
            state.notifSlideY = -8; state.notifScrollX = 0; state.notifOpenTime = now;
        } else if (state.config.buttons == "navigate") navigateNext(state);
        postEvent(state, "right");
    }

    btnLeftLast = l; btnMidLast = m; btnRightLast = r;
}
