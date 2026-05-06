require('dotenv').config();
const express = require('express');
const path = require('path');
const os = require('os');
const ScreenRegistry = require('./lib/registry');
const HomeAssistantAdapter = require('./adapters/homeassistant');

const app = express();
app.use(express.json());
const PORT = process.env.SERVER_PORT || 3000;

function getLocalIP() {
  const nets = os.networkInterfaces();
  for (const iface of Object.values(nets)) {
    for (const net of iface) {
      if (net.family === 'IPv4' && !net.internal) return net.address;
    }
  }
  return '127.0.0.1';
}

const LOCAL_IP = getLocalIP();
const BASE = `http://${LOCAL_IP}:${PORT}`;

// Shared config passed to all modules
const config = {
  BASE,
  timezone: parseInt(process.env.TIMEZONE) || 0,
  brightness: parseInt(process.env.BRIGHTNESS) || 40,
  time_format: process.env.TIME_FORMAT || '12h',
  temp_unit: process.env.TEMP_UNIT || 'F',
  ha_url: process.env.HA_URL,
  ha_token: process.env.HA_TOKEN,
};

// --- Load screen modules ---
console.log('\nLoading screens:');
const registry = new ScreenRegistry();
registry.loadDir(path.join(__dirname, 'screens'));

// --- Setup adapters ---
console.log('\nAdapters:');
const ha = new HomeAssistantAdapter(config);
ha.setup(app, config);

// --- Alert engine ---
const AlertEngine = require('./lib/alerts');
const alerts = new AlertEngine({ deviceIP: process.env.DEVICE_IP, timezone: config.timezone });
config.alerts = alerts;

// Poll device data and do server-side ping for alerts
async function pollDeviceForAlerts() {
  const deviceIP = process.env.DEVICE_IP;

  // Server-side HTTP ping (same as device does)
  try {
    const start = Date.now();
    const resp = await fetch('http://1.1.1.1/', { signal: AbortSignal.timeout(3000) });
    const ping = Date.now() - start;
    alerts.pushData('network', { ping, status: 1, rssi: 0 });
  } catch (e) {
    alerts.pushData('network', { ping: 0, status: 0, rssi: 0 });
  }

  // Also grab device sensors if available
  if (deviceIP) {
    try {
      const resp = await fetch(`http://${deviceIP}/sensors`, { signal: AbortSignal.timeout(2000) });
      const data = await resp.json();
      alerts.pushData('sensors', data);
    } catch (e) {}
  }
}

// Also allow screens to push data from their own routes
config.pushAlert = (screenId, data) => alerts.pushData(screenId, data);

// --- Register screen routes (once at startup) ---
registry.registerRoutes(app, config);
alerts.registerFromModules(registry.modules);

// Log active screens
const active = registry.getActiveModules();
console.log(`\nActive screens (${active.length}/${registry.modules.length}):`);
active.forEach((m, i) => console.log(`  ${i}: ${m.name}`));

// --- Device config endpoint ---
// --- Night mode ---
function isNightMode() {
  const nightHours = (process.env.NIGHT_HOURS || '')
    .split(',').map(h => parseInt(h.trim())).filter(h => !isNaN(h));
  if (nightHours.length === 0) return false;
  const now = new Date();
  const localHour = (now.getUTCHours() + config.timezone + 24) % 24;
  return nightHours.includes(localHour);
}

function getCurrentBrightness() {
  const nightBrightness = parseInt(process.env.BRIGHTNESS_NIGHT) || 10;
  return isNightMode() ? nightBrightness : config.brightness;
}

