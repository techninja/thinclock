#pragma once
#include "thinclock.h"
#include "display.h"

uint32_t colorFromRange(const ColorRange& range, float value);
void drawGauge(Display& display, GaugeStyle style, uint8_t w, uint8_t h, const ColorRange& range, float value, int16_t x, int16_t y);
void remapIconColor(std::vector<uint8_t>& frame, uint32_t keyColor, uint32_t newColor);
