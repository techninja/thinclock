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
    int16_t nativeTextWidth(const String& text, uint8_t spacing = 1, bool large = false);
    void applyEdgeFade(uint8_t fadePixels);
    void clearRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void fadeAll(uint8_t scale);
    void snapshot();
    void crossfade(uint8_t progress);
    void renderToPrev();
    void renderToMain();

    // Opacity support: snapshot before layer, blend after
    void snapshotLayer();  // save current buffer state
    void applyLayerOpacity(uint8_t opacity);  // blend new pixels with saved state

    // Native pixel-perfect renderers (no font library)
    void drawDigit(int16_t x, int16_t y, uint8_t digit, uint32_t color, bool large = false);
    void drawColon(int16_t x, int16_t y, uint32_t color, bool large = false);
    int16_t drawNativeText(const String& text, int16_t x, int16_t y, uint32_t color, uint8_t spacing = 1, bool large = false);
};
