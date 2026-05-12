import express from 'express';
import path from 'path';
import os from 'os';
import http from 'http';
import { fileURLToPath } from 'url';
import { WebSocketServer } from 'ws';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const app = express();
app.use(express.json());

// CORS for device communication
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'GET,POST,DELETE,OPTIONS');
  res.header('Access-Control-Allow-Headers', 'Content-Type');
  if (req.method === 'OPTIONS') return res.sendStatus(200);
  next();
});

// Serve UI static files from src/ (index.html, components, pages, etc.)
app.use(express.static(__dirname));

const PORT = process.env.PORT || process.env.SERVER_PORT || 3000;

/**
 *
 */
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

// Shared config for screen modules
const config = {
  BASE: `${BASE}/api`,
  timezone: parseInt(process.env.TIMEZONE) || 0,
  brightness: parseInt(process.env.BRIGHTNESS) || 40,
  time_format: process.env.TIME_FORMAT || '12h',
  temp_unit: process.env.TEMP_UNIT || 'F',
  ha_url: process.env.HA_URL,
  ha_token: process.env.HA_TOKEN,
};

// --- Load screen modules ---
import ScreenRegistry from './api/lib/registry.js';
import AlertEngine from './api/lib/alerts.js';
import HomeAssistantAdapter from './api/adapters/homeassistant.js';
import { encodeGif } from './api/lib/gif.js';
import { getSchedules, getSchedule, setSchedule, deleteSchedule } from './api/lib/schedules.js';
import { listCustomScreens, getCustomScreen, saveCustomScreen, deleteCustomScreen } from './api/lib/custom-screens.js';

console.log('\nLoading screens:');
const registry = new ScreenRegistry();
await registry.loadDir(path.join(__dirname, 'api/screens'));

// --- Alert engine ---
const alerts = new AlertEngine({ deviceIP: process.env.DEVICE_IP, timezone: config.timezone });
config.alerts = alerts;
config.pushAlert = (screenId, data) => alerts.pushData(screenId, data);

// --- Adapters ---
console.log('\nAdapters:');
const ha = new HomeAssistantAdapter(config);
ha.setup(app, config);

// --- Register screen routes (prefixed to /api/) ---
// Screen routes register on `app` but their data endpoints need /api/ prefix
const apiRouter = express.Router();
registry.registerRoutes(apiRouter, config);
alerts.registerFromModules(registry.modules);
app.use('/api', apiRouter);

// Log active screens
const active = registry.getActiveModules();
console.log(`\nActive screens (${active.length}/${registry.modules.length}):`);
active.forEach((m, i) => console.log(`  ${i}: ${m.name}`));

// --- Night mode ---
/**
 *
 */
function isNightMode() {
  const nightHours = (process.env.NIGHT_HOURS || '')
    .split(',')
    .map((h) => parseInt(h.trim()))
    .filter((h) => !isNaN(h));
  if (nightHours.length === 0) return false;
  const now = new Date();
  const localHour = (now.getUTCHours() + config.timezone + 24) % 24;
  return nightHours.includes(localHour);
}

/**
 *
 */
function getCurrentBrightness() {
  const nightBrightness = parseInt(process.env.BRIGHTNESS_NIGHT) || 10;
  return isNightMode() ? nightBrightness : config.brightness;
}

// --- API endpoints ---

app.get('/api/config', (req, res) => {
  let screens, icons;

  if (isNightMode()) {
    const nightMods = registry.getActiveModules().filter((m) => m.tags.includes('night'));
    screens = [];
    icons = {};
    for (const mod of nightMods) {
      if (mod.icons) Object.assign(icons, mod.icons);
      if (mod.screen) {
        const scr = typeof mod.screen === 'function' ? mod.screen(config) : mod.screen;
        screens.push(scr);
      }
    }
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
      event_url: `${BASE}/api/event`,
      buttons: 'navigate',
      allow_beep: process.env.ALLOW_BEEPING !== 'false',
      transition: 12,
    },
    screens,
    icons,
  });
});

app.get('/api/screens', (req, res) => {
  res.json(registry.list());
});

app.post('/api/screens/:id/enable', (req, res) => {
  registry.setEnabled(req.params.id, true);
  console.log(`[screens] ${req.params.id} → enabled`);
  res.json({ ok: true, active: registry.getActiveModules().map((m) => m.name) });
});

app.post('/api/screens/:id/disable', (req, res) => {
  registry.setEnabled(req.params.id, false);
  console.log(`[screens] ${req.params.id} → disabled`);
  res.json({ ok: true, active: registry.getActiveModules().map((m) => m.name) });
});

app.get('/api/active', (req, res) => {
  const active = registry.getActiveModules();
  res.json(
    active.map((m, i) => ({
      index: i,
      id: m._id,
      name: m.name,
      contextAction: m.contextAction || 'pause',
    })),
  );
});

