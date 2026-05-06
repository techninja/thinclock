# Writing Screen Modules

Screen modules are JS files in `server/screens/`. The server auto-discovers them on startup.

## Minimal Example

```js
// server/screens/my-screen.js
exports.name = 'My Screen';
exports.enabled = true;

exports.screen = () => ({
  duration: 10000,
  layers: [
    { type: 'native', label: 'HELLO', x: 4, y: 1, color: '00FF00', large: false, spacing: 1 },
  ],
});
```

## Full Module Structure

```js
const { makeIcon } = require('../lib/icons');

exports.name = 'My Screen';        // Display name
exports.enabled = true;             // Active by default
exports.priority = 5;               // Higher = earlier in rotation (0-10)
exports.tags = ['utility'];         // For filtering (e.g., 'night' for night mode)
exports.contextAction = 'pause';    // Middle long-press action name
exports.schedule = {                // When this screen is available
  months: [12, 1, 2],              // Only these months
  hours: [18, 19, 20, 21],         // Only these hours
  days: [0, 6],                    // Only these weekdays (0=Sun)
  dateRange: [1201, 1225],         // MMDD range
};

exports.icons = {
  my_icon: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([...], 0xFF, 0x00, 0x00)],
  },
};

exports.screen = (config) => ({
  duration: 10000,
  data_url: `${config.BASE}/data/my-endpoint`,
  layers: [...],
});

exports.routes = (app, config) => {
  app.get('/data/my-endpoint', (req, res) => {
    res.json({ value: 42 });
  });
};

exports.alerts = [
  {
    id: 'my_alert',
    condition: (history) => history[history.length - 1]?.value > 100,
    message: 'Value too high!',
    color: 'FF0000',
    beep: 'single',
    cooldown: 300000,
  },
];
```

## Layer Types

### icon
```json
{ "type": "icon", "name": "icon_name", "x": 0, "y": 0 }
```

### text
```json
{ "type": "text", "label": "{value} units", "x": 0, "y": 0,
  "color": "FFFFFF", "scroll": "auto", "scroll_speed": 50, "fade_edge": 2 }
```
Scroll modes: `none`, `auto`, `bounce`, `left`

### native
Pixel-perfect digits and letters (3×5 or 5×7 font).
```json
{ "type": "native", "label": "{temp}F", "x": 0, "y": 0,
  "color": "FF8800", "large": false, "spacing": 1 }
```
Supports: 0-9, A-Z, a-z, `:`, `.`, `!`, `-`, `%`, `F`/`C` (degree symbol)

### clock
Native time or timer display.
```json
{ "type": "clock", "format": "12h", "x": 0, "y": 0,
  "color": "4488FF", "large": false, "spacing": 1 }
```
Formats: `"12h"`, `"24h"`, `"timer"` (device countdown)

### particles
```json
{ "type": "particles", "gravity": 10, "edge": "die", "blend": "add", "opacity": 128,
  "colors": { "min": 0, "max": 1, "stops": [[0,"FFFFFF"],[1,"000000"]] },
  "emitters": [
    { "x": -1, "y": 0, "vx_min": -1, "vx_max": 1, "vy_min": 2, "vy_max": 5,
      "rate": 8, "life_min": 500, "life_max": 1500, "size": 1, "rocket": false }
  ],
  "mask": "################################......" }
```
- `edge`: `"die"`, `"bounce"`, `"wrap"`
- `x/y: -1` = random position
- `rocket: true` = bursts into ring at apex
- `mask`: 32×8 chars, `#` = solid boundary

### gauge
```json
{ "type": "gauge", "style": "vbar", "x": 0, "y": 0, "width": 8, "height": 8,
  "value_key": "temperature",
  "range": { "min": 0, "max": 100, "stops": [[0,"0000FF"],[1,"FF0000"]] } }
```
Styles: `"vbar"`, `"hbar"`, `"dot"`

### gradient
```json
{ "type": "gradient", "x": 0, "y": 0, "width": 32, "height": 8,
  "direction": "diagonal", "opacity": 180,
  "colors": { "min": 0, "max": 1, "stops": [[0,"FF0000"],[0.5,"000000"],[1,"0000FF"]] } }
```
Directions: `"horizontal"`, `"vertical"`, `"diagonal"`

### pixels
```json
{ "type": "pixels", "pattern": "dots", "x": 0, "y": 7, "color": "FFFFFF",
  "points": [[0,0],[1,0],[2,0],[4,0],[5,0]] }
```
Patterns: `"week_dots"`, `"vline"`, `"dots"` (arbitrary points)

## Universal Layer Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `x`, `y` | int | 0 | Position |
| `opacity` | int | 255 | 0-255, applied after render |
| `blend` | string | `"normal"` | `"normal"` or `"add"` (additive) |
| `tweens` | array | [] | Animation definitions |

## Tweens

```json
"tweens": [
  { "prop": "x", "from": 0, "to": 24, "duration": 3000,
    "easing": "sine", "loop": "pingpong", "delay": 0 }
]
```

| Property | Values |
|----------|--------|
| `prop` | `"x"`, `"y"`, `"opacity"` |
| `easing` | `"linear"`, `"sine"`, `"ease_in"`, `"ease_out"`, `"ease_in_out"` |
| `loop` | `"none"`, `"repeat"`, `"pingpong"` |

## Icon Helpers

```js
const { makeIcon, makeLine, colorRange } = require('../lib/icons');

// ASCII art icon: '#' = primary color, 'W' = white, '.' = transparent
const icon = makeIcon([
  '..##..',
  '.####.',
  '######',
  '.####.',
  '..##..',
], 0xFF, 0x00, 0x00);

// Custom color map
const icon2 = makeIcon([...], 0xFF, 0x00, 0x00, { G: '00AA00', Y: 'FFCC00' });

// Full-width line
const ground = makeLine(32, 'FFFFFF');
```

## Icon Color Remapping

Icons can dynamically recolor based on data values:

```js
exports.icons = {
  thermometer: {
    width: 8, height: 8, fps: 0,
    remap_key: 'FF0000',           // color to replace
    remap_value_key: 'temperature', // data key to read
    remap_range: { min: 0, max: 50, stops: [[0,'0000FF'],[0.5,'00FF00'],[1,'FF0000']] },
    data: ['...hex...'],
  },
};
```

## Config Object (passed to screen functions)

```js
exports.screen = (config) => {
  config.BASE        // server base URL
  config.timezone    // UTC offset
  config.time_format // '12h' or '24h'
  config.temp_unit   // 'F' or 'C'
  config.pushAlert   // function(screenId, data) — push to alert engine
};
```
