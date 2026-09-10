#include "display.h"
#include <FastLED_NeoMatrix.h>

static CRGB leds[NUM_LEDS];
static CRGB render_buf[NUM_LEDS];
static CRGB prev_frame[NUM_LEDS];
static CRGB layer_snap[NUM_LEDS];
static CRGB* active_buf = render_buf;

static FastLED_NeoMatrix* mainMatrix = nullptr;
static FastLED_NeoMatrix* prevMatrix = nullptr;
static FastLED_NeoMatrix* neoMatrix = nullptr; // current active

static const uint16_t matrixFlags =
    NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
    NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG;

void Display::begin() {
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(40);

    mainMatrix = new FastLED_NeoMatrix(render_buf, MATRIX_WIDTH, MATRIX_HEIGHT, matrixFlags);
    mainMatrix->begin();
    mainMatrix->setTextWrap(false);

    prevMatrix = new FastLED_NeoMatrix(prev_frame, MATRIX_WIDTH, MATRIX_HEIGHT, matrixFlags);
    prevMatrix->begin();
    prevMatrix->setTextWrap(false);

    neoMatrix = mainMatrix;
    active_buf = render_buf;

    neoMatrix->clear();
    memset(leds, 0, sizeof(leds));
    memset(prev_frame, 0, sizeof(prev_frame));
    FastLED.show();
}

void Display::renderToPrev() {
    neoMatrix = prevMatrix;
    active_buf = prev_frame;
}

void Display::renderToMain() {
    neoMatrix = mainMatrix;
    active_buf = render_buf;
}

const uint8_t* Display::getFramebuffer() {
    // CRGB is {r, g, b} — 3 bytes per pixel, same layout as raw RGB
    return reinterpret_cast<const uint8_t*>(render_buf);
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

int16_t Display::nativeTextWidth(const String& text, uint8_t spacing, bool large) {
    int16_t w = 0;
    uint8_t charW = large ? 5 : 3;
    for (size_t i = 0; i < text.length(); i++) {
        char ch = text[i];
        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            w += charW + spacing;
        } else if (ch == ':' || ch == '.' || ch == '!' || ch == '@') {
            w += 1 + spacing;
        } else if (ch == '-' || ch == '/') {
            w += 3 + spacing;
        } else if (ch == ' ') {
            w += 2;
        } else if (ch == '%') {
            w += 3 + spacing;
        } else {
            w += 2;
        }
    }
    return w > 0 ? w - spacing : 0; // remove trailing spacing
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

void Display::fadeAll(uint8_t scale) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        active_buf[i].nscale8(scale);
    }
}

void Display::snapshot() {
    memcpy(prev_frame, render_buf, sizeof(prev_frame));
}

void Display::snapshotLayer() {
    memcpy(layer_snap, active_buf, sizeof(layer_snap));
}

void Display::applyLayerOpacity(uint8_t opacity) {
    if (opacity >= 255) return;
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        active_buf[i] = blend(layer_snap[i], active_buf[i], opacity);
    }
}

void Display::applyLayerAdditive() {
    // Additive blend: new pixels ADD to the background. Black = no change.
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        active_buf[i] = CRGB(
            min(255, (int)layer_snap[i].r + (int)active_buf[i].r),
            min(255, (int)layer_snap[i].g + (int)active_buf[i].g),
            min(255, (int)layer_snap[i].b + (int)active_buf[i].b)
        );
    }
}

void Display::crossfade(uint8_t progress) {
    // Blend prev_frame + render_buf into render_buf
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        render_buf[i] = blend(prev_frame[i], render_buf[i], progress);
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

// --- Native pixel-perfect renderers ---

// 3x5 font: columns stored as bitmask, LSB = top row
static const uint8_t FONT_3X5[][3] = {
    {0x1F,0x11,0x1F}, // 0
    {0x00,0x1F,0x00}, // 1
    {0x1D,0x15,0x17}, // 2
    {0x15,0x15,0x1F}, // 3
    {0x07,0x04,0x1F}, // 4
    {0x17,0x15,0x1D}, // 5
    {0x1F,0x15,0x1D}, // 6
    {0x01,0x01,0x1F}, // 7
    {0x1F,0x15,0x1F}, // 8
    {0x17,0x15,0x1F}, // 9
};

// 3x5 alphabet: A-Z
static const uint8_t FONT_3X5_ALPHA[][3] = {
    {0x1E,0x05,0x1E}, // A
    {0x1F,0x15,0x0A}, // B
    {0x0E,0x11,0x11}, // C
    {0x1F,0x11,0x0E}, // D
    {0x1F,0x15,0x11}, // E
    {0x1F,0x05,0x01}, // F
    {0x0E,0x11,0x19}, // G
    {0x1F,0x04,0x1F}, // H
    {0x11,0x1F,0x11}, // I
    {0x08,0x10,0x0F}, // J
    {0x1F,0x04,0x1B}, // K
    {0x1F,0x10,0x10}, // L
    {0x1F,0x02,0x1F}, // M
    {0x1F,0x06,0x1F}, // N
    {0x0E,0x11,0x0E}, // O
    {0x1F,0x05,0x02}, // P
    {0x0E,0x19,0x1E}, // Q
    {0x1F,0x05,0x1A}, // R
    {0x12,0x15,0x09}, // S
    {0x01,0x1F,0x01}, // T
    {0x0F,0x10,0x0F}, // U
    {0x07,0x18,0x07}, // V
    {0x1F,0x08,0x1F}, // W
    {0x1B,0x04,0x1B}, // X
    {0x03,0x1C,0x03}, // Y
    {0x19,0x15,0x13}, // Z
};

// 5x7 font: columns stored as bitmask, LSB = top row
static const uint8_t FONT_5X7[][5] = {
    {0x7F,0x41,0x41,0x41,0x7F}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x79,0x49,0x49,0x49,0x4F}, // 2
    {0x41,0x49,0x49,0x49,0x7F}, // 3
    {0x0F,0x08,0x08,0x08,0x7F}, // 4
    {0x4F,0x49,0x49,0x49,0x79}, // 5
    {0x7F,0x49,0x49,0x49,0x79}, // 6
    {0x01,0x01,0x01,0x01,0x7F}, // 7
    {0x7F,0x49,0x49,0x49,0x7F}, // 8
    {0x4F,0x49,0x49,0x49,0x7F}, // 9
};

