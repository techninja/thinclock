# Alert System

Screen modules can declare alert conditions that the server evaluates automatically. When conditions are met, notifications are pushed to the device.

## Defining Alerts

Add an `exports.alerts` array to any screen module:

```js
exports.alerts = [
  {
    id: 'unique_alert_id',
    condition: (history) => {
      // history = array of data objects with _ts timestamp
      // return true to fire the alert
      const current = history[history.length - 1];
      return current && current.value > 100;
    },
    message: 'Alert text shown on device',
    color: 'FF0000',        // notification dot color
    beep: 'single',         // 'single', 'alert', 'none'
    cooldown: 300000,       // ms before can re-trigger (default 5 min)
  },
];
```

## Pushing Data

Alerts evaluate against a rolling history of data points. Push data from your screen's routes:

```js
exports.routes = (app, config) => {
  // When you fetch/compute data, push it to the alert engine
  const data = { temperature: 95, humidity: 20 };
  config.pushAlert('my-screen-id', data);
};
```

The `screen-id` must match your module's filename (without `.js`).

## Condition Patterns

### Threshold crossing

```js
condition: (history) => {
  if (history.length < 2) return false;
  const prev = history[history.length - 2];
  const curr = history[history.length - 1];
  return prev.value <= 100 && curr.value > 100;
}
```

### Sustained condition (N consecutive readings)

```js
condition: (history) => {
  if (history.length < 5) return false;
  return history.slice(-5).every(d => d.ping > 600);
}
```

### State change

```js
condition: (history) => {
  if (history.length < 2) return false;
  const prev = history[history.length - 2];
  const curr = history[history.length - 1];
  return prev.status === 'closed' && curr.status === 'open';
}
```

### Time-based (hasn't updated in X minutes)

```js
condition: (history) => {
  if (history.length === 0) return false;
  const last = history[history.length - 1];
  return Date.now() - last._ts > 600000; // 10 min stale
}
```

## Built-in Alerts

| Module | Alert ID | Condition | Cooldown |
|--------|----------|-----------|----------|
| network | `internet_slow` | 5 consecutive pings > 600ms | 5 min |
| network | `internet_down` | 3 consecutive failed pings | 1 min |
| weather | `rain_started` | Condition changed to precipitation | 30 min |
| weather | `severe_weather` | Thunderstorm detected | 1 hour |
| aqi | `aqi_unhealthy` | AQI crossed above 100 | 1 hour |
| aqi | `aqi_dangerous` | AQI above 200 | 1 hour |

## Testing Alerts

The server exposes test endpoints:

```bash
# View registered alerts and state
curl http://server:3232/test/alerts

# Simulate data to trigger conditions
curl -X POST http://server:3232/test/alert \
  -H "Content-Type: application/json" \
  -d '{"screen": "network", "data": {"ping": 0, "status": 0}}'
```

## Server Polling

The server polls every 15 seconds:

- HTTP ping to 1.1.1.1 (measures internet latency)
- Device `/sensors` endpoint (if DEVICE_IP is set)

Weather and AQI data is pushed to the alert engine on their own fetch schedules (10 min and 30 min respectively).

## Configuration

```bash
# .env
DEVICE_IP=192.168.86.60   # Required for server→device notifications
```

Without `DEVICE_IP`, alerts evaluate but can't push notifications to the device.
