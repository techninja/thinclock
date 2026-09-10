# ThinClock — Home Assistant Integration

## Quick Install

### 1. Add this repo as a custom add-on repository

In HA: **Settings → Add-ons → Add-on Store → ⋮ → Repositories**

Add: `https://github.com/techninja/thinclock`

Then install **ThinClock** from the store, configure your options, and start it.

### 2. Install the custom integration

Copy `custom_components/thinclock/` into your HA config directory:

```
config/
  custom_components/
    thinclock/   ← copy this folder here
```

Restart HA. The integration will appear under **Settings → Integrations**.

### 3. Flash the firmware

See the main [README](../README.md). Once the device connects to WiFi it advertises
itself via mDNS (`thinclock.local`) and HA will show a discovery notification automatically.

---

## Architecture

```
Home Assistant
  ├── ThinClock add-on  (Node.js server, persistent, ingress UI in sidebar)
  │     ├── /api/config        → served to device on boot
  │     ├── /data/ha/:entity   → proxies any HA entity state to screens
  │     └── WebSocket          → live framebuffer, render queue
  │
  ├── ThinClock integration  (custom_components/thinclock/)
  │     ├── sensor.*           → temperature, humidity, light (from device)
  │     ├── select.*           → current screen
  │     ├── number.*           → brightness
  │     └── button.*           → left / middle / right physical buttons
  │
  └── ThinClock device  (ESP32, thinclock.local)
        ├── Fetches config from add-on on boot
        └── Advertises _thinclock._tcp via mDNS
```

## Using HA Entities in Screens

Any HA entity is available as a data URL in screen modules:

```js
// src/api/screens/my-screen.js
export const screen = {
  layers: [{ type: 'text', label: '{{state}}°', data_url: '{{BASE}}/data/ha/sensor.thermostat_1_nativezone_temperature' }]
};
```

The add-on connects to HA's WebSocket API using the Supervisor token automatically —
no manual token configuration needed when running as an add-on.

## Add-on Options

| Option | Description | Default |
|---|---|---|
| `device_ip` | ESP32 device IP (optional — used for live preview) | |
| `timezone` | UTC offset | `-7` |
| `brightness` | Display brightness 1–100 | `40` |
| `brightness_night` | Night mode brightness | `10` |
| `night_hours` | Comma-separated hours for night mode | `22,23,0,1,2,3,4,5` |
| `time_format` | `12h` or `24h` | `12h` |
| `temp_unit` | `F` or `C` | `F` |
| `screen_blocklist` | Comma-separated screen IDs to disable | |
| `allow_beeping` | Enable buzzer | `true` |
| `wifi_ssid` / `wifi_pass` | Printed to serial for easy device setup | |
| `owm_api_key` / `owm_city` | OpenWeatherMap (for weather screen) | |
