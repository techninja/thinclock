#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "app_state.h"

extern WebServer httpServer;

void registerHttpRoutes(AppState& state);

// render_routes.cpp
void unzigzag(const uint8_t* fb, uint8_t* out);
void handleFramebuffer(AppState& state);
void handlePreview(AppState& state);
void handleRender(AppState& state);
void handleGifGet(AppState& state);
void handleGifPost(AppState& state);
