#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

extern WebServer httpServer;

void registerHttpRoutes();
