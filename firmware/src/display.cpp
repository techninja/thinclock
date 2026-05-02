#include "display.h"
#include <FastLED_NeoMatrix.h>

static CRGB leds[NUM_LEDS];
static FastLED_NeoMatrix* neoMatrix = nullptr;

void Display::begin() {
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(40);

    neoMatrix = new FastLED_NeoMatrix(
        leds, MATRIX_WIDTH, MATRIX_HEIGHT,
        NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
        NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG
    );
    neoMatrix->begin();
    neoMatrix->setTextWrap(false);
    neoMatrix->clear();
    FastLED.show();
}

void Display::clear() {
    neoMatrix->clear();
}

void Display::show() {
    FastLED.show();
}

void Display::setBrightness(uint8_t b) {
    FastLED.setBrightness(b);
}

void Display::drawPixel(int16_t x, int16_t y, uint32_t color) {
    neoMatrix->drawPixel(x, y, neoMatrix->Color(
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF
    ));
}

void Display::drawText(const String& text, int16_t x, int16_t y, uint32_t color, bool small) {
    neoMatrix->setCursor(x, y);
    neoMatrix->setTextColor(neoMatrix->Color(
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF
    ));
    neoMatrix->print(text);
}

int16_t Display::textWidth(const String& text) {
    int16_t x1, y1;
    uint16_t w, h;
    neoMatrix->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return (int16_t)w;
}

void Display::applyEdgeFade(uint8_t fadePixels) {
    if (fadePixels == 0) return;
    for (uint8_t f = 0; f < fadePixels; f++) {
        // Scale: pixel 0 = dimmest, pixel fadePixels-1 = full
        uint8_t scale = (255 * (f + 1)) / (fadePixels + 1);
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            // Left edge
            uint16_t idxL = neoMatrix->XY(f, y);
            leds[idxL].nscale8(scale);
            // Right edge
            uint16_t idxR = neoMatrix->XY(MATRIX_WIDTH - 1 - f, y);
            leds[idxR].nscale8(scale);
        }
    }
}

void Display::drawSprite(const uint8_t* data, uint8_t w, uint8_t h, int16_t x, int16_t y) {
    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            size_t offset = (row * w + col) * 3;
            uint8_t r = data[offset];
            uint8_t g = data[offset + 1];
            uint8_t b = data[offset + 2];
            if (r || g || b) {
                neoMatrix->drawPixel(x + col, y + row, neoMatrix->Color(r, g, b));
            }
        }
    }
}