void Display::drawDigit(int16_t x, int16_t y, uint8_t digit, uint32_t color, bool large) {
    if (digit > 9) return;
    if (large) {
        for (int col = 0; col < 5; col++) {
            uint8_t colData = FONT_5X7[digit][col];
            for (int row = 0; row < 7; row++) {
                if (colData & (1 << row)) drawPixel(x + col, y + row, color);
            }
        }
    } else {
        for (int col = 0; col < 3; col++) {
            uint8_t colData = FONT_3X5[digit][col];
            for (int row = 0; row < 5; row++) {
                if (colData & (1 << row)) drawPixel(x + col, y + row, color);
            }
        }
    }
}

void Display::drawColon(int16_t x, int16_t y, uint32_t color, bool large) {
    if (large) {
        drawPixel(x, y + 2, color);
        drawPixel(x, y + 4, color);
    } else {
        drawPixel(x, y + 1, color);
        drawPixel(x, y + 3, color);
    }
}

int16_t Display::drawNativeText(const String& text, int16_t x, int16_t y, uint32_t color, uint8_t spacing, bool large) {
    int16_t cx = x;
    uint8_t charW = large ? 5 : 3;

    for (size_t i = 0; i < text.length(); i++) {
        char ch = text[i];
        if (ch >= '0' && ch <= '9') {
            drawDigit(cx, y, ch - '0', color, large);
            cx += charW + spacing;
        } else if (ch >= 'A' && ch <= 'Z' && !large) {
            for (int col = 0; col < 3; col++) {
                uint8_t colData = FONT_3X5_ALPHA[ch - 'A'][col];
                for (int row = 0; row < 5; row++) {
                    if (colData & (1 << row)) drawPixel(cx + col, y + row, color);
                }
            }
            cx += charW + spacing;
        } else if (ch >= 'a' && ch <= 'z' && !large) {
            // Render lowercase as uppercase
            for (int col = 0; col < 3; col++) {
                uint8_t colData = FONT_3X5_ALPHA[ch - 'a'][col];
                for (int row = 0; row < 5; row++) {
                    if (colData & (1 << row)) drawPixel(cx + col, y + row, color);
                }
            }
            cx += charW + spacing;
        } else if (ch == ':') {
            drawColon(cx, y, color, large);
            cx += 1 + spacing;
        } else if (ch == ' ') {
            cx += 2;
        } else if (ch == '.') {
            drawPixel(cx, y + (large ? 6 : 4), color);
            cx += 1 + spacing;
        } else if (ch == '!') {
            drawPixel(cx, y, color);
            drawPixel(cx, y + 1, color);
            drawPixel(cx, y + 2, color);
            drawPixel(cx, y + 4, color);
            cx += 1 + spacing;
        } else if (ch == '-') {
            drawPixel(cx, y + 2, color);
            drawPixel(cx + 1, y + 2, color);
            drawPixel(cx + 2, y + 2, color);
            cx += 3 + spacing;
        } else if (ch == '/') {
            drawPixel(cx + 2, y, color);
            drawPixel(cx + 1, y + 1, color);
            drawPixel(cx + 1, y + 2, color);
            drawPixel(cx, y + 3, color);
            drawPixel(cx, y + 4, color);
            cx += 3 + spacing;
        } else if (ch == '@') {
            // Renders as a small dot — used as compact separator
            drawPixel(cx, y + 4, color);
            cx += 1 + spacing;
        } else if (ch == '%') {
            drawPixel(cx, y, color);
            drawPixel(cx + 2, y + (large ? 6 : 4), color);
            drawPixel(cx + 1, y + (large ? 3 : 2), color);
            cx += 3 + spacing;
        } else if (ch == 'F' || ch == 'C') {
            // Degree circle: 4px cross pattern
            drawPixel(cx + 1, y, color);     // top
            drawPixel(cx, y + 1, color);     // left
            drawPixel(cx + 2, y + 1, color); // right
            drawPixel(cx + 1, y + 2, color); // bottom
            cx += 4 + spacing;
        } else {
            cx += 2;
        }
    }
    return cx - x;
}
