# thinclock JSON Config Format

The device GETs a config URL every 30 seconds and renders whatever it receives.

## Config Structure

```json
{
  "settings": { ... },
  "screens": [ ... ],
  "icons": { ... }
}
```

## Settings

```json
{
  "brightness": 40,
  "timezone": -5,
  "scroll_speed": 50,
  "time_format": "12h",
  "temp_unit": "F",
  "event_url": "http://server:3232/event",
  "buttons": "navigate",
  "allow_beep": true,
  "transition": 12
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `brightness` | int | 40 | LED brightness (0-255) |
| `timezone` | int | 0 | UTC offset in hours |
| `scroll_speed` | int | 50 | Default scroll speed (ms/pixel) |
| `time_format` | string | `"24h"` | `"12h"` or `"24h"` |
| `temp_unit` | string | `"C"` | `"C"` or `"F"` |
| `event_url` | string | `""` | URL to POST button events |
| `buttons` | string | `"navigate"` | `"navigate"` or `"events"` |
| `allow_beep` | bool | true | Global buzzer enable |
| `transition` | int | 12 | Crossfade speed (higher = faster) |

## Screens

Each screen is an ordered array of layers with a duration and optional data source.

```json
{
  "duration": 10000,
  "data_url": "http://server/data/endpoint",
  "layers": [ ... ]
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `duration` | int | 5000 | ms to display before cycling |
| `data_url` | string | `""` | JSON endpoint for `{placeholder}` values |
| `layers` | array | required | Ordered render layers |

## Layers

See [SCREENS.md](SCREENS.md) for complete layer type documentation.

Every layer supports:

```json
{
  "type": "...",
  "x": 0, "y": 0,
  "opacity": 255,
  "blend": "normal",
  "tweens": []
}
```

## Icons

Named icon definitions referenced by layers.

```json
{
  "icons": {
    "my_icon": {
      "width": 8, "height": 8, "fps": 0,
      "data": ["hex_rgb888_string_per_frame"],
      "remap_key": "FF0000",
      "remap_value_key": "temperature",
      "remap_range": { "min": 0, "max": 100, "stops": [...] }
    }
  }
}
```

### Icon Data Format

- RGB888 hex string, row-major: `"FF0000FF000000FF00..."` (3 bytes per pixel)
- `000000` = transparent (black)
- Multiple strings in `data` array = animation frames
- `fps` > 0 enables frame cycling

### Color Remapping

Set `remap_key` to a hex color present in the icon. At render time, all pixels matching that color are replaced with a color interpolated from `remap_range` based on the value of `remap_value_key` from the screen's data.

## Color Range Format

Used by gauges, gradients, particles, and icon remapping:

```json
{
  "min": 0, "max": 100,
  "stops": [[0, "0000FF"], [0.5, "00FF00"], [1, "FF0000"]]
}
```

Each stop is `[position (0-1), "RRGGBB"]`. Colors interpolate linearly between stops.

## Data Endpoints

Screen `data_url` should return flat JSON:

```json
{"temperature": 72, "humidity": 45, "status": "OK"}
```

Keys match `{placeholder}` tokens in text/native layer labels.

### Special URLs

- `self://sensors` — device's onboard temp/humidity/light (no network)
- `self://ping` — device HTTP ping to 1.1.1.1 (latency + RSSI)

## Example: Full Clock Screen

```json
{
  "duration": 30000,
  "data_url": "http://server:3232/data/datetime",
  "layers": [
    { "type": "gradient", "x": 1, "y": 2, "width": 7, "height": 6,
      "direction": "vertical",
      "colors": { "min": 0, "max": 1, "stops": [[0,"FFFFFF"],[1,"DDCCAA"]] } },
    { "type": "icon", "name": "calendar", "x": 0, "y": 0 },
    { "type": "native", "label": "{day}", "x": 3, "y": 2, "color": "000000" },
    { "type": "clock", "format": "12h", "x": 12, "y": 1, "color": "4488FF",
      "large": false, "spacing": 1 },
    { "type": "pixels", "pattern": "week_dots", "x": 10, "y": 7,
      "color": "4488FF", "dim_color": "112244" }
  ]
}
```

## Example: Particles + Icon Composited

```json
{
  "duration": 12000,
  "layers": [
    { "type": "gradient", "x": 0, "y": 0, "width": 32, "height": 6,
      "direction": "vertical",
      "colors": { "min": 0, "max": 1, "stops": [[0,"030308"],[1,"101028"]] } },
    { "type": "icon", "name": "tree", "x": 22, "y": 0 },
    { "type": "particles", "gravity": 3, "edge": "die", "blend": "add",
      "colors": { "min": 0, "max": 1, "stops": [[0,"000000"],[0.3,"444466"],[0.7,"444466"],[1,"000000"]] },
      "emitters": [
        { "x": -1, "y": -1, "vx_min": 0, "vx_max": 0, "vy_min": 0, "vy_max": 0,
          "rate": 1.5, "life_min": 2000, "life_max": 4000, "size": 1 }
      ] }
  ]
}
```
