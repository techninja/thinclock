require('dotenv').config();
const express = require('express');
const os = require('os');

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

// Helper: create 8x8 icon from a simple pixel map
// Each row is 8 chars, '#' = colored pixel, '.' = black
function makeIcon(rows, r, g, b) {
  let hex = '';
  for (const row of rows) {
    for (const ch of row.padEnd(8, '.')) {
      if (ch === '#') {
        hex += [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
      } else {
        hex += '000000';
      }
    }
  }
  return hex;
}

// Simple 8x8 icons
const ICONS = {
  thermo: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '...##...',
      '..#..#..',
      '..#.##..',
      '..#..#..',
      '..#.##..',
      '.#.##.#.',
      '.#.##.#.',
      '..####..',
    ], 0xFF, 0x44, 0x00)],
  },
  sun: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '#..##..#',
      '.######.',
      '.######.',
      '########',
      '########',
      '.######.',
      '.######.',
      '#..##..#',
    ], 0xFF, 0xFF, 0x00)],
  },
  heart: {
    width: 8, height: 8, fps: 2,
    data: [
      makeIcon([
        '.##.##..',
        '########',
        '########',
        '.######.',
        '..####..',
        '...##...',
        '........',
        '........',
      ], 0xFF, 0x00, 0x44),
      makeIcon([
        '........',
        '..#..#..',
        '.######.',
        '.######.',
        '..####..',
        '...##...',
        '........',
        '........',
      ], 0xFF, 0x00, 0x44),
    ],
  },
};

// Main config
app.get('/config', (req, res) => {
  res.json({
    settings: {
      brightness: parseInt(process.env.BRIGHTNESS) || 40,
      timezone: parseInt(process.env.TIMEZONE) || 0,
      scroll_speed: 50,
      time_format: process.env.TIME_FORMAT || '12h',
      temp_unit: process.env.TEMP_UNIT || 'F',
      event_url: `${BASE}/event`,
      transition: 12,
    },
    screens: [
      {
        label: '{time}',
        data_url: `${BASE}/data/clock`,
        duration: 10000,
        x: 2, y: 0,
        color: '00AAFF',
        scroll: 'none',
      },
      {
        label: 'Up {uptime}',
        data_url: `${BASE}/data/system`,
        duration: 8000,
        x: 0, y: 0,
        color: '44FF44',
        scroll: 'auto',
        scroll_speed: 60,
        fade_edge: 2,
      },
      {
        icon: 'thermo',
        label: '{temperature}F {humidity}%',
        data_url: 'self://sensors',
        duration: 6000,
        x: -1, y: 0,
        color: 'FF8800',
        scroll: 'auto',
      },
      {
        icon: 'sun',
        label: '{light}%',
        data_url: 'self://sensors',
        duration: 6000,
        x: -1, y: 0,
        color: 'FFFF00',
        scroll: 'auto',
      },
      {
        icon: 'heart',
        label: 'Hello from thinclock!',
        duration: 15000,
        x: -1, y: 0,
        color: 'FF0088',
        scroll: 'left',
        scroll_speed: 40,
        fade_edge: 3,
      },
    ],
    icons: ICONS,
  });
});

// Clock data
app.get('/data/clock', (req, res) => {
  const now = new Date();
  const tz = parseInt(process.env.TIMEZONE) || 0;
  const local = new Date(now.getTime() + tz * 3600000);
  const fmt = process.env.TIME_FORMAT || '12h';
  let hh = local.getUTCHours();
  if (fmt === '12h') {
    hh = hh % 12 || 12;
  }
  const mm = String(local.getUTCMinutes()).padStart(2, '0');
  const hhStr = fmt === '12h' ? String(hh) : String(hh).padStart(2, '0');
  res.json({ time: `${hhStr}:${mm}` });
});

// System data
app.get('/data/system', (req, res) => {
  const up = process.uptime();
  const m = Math.floor(up / 60);
  const s = Math.floor(up % 60);
  res.json({ uptime: `${m}m${s}s` });
});

// Button event callback
app.post('/event', (req, res) => {
  const { event, screen } = req.body;
  console.log(`[Event] button=${event} screen=${screen}`);
  res.json({ ok: true });
});

app.listen(PORT, () => {
  console.log(`\nthinclock Config Server`);
  console.log(`======================`);
  console.log(`Config:  ${BASE}/config`);
  console.log(`Events:  ${BASE}/event`);
  console.log(`\nSend to serial monitor:`);
  console.log(`{"ssid":"${process.env.WIFI_SSID}","pass":"${process.env.WIFI_PASS}","config_url":"${BASE}/config"}`);
  console.log();
});
