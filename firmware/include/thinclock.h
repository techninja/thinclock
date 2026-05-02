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
#define BUTTON_LEFT 26
#define BUTTON_MID 27
#define BUTTON_RIGHT 14
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
    uint16_t scroll_speed;  // ms per pixel step
    uint8_t fade_edge;      // pixels to fade at edges (0=off)
};

struct Config {
    String config_url;
    std::vector<Screen> screens;
    std::vector<Sprite> sprites;
    uint8_t brightness;
    int8_t timezone_offset;
    uint32_t scroll_speed;  // default scroll speed
    bool valid;
};
