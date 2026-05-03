#pragma once
#include "thinclock.h"
#include "display.h"

// Interpolate a color from a ColorRange given a raw value
uint32_t colorFromRange(const ColorRange& range, float value);

// Draw a gauge icon procedurally
void drawGauge(Display& display, const Icon& icon, float value, int16_t x, int16_t y);

// Remap key_color pixels in a frame buffer to a new color
void remapIconColor(std::vector<uint8_t>& frame, uint32_t keyColor, uint32_t newColor);
