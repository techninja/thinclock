#include "gif_encoder.h"
#include <string.h>
#include <math.h>

// Static buffers for scaled frames (max ~4KB each at scale=3 with gap)
static uint8_t _idxBufA[GIF_MAX_PIXELS];
static uint8_t _idxBufB[GIF_MAX_PIXELS];

void GifEncoder::write(const uint8_t* data, size_t len) { _client->write(data, len); }
void GifEncoder::write8(uint8_t val) { _client->write(val); }
void GifEncoder::write16(uint16_t val) { write8(val & 0xFF); write8((val >> 8) & 0xFF); }

static void buildFixedPalette(uint8_t* palette) {
    uint16_t idx = 0;
    for (uint8_t r = 0; r < 6; r++)
        for (uint8_t g = 0; g < 6; g++)
            for (uint8_t b = 0; b < 6; b++) {
                palette[idx*3]     = r * 51;
                palette[idx*3 + 1] = g * 51;
                palette[idx*3 + 2] = b * 51;
                idx++;
            }
    for (uint16_t i = 216; i < 256; i++) {
        uint8_t v = (i - 216) * 6 + 3;
        palette[i*3] = v; palette[i*3+1] = v; palette[i*3+2] = v;
    }
}

uint8_t GifEncoder::colorToIndex(uint8_t r, uint8_t g, uint8_t b) {
    r = _gammaLUT[r]; g = _gammaLUT[g]; b = _gammaLUT[b];
    uint8_t ri = (r + 25) / 51; if (ri > 5) ri = 5;
    uint8_t gi = (g + 25) / 51; if (gi > 5) gi = 5;
    uint8_t bi = (b + 25) / 51; if (bi > 5) bi = 5;
    return ri * 36 + gi * 6 + bi;
}

void GifEncoder::buildScaledFrame(const uint8_t* rgb) {
    // Black (index 0) for gaps
    memset(_indexed, 0, _outPixels);

    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 32; x++) {
            uint16_t srcIdx = (y * 32 + x) * 3;
            uint8_t r = rgb[srcIdx], g = rgb[srcIdx+1], b = rgb[srcIdx+2];
            uint8_t idx = colorToIndex(r, g, b);

            // Fill the scaled pixel block (skip gap pixels — they stay black)
            uint16_t startX = x * (_scale + _gap);
            uint16_t startY = y * (_scale + _gap);
            for (uint8_t sy = 0; sy < _scale; sy++) {
                for (uint8_t sx = 0; sx < _scale; sx++) {
                    uint16_t outIdx = (startY + sy) * _outW + (startX + sx);
                    if (outIdx < _outPixels) _indexed[outIdx] = idx;
                }
            }
        }
    }
}

void GifEncoder::begin(WiFiClient& client, uint16_t delayMs, uint8_t scale, uint8_t gap, uint8_t gamma) {
    _client = &client;
    _delay = delayMs / 10;
    if (_delay < 2) _delay = 2;
    _scale = scale < 1 ? 1 : scale;
    _gap = gap;
    _gamma = gamma / 10.0f;

    _outW = 32 * _scale + 31 * _gap;
    _outH = 8 * _scale + 7 * _gap;
    _outPixels = _outW * _outH;
    if (_outPixels > GIF_MAX_PIXELS) {
        // Clamp scale down
        _scale = 1; _gap = 0;
        _outW = 32; _outH = 8;
        _outPixels = 256;
    }

    // Build gamma LUT
    for (uint16_t i = 0; i < 256; i++) {
        _gammaLUT[i] = (uint8_t)(powf((float)i / 255.0f, 1.0f / _gamma) * 255.0f);
    }

    buildFixedPalette(_palette);
    _paletteSize = 256;
    _indexed = _idxBufA;
    _prevIndexed = _idxBufB;
    _firstFrame = true;
    _pendingDelay = 0;
    memset(_prevIndexed, 0, GIF_MAX_PIXELS);
}

void GifEncoder::writeHeader() {
    write(reinterpret_cast<const uint8_t*>("GIF89a"), 6);
    write16(_outW);
    write16(_outH);
    write8(0xF7); // GCT 256 colors
    write8(0);
    write8(0);
    write(_palette, 256 * 3);
    // NETSCAPE looping
    write8(0x21); write8(0xFF); write8(0x0B);
    write(reinterpret_cast<const uint8_t*>("NETSCAPE2.0"), 11);
    write8(0x03); write8(0x01);
    write16(0);
    write8(0x00);
}

void GifEncoder::writeFrameData() {
    write8(0x21); write8(0xF9); write8(0x04);
    write8(0x04);
    write16(_pendingDelay);
    write8(0x00);
    write8(0x00);
    write8(0x2C);
    write16(0); write16(0);
    write16(_outW); write16(_outH);
    write8(0x00);
    writeLZW();
}

void GifEncoder::writeLZW() {
    uint8_t minCodeSize = 8;
    write8(minCodeSize);

    uint16_t clearCode = 256;
    uint16_t eoiCode = 257;
    uint8_t block[255];
    uint8_t blockLen = 0;
    uint32_t bitBuf = 0;
    uint8_t bitCount = 0;
    uint8_t codeSize = 9;

    auto flush = [&]() {
        while (bitCount >= 8) {
            block[blockLen++] = bitBuf & 0xFF;
            bitBuf >>= 8;
            bitCount -= 8;
            if (blockLen == 255) {
                write8(blockLen);
                write(block, blockLen);
                blockLen = 0;
            }
        }
    };

    auto emit = [&](uint16_t code) {
        bitBuf |= ((uint32_t)code << bitCount);
        bitCount += codeSize;
        flush();
    };

    emit(clearCode);
    uint16_t sinceReset = 0;
    for (uint16_t i = 0; i < _outPixels; i++) {
        emit(_indexed[i]);
        sinceReset++;
        if (sinceReset >= 254) {
            emit(clearCode);
            sinceReset = 0;
        }
    }
    emit(eoiCode);

    if (bitCount > 0) block[blockLen++] = bitBuf & 0xFF;
    if (blockLen > 0) { write8(blockLen); write(block, blockLen); }
    write8(0x00);
}

void GifEncoder::addFrame(const uint8_t* rgb) {
    buildScaledFrame(rgb);

    if (!_firstFrame && memcmp(_indexed, _prevIndexed, _outPixels) == 0) {
        _pendingDelay += _delay;
        return;
    }

    if (_firstFrame) {
        writeHeader();
        _pendingDelay = _delay;
        _firstFrame = false;
    } else {
        writeFrameData();
        _pendingDelay = _delay;
    }
    memcpy(_prevIndexed, _indexed, _outPixels);
}

void GifEncoder::end() {
    if (!_firstFrame) writeFrameData();
    write8(0x3B);
}
