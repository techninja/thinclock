/**
 * Home Assistant WebSocket Adapter
 *
 * Connects to HA's WebSocket API, subscribes to state changes,
 * and exposes entity states as data endpoints for thinclock screens.
 *
 * In add-on context: HA_URL=http://supervisor/core, HA_TOKEN=SUPERVISOR_TOKEN
 * Standalone: HA_URL=http://homeassistant.local:8123, HA_TOKEN=<long_lived_token>
 *
 * Provides:
 *   GET /data/ha/:entity_id → { state, ...attributes }
 *   GET /data/ha            → [entity_id, ...]
 */

import { WebSocket } from 'ws';

export default class HomeAssistantAdapter {
  constructor(config) {
    this.url = config.ha_url || process.env.HA_URL;
    this.token = config.ha_token || process.env.HA_TOKEN;
    this.entities = {};
    this.ws = null;
    this._msgId = 1;
    this._pending = new Map();
  }

  setup(app, config) {
    if (!this.url || !this.token) {
      console.log('  [ha] Not configured (set HA_URL and HA_TOKEN)');
      return;
    }

    app.get('/data/ha/:entity_id', (req, res) => {
      const e = this.entities[req.params.entity_id];
      if (!e) return res.status(404).json({ error: 'entity not found' });
      res.json({ state: e.state, ...e.attributes });
    });

    app.get('/data/ha', (req, res) => res.json(Object.keys(this.entities)));

    console.log(`  [ha] Connecting to ${this.url}`);
    this._connect();
  }

  _connect() {
    const wsUrl = this.url.replace(/^http/, 'ws') + '/api/websocket';
    this.ws = new WebSocket(wsUrl);

    this.ws.on('message', (raw) => {
      const msg = JSON.parse(raw);
      if (msg.type === 'auth_required') {
        this.ws.send(JSON.stringify({ type: 'auth', access_token: this.token }));
      } else if (msg.type === 'auth_ok') {
        console.log('  [ha] Authenticated');
        this._fetchAllStates();
        this._subscribe();
      } else if (msg.type === 'auth_invalid') {
        console.error('  [ha] Auth failed — check HA_TOKEN');
      } else if (msg.type === 'result' && this._pending.has(msg.id)) {
        this._pending.get(msg.id)(msg);
        this._pending.delete(msg.id);
      } else if (msg.type === 'event' && msg.event?.event_type === 'state_changed') {
        const { entity_id, new_state } = msg.event.data;
        if (new_state) this.entities[entity_id] = new_state;
      }
    });

    this.ws.on('close', () => {
      console.log('  [ha] Disconnected — reconnecting in 10s');
      setTimeout(() => this._connect(), 10000);
    });

    this.ws.on('error', (e) => console.error('  [ha] WS error:', e.message));
  }

  _send(msg) {
    const id = this._msgId++;
    return new Promise((resolve) => {
      this._pending.set(id, resolve);
      this.ws.send(JSON.stringify({ ...msg, id }));
    });
  }

  async _fetchAllStates() {
    const result = await this._send({ type: 'get_states' });
    for (const state of result.result || []) {
      this.entities[state.entity_id] = state;
    }
    console.log(`  [ha] Loaded ${Object.keys(this.entities).length} entities`);
  }

  async _subscribe() {
    await this._send({ type: 'subscribe_events', event_type: 'state_changed' });
  }
}
