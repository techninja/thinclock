#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

extern WebServer httpServer;

void registerHttpRoutes();

// render_routes.cpp
void unzigzag(const uint8_t* fb, uint8_t* out);
void handleFramebuffer();
void handlePreview();
void handleRender();
void handleGifGet();
void handleGifPost();
