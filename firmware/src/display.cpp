#include "display.h"
#include <FastLED_NeoMatrix.h>

static CRGB leds[NUM_LEDS];
static CRGB render_buf[NUM_LEDS];
static CRGB prev_frame[NUM_LEDS];
static CRGB* active_buf = render_buf;
static FastLED_NeoMatrix* neoMatrix = nullptr;

void Display::begin() {
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(40);

    neoMatrix = new FastLED_NeoMatrix(
        render_buf, MATRIX_WIDTH, MATRIX_HEIGHT,
        NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
        NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG
    );
    neoMatrix->begin();
    neoMatrix->setTextWrap(false);
    neoMatrix->clear();
    memset(leds, 0, sizeof(leds));
    memset(prev_frame, 0, sizeof(prev_frame));
    active_buf = render_buf;
    FastLED.show();
}

void Display::renderToPrev() {
    active_buf = prev_frame;
    // Recreate matrix pointing at prev_frame
    delete neoMatrix;
    neoMatrix = new FastLED_NeoMatrix(
        prev_frame, MATRIX_WIDTH, MATRIX_HEIGHT,
        NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
        NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG
    );
    neoMatrix->begin();
    neoMatrix->setTextWrap(false);
}

void Display::renderToMain() {
    active_buf = render_buf;
    delete neoMatrix;
    neoMatrix = new FastLED_NeoMatrix(
        render_buf, MATRIX_WIDTH, MATRIX_HEIGHT,
        NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
        NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG
    );
    neoMatrix->begin();
    neoMatrix->setTextWrap(false);
}

void Display::clear() {
    neoMatrix->clear();
}

void Display::show() {
    memcpy(leds, render_buf, sizeof(leds));
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
        uint8_t scale = (255 * (f + 1)) / (fadePixels + 1);
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            uint16_t idxL = neoMatrix->XY(f, y);
            active_buf[idxL].nscale8(scale);
            uint16_t idxR = neoMatrix->XY(MATRIX_WIDTH - 1 - f, y);
            active_buf[idxR].nscale8(scale);
        }
    }
}

void Display::fadeAll(uint8_t scale) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        active_buf[i].nscale8(scale);
    }
}

void Display::clearRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t row = y; row < y + h && row < MATRIX_HEIGHT; row++) {
        for (int16_t col = x; col < x + w && col < MATRIX_WIDTH; col++) {
            if (col >= 0 && row >= 0) {
                uint16_t idx = neoMatrix->XY(col, row);
                active_buf[idx] = CRGB::Black;
            }
        }
    }
}

void Display::snapshot() {
    memcpy(prev_frame, render_buf, sizeof(prev_frame));
}

void Display::crossfade(uint8_t progress) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(prev_frame[i], render_buf[i], progress);
    }
    FastLED.show();
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
