#pragma once
#include <Arduino.h>
#include "thinclock.h"
#include "app_state.h"

void beepOnce(uint16_t freq = 2000, uint16_t duration = 80);
void beepTriple();
void postEvent(AppState& state, const char* event);
void checkButtons(AppState& state);
void checkLDR(AppState& state);
void navigatePrev(AppState& state);
void navigateNext(AppState& state);
void simulateButton(AppState& state, const String& btn);