app.get('/api/device-ip', (req, res) => {
  res.json({ ip: process.env.DEVICE_IP || null });
});

// --- Schedules API ---
app.get('/api/schedules', (req, res) => {
  res.json(getSchedules());
});

app.get('/api/schedules/:name', (req, res) => {
  const s = getSchedule(req.params.name);
  if (!s) return res.status(404).json({ error: 'not found' });
  res.json(s);
});

app.put('/api/schedules/:name', (req, res) => {
  setSchedule(req.params.name, req.body);
  res.json({ ok: true });
});

app.delete('/api/schedules/:name', (req, res) => {
  deleteSchedule(req.params.name);
  res.json({ ok: true });
});

// --- Custom Screens API ---
app.get('/api/custom-screens', (req, res) => {
  res.json(listCustomScreens());
});

app.get('/api/custom-screens/:id', (req, res) => {
  const screen = getCustomScreen(req.params.id);
  if (!screen) return res.status(404).json({ error: 'not found' });
  res.json(screen);
});

app.put('/api/custom-screens/:id', (req, res) => {
  saveCustomScreen(req.params.id, req.body);
  res.json({ ok: true });
});

app.post('/api/custom-screens', (req, res) => {
  const id = req.body.id || `screen-${Date.now()}`;
  saveCustomScreen(id, req.body);
  res.json({ ok: true, id });
});

app.delete('/api/custom-screens/:id', (req, res) => {
  deleteCustomScreen(req.params.id);
  res.json({ ok: true });
});

// --- Button events ---
let paused = false;

