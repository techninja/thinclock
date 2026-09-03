/**
 * API route registrations for the thinclock server.
 * @module api/routes
 */

import http from 'http';
import { getSchedules, getSchedule, setSchedule, deleteSchedule } from './lib/schedules.js';
import {
  listCustomScreens,
  getCustomScreen,
  saveCustomScreen,
  deleteCustomScreen,
} from './lib/custom-screens.js';

/**
 * Register all API routes on the express app.
 * @param {object} app
 * @param {object} registry
 * @param {object} alerts
 * @param {Function} getDeviceIP
 * @param {number} PORT
 */
export function registerRoutes(app, registry, alerts, getDeviceIP, PORT) {
  let paused = false;

  app.get('/api/screens', (req, res) => res.json(registry.list()));
  app.post('/api/screens/:id/enable', (req, res) => {
    registry.setEnabled(req.params.id, true);
    res.json({ ok: true });
  });
  app.post('/api/screens/:id/disable', (req, res) => {
    registry.setEnabled(req.params.id, false);
    res.json({ ok: true });
  });
  app.post('/api/screens/:id/pin', (req, res) => {
    registry.setPinned(req.params.id, true);
    res.json({ ok: true });
  });
  app.post('/api/screens/:id/unpin', (req, res) => {
    registry.setPinned(req.params.id, false);
    res.json({ ok: true });
  });
  app.get('/api/active', (req, res) =>
    res.json(
      registry.getActiveModules().map((m, i) => ({
        index: i,
        id: m._id,
        name: m.name,
        contextAction: m.contextAction || 'pause',
      })),
    ),
  );
  app.get('/api/device-ip', (req, res) => res.json({ ip: getDeviceIP() }));

  app.get('/api/schedules', (req, res) => res.json(getSchedules()));
  app.get('/api/schedules/:name', (req, res) => {
    const s = getSchedule(req.params.name);
    s ? res.json(s) : res.status(404).json({ error: 'not found' });
  });
  app.put('/api/schedules/:name', (req, res) => {
    setSchedule(req.params.name, req.body);
    res.json({ ok: true });
  });
  app.delete('/api/schedules/:name', (req, res) => {
    deleteSchedule(req.params.name);
    res.json({ ok: true });
  });

  app.get('/api/custom-screens', (req, res) => res.json(listCustomScreens()));
  app.get('/api/custom-screens/:id', (req, res) => {
    const s = getCustomScreen(req.params.id);
    s ? res.json(s) : res.status(404).json({ error: 'not found' });
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

  app.post('/api/event', (req, res) => {
    const { event, screen } = req.body;
    console.log(`[event] button=${event} screen=${screen}`);
    if (event === 'select' || event === 'select_long') {
      const action = registry.getContextAction(screen);
      if (action === 'pomodoro') {
        const r = http.request(`http://localhost:${PORT}/api/pomodoro/toggle`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'Content-Length': 2 },
        });
        r.write('{}');
        r.end();
      } else if (action === 'pause') {
        paused = !paused;
      }
      res.json({ ok: true, action, paused });
    } else {
      res.json({ ok: true });
    }
  });

  app.post('/api/test/alert', (req, res) => {
    const { screen, data } = req.body;
    if (screen && data) {
      alerts.pushData(screen, data);
      res.json({ ok: true });
    } else res.json({ error: 'need {screen, data}' });
  });
}
