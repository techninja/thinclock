#include <WiFi.h>
#include "buttons.h"
#include "screens.h"
#include "config_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>

extern Config config;
extern String configURL;
extern Notification notifications[MAX_NOTIFICATIONS];

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

static bool    btnLeftLast = HIGH, btnMidLast = HIGH, btnRightLast = HIGH;
static uint32_t btnLeftDown = 0, btnMidDown = 0, btnRightDown = 0;
static bool    btnLeftLongFired = false, btnMidLongFired = false, btnRightLongFired = false;
static uint32_t lastNavTime = 0;

String lastButtonEvent = "";

static bool    ldrCovered = false;
static uint32_t ldrCoverStart = 0;
static uint32_t lastLdrTrigger = 0;
static uint16_t ldrBaseline = 0;

// -----------------------------------------------------------------------

void beepOnce(uint16_t freq, uint16_t duration) {
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
    for (int i = 0; i < 3; i++) { beepOnce(2500, 60); if (i < 2) delay(80); }
}

void postEvent(const char* event) {
    if (strcmp(event, "screen_changed") != 0) lastButtonEvent = event;
    if (config.event_url.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(config.event_url);
    http.setTimeout(2000);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{\"event\":\"" + String(event) + "\",\"screen\":" + currentScreen + "}");
    Serial.printf("[event] %s → %s (%d)\n", event, config.event_url.c_str(), code);
    http.end();
}

void simulateButton(const String& btn) {
    if (btn == "left")        { beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR); navigatePrev(); postEvent("left"); }
    else if (btn == "right")  { beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR); navigateNext(); postEvent("right"); }
    else if (btn == "select") {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (notifCount > 0 || timer.active) {
            notifViewerOpen = !notifViewerOpen;
            if (notifViewerOpen) { notifViewerIdx = timer.active ? -1 : 0; notifSlideY = -8; notifScrollX = 0; notifOpenTime = millis(); }
        } else { lastConfigFetch = 0; }
        postEvent("select");
    }
}

void navigatePrev() {
    if (config.screens.empty() || millis() - lastNavTime < NAV_COOLDOWN_MS) return;
    lastNavTime = millis();
    resetCurrentScreen(true);
}

void navigateNext() {
    if (config.screens.empty() || millis() - lastNavTime < NAV_COOLDOWN_MS) return;
    lastNavTime = millis();
    switchScreen();
}

void checkLDR() {
    uint16_t ldr = analogRead(LDR_PIN);
    uint32_t now = millis();
    if (!ldrCovered) ldrBaseline = ldrBaseline ? (ldrBaseline * 15 + ldr) / 16 : ldr;
    bool armed     = ldrBaseline >= LDR_AMBIENT_MIN;
    bool isCovered = armed && ldr < (uint16_t)(ldrBaseline * LDR_COVER_RATIO);

    if (isCovered) {
        if (!ldrCovered) { ldrCovered = true; ldrCoverStart = now; }
        else if (now - ldrCoverStart >= LDR_COVER_MIN_MS && now - lastLdrTrigger >= LDR_COOLDOWN_MS) {
            lastLdrTrigger = now; ldrCovered = false;
            if (timer.active && !timer.fired) {
                if (timerPaused) {
                    timer.endTime = millis() + timerPausedRemaining;
                    timerPaused = false; beepOnce(1800, 40);
                } else {
                    timerPausedRemaining = timer.endTime - millis();
                    timerPaused = true; beepOnce(1200, 40);
                }
            }
            postEvent("ldr_cover");
        }
    } else { ldrCovered = false; }
}

void checkButtons() {
    bool l = digitalRead(BUTTON_LEFT);
    bool m = digitalRead(BUTTON_MID);
    bool r = digitalRead(BUTTON_RIGHT);
    uint32_t now = millis();

    // --- LEFT ---
    if (l == LOW && btnLeftLast == HIGH) { btnLeftDown = now; btnLeftLongFired = false; beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR); }
    if (l == LOW && !btnLeftLongFired && now - btnLeftDown >= LONG_PRESS_MS) {
        btnLeftLongFired = true; beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR); postEvent("left_long");
    }
    if (l == HIGH && btnLeftLast == LOW && !btnLeftLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (notifViewerOpen) {
            if (notifViewerIdx > 0) notifViewerIdx--;
            else if (notifViewerIdx == 0 && timer.active) notifViewerIdx = -1;
            else if (notifViewerIdx == -1 && timer.fired) { timer.active = false; timer.fired = false; notifViewerOpen = false; }
            notifSlideY = -8; notifScrollX = 0; notifOpenTime = now;
        } else if (config.buttons == "navigate") navigatePrev();
        postEvent("left");
    }

    // --- MIDDLE ---
    if (m == LOW && btnMidLast == HIGH) { btnMidDown = now; btnMidLongFired = false; beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR); }
    if (m == LOW && !btnMidLongFired && now - btnMidDown >= LONG_PRESS_MS) {
        btnMidLongFired = true; beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR);
        if (notifViewerOpen && notifViewerIdx == -1 && timer.active) {
            timer.active = false; timer.fired = false; timerPaused = false;
            notifViewerOpen = false; notifSlideY = -8;
            beepOnce(1000, 60); delay(80); beepOnce(600, 80);
        } else { timerPaused = false; postEvent("select_long"); lastConfigFetch = 0; }
    }
    if (m == HIGH && btnMidLast == LOW && !btnMidLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (notifViewerOpen) { notifViewerOpen = false; notifSlideY = -8; }
        else if (notifCount > 0 || timer.active) {
            notifViewerOpen = true; notifViewerIdx = timer.active ? -1 : 0;
            notifSlideY = -8; notifScrollX = 0; notifOpenTime = now;
        } else { timerPaused = false; postEvent("select"); lastConfigFetch = 0; }
    }

    // --- RIGHT ---
    if (r == LOW && btnRightLast == HIGH) { btnRightDown = now; btnRightLongFired = false; beepOnce(BEEP_DOWN_FREQ, BEEP_DOWN_DUR); }
    if (r == LOW && !btnRightLongFired && now - btnRightDown >= LONG_PRESS_MS) {
        btnRightLongFired = true; beepOnce(BEEP_LONG_FREQ, BEEP_LONG_DUR); postEvent("right_long");
    }
    if (r == HIGH && btnRightLast == LOW && !btnRightLongFired) {
        beepOnce(BEEP_SHORT_FREQ, BEEP_SHORT_DUR);
        if (notifViewerOpen) {
            if (notifViewerIdx == -1) {
                if (timer.fired) { timer.active = false; timer.fired = false; }
                if (notifCount > 0) notifViewerIdx = 0; else notifViewerOpen = false;
            } else {
                notifViewerIdx++;
                if (notifViewerIdx >= notifCount) {
                    notifViewerOpen = false; notifCount = 0;
                    for (int i = 0; i < MAX_NOTIFICATIONS; i++) notifications[i].active = false;
                }
            }
            notifSlideY = -8; notifScrollX = 0; notifOpenTime = now;
        } else if (config.buttons == "navigate") navigateNext();
        postEvent("right");
    }

    btnLeftLast = l; btnMidLast = m; btnRightLast = r;
}
