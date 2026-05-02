# Clock Top JSON Format Specification

**Version: 0.1.0 (MVP)**

Clock Top is a config-driven firmware for ESP32 LED matrix displays. The device
fetches a single JSON config from a URL, then renders screens as defined in that
config. Any system that can serve JSON can drive the display.

---

## Overview

```
Device boots
  → Connects to WiFi
  → Fetches config from config_url
  → For each screen:
      → Fetches data from screen's data_url (if set)
      → Resolves {placeholders} in label text
      → Renders text with scroll/color/position
      → Waits for duration, then next screen
  → Loops forever, re-fetching config every 30s
```

---

## Config Endpoint

The device GETs this URL and expects the following JSON structure.

### Top-Level Object

| Field      | Type     | Required | Description                    |
|------------|----------|----------|--------------------------------|
| `settings` | object   | no       | Global display settings        |
| `screens`  | array    | yes      | Ordered list of screen objects |
| `sprites`  | array    | no       | Named sprite definitions (WIP) |

### Settings Object

| Field          | Type   | Default | Description                        |
|----------------|--------|---------|------------------------------------|
| `brightness`   | int    | 40      | LED brightness (0-255)             |
| `timezone`     | int    | 0       | UTC offset in hours                |
| `scroll_speed` | int    | 50      | Default scroll speed (ms per pixel)|

### Screen Object

| Field          | Type   | Default    | Description                              |
|----------------|--------|------------|------------------------------------------|
| `label`        | string | `""`       | Display text. Supports `{key}` placeholders resolved from `data_url` response |
| `data_url`     | string | `""`       | URL returning flat JSON for placeholder values. Empty = no data fetch |
| `duration`     | int    | 5000       | Milliseconds to display this screen      |
| `x`            | int    | 0          | Text X position in pixels. -1 = auto     |
| `y`            | int    | 0          | Text Y position in pixels. -1 = auto     |
| `color`        | string | `"FFFFFF"` | Text color as 6-digit RGB hex (no `#`)   |
| `scroll`       | string | `"auto"`   | Scroll mode (see below)                  |
| `scroll_speed` | int    | (global)   | Override scroll speed for this screen    |
| `fade_edge`    | int    | 2          | Pixels to fade at left/right edges during scroll (0 = off) |
| `icon`         | string | `""`       | Sprite name reference (WIP)              |

### Scroll Modes

| Mode     | Behavior |
|----------|----------|
| `none`   | Static text, no scrolling. Text clips at display edge |
| `auto`   | Uses `bounce` if text is wider than display, otherwise `none` |
| `bounce` | Scrolls left until end of text is visible, pauses, scrolls back right, pauses, repeats |
| `left`   | Banner style. Text enters from right, scrolls fully off left edge to black, then re-enters from right |

### Sprite Object (WIP — not yet implemented)

| Field    | Type   | Default | Description                          |
|----------|--------|---------|--------------------------------------|
| `name`   | string | `""`    | Reference name used by screen `icon` |
| `url`    | string | `""`    | URL to RGB888 packed pixel data      |
| `width`  | int    | 8       | Sprite width in pixels               |
| `height` | int    | 8       | Sprite height in pixels              |
| `frames` | int    | 1       | Number of animation frames           |

---

## Data Endpoint

Each screen's `data_url` should return **flat JSON** with string, number, or
integer values. Keys are matched to `{placeholder}` tokens in the screen's
`label`.

```json
{"temperature": 72.5, "humidity": 45, "status": "OK"}
```

With a label of `"{temperature}F {status}"`, this renders as `"72.5F OK"`.

- Floats render with 1 decimal place
- Integers render as-is
- Strings render as-is
- Missing keys render as empty string

---

## Device Setup

### Serial Configuration

On first boot (or after flash erase), send WiFi credentials via serial at
115200 baud:

```json
{"ssid": "MyNetwork", "pass": "MyPassword", "config_url": "http://192.168.1.100:3000/config"}
```

Credentials are stored in non-volatile storage and persist across reboots.

### Fallback Behavior

When no config is available (no WiFi, no config_url, or fetch fails), the
device displays a basic clock:
- NTP-synced time if WiFi is connected
- Uptime counter if no network

---

## Hardware

| Component  | Detail                          |
|------------|---------------------------------|
| Board      | ESP32-D0WD (Ulanzi TC001)       |
| Display    | 32×8 WS2812 addressable LEDs    |
| LED Pin    | GPIO 32                         |
| Buzzer     | GPIO 15 (active high, held low) |
| Buttons    | GPIO 26, 27, 14                 |
| LDR        | GPIO 35                         |
| Font       | 5×7 pixels (Adafruit GFX default) |

---

## Examples

### Minimal Config — Just a Clock

```json
{
  "screens": [
    {
      "label": "{time}",
      "data_url": "http://myserver/time",
      "duration": 60000,
      "x": 2, "y": 0,
      "color": "00AAFF",
      "scroll": "none"
    }
  ]
}
```

Where `/time` returns: `{"time": "14:30"}`

### Multi-Screen Dashboard

```json
{
  "settings": {
    "brightness": 60,
    "scroll_speed": 50
  },
  "screens": [
    {
      "label": "{time}",
      "data_url": "http://myserver/data/clock",
      "duration": 10000,
      "x": 2, "y": 0,
      "color": "00AAFF",
      "scroll": "none"
    },
    {
      "label": "{temp}F {humidity}%",
      "data_url": "http://myserver/data/weather",
      "duration": 5000,
      "x": 0, "y": 0,
      "color": "FF8800",
      "scroll": "auto"
    },
    {
      "label": "Welcome to Clock Top!",
      "duration": 10000,
      "x": 0, "y": 0,
      "color": "FF0088",
      "scroll": "left",
      "scroll_speed": 40,
      "fade_edge": 3
    }
  ]
}
```

### Home Assistant Integration

Serve config from a Node/Python/etc server that reads HA sensors:

```json
{
  "screens": [
    {
      "label": "{temp}F",
      "data_url": "http://ha-bridge:3000/sensor/living_room_temp",
      "duration": 5000,
      "color": "FF4400",
      "scroll": "auto"
    },
    {
      "label": "{state}",
      "data_url": "http://ha-bridge:3000/sensor/front_door",
      "duration": 5000,
      "color": "44FF44",
      "scroll": "none"
    }
  ]
}
```

---

## Roadmap

- [ ] Sprite/icon rendering (fetch, cache to SPIFFS, display alongside text)
- [ ] Multi-frame sprite animation
- [ ] Second font size (smaller 3×5 for compact data)
- [ ] Screen transition effects (fade, slide)
- [ ] Text alignment (center, right)
- [ ] Background color per screen
- [ ] Button input events (POST to a callback URL)
- [ ] LDR auto-brightness
- [ ] Captive portal for WiFi setup (no serial needed)
- [ ] OTA firmware updates
