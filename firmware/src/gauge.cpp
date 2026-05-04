#include "gauge.h"

static uint8_t lerp8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)(a + (b - a) * t);
}

uint32_t colorFromRange(const ColorRange& range, float value) {
    if (range.stops.empty()) return 0xFFFFFF;

    float norm = (value - range.min_val) / (range.max_val - range.min_val);
    if (norm <= 0.0f) return ((uint32_t)range.stops.front().r << 16) |
                             ((uint32_t)range.stops.front().g << 8) |
                             range.stops.front().b;
    if (norm >= 1.0f) return ((uint32_t)range.stops.back().r << 16) |
                             ((uint32_t)range.stops.back().g << 8) |
                             range.stops.back().b;

    for (size_t i = 0; i + 1 < range.stops.size(); i++) {
        if (norm >= range.stops[i].pos && norm <= range.stops[i + 1].pos) {
            float t = (norm - range.stops[i].pos) /
                      (range.stops[i + 1].pos - range.stops[i].pos);
            uint8_t r = lerp8(range.stops[i].r, range.stops[i + 1].r, t);
            uint8_t g = lerp8(range.stops[i].g, range.stops[i + 1].g, t);
            uint8_t b = lerp8(range.stops[i].b, range.stops[i + 1].b, t);
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
    return 0xFFFFFF;
}

void drawGauge(Display& display, GaugeStyle style, uint8_t w, uint8_t h, const ColorRange& range, float value, int16_t x, int16_t y) {
    float norm = (value - range.min_val) / (range.max_val - range.min_val);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    uint32_t color = colorFromRange(range, value);

    uint32_t border = 0x666666;

    switch (style) {
        case GAUGE_VBAR: {
            // Border
            for (uint8_t col = 0; col < w; col++) {
                display.drawPixel(x + col, y, border);
                display.drawPixel(x + col, y + h - 1, border);
            }
            for (uint8_t row = 1; row < h - 1; row++) {
                display.drawPixel(x, y + row, border);
                display.drawPixel(x + w - 1, y + row, border);
            }
            // Fill interior bottom-up
            uint8_t innerH = h - 2;
            uint8_t fillH = (uint8_t)(norm * innerH + 0.5f);
            for (uint8_t row = 0; row < fillH; row++) {
                for (uint8_t col = 1; col < w - 1; col++) {
                    display.drawPixel(x + col, y + h - 2 - row, color);
                }
            }
            break;
        }
        case GAUGE_HBAR: {
            for (uint8_t col = 0; col < w; col++) {
                display.drawPixel(x + col, y, border);
                display.drawPixel(x + col, y + h - 1, border);
            }
            for (uint8_t row = 1; row < h - 1; row++) {
                display.drawPixel(x, y + row, border);
                display.drawPixel(x + w - 1, y + row, border);
            }
            uint8_t innerW = w - 2;
            uint8_t fillW = (uint8_t)(norm * innerW + 0.5f);
            for (uint8_t col = 0; col < fillW; col++) {
                for (uint8_t row = 1; row < h - 1; row++) {
                    display.drawPixel(x + 1 + col, y + row, color);
                }
            }
            break;
        }
        case GAUGE_DOT: {
            uint8_t midY = y + h / 2;
            for (uint8_t col = 0; col < w; col++) {
                display.drawPixel(x + col, midY, color);
                display.drawPixel(x + col, midY - 1, color);
            }
            break;
        }
        default:
            break;
    }
}

void remapIconColor(std::vector<uint8_t>& frame, uint32_t keyColor, uint32_t newColor) {
    uint8_t kr = (keyColor >> 16) & 0xFF;
    uint8_t kg = (keyColor >> 8) & 0xFF;
    uint8_t kb = keyColor & 0xFF;
    uint8_t nr = (newColor >> 16) & 0xFF;
    uint8_t ng = (newColor >> 8) & 0xFF;
    uint8_t nb = newColor & 0xFF;

    for (size_t i = 0; i + 2 < frame.size(); i += 3) {
        if (frame[i] == kr && frame[i + 1] == kg && frame[i + 2] == kb) {
            frame[i] = nr;
            frame[i + 1] = ng;
            frame[i + 2] = nb;
        }
    }
}
