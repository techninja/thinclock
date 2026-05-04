#include "particles.h"
#include "gauge.h"

void ParticleSystem::init(const ParticleConfig& cfg) {
    config = cfg;
    for (auto& p : particles) p.alive = false;
    for (auto& em : config.emitters) em.accumulator = 0;

    memset(maskGrid, 0, sizeof(maskGrid));
    if (config.mask.length() >= MATRIX_WIDTH * MATRIX_HEIGHT) {
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
                maskGrid[x][y] = config.mask[y * MATRIX_WIDTH + x] == '#';
            }
        }
    }
}

bool ParticleSystem::isMaskSolid(int16_t x, int16_t y) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return false;
    return maskGrid[x][y];
}

uint32_t ParticleSystem::randomColor() {
    if (config.colors.stops.empty()) return 0xFFFFFF;
    float pos = random(0, 1000) / 1000.0f;
    float val = config.colors.min_val + pos * (config.colors.max_val - config.colors.min_val);
    return colorFromRange(config.colors, val);
}

void ParticleSystem::spawn(const ParticleEmitter& em) {
    for (auto& p : particles) {
        if (p.alive) continue;
        p.alive = true;
        p.x = em.x < 0 ? random(0, MATRIX_WIDTH * 100) / 100.0f : em.x;
        p.y = em.y < 0 ? random(0, MATRIX_HEIGHT * 100) / 100.0f : em.y;
        p.vx = em.vx_min + (random(0, 1000) / 1000.0f) * (em.vx_max - em.vx_min);
        p.vy = em.vy_min + (random(0, 1000) / 1000.0f) * (em.vy_max - em.vy_min);
        p.lifetime = em.lifetime_min + random(0, em.lifetime_max - em.lifetime_min + 1);
        p.age = 0;
        p.size = em.size;
        p.color = em.is_rocket ? 0xFFFFFF : randomColor();
        p.isRocket = em.is_rocket;
        return;
    }
}

void ParticleSystem::burst(float x, float y) {
    int count = 8 + random(0, 6);
    uint32_t burstColor = randomColor();
    for (int i = 0; i < count; i++) {
        for (auto& p : particles) {
            if (p.alive) continue;
            p.alive = true;
            p.x = x;
            p.y = y;
            float angle = (float)i / count * 6.2832f + (random(0, 100) / 100.0f * 0.3f);
            float speed = 4.0f + random(0, 300) / 100.0f;
            p.vx = cos(angle) * speed * 2.0f;
            // Boost upward particles to counteract gravity, making a rounder burst
            float vy = sin(angle) * speed;
            if (vy < 0) vy *= 1.8f;  // upward gets extra kick
            p.vy = vy;
            p.lifetime = 500 + random(0, 400);
            p.age = 0;
            p.size = 1;
            p.color = burstColor;
            p.isRocket = false;
            break;
        }
    }
}

