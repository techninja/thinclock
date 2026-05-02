#pragma once
#include "thinclock.h"
#include <FastLED.h>

class Display {
public:
    void begin();
    void clear();
    void show();
    void setBrightness(uint8_t b);
    void drawPixel(int16_t x, int16_t y, uint32_t color);
    void drawText(const String& text, int16_t x, int16_t y, uint32_t color, bool small = false);
    void drawSprite(const uint8_t* data, uint8_t w, uint8_t h, int16_t x, int16_t y);
    int16_t textWidth(const String& text);
    void applyEdgeFade(uint8_t fadePixels);
};
