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
    LAYER_NATIVE,     // pixel-perfect text (digits, colon, %, F/C)
    LAYER_PARTICLES,
    LAYER_GAUGE,
    LAYER_CLOCK,
    LAYER_PIXELS,
    LAYER_GRADIENT,
};

// --- Color ---

struct ColorStop {
    float pos;
    uint8_t r, g, b;
};

struct ColorRange {
    float min_val;
    float max_val;
    std::vector<ColorStop> stops;
};

// --- Icon ---

struct Icon {
    uint8_t width;
    uint8_t height;
    uint8_t fps;
    std::vector<std::vector<uint8_t>> frames;
    uint32_t remap_key;
    String remap_value_key;
    ColorRange remap_range;
};

// --- Particles ---

struct ParticleEmitter {
    float x, y;
    float vx_min, vx_max;
    float vy_min, vy_max;
    float rate;
    uint16_t lifetime_min, lifetime_max;
    uint8_t size;
    bool is_rocket;
    float accumulator;
};

struct ParticleConfig {
    std::vector<ParticleEmitter> emitters;
    float gravity;
    ParticleBehavior edge;
    ColorRange colors;
    String mask;
    bool active;
};

// --- Pixel pattern ---

struct PixelDot {
    int16_t x, y;
    uint32_t color;
};

// --- Layer ---

struct Layer {
    LayerType type;
    int16_t x, y;
    uint8_t opacity;  // 0-255, applied to all pixels this layer draws
    String blend;      // "normal" (default), "add" (additive - black=transparent)

    // LAYER_ICON
    String icon_name;

    // LAYER_TEXT
    String label;
    String data_url;
    uint32_t color;
    ScrollMode scroll;
    uint16_t scroll_speed;
    uint8_t fade_edge;

    // LAYER_PARTICLES
    ParticleConfig particles;

    // LAYER_GAUGE
    GaugeStyle gauge;
    uint8_t gauge_w, gauge_h;
    String value_key;
    ColorRange range;

    // LAYER_CLOCK
    String clock_format; // "12h" or "24h"

    // LAYER_NATIVE
    bool native_large;   // true = 5x7, false = 3x5
    uint8_t native_spacing; // px between chars

    // LAYER_PIXELS
    String pixels_pattern; // "week_dots", "bar", etc.
    String pixels_data_key;
    uint32_t pixels_color;
    uint32_t pixels_dim_color;
    std::vector<std::pair<int8_t, int8_t>> pixels_points; // for "dots" pattern

    // LAYER_GRADIENT
    uint8_t grad_w, grad_h;       // size (0 = full screen)
    String grad_direction;         // "horizontal", "vertical", "diagonal"
    ColorRange grad_colors;        // color stops across the gradient

    // Tweens (per-layer animation)
    struct Tween {
        String prop;       // "x", "y", "opacity"
        float from, to;
        uint16_t duration; // ms
        String easing;     // "linear", "sine", "ease_in", "ease_out", "ease_in_out"
        String loop;       // "none", "repeat", "pingpong"
        uint16_t delay;    // ms before starting
    };
    std::vector<Tween> tweens;
};

// --- Screen ---

struct Screen {
    std::vector<Layer> layers;
    uint32_t duration;
    String data_url;
};

// --- Notifications ---

#define MAX_NOTIFICATIONS 8

struct Notification {
    std::vector<Layer> layers;
    uint32_t color;
    String icon_name;   // optional icon to show left of text
    uint8_t beep;
    uint32_t alertInterval;
    uint32_t lastBeep;
    bool active;
};

// --- Timer ---

struct Timer {
    uint32_t endTime;    // millis() when timer expires
    uint32_t duration;   // original duration in ms
    uint32_t color;
    bool active;
    bool fired;          // has the completion beep fired
};

// --- Config ---

struct Config {
    String config_url;
    String event_url;
    String time_format;
    String temp_unit;
    std::vector<Screen> screens;
    std::map<String, Icon> icons;
    uint8_t brightness;
    int8_t timezone_offset;
    uint32_t scroll_speed;
    uint8_t transition_ms;
    String buttons;
    bool allow_beep;
    bool valid;
};
