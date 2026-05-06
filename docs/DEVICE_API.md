# Device HTTP API

The thinclock device runs a lightweight HTTP server on port 80. These endpoints allow any system to interact with the device directly.

## Endpoints

### GET /sensors
Returns onboard sensor readings.

```json
{"temperature": 34.1, "humidity": 31.6, "light": 23, "light_raw": 945, "sensor": true}
```

### GET /status
Returns device state.

```json
{"uptime": 3600, "wifi": -42, "ip": "192.168.86.60", "config_valid": true, "screen": 2}
```

### POST /notify
Add a notification to the queue (max 8).

```bash
curl -X POST http://device/notify \
  -H "Content-Type: application/json" \
  -d '{"text": "Hello!", "color": "FF8800", "beep": "single"}'
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `text` | string | required | Notification message |
| `color` | string | `"FFAA00"` | Indicator dot color (hex RGB) |
| `text_color` | string | `"FFFFFF"` | Text color |
| `beep` | string | `"single"` | `"single"`, `"alert"`, `"none"` |
| `alert_interval` | int | 30000 | ms between alert repeats |

### GET /notify
Returns notification count.

```json
{"count": 3}
```

### DELETE /notify
Clear all notifications.

### POST /timer
Start a countdown timer.

```bash
curl -X POST http://device/timer \
  -d '{"duration": 1500000, "color": "FF8800"}'
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `duration` | int | 60000 | Timer duration in ms |
| `color` | string | `"00AAFF"` | Timer indicator color |

### GET /timer
Returns timer state.

```json
{"active": true, "remaining": 845000, "duration": 1500000}
```

### DELETE /timer
Cancel active timer.

### POST /beep
Trigger a beep pattern.

```bash
# Named patterns
curl -X POST http://device/beep -d '{"type": "single"}'
curl -X POST http://device/beep -d '{"type": "double"}'
curl -X POST http://device/beep -d '{"type": "triple"}'
curl -X POST http://device/beep -d '{"type": "alarm"}'

# Custom pattern: [[frequency_hz, duration_ms, pause_ms], ...]
curl -X POST http://device/beep \
  -d '{"pattern": [[2000,100,50],[1500,100,50],[1000,200,0]]}'
```

## Data Sources (self://)

The device can resolve data locally without HTTP round-trips:

| URL | Returns |
|-----|---------|
| `self://sensors` | `{temperature, humidity, light, light_raw}` |
| `self://ping` | `{ping (ms), status (0/1), rssi}` |

Use these as `data_url` in screen layers for zero-latency sensor display.