app.post('/api/event', (req, res) => {
  const { event, screen } = req.body;
  console.log(`[event] button=${event} screen=${screen}`);

  if (event === 'select' || event === 'select_long') {
    const action = registry.getContextAction(screen);
    console.log(`[action] screen=${screen} event=${event} action=${action}`);

    if (action === 'pomodoro') {
      const postData = JSON.stringify({});
      const r = http.request(`http://localhost:${PORT}/api/pomodoro/toggle`, {
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
app.post('/api/test/alert', (req, res) => {
  const { screen, data } = req.body;
  if (screen && data) {
    alerts.pushData(screen, data);
    res.json({ ok: true, history: alerts.history[screen]?.length || 0 });
  } else {
    res.json({ error: 'need {screen, data}' });
  }
});

app.get('/api/test/alerts', (req, res) => {
  res.json({
    registered: alerts.alerts.map((a) => ({ id: a.id, module: a._module, cooldown: a.cooldown })),
    history: Object.fromEntries(Object.entries(alerts.history).map(([k, v]) => [k, v.length])),
    lastFired: alerts.lastFired,
  });
});

// --- Device polling for alerts ---
/**
 *
 */
async function pollDeviceForAlerts() {
  const deviceIP = process.env.DEVICE_IP;
  try {
    const start = Date.now();
    const resp = await fetch('http://1.1.1.1/', { signal: AbortSignal.timeout(3000) });
    const ping = Date.now() - start;
    alerts.pushData('network', { ping, status: 1, rssi: 0 });
  } catch (e) {
    alerts.pushData('network', { ping: 0, status: 0, rssi: 0 });
  }
  if (deviceIP) {
    try {
      const resp = await fetch(`http://${deviceIP}/sensors`, { signal: AbortSignal.timeout(2000) });
      const data = await resp.json();
      alerts.pushData('sensors', data);
    } catch (e) {}
  }
}

// --- Device proxy — forward requests to ESP32 ---
const DEVICE_IP = process.env.DEVICE_IP;

// Binary proxy for framebuffer (raw 768 bytes RGB)
app.get('/api/device/framebuffer', async (req, res) => {
  if (!DEVICE_IP) return res.status(503).end();
  try {
    const resp = await fetch(`http://${DEVICE_IP}/framebuffer`, { signal: AbortSignal.timeout(3000) });
    const buf = Buffer.from(await resp.arrayBuffer());
    res.set('Content-Type', 'application/octet-stream');
    res.set('Cache-Control', 'no-store');
    res.send(buf);
  } catch (e) {
    res.status(502).end();
  }
});

// GIF proxy: animated GIF of a screen
app.get('/api/device/gif', async (req, res) => {
  if (!DEVICE_IP) return res.status(503).end();
  const { screen = 0, seconds = 2 } = req.query;
  try {
    const resp = await fetch(
      `http://${DEVICE_IP}/gif?screen=${screen}&seconds=${seconds}`,
      { signal: AbortSignal.timeout(30000) }
    );
    res.set('Content-Type', 'image/gif');
    res.set('Cache-Control', 'public, max-age=60');
    const buf = Buffer.from(await resp.arrayBuffer());
    res.send(buf);
  } catch (e) {
    res.status(502).end();
  }
});

// Preview proxy: stream N rendered frames for a screen
app.get('/api/device/preview', async (req, res) => {
  if (!DEVICE_IP) return res.status(503).end();
  const { screen = 0, frames = 30 } = req.query;
  try {
    const resp = await fetch(
      `http://${DEVICE_IP}/preview?screen=${screen}&frames=${frames}`,
      { signal: AbortSignal.timeout(15000) }
    );
    res.set('Content-Type', 'application/octet-stream');
    res.set('X-Frames', resp.headers.get('X-Frames') || frames);
    res.set('X-Frame-Ms', resp.headers.get('X-Frame-Ms') || '20');
    res.set('Cache-Control', 'public, max-age=60');
    const buf = Buffer.from(await resp.arrayBuffer());
    res.send(buf);
  } catch (e) {
    res.status(502).end();
  }
});

// Render proxy: POST arbitrary layers, get frames back
app.post('/api/device/render', async (req, res) => {
  if (!DEVICE_IP) return res.status(503).end();
  try {
    const resp = await fetch(`http://${DEVICE_IP}/render`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(req.body),
      signal: AbortSignal.timeout(15000),
    });
    res.set('Content-Type', 'application/octet-stream');
    res.set('X-Frames', resp.headers.get('X-Frames') || '1');
    res.set('X-Frame-Ms', resp.headers.get('X-Frame-Ms') || '20');
    const buf = Buffer.from(await resp.arrayBuffer());
    res.send(buf);
  } catch (e) {
    res.status(502).end();
  }
});

app.get('/api/device/:endpoint', async (req, res) => {
  if (!DEVICE_IP) return res.json({});
  try {
    const resp = await fetch(`http://${DEVICE_IP}/${req.params.endpoint}`, { signal: AbortSignal.timeout(3000) });
    const data = await resp.json();
    res.json(data);
  } catch (e) {
    res.json({});
  }
});

app.post('/api/device/:endpoint', async (req, res) => {
  if (!DEVICE_IP) return res.status(503).json({ error: 'No device IP' });
  try {
    const resp = await fetch(`http://${DEVICE_IP}/${req.params.endpoint}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(req.body),
      signal: AbortSignal.timeout(3000),
    });
    const data = await resp.json().catch(() => ({ ok: true }));
    res.json(data);
  } catch (e) {
    res.status(502).json({ error: 'Device unreachable' });
  }
});

// --- WebSocket: device connection + browser framebuffer stream ---
const server = http.createServer(app);
const wssBrowser = new WebSocketServer({ noServer: true });
const wssDevice = new WebSocketServer({ noServer: true });

// Device WS connection state
let deviceWs = null;
let renderQueue = [];
let currentJob = null;
let jobFrames = [];

// Handle device connection
wssDevice.on('connection', (ws) => {
  console.log('[ws/device] Device connected');
  deviceWs = ws;

  ws.on('message', (data, isBinary) => {
    if (isBinary) {
      // Binary = raw RGB frame (768 bytes)
      if (currentJob) {
        jobFrames.push(Buffer.from(data));
        if (jobFrames.length >= currentJob.frames) {
          finishJob();
        }
      } else {
        // Live framebuffer — forward to all browser clients
        for (const client of wssBrowser.clients) {
          if (client.readyState === 1) client.send(data);
        }
      }
    } else {
      // Text = status messages
      try {
        const msg = JSON.parse(data.toString());
        if (msg.type === 'done') finishJob();
        else if (msg.type === 'error') failJob(msg.msg);
        else if (msg.type === 'busy') failJob('device busy');
      } catch (e) {}
    }
  });

  ws.on('close', () => {
    console.log('[ws/device] Device disconnected');
    deviceWs = null;
    if (currentJob) failJob('device disconnected');
  });
});

function finishJob() {
  if (!currentJob) return;
  currentJob.resolve(jobFrames);
  currentJob = null;
  jobFrames = [];
  processQueue();
}

function failJob(reason) {
  if (!currentJob) return;
  currentJob.reject(new Error(reason));
  currentJob = null;
  jobFrames = [];
  processQueue();
}

function processQueue() {
  if (currentJob || renderQueue.length === 0 || !deviceWs) return;
  currentJob = renderQueue.shift();
  jobFrames = [];
  deviceWs.send(JSON.stringify(currentJob.command));
}

/**
 * Queue a render job. Returns promise that resolves with array of frame Buffers.
 */
function queueRender(command) {
  return new Promise((resolve, reject) => {
    const job = { command, frames: command.frames || 30, resolve, reject };
    // Timeout after 30s
    job.timeout = setTimeout(() => failJob('timeout'), 30000);
    const origResolve = resolve;
    const origReject = reject;
    job.resolve = (frames) => { clearTimeout(job.timeout); origResolve(frames); };
    job.reject = (err) => { clearTimeout(job.timeout); origReject(err); };
    renderQueue.push(job);
    processQueue();
  });
}

// Browser framebuffer WS — just forwards live frames from device
wssBrowser.on('connection', () => {});

// Upgrade handler — route WS connections to the right server
server.on('upgrade', (req, socket, head) => {
  if (req.url === '/ws/device') {
    wssDevice.handleUpgrade(req, socket, head, (ws) => wssDevice.emit('connection', ws, req));
  } else if (req.url === '/ws/framebuffer') {
    wssBrowser.handleUpgrade(req, socket, head, (ws) => wssBrowser.emit('connection', ws, req));
  } else {
    socket.destroy();
  }
});

// --- Preview endpoint with disk cache + background generation ---
import fs from 'fs';
const CACHE_DIR = path.join(__dirname, '..', 'data', 'preview-cache');
fs.mkdirSync(CACHE_DIR, { recursive: true });

let previewGenerating = false;
const previewQueue = [];

async function generatePreview(screenId) {
  if (!DEVICE_IP) return null;
  const mod = registry.modules.find(m => m._id === screenId);
  if (!mod) return null;
  const { icons } = registry.build(app, config);
  const screen = typeof mod.screen === 'function' ? mod.screen(config) : mod.screen;
  if (!screen) return null;
  const usedIcons = {};
  for (const layer of screen.layers || []) {
    if (layer.type === 'icon' && layer.name && icons[layer.name]) {
      usedIcons[layer.name] = icons[layer.name];
    }
  }
  try {
    const resp = await fetch(`http://${DEVICE_IP}/gif`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ layers: screen.layers, data_url: screen.data_url || '', icons: usedIcons, seconds: 1, scale: 5, gap: 1, gamma: 18 }),
      signal: AbortSignal.timeout(30000),
    });
    if (!resp.ok) return null;
    return Buffer.from(await resp.arrayBuffer());
  } catch (e) { return null; }
}

async function processPreviewQueue() {
  if (previewGenerating || previewQueue.length === 0) return;
  previewGenerating = true;
  const screenId = previewQueue.shift();
  console.log(`[preview] generating ${screenId} (${previewQueue.length} remaining)`);
  const buf = await generatePreview(screenId);
  if (buf && buf.length > 10) {
    fs.writeFileSync(path.join(CACHE_DIR, `${screenId}.gif`), buf);
  }
  await new Promise(r => setTimeout(r, 500));
  previewGenerating = false;
  processPreviewQueue();
}

function queueAllPreviews() {
  for (const mod of registry.modules) {
    const cached = path.join(CACHE_DIR, `${mod._id}.gif`);
    if (!fs.existsSync(cached)) {
      if (!previewQueue.includes(mod._id)) previewQueue.push(mod._id);
    }
  }
  processPreviewQueue();
}

app.get('/api/preview/:screenId.gif', (req, res) => {
  const { screenId } = req.params;
  const cached = path.join(CACHE_DIR, `${screenId}.gif`);
  if (fs.existsSync(cached)) {
    res.set('Content-Type', 'image/gif');
    res.set('Cache-Control', 'public, max-age=300');
    res.sendFile(cached);
  } else {
    // Queue generation and return placeholder
    if (!previewQueue.includes(screenId)) {
      previewQueue.push(screenId);
      processPreviewQueue();
    }
    res.status(202).json({ status: 'generating' });
  }
});

// Regenerate all previews
app.post('/api/preview/regenerate', (req, res) => {
  // Clear cache
  for (const f of fs.readdirSync(CACHE_DIR)) fs.unlinkSync(path.join(CACHE_DIR, f));
  queueAllPreviews();
  res.json({ ok: true, queued: previewQueue.length });
});

// --- SPA fallback — serve index.html for client-side routes ---
app.get(/^\/(rotation|settings|notify|editor)?(\/.*)?$/, (req, res) => {
  res.sendFile(path.join(__dirname, 'index.html'));
});

// --- Start ---
server.listen(PORT, () => {
  console.log(`\nthinclock server (mode: ${registry.mode})`);
  console.log(`${'='.repeat(40)}`);
  console.log(`UI:       ${BASE}/`);
  console.log(`API:      ${BASE}/api/config`);
  console.log(`Screens:  ${BASE}/api/screens`);
  console.log(`\nSend to device serial:`);
  console.log(
    `{"ssid":"${process.env.WIFI_SSID}","pass":"${process.env.WIFI_PASS}","config_url":"${BASE}/api/config"}`,
  );
  console.log();

  setInterval(pollDeviceForAlerts, 15000);
  pollDeviceForAlerts();

  // Start generating preview cache in background
  setTimeout(() => queueAllPreviews(), 5000);
});
