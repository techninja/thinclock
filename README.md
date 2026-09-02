# thinclock

> Thin, config-driven ESP32 LED matrix firmware. JSON in, pixels out.

![ThinClock](thinclock_logo.png)

Built for the [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-clock-2882) and compatible 32×8 LED matrix clocks.

---

## What is this?

ThinClock replaces the stock Awtrix firmware with a lightweight, fully open alternative. A Node.js server generates screen configs from modular JS screen definitions. The ESP32 fetches config on boot and renders everything locally — no cloud, no subscriptions.

When running as a Home Assistant add-on, the server lives inside HA, the device auto-discovers via mDNS, and your HA entities (sensors, lights, weather, thermostat) are available to any screen with a single data URL.

---

## Quick Start

### 1. Flash the firmware

```bash
git clone https://github.com/techninja/thinclock
cd thinclock
npm install
npm run flash
```

On first boot with no WiFi credentials, the device starts an AP: **`thinclock-setup`** (password: `thinclock`). Connect and visit `192.168.4.1` to enter your WiFi and server URL.

### 2a. Run the server standalone

```bash
cp .env.example .env   # fill in your values
npm run dev
```

The server starts at `http://localhost:3232`. Set your device's config URL to `http://<your-machine-ip>:3232/api/config`.

### 2b. Run as a Home Assistant add-on *(recommended)*

1. In HA: **Settings → Add-ons → Add-on Store → ⋮ → Repositories**
2. Add: `https://github.com/techninja/thinclock`
3. Install **ThinClock**, configure options, start it
4. The server runs persistently at `http://homeassistant.local:3232`

### 3. Install the HA integration *(optional but great)*

Copy `homeassistant/custom_components/thinclock/` into your HA `config/custom_components/` directory and restart HA.

Once the device is on your network, HA will show a **"New device discovered: ThinClock"** notification. Confirm it and you get:

- 🌡️ Temperature, humidity, light sensors from the device
- 📺 Screen selector
- 🔆 Brightness slider  
- 🔘 Left / middle / right button entities (trigger automations!)

---

## Monorepo Structure

```
thinclock/
  firmware/              ESP32 firmware (PlatformIO / Arduino)
  src/                   Node.js config server + web UI
    api/
      screens/           Screen modules — one file per screen
      adapters/          Data adapters (Home Assistant, etc.)
      lib/               Registry, schedules, alerts, GIF encoder
    components/          Hybrids.js web components (no build step)
    pages/               Dashboard, Rotation, Editor, Settings
  homeassistant/
    custom_components/   HA custom integration (Python)
    README.md            Integration install guide
  thinclock-addon/       HA Supervisor add-on (Docker)
  docs/                  Specs and API docs
  scripts/               Build, test, release tooling
```

---

## Screens

Screens are plain JS modules in `src/api/screens/`. Each exports a `screen` object (layers, data URL, duration) and optional metadata:

```js
export const name = 'my-screen';
export const tags = ['daytime'];
export const schedule = 'work';   // named schedule or inline object

export const screen = (config) => ({
  duration: 10000,
  data_url: `${config.BASE}/data/ha/sensor.living_room_temperature`,
  layers: [
    { type: 'icon',  name: 'thermometer', x: 0, y: 0 },
    { type: 'native', label: '{{state}}°', x: 9, y: 1, color: 0x00AAFF },
  ],
});
```

Any Home Assistant entity is available via `{{BASE}}/data/ha/<entity_id>`.

---

## Device API

The ESP32 exposes a small HTTP API on port 80:

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Config form (WiFi, server URL) |
| `/info` | GET | Firmware version, chip, IP, RSSI |
| `/sensors` | GET | Temperature, humidity, light |
| `/status` | GET | Uptime, screen index |
| `/notify` | POST | Push a notification |
| `/timer` | POST/DELETE/GET | Start/cancel/query timer |
| `/gif` | GET | Animated GIF preview of a screen |
| `/framebuffer` | GET | Raw 768-byte RGB framebuffer |

The device is also reachable at **`thinclock.local`** via mDNS.

---

## Development

```bash
npm run dev       # Start server with file watching
npm test          # Run tests
npm run spec      # Spec compliance checker
npm run build     # Compile firmware
npm run flash     # Flash to connected device
npm run monitor   # Serial monitor
```

---

## License

MIT
