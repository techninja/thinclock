/**
 * Device proxy routes — forwards requests to the ESP32.
 * All routes mounted at /api/device/* by server.js.
 */

const TIMEOUT = { json: 3000, binary: 15000, gif: 30000 };

/** Fetch from device, return Response or null on error. */
async function deviceFetch(url, opts = {}) {
  try {
    return await fetch(url, opts);
  } catch {
    return null;
  }
}

/** Proxy a binary device response (framebuffer, preview, render). */
async function binaryProxy(res, url, opts = {}, headers = {}) {
  const resp = await deviceFetch(url, opts);
  if (!resp) return res.status(502).end();
  for (const [k, v] of Object.entries(headers)) res.set(k, v);
  res.send(Buffer.from(await resp.arrayBuffer()));
}
export function registerDeviceRoutes(app, getDeviceIP) {
  app.get('/api/device/framebuffer', async (req, res) => {
    const ip = getDeviceIP();
    if (!ip) return res.status(503).end();
    await binaryProxy(
      res,
      `http://${ip}/framebuffer`,
      { signal: AbortSignal.timeout(TIMEOUT.json) },
      {
        'Content-Type': 'application/octet-stream',
        'Cache-Control': 'no-store',
      },
    );
  });

  app.get('/api/device/gif', async (req, res) => {
    const ip = getDeviceIP();
    if (!ip) return res.status(503).end();
    const { screen = 0, seconds = 2 } = req.query;
    await binaryProxy(
      res,
      `http://${ip}/gif?screen=${screen}&seconds=${seconds}`,
      { signal: AbortSignal.timeout(TIMEOUT.gif) },
      { 'Content-Type': 'image/gif', 'Cache-Control': 'public, max-age=60' },
    );
  });

  app.get('/api/device/preview', async (req, res) => {
    const ip = getDeviceIP();
    if (!ip) return res.status(503).end();
    const { screen = 0, frames = 30 } = req.query;
    const resp = await deviceFetch(`http://${ip}/preview?screen=${screen}&frames=${frames}`, {
      signal: AbortSignal.timeout(TIMEOUT.binary),
    });
    if (!resp) return res.status(502).end();
    res.set('Content-Type', 'application/octet-stream');
    res.set('X-Frames', resp.headers.get('X-Frames') || frames);
    res.set('X-Frame-Ms', resp.headers.get('X-Frame-Ms') || '20');
    res.set('Cache-Control', 'public, max-age=60');
    res.send(Buffer.from(await resp.arrayBuffer()));
  });

  app.post('/api/device/render', async (req, res) => {
    const ip = getDeviceIP();
    if (!ip) return res.status(503).end();
    const resp = await deviceFetch(`http://${ip}/render`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(req.body),
      signal: AbortSignal.timeout(TIMEOUT.binary),
    });
    if (!resp) return res.status(502).end();
    res.set('Content-Type', 'application/octet-stream');
    res.set('X-Frames', resp.headers.get('X-Frames') || '1');
    res.set('X-Frame-Ms', resp.headers.get('X-Frame-Ms') || '20');
    res.send(Buffer.from(await resp.arrayBuffer()));
  });

  // Generic JSON passthrough for /info, /sensors, /status, etc.
  app.get('/api/device/:endpoint', async (req, res) => {
    const ip = getDeviceIP();
    if (!ip) return res.json({});
    const resp = await deviceFetch(`http://${ip}/${req.params.endpoint}`, {
      signal: AbortSignal.timeout(TIMEOUT.json),
    });
    res.json(resp ? await resp.json().catch(() => ({})) : {});
  });

  app.post('/api/device/:endpoint', async (req, res) => {
    const ip = getDeviceIP();
    if (!ip) return res.status(503).json({ error: 'No device IP' });
    const resp = await deviceFetch(`http://${ip}/${req.params.endpoint}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(req.body),
      signal: AbortSignal.timeout(TIMEOUT.json),
    });
    if (!resp) return res.status(502).json({ error: 'Device unreachable' });
    res.json(await resp.json().catch(() => ({ ok: true })));
  });
}

/** Generate a GIF preview for a screen and write to cache dir. */
export async function generatePreview(ip, screen, icons) {
  if (!ip) return null;
  const resp = await deviceFetch(`http://${ip}/gif`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      layers: screen.layers,
      data_url: screen.data_url || '',
      icons,
      seconds: 1,
      scale: 5,
      gap: 1,
      gamma: 18,
    }),
    signal: AbortSignal.timeout(TIMEOUT.gif),
  });
  if (!resp?.ok) return null;
  return Buffer.from(await resp.arrayBuffer());
}

/** Poll device sensors and network latency, push to alert engine. */
export async function pollDevice(ip, alerts) {
  try {
    const start = Date.now();
    await fetch('http://1.1.1.1/', { signal: AbortSignal.timeout(3000) });
    alerts.pushData('network', { ping: Date.now() - start, status: 1, rssi: 0 });
  } catch {
    alerts.pushData('network', { ping: 0, status: 0, rssi: 0 });
  }
  if (!ip) return;
  const resp = await deviceFetch(`http://${ip}/sensors`, { signal: AbortSignal.timeout(2000) });
  if (resp) alerts.pushData('sensors', await resp.json().catch(() => ({})));
}

export { PreviewCache } from './preview-cache.js';
