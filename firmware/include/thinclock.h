#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

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

struct Sprite {
    String name;
    String url;
    uint8_t width;
    uint8_t height;
    uint8_t frames;
    bool cached;
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
    String event_url;       // POST button events here
    String time_format;     // "12h" or "24h"
    String temp_unit;       // "C" or "F"
    std::vector<Screen> screens;
    std::vector<Sprite> sprites;
    uint8_t brightness;
    int8_t timezone_offset;
    uint32_t scroll_speed;
    uint8_t transition_ms;  // crossfade duration in frames (0=instant)
    bool valid;
};
