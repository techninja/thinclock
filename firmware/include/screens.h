#pragma once
#include <Arduino.h>
#include "thinclock.h"
#include "screen_state.h"
#include "config_manager.h"
#include "app_state.h"
#include <ArduinoJson.h>

void resetState(ScreenState& state);
void initScreenState(ScreenState& state, const Screen& scr);
void renderScreen(AppState& state, Screen& scr, ScreenState& ss, const JsonDocument& data);
void switchScreen(AppState& state);
void resetCurrentScreen(AppState& state, bool prev);
void showClock();
bool screenHasScrolling(const ScreenState& state);
