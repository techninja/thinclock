require('dotenv').config();
const express = require('express');
const os = require('os');

const app = express();
const PORT = process.env.SERVER_PORT || 3000;

// Get local IP for display in console
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

// Main config endpoint - the clock fetches this
app.get('/config', (req, res) => {
  res.json({
    settings: {
      brightness: parseInt(process.env.BRIGHTNESS) || 40,
      timezone: parseInt(process.env.TIMEZONE) || 0,
      scroll_speed: 50,
    },
    screens: [
      {
        icon: '',
        label: '{time}',
        data_url: `${BASE}/data/clock`,
        duration: 10000,
        x: 2, y: 0,
        color: '00AAFF',
        scroll: 'none',
      },
      {
        icon: '',
        label: 'Uptime: {uptime}',
        data_url: `${BASE}/data/system`,
        duration: 8000,
        x: 0, y: 0,
        color: '44FF44',
        scroll: 'auto',
        scroll_speed: 60,
        fade_edge: 2,
      },
      {
        icon: '',
        label: 'Hello from Clock Top!',
        data_url: '',
        duration: 10000,
        x: 0, y: 0,
        color: 'FF8800',
        scroll: 'left',
        scroll_speed: 40,
        fade_edge: 3,
      },
    ],
    sprites: [],
  });
});

// Clock data endpoint
app.get('/data/clock', (req, res) => {
  const now = new Date();
  const tz = parseInt(process.env.TIMEZONE) || 0;
  const local = new Date(now.getTime() + tz * 3600000);
  const hh = String(local.getUTCHours()).padStart(2, '0');
  const mm = String(local.getUTCMinutes()).padStart(2, '0');
  res.json({ time: `${hh}:${mm}` });
});

// System data endpoint
app.get('/data/system', (req, res) => {
  const up = process.uptime();
  const m = Math.floor(up / 60);
  const s = Math.floor(up % 60);
  res.json({ uptime: `${m}m${s}s` });
});

app.listen(PORT, () => {
  console.log(`\nClock Top Config Server`);
  console.log(`======================`);
  console.log(`Config URL: ${BASE}/config`);
  console.log(`\nSend this to the clock via serial monitor:`);
  console.log(`{"ssid":"${process.env.WIFI_SSID}","pass":"${process.env.WIFI_PASS}","config_url":"${BASE}/config"}`);
  console.log();
});
