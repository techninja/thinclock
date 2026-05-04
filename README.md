# thinclock

Thin, config-driven ESP32 firmware for LED matrix displays. JSON in, pixels out.

The firmware is intentionally dumb — it fetches JSON from a URL and renders
what it's told. The server is smart — screen definitions are composable JS
modules that can pull data from any source.

## Quick Start

```bash
npm install
cp .env.example .env   # edit with your WiFi + settings
npm run dev            # start config server
npm run flash          # build & upload firmware
```

## Architecture

```
thinclock/
├── firmware/          # ESP32 PlatformIO project (thin renderer)
├── server/            # Node.js config server (smart brain)
│   ├── screens/       # Screen modules (JS) — add your own!
│   ├── adapters/      # Data adapters (HA, MQTT, etc.)
│   └── lib/           # Shared utilities
├── homeassistant/     # HA custom component
│   └── custom_components/thinclock/
└── docs/              # Format spec & examples
```

## Screen Modules

Each screen is a JS file in `server/screens/` that exports:

```js
exports.name = 'My Screen';
exports.enabled = true;

exports.icons = { /* optional icon definitions */ };

exports.screen = (config) => ({
  duration: 10000,
  layers: [
    { type: 'clock', x: 0, y: 0, color: 'FFFFFF' },
    { type: 'particles', gravity: 5, edge: 'die', ... },
  ],
});

exports.routes = (app, config) => { /* optional data endpoints */ };
```

## Layer Types

| Type | Description |
|------|-------------|
| `icon` | Static/animated sprite |
| `text` | Scrollable text with data placeholders |
| `native` | Pixel-perfect digits (3×5 or 5×7) |
| `clock` | Native time display |
| `gauge` | Procedural value indicator (vbar/hbar/dot) |
| `particles` | Particle system (bouncing, rain, fireworks, etc.) |
| `pixels` | Pattern-based pixel draws (week_dots, etc.) |

## Scripts

| Command | Description |
|---------|-------------|
| `npm run dev` | Start config server |
| `npm run build` | Compile firmware |
| `npm run flash` | Compile and upload |
| `npm run flash:force` | Erase + upload |
| `npm run monitor` | Serial monitor |

## Hardware

Ulanzi TC001 — ESP32-D0WD, 32×8 WS2812 matrix, GPIO 32 data pin.

## License

MIT
