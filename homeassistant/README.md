# ThinClock Home Assistant Integration

Custom component for integrating ThinClock LED matrix displays with Home Assistant.

## Architecture

```
Home Assistant
  ├── thinclock integration (this component)
  │     ├── Auto-discovers thinclock devices on network
  │     ├── Exposes HA entities as data sources
  │     └── Provides UI for screen configuration
  │
  └── thinclock server (Node.js, runs as HA add-on or standalone)
        ├── Serves config JSON to the device
        ├── Screen modules (JS) define what to display
        └── HA adapter pulls entity states for screen data
```

## Installation

### As Custom Component
1. Copy `custom_components/thinclock/` to your HA `config/custom_components/` directory
2. Restart Home Assistant
3. Go to Settings → Integrations → Add Integration → ThinClock
4. Enter the IP address of your thinclock device

### As Add-on (planned)
The thinclock server will be available as an HA add-on that:
- Runs the Node.js config server
- Auto-connects to HA's WebSocket API
- Provides a UI panel for managing screens
- Discovers thinclock devices via mDNS

## Services (planned)

- `thinclock.set_screen` — Switch to a specific screen
- `thinclock.notify` — Show a temporary notification
- `thinclock.set_brightness` — Adjust brightness

## Sensors (planned)

The integration exposes the device's onboard sensors:
- `sensor.thinclock_temperature`
- `sensor.thinclock_humidity`
- `sensor.thinclock_light`