void ParticleSystem::tick(uint32_t dt_ms) {
    if (dt_ms == 0 || dt_ms > 200) dt_ms = 20;
    float dt = dt_ms / 1000.0f;

    // Spawn from emitters
    for (auto& em : config.emitters) {
        em.accumulator += em.rate * dt;
        while (em.accumulator >= 1.0f) {
            spawn(em);
            em.accumulator -= 1.0f;
        }
    }

    // Particle-particle collision (for bouncing mode with size >= 2)
    if (config.edge == PB_BOUNCE) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].alive || particles[i].size < 2) continue;
            for (int j = i + 1; j < MAX_PARTICLES; j++) {
                if (!particles[j].alive || particles[j].size < 2) continue;
                float dx = particles[j].x - particles[i].x;
                float dy = particles[j].y - particles[i].y;
                float dist = dx * dx + dy * dy;
                // Collision radius ~2px
                if (dist < 4.0f && dist > 0.01f) {
                    // Swap velocities (elastic)
                    float tvx = particles[i].vx;
                    float tvy = particles[i].vy;
                    particles[i].vx = particles[j].vx;
                    particles[i].vy = particles[j].vy;
                    particles[j].vx = tvx;
                    particles[j].vy = tvy;
                    // Push apart
                    float d = sqrtf(dist);
                    float overlap = (2.0f - d) * 0.5f;
                    float nx = dx / d;
                    float ny = dy / d;
                    particles[i].x -= nx * overlap;
                    particles[i].y -= ny * overlap;
                    particles[j].x += nx * overlap;
                    particles[j].y += ny * overlap;
                }
            }
        }
    }

    // Update particles
    for (auto& p : particles) {
        if (!p.alive) continue;

        p.age += dt_ms;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }

        float oldVy = p.vy;

        // Apply gravity
        p.vy += config.gravity * dt;

        // Rocket: burst at apex (vy crosses from negative to positive)
        if (p.isRocket && oldVy < 0 && p.vy >= 0) {
            burst(p.x, p.y);
            p.alive = false;
            continue;
        }

        // Move
        float nx = p.x + p.vx * dt;
        float ny = p.y + p.vy * dt;

        // Mask collision
        int16_t ix = (int16_t)nx;
        int16_t iy = (int16_t)ny;
        if (isMaskSolid(ix, iy)) {
            if (config.edge == PB_DIE) { p.alive = false; continue; }
            if (isMaskSolid((int16_t)nx, (int16_t)p.y)) { p.vx = -p.vx * 0.7f; nx = p.x; }
            if (isMaskSolid((int16_t)p.x, (int16_t)ny)) { p.vy = -p.vy * 0.7f; ny = p.y; }
        }

        // Edge handling
        if (config.edge == PB_BOUNCE) {
            if (nx < 0) { nx = 0; p.vx = -p.vx * 0.8f; }
            if (nx >= MATRIX_WIDTH - 1) { nx = MATRIX_WIDTH - 1.01f; p.vx = -p.vx * 0.8f; }
            if (ny < 0) { ny = 0; p.vy = -p.vy * 0.8f; }
            if (ny >= MATRIX_HEIGHT - 1) { ny = MATRIX_HEIGHT - 1.01f; p.vy = -p.vy * 0.8f; }
        } else if (config.edge == PB_WRAP) {
            if (nx < 0) nx += MATRIX_WIDTH;
            if (nx >= MATRIX_WIDTH) nx -= MATRIX_WIDTH;
            if (ny < 0) ny += MATRIX_HEIGHT;
            if (ny >= MATRIX_HEIGHT) ny -= MATRIX_HEIGHT;
        } else { // PB_DIE
            if (nx < -2 || nx >= MATRIX_WIDTH + 2 || ny < -2 || ny >= MATRIX_HEIGHT + 2) {
                p.alive = false;
                continue;
            }
        }

        p.x = nx;
        p.y = ny;
    }
}

void ParticleSystem::render(Display& display) {
    // Draw mask
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
        for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
            if (maskGrid[x][y]) display.drawPixel(x, y, 0x222222);
        }
    }

    // Draw particles
    for (auto& p : particles) {
        if (!p.alive) continue;

        uint32_t color = p.color;

        // Fade out in last 25% of life
        float progress = (float)p.age / p.lifetime;
        if (progress > 0.75f) {
            float fade = 1.0f - (progress - 0.75f) * 4.0f;
            uint8_t r = ((color >> 16) & 0xFF) * fade;
            uint8_t g = ((color >> 8) & 0xFF) * fade;
            uint8_t b = (color & 0xFF) * fade;
            color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }

        int16_t px = (int16_t)p.x;
        int16_t py = (int16_t)p.y;

        if (px >= 0 && px < MATRIX_WIDTH && py >= 0 && py < MATRIX_HEIGHT) {
            display.drawPixel(px, py, color);
        }
        if (p.size >= 2) {
            // 2x2 block
            if (px + 1 < MATRIX_WIDTH && py >= 0 && py < MATRIX_HEIGHT)
                display.drawPixel(px + 1, py, color);
            if (py + 1 < MATRIX_HEIGHT && px >= 0 && px < MATRIX_WIDTH)
                display.drawPixel(px, py + 1, color);
            if (px + 1 < MATRIX_WIDTH && py + 1 < MATRIX_HEIGHT)
                display.drawPixel(px + 1, py + 1, color);
        }
    }
}
