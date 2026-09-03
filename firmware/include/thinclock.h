#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

// Ulanzi TC001: 32x8 pixel matrix
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define LED_PIN 32
#define BUZZER_PIN 15
#define LDR_PIN 35
#define BATTERY_PIN 34
#define BUTTON_LEFT 26
#define BUTTON_MID 27
#define BUTTON_RIGHT 14
#define I2C_SDA 21
#define I2C_SCL 22
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#define CONFIG_POLL_MS 30000
#define DATA_POLL_MS 5000

// --- Enums ---

enum ScrollMode { SCROLL_NONE, SCROLL_AUTO, SCROLL_LEFT, SCROLL_BOUNCE };
enum GaugeStyle { GAUGE_NONE, GAUGE_VBAR, GAUGE_HBAR, GAUGE_DOT };
enum ParticleBehavior { PB_DIE, PB_BOUNCE, PB_WRAP };
enum LayerType {
    LAYER_ICON,
    LAYER_TEXT,
    LAYER_NATIVE,
    LAYER_PARTICLES,
    LAYER_GAUGE,
    LAYER_CLOCK,
    LAYER_PIXELS,
    LAYER_GRADIENT,
};

// --- Color ---

struct ColorStop {
    float pos = 0;
    uint8_t r = 0, g = 0, b = 0;
};

struct ColorRange {
    float min_val = 0;
    float max_val = 1;
    std::vector<ColorStop> stops;
};

// --- Icon ---

struct Icon {
    uint8_t width = 0;
    uint8_t height = 0;
    uint8_t fps = 0;
    std::vector<std::vector<uint8_t>> frames;
    uint32_t remap_key = 0;
    String remap_value_key;
    ColorRange remap_range;
};

// --- Particles ---

struct ParticleEmitter {
    float x = 0, y = 0;
    float vx_min = 0, vx_max = 0;
    float vy_min = 0, vy_max = 0;
    float rate = 0;
    uint16_t lifetime_min = 0, lifetime_max = 0;
    uint8_t size = 1;
    bool is_rocket = false;
    float accumulator = 0;
};

struct ParticleConfig {
    std::vector<ParticleEmitter> emitters;
    float gravity = 0;
    ParticleBehavior edge = PB_DIE;
    ColorRange colors;
    String mask;
    uint16_t warmup = 0;
    bool active = false;
};

// --- Pixel pattern ---

struct PixelDot {
    int16_t x = 0, y = 0;
    uint32_t color = 0;
};

// --- Layer ---

struct Layer {
    LayerType type = LAYER_TEXT;
    int16_t x = 0, y = 0;
    uint8_t opacity = 255;
    String blend;

    // LAYER_ICON
    String icon_name;

    // LAYER_TEXT
    String label;
    String data_url;
    uint32_t color = 0xFFFFFF;
    ScrollMode scroll = SCROLL_AUTO;
    uint16_t scroll_speed = 50;
    uint8_t fade_edge = 0;

    // LAYER_PARTICLES
    ParticleConfig particles;

    // LAYER_GAUGE
    GaugeStyle gauge = GAUGE_NONE;
    uint8_t gauge_w = 0, gauge_h = 0;
    String value_key;
    ColorRange range;

    // LAYER_CLOCK
    String clock_format;

    // LAYER_NATIVE
    bool native_large = false;
    uint8_t native_spacing = 1;
    String align;
    uint8_t align_width = 0;

    // LAYER_PIXELS
    String pixels_pattern;
    String pixels_data_key;
    uint32_t pixels_color = 0xFFFFFF;
    uint32_t pixels_dim_color = 0x222222;
    std::vector<std::pair<int8_t, int8_t>> pixels_points;

    // LAYER_GRADIENT
    uint8_t grad_w = 0, grad_h = 0;
    String grad_direction;
    ColorRange grad_colors;

    // Tweens
    struct Tween {
        String prop;
        float from = 0, to = 0;
        uint16_t duration = 1000;
        String easing;
        String loop;
        uint16_t delay = 0;
    };
    std::vector<Tween> tweens;
};

// --- Screen ---

struct Screen {
    std::vector<Layer> layers;
    uint32_t duration = 10000;
    String data_url;
};

// --- Notifications ---

#define MAX_NOTIFICATIONS 8

struct Notification {
    std::vector<Layer> layers;
    uint32_t color = 0xFFAA00;
    String icon_name;
    uint8_t beep = 1;
    uint32_t alertInterval = 30000;
    uint32_t lastBeep = 0;
    bool active = false;
};

// --- Timer ---

struct Timer {
    uint32_t endTime = 0;
    uint32_t duration = 0;
    uint32_t color = 0x00AAFF;
    bool active = false;
    bool fired = false;
};

// --- Config ---

struct Config {
    String config_url;
    String event_url;
    String time_format;
    String temp_unit;
    std::vector<Screen> screens;
    std::map<String, Icon> icons;
    uint8_t brightness = 40;
    int8_t timezone_offset = 0;
    uint32_t scroll_speed = 50;
    uint8_t transition_ms = 8;
    String buttons;
    bool allow_beep = true;
    bool valid = false;
};
