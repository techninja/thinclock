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
    void clearRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void fadeAll(uint8_t scale);
    void snapshot();
    void crossfade(uint8_t progress);
    void renderToPrev();   // redirect drawing to prev_frame buffer
    void renderToMain();   // redirect drawing back to render_buf
};
