#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "thinclock.h"

// Shared state needed by main loop and screens
extern Timer   timer;
extern bool    timerPaused;
extern uint32_t timerPausedRemaining;
extern bool    notifViewerOpen;
extern int     notifViewerIdx;
extern int16_t notifSlideY;
extern int16_t notifScrollX;
extern uint32_t notifOpenTime;
extern int     notifCount;
extern int     currentScreen;
extern uint32_t lastConfigFetch;
extern String  lastButtonEvent;

void beepOnce(uint16_t freq = 2000, uint16_t duration = 80);
void beepTriple();
void postEvent(const char* event);
void checkButtons();
void checkLDR();
void navigatePrev();
void navigateNext();
void simulateButton(const String& btn);
