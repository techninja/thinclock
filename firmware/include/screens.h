#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "thinclock.h"
#include "screen_state.h"
#include "config_manager.h"
#include <ArduinoJson.h>

extern ScreenState currentState;
extern ScreenState prevState;
extern int         prevScreenIdx;
extern JsonDocument prevScreenData;
extern uint16_t    transitionProgress;
extern bool        transitioning;
extern int         currentScreen;
extern uint32_t    lastScreenSwitch;
extern uint32_t    lastDataFetch;
extern JsonDocument screenData;

void resetState(ScreenState& state);
void initScreenState(ScreenState& state, const Screen& scr);
void renderScreen(Screen& scr, ScreenState& state, const JsonDocument& data);
void switchScreen();
void resetCurrentScreen(bool prev);  // used by navigatePrev
void showClock();
bool screenHasScrolling(const ScreenState& state);
