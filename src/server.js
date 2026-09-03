import express from 'express';
import path from 'path';
import os from 'os';
import http from 'http';
import { fileURLToPath } from 'url';
import ScreenRegistry from './api/lib/registry.js';
import AlertEngine from './api/lib/alerts.js';
import HomeAssistantAdapter from './api/adapters/homeassistant.js';
import {
  registerDeviceRoutes,
  pollDevice,
  generatePreview,
  PreviewCache,
} from './api/lib/device-proxy.js';
import { handleUpgrade, getConnectedDeviceIP } from './api/lib/ws-render.js';
import { registerRoutes } from './api/routes.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const app = express();
app.use(express.json());
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'GET,POST,DELETE,OPTIONS');
  res.header('Access-Control-Allow-Headers', 'Content-Type');
  if (req.method === 'OPTIONS') return res.sendStatus(200);
  next();
});
app.use(express.static(__dirname));

const PORT = process.env.PORT || 3232;
const LOCAL_IP = (() => {
  for (const iface of Object.values(os.networkInterfaces()))
    for (const net of iface) if (net.family === 'IPv4' && !net.internal) return net.address;
  return '127.0.0.1';
})();
const BASE = `http://${LOCAL_IP}:${PORT}`;
const getDeviceIP = () => getConnectedDeviceIP() || process.env.DEVICE_IP || null;

const config = {
  BASE: `${BASE}/api`,
  timezone: parseInt(process.env.TIMEZONE) || 0,
  brightness: parseInt(process.env.BRIGHTNESS) || 40,
  time_format: process.env.TIME_FORMAT || '12h',
  temp_unit: process.env.TEMP_UNIT || 'F',
  ha_url: process.env.HA_URL,
  ha_token: process.env.HA_TOKEN,
};

console.log('\nLoading screens:');
const registry = new ScreenRegistry();
await registry.loadDir(path.join(__dirname, 'api/screens'));

const alerts = new AlertEngine({ deviceIP: getDeviceIP(), timezone: config.timezone });
config.alerts = alerts;
config.pushAlert = (id, data) => alerts.pushData(id, data);

console.log('\nAdapters:');
new HomeAssistantAdapter(config).setup(app, config);

const apiRouter = express.Router();
registry.registerRoutes(apiRouter, config);
alerts.registerFromModules(registry.modules);
app.use('/api', apiRouter);

const active = registry.getActiveModules();
console.log(`\nActive screens (${active.length}/${registry.modules.length}):`);
active.forEach((m, i) => console.log(`  ${i}: ${m.name}`));

const nightHours = () => (process.env.NIGHT_HOURS || '').split(',').map(Number).filter(Boolean);
const isNightMode = () =>
  nightHours().includes((new Date().getUTCHours() + config.timezone + 24) % 24);
const getBrightness = () =>
  isNightMode() ? parseInt(process.env.BRIGHTNESS_NIGHT) || 10 : config.brightness;

app.get('/api/config', (req, res) => {
  let { screens, icons } = registry.build(app, config);
  if (isNightMode()) {
    const night = registry.getActiveModules().filter((m) => m.tags.includes('night'));
    if (night.length) {
      screens = night.map((m) => (typeof m.screen === 'function' ? m.screen(config) : m.screen));
      icons = Object.assign({}, ...night.map((m) => m.icons || {}));
    }
  }
  res.json({
    settings: {
      brightness: getBrightness(),
      timezone: config.timezone,
      scroll_speed: 50,
      time_format: config.time_format,
      temp_unit: config.temp_unit,
      event_url: `${BASE}/api/event`,
      buttons: 'navigate',
      allow_beep: process.env.ALLOW_BEEPING !== 'false',
      transition: 12,
    },
    screens,
    icons,
  });
});

registerRoutes(app, registry, alerts, getDeviceIP, PORT);
registerDeviceRoutes(app, getDeviceIP);

const server = http.createServer(app);
server.on('upgrade', handleUpgrade);

const cache = new PreviewCache(path.join(__dirname, '..', 'data', 'preview-cache'));
cache._generate = async (id) => {
  const mod = registry.modules.find((m) => m._id === id);
  if (!mod) return null;
  const { icons } = registry.build(app, config);
  const screen = typeof mod.screen === 'function' ? mod.screen(config) : mod.screen;
  if (!screen) return null;
  const usedIcons = Object.fromEntries(
    (screen.layers || [])
      .filter((l) => l.type === 'icon' && icons[l.name])
      .map((l) => [l.name, icons[l.name]]),
  );
  return generatePreview(getDeviceIP(), screen, usedIcons);
};

app.get('/api/preview/:screenId.gif', (req, res) => cache.serve(req.params.screenId, res));
app.post('/api/preview/regenerate', (req, res) => {
  cache.clear();
  cache.enqueueAll(registry.modules);
  res.json({ ok: true, queued: cache._queue.length });
});

app.get(/^\/(rotation|settings|notify|editor)?(\/.*)?$/, (req, res) =>
  res.sendFile(path.join(__dirname, 'index.html')),
);

server.listen(PORT, () => {
  console.log(`\nthinclock server (mode: ${registry.mode})`);
  console.log(`${'='.repeat(40)}`);
  console.log(`UI:      ${BASE}/`);
  console.log(`API:     ${BASE}/api/config`);
  console.log(`\nSend to device serial:`);
  console.log(
    `{"ssid":"${process.env.WIFI_SSID}","pass":"${process.env.WIFI_PASS}","config_url":"${BASE}/api/config"}`,
  );
  console.log();
  setInterval(() => pollDevice(getDeviceIP(), alerts), 15000);
  pollDevice(getDeviceIP(), alerts);
  setTimeout(() => cache.enqueueAll(registry.modules), 5000);
});
