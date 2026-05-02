# thinclock

Thin, config-driven ESP32 firmware for the Ulanzi TC001 (32×8 LED matrix).
JSON in, pixels out.

The firmware is intentionally dumb — it fetches JSON from a URL and renders
what it's told. Any system that can serve JSON can drive the display.

## Quick Start

```bash
npm install
cp .env.example .env   # edit with your WiFi credentials
npm run dev             # start config server
```

In another terminal:
```bash
npm run flash           # build & upload firmware
npm run monitor         # open serial monitor
```

Paste the JSON line the server printed into the serial monitor. Device reboots
and starts rendering.

## Scripts

| Command            | Description                              |
|--------------------|------------------------------------------|
| `npm run dev`      | Start the config/data server             |
| `npm run build`    | Compile firmware only                    |
| `npm run flash`    | Compile and upload firmware              |
| `npm run flash:force` | Erase flash, then compile and upload  |
| `npm run erase`    | Erase ESP32 flash completely             |
| `npm run monitor`  | Open serial monitor (115200 baud)        |

## How It Works

```
Config URL → screens[] → for each screen:
  label: "It is {temp}F"      ← text with placeholders
  data_url: "http://.../temp"  ← returns {"temp": 72}
  → renders: "It is 72F"
  → scrolls if text overflows
  → next screen after duration
```

Falls back to a basic clock when unconfigured.

## Project Structure

```
thinclock/
├── firmware/          # ESP32 PlatformIO project
│   ├── src/           # Firmware source
│   ├── include/       # Headers
│   └── platformio.ini
├── server/            # Node.js config & data server
│   └── index.js
├── docs/              # Format spec & examples
│   ├── FORMAT.md
│   └── example_config.json
├── .env.example       # Environment template
└── package.json       # Project orchestrator
```

## JSON Format

See [docs/FORMAT.md](docs/FORMAT.md) for the complete specification, scroll
modes, data endpoint format, and integration examples.

## Hardware

Ulanzi TC001 — ESP32-D0WD, 32×8 WS2812 matrix, GPIO 32 data pin.

## License

MIT
