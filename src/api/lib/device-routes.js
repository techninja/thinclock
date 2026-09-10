import { approveDevice, removeDevice, listDevices } from './device-registry.js';
import { approveAndConnect } from './ws-render.js';

export function registerDeviceRoutes(app) {
  app.get('/api/devices', (req, res) => res.json(listDevices()));
  app.post('/api/devices/:ip/approve', (req, res) => {
    const ip = decodeURIComponent(req.params.ip);
    approveDevice(ip, req.body.name || '');
    approveAndConnect(ip);
    res.json({ ok: true });
  });
  app.delete('/api/devices/:ip', (req, res) => {
    removeDevice(decodeURIComponent(req.params.ip));
    res.json({ ok: true });
  });
}
