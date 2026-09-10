#pragma once
#include "thinclock.h"
#include "particles.h"

// --- Per-layer render state ---

struct TextState {
    int16_t offset = 0;
    int8_t dir = 1;
    uint32_t pauseUntil = 0;
    uint32_t lastStep = 0;
    ScrollMode mode = SCROLL_NONE;
    int16_t textW = 0;
    bool completedOnce = false;
    String resolved;
};

struct IconState {
    uint8_t frame = 0;
    uint32_t lastStep = 0;
};

struct ScreenState {
    std::vector<TextState> textStates;
    std::vector<IconState> iconStates;
    std::vector<ParticleSystem> particleSystems;
    uint32_t lastTick = 0;
    uint32_t startTime = 0;
    bool inited = false;
};
