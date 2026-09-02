#pragma once
#include "thinclock.h"
#include "display.h"

#define MAX_PARTICLES 48

struct Particle {
    float x, y, vx, vy;
    uint16_t lifetime;
    uint16_t age;
    uint8_t size;
    uint32_t color;    // fixed at spawn
    bool alive;
    bool isRocket;     // if true, spawns burst at apex
};

class ParticleSystem {
public:
    ParticleSystem() : config{}, particles{}, maskGrid{} {}
    void init(const ParticleConfig& cfg);
    void tick(uint32_t dt_ms);
    void render(Display& display);
    bool isActive() { return config.active; }

private:
    ParticleConfig config;
    Particle particles[MAX_PARTICLES];
    bool maskGrid[MATRIX_WIDTH][MATRIX_HEIGHT];

    void spawn(const ParticleEmitter& em);
    void burst(float x, float y);
    bool isMaskSolid(int16_t x, int16_t y);
    uint32_t randomColor();
};
