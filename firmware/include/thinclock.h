#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

// Ulanzi TC001: 32x8 pixel matrix
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define PANEL_CHAIN 1

// Ulanzi TC001 pin mapping
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

// Config fetch interval
#define CONFIG_POLL_MS 30000
#define DATA_POLL_MS 5000

enum ScrollMode { SCROLL_NONE, SCROLL_AUTO, SCROLL_LEFT, SCROLL_BOUNCE };
enum GaugeStyle { GAUGE_NONE, GAUGE_VBAR, GAUGE_HBAR, GAUGE_DOT };

struct ColorStop {
    float pos;       // 0.0 - 1.0
    uint8_t r, g, b;
};

struct ColorRange {
    float min_val;
    float max_val;
    std::vector<ColorStop> stops;
};

struct Icon {
    uint8_t width;
    uint8_t height;
    uint8_t fps;
    std::vector<std::vector<uint8_t>> frames;

    // Gauge mode (procedurally drawn, no pixel data needed)
    GaugeStyle gauge;
    String value_key;     // data key to read value from, e.g. "temperature"
    ColorRange range;

    // Color remap: replace key_color pixels with range-derived color
    uint32_t remap_key;   // RGB color to replace (0 = disabled)
};

struct Screen {
    String icon;
    String label;
    String data_url;
    uint32_t duration;
    int16_t text_x;
    int16_t text_y;
    uint32_t color;
    ScrollMode scroll;
    uint16_t scroll_speed;
    uint8_t fade_edge;
};

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
    bool valid;
};
