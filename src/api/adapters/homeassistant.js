/**
 * Home Assistant Data Adapter
 *
 * Connects to HA's WebSocket API and exposes entity states
 * as data endpoints for thinclock screens.
 *
 * Config (via .env or passed config):
 *   HA_URL=http://homeassistant.local:8123
 *   HA_TOKEN=<long_lived_access_token>
 *
 * Provides:
 *   GET /data/ha/:entity_id → { state, attributes... }
 *
 * Screen modules can use:
 *   data_url: "{{BASE}}/data/ha/sensor.living_room_temperature"
 */

export default class HomeAssistantAdapter {
  constructor(config) {
    this.url = config.ha_url || process.env.HA_URL;
    this.token = config.ha_token || process.env.HA_TOKEN;
    this.entities = {};
    this.connected = false;
  }

  setup(app, config) {
    if (!this.url || !this.token) {
      console.log('  [ha] Not configured (set HA_URL and HA_TOKEN)');
      return;
    }

    // REST endpoint for entity data
    app.get('/data/ha/:entity_id', (req, res) => {
      const entity = this.entities[req.params.entity_id];
      if (!entity) return res.status(404).json({ error: 'entity not found' });
      res.json({
        state: entity.state,
        ...entity.attributes,
      });
    });

    // List available entities
    app.get('/data/ha', (req, res) => {
      res.json(Object.keys(this.entities));
    });

    console.log(`  [ha] Adapter ready (${this.url})`);
    this.connect();
  }

  async connect() {
    // TODO: Implement WebSocket connection to HA
    // - Connect to ws://{HA_URL}/api/websocket
    // - Authenticate with token
    // - Subscribe to state_changed events
    // - Populate this.entities with current states
    console.log('  [ha] WebSocket connection not yet implemented');
  }
}
