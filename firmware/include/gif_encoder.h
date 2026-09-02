#pragma once
#include <Arduino.h>
#include <WiFiClient.h>

#define GIF_MAX_COLORS 256
#define GIF_MAX_PIXELS 16384  // max scaled size (e.g. 320x80 = 25600 won't fit, but indexed fits)

/**
 * GIF89a encoder with LED-style rendering.
 * Supports variable dimensions, pixel gaps, and gamma correction.
 */
class GifEncoder {
public:
    GifEncoder() : _client(nullptr), _delay(0), _scale(1), _gap(0), _gamma(1.0f),
                   _outW(0), _outH(0), _outPixels(0), _palette{}, _paletteSize(0),
                   _indexed(nullptr), _prevIndexed(nullptr), _firstFrame(true),
                   _pendingDelay(0), _gammaLUT{} {}
    /**
     * Begin encoding with LED-style options.
     * @param client WiFi client to stream to
     * @param delayMs Frame delay in milliseconds
     * @param scale Pixel scale factor (1=raw, 2=2x, etc.)
     * @param gap Gap pixels between LEDs (0=none)
     * @param gamma Gamma correction (10=1.0, 18=1.8, 25=2.5)
     */
    void begin(WiFiClient& client, uint16_t delayMs, uint8_t scale = 1, uint8_t gap = 0, uint8_t gamma = 10);
    void addFrame(const uint8_t* rgb);  // 768 bytes raw 32x8 RGB
    void end();

private:
    WiFiClient* _client;
    uint16_t _delay;
    uint8_t _scale;
    uint8_t _gap;
    float _gamma;
    uint16_t _outW, _outH;
    uint16_t _outPixels;
    uint8_t _palette[GIF_MAX_COLORS * 3];
    uint16_t _paletteSize;
    uint8_t* _indexed;
    uint8_t* _prevIndexed;
    bool _firstFrame;
    uint16_t _pendingDelay;
    uint8_t _gammaLUT[256];

    uint8_t colorToIndex(uint8_t r, uint8_t g, uint8_t b);
    void buildScaledFrame(const uint8_t* rgb);
    void writeHeader();
    void writeFrameData();
    void writeLZW();
    void write(const uint8_t* data, size_t len);
    void write8(uint8_t val);
    void write16(uint16_t val);
};
