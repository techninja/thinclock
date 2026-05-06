# thinclock

A thin, config-driven ESP32 firmware for LED matrix displays. JSON in, pixels out.

The firmware is intentionally dumb — it fetches JSON from a URL and renders what it's told. The server is smart — screen definitions are composable JS modules that can pull data from any source. Everything on the display is defined externally. No reflashing to change what it shows.

## Quick Start

```bash
git clone <repo>
cd thinclock
npm install
cp .env.example .env   # edit with your WiFi, timezone, API keys
npm run flash           # build & upload firmware to Ulanzi TC001
npm run dev             # start config server
```

On first boot, open serial monitor and paste the JSON line the server prints. Device reboots, connects, and starts rendering.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  thinclock device (ESP32 + 32×8 LED matrix)         │
│  - Fetches config JSON every 30s                    │
│  - Renders layers at 50fps                          │
│  - Onboard sensors, buttons, buzzer                 │
│  - HTTP API: /notify, /timer, /beep, /sensors       │
└──────────────────────┬──────────────────────────────┘
                       │ HTTP (JSON)
┌──────────────────────┴──────────────────────────────┐
│  thinclock server (Node.js)                         │
│  - Screen modules define what to display            │
│  - Alert engine monitors conditions                 │
│  - Adapters pull data (weather, HA, APIs)           │
│  - Management API for enable/disable/scheduling     │
└─────────────────────────────────────────────────────┘
```

## Features

### Display Engine (Firmware)
- **Layer-based compositing** — screens are stacks of typed layers rendered in order
- **Particle system** — configurable emitters, gravity, collision, rocket/burst, edge behavior
- **Tweens** — animate any layer property (x, y, opacity) with easing and looping
- **Gradients** — horizontal, vertical, diagonal with color stops and opacity
- **Native pixel font** — 3×5 and 5×7 digit/letter renderers with tight spacing
- **Icon rendering** — inline hex pixel data, animation frames, color remapping
- **Gauges** — vertical bar, horizontal bar, dot indicators with color ranges
- **Crossfade transitions** — smooth blending between screens with both animating
- **Opacity & blend modes** — per-layer opacity, additive blending (for stars, glow)
- **Scrolling text** — bounce, left (banner), auto modes with edge fade

### Notifications & Timer
- **Push notifications** via HTTP — colored indicator dots, scrollable text viewer
- **Timer** with breathing indicator dot that accelerates as time runs out
- **Pomodoro** — work/break cycles with phase indicator bar
- **Alert beeps** — single, double, triple, alarm, or custom frequency patterns
- **Buzzer API** — trigger any beep pattern via HTTP

### Buttons & Input
- **Short press / long press** detection with audio feedback
- **Left/Right** — navigate screens (short), reserved (long)
- **Middle** — open notification viewer (short), context action (long)
- **LDR "button"** — cover light sensor to pause/resume timer
- **Server events** — all presses POST to server for custom handling

### Server
- **Screen modules** — self-contained JS files, auto-discovered
- **Scheduling** — per-screen time/date/month restrictions
- **Priority sorting** — higher priority screens appear first in rotation
- **Night mode** — reduced brightness + night-only screens during configured hours
- **Alert engine** — screen modules declare conditions, server evaluates and notifies
- **Management API** — enable/disable screens, inspect state
- **Weather** — OpenWeatherMap integration with dynamic rain/snow particles
- **AQI** — air quality with EPA color scale
- **Home Assistant adapter** — placeholder for HA WebSocket integration

## Project Structure

```
thinclock/
├── firmware/              # ESP32 PlatformIO project
│   ├── src/               # main, display, particles, gauge, sensors, config
│   ├── include/           # headers
│   └── platformio.ini
├── server/                # Node.js config server
│   ├── screens/           # Screen modules (drop in to add!)
│   ├── adapters/          # Data adapters (HA, etc.)
│   └── lib/               # Registry, alerts, icon helpers
├── homeassistant/         # HA custom component
├── docs/                  # Format spec, API docs
└── package.json           # Project orchestrator
```

## Scripts

| Command | Description |
|---------|-------------|
| `npm run dev` | Start config server |
| `npm run build` | Compile firmware |
| `npm run flash` | Compile and upload |
| `npm run flash:force` | Erase flash + upload |
| `npm run erase` | Erase ESP32 flash |
| `npm run monitor` | Serial monitor (115200) |

## Documentation

- **[Layer Format & API](docs/FORMAT.md)** — complete JSON schema for screens, layers, and all config options
- **[Device HTTP API](docs/DEVICE_API.md)** — notification, timer, beep, and sensor endpoints
- **[Screen Module Guide](docs/SCREENS.md)** — how to write custom screen modules
- **[Alert System](docs/ALERTS.md)** — defining alert conditions in screen modules

## Hardware

| Component | Detail |
|-----------|--------|
| Board | ESP32-D0WD (Ulanzi TC001) |
| Display | 32×8 WS2812 addressable LEDs |
| LED Pin | GPIO 32 |
| Buzzer | GPIO 15 |
| Buttons | GPIO 26 (L), 27 (M), 14 (R) |
| LDR | GPIO 35 |
| I2C | SDA 21, SCL 22 |
| Temp/Humidity | BME280/BMP280/HTU21DF/SHT31 (auto-detect) |

## Environment Variables

```bash
# Required
WIFI_SSID=your_network
WIFI_PASS=your_password
SERVER_PORT=3000
TIMEZONE=-5

# Display
BRIGHTNESS=40
BRIGHTNESS_NIGHT=10
NIGHT_HOURS=22,23,0,1,2,3,4,5,6
TIME_FORMAT=12h
TEMP_UNIT=F
ALLOW_BEEPING=true

# Screen management
SCREEN_MODE=auto          # auto, manual, all
MAX_SCREENS=8
# SCREEN_ALLOWLIST=clock,weather,sensors
# SCREEN_BLOCKLIST=water-fill

# Device (for server→device communication)
DEVICE_IP=192.168.x.x

# Weather (OpenWeatherMap free tier)
OWM_API_KEY=your_key
OWM_CITY=Sacramento,CA,US

# Home Assistant (optional)
HA_URL=http://homeassistant.local:8123
HA_TOKEN=your_long_lived_token
```

## Included Screens

| Screen | Type | Schedule | Description |
|--------|------|----------|-------------|
| Awtrix Clock | utility | always | Calendar icon + clock + week dots |
| Weather | utility | always | Dynamic icon + temp + rain/snow particles |
| Air Quality | utility | always | EPA color bar + AQI value |
| Network Monitor | utility | always | Ping latency + WiFi signal gauge |
| Sensor Dashboard | utility | always | Light gauge + temperature |
| Pomodoro | utility | always | Work/break timer with phase bar |
| Night Clock | night | 10pm-7am | Dim red clock with sunrise transition |
| Bouncing Balls | fun | always | Colliding 2×2 rainbow particles |
| Fireworks | seasonal | July 1-4 | Rockets burst at apex |
| Winter Scene | seasonal | Nov-Feb | Tree + snowman + snow particles |
| Campfire | ambient | evening | Fire + moon + stars + tent |
| Ocean Waves | ambient | always | Gradient water + foam particles |
| Starfield | ambient | always | Parallax warp from center |
| Fire | ambient | always | Rising flame particles |
| Lava Lamp | ambient | always | Warm rising blobs |
| Ambient Gradient | mood | evening | Slowly shifting color diagonal |

## License

MIT
