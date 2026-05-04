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

// --- Register screen routes (once at startup) ---
registry.registerRoutes(app, config);

// --- Build config endpoint ---
app.get('/config', (req, res) => {
  const { screens, icons } = registry.build(app, config);

  res.json({
    settings: {
      brightness: config.brightness,
      timezone: config.timezone,
      scroll_speed: 50,
      time_format: config.time_format,
      temp_unit: config.temp_unit,
      event_url: `${BASE}/event`,
      buttons: 'navigate',
      transition: 12,
    },
    screens,
    icons,
  });
});

// --- Management endpoints ---

// List screens with enable/disable status
app.get('/screens', (req, res) => {
  res.json(registry.list());
});

// Enable/disable a screen
app.post('/screens/:file', (req, res) => {
  const { enabled } = req.body;
  registry.setEnabled(req.params.file, enabled);
  console.log(`[screens] ${req.params.file} → ${enabled ? 'enabled' : 'disabled'}`);
  res.json({ ok: true });
});

// Button event callback
app.post('/event', (req, res) => {
  const { event, screen } = req.body;
  console.log(`[event] button=${event} screen=${screen}`);
  res.json({ ok: true });
});

// --- Start ---
app.listen(PORT, () => {
  console.log(`\nthinclock server`);
  console.log(`================`);
  console.log(`Config:   ${BASE}/config`);
  console.log(`Screens:  ${BASE}/screens`);
  console.log(`Events:   ${BASE}/event`);
  console.log(`\nSend to device serial:`);
  console.log(`{"ssid":"${process.env.WIFI_SSID}","pass":"${process.env.WIFI_PASS}","config_url":"${BASE}/config"}`);
  console.log();
});