app.get('/config', (req, res) => {
  let screens, icons;

  if (isNightMode()) {
    // Night mode: only night-tagged screens
    const nightMods = registry.getActiveModules().filter(m => m.tags.includes('night'));
    screens = [];
    icons = {};
    for (const mod of nightMods) {
      if (mod.icons) Object.assign(icons, mod.icons);
      if (mod.screen) {
        const scr = typeof mod.screen === 'function' ? mod.screen(config) : mod.screen;
        screens.push(scr);
      }
    }
    // Fallback if no night screens
    if (screens.length === 0) {
      ({ screens, icons } = registry.build(app, config));
    }
  } else {
    ({ screens, icons } = registry.build(app, config));
  }

  res.json({
    settings: {
      brightness: getCurrentBrightness(),
      timezone: config.timezone,
      scroll_speed: 50,
      time_format: config.time_format,
      temp_unit: config.temp_unit,
      event_url: `${BASE}/event`,
      buttons: 'navigate',
      allow_beep: process.env.ALLOW_BEEPING !== 'false',
      transition: 12,
    },
    screens,
    icons,
  });
});

// --- Management API ---

// List all screens with status
app.get('/screens', (req, res) => {
  res.json(registry.list());
});

// Enable/disable a screen
app.post('/screens/:id/enable', (req, res) => {
  registry.setEnabled(req.params.id, true);
  console.log(`[screens] ${req.params.id} → enabled`);
  res.json({ ok: true, active: registry.getActiveModules().map(m => m.name) });
});

app.post('/screens/:id/disable', (req, res) => {
  registry.setEnabled(req.params.id, false);
  console.log(`[screens] ${req.params.id} → disabled`);
  res.json({ ok: true, active: registry.getActiveModules().map(m => m.name) });
});

// Get currently active rotation
app.get('/active', (req, res) => {
  const active = registry.getActiveModules();
  res.json(active.map((m, i) => ({
    index: i,
    id: m._id,
    name: m.name,
    contextAction: m.contextAction || 'pause',
  })));
});

// --- Button event handling ---
let paused = false;

app.post('/event', (req, res) => {
  const { event, screen } = req.body;
  console.log(`[event] button=${event} screen=${screen}`);

  if (event === 'select' || event === 'select_long') {
    const action = registry.getContextAction(screen);
    console.log(`[action] screen=${screen} event=${event} action=${action}`);

    if (action === 'pomodoro') {
      const fetch = require('http');
      const postData = JSON.stringify({});
      const r = fetch.request(`http://localhost:${PORT}/pomodoro/toggle`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': postData.length },
      });
      r.write(postData);
      r.end();
      console.log('[action] pomodoro toggle');
    } else if (action === 'pause') {
      paused = !paused;
      console.log(`[action] rotation ${paused ? 'paused' : 'resumed'}`);
    }
    res.json({ ok: true, action, paused });
  } else {
    res.json({ ok: true });
  }
});

// --- Test/simulate alerts ---
app.post('/test/alert', (req, res) => {
  const { screen, data } = req.body;
  if (screen && data) {
    alerts.pushData(screen, data);
    res.json({ ok: true, history: alerts.history[screen]?.length || 0 });
  } else {
    res.json({ error: 'need {screen, data}' });
  }
});

app.get('/test/alerts', (req, res) => {
  res.json({
    registered: alerts.alerts.map(a => ({ id: a.id, module: a._module, cooldown: a.cooldown })),
    history: Object.fromEntries(Object.entries(alerts.history).map(([k,v]) => [k, v.length])),
    lastFired: alerts.lastFired,
  });
});

// --- Start ---
app.listen(PORT, () => {
  console.log(`\nthinclock server (mode: ${registry.mode})`);
  console.log(`${'='.repeat(40)}`);
  console.log(`Config:   ${BASE}/config`);
  console.log(`Screens:  ${BASE}/screens`);
  console.log(`Active:   ${BASE}/active`);
  console.log(`Events:   ${BASE}/event`);
  console.log(`\nSend to device serial:`);
  console.log(`{"ssid":"${process.env.WIFI_SSID}","pass":"${process.env.WIFI_PASS}","config_url":"${BASE}/config"}`);
  console.log();

  // Start polling for alerts (every 15s)
  setInterval(pollDeviceForAlerts, 15000);
  pollDeviceForAlerts();
});
