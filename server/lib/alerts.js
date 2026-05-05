const http = require('http');

class AlertEngine {
  constructor(options = {}) {
    this.deviceIP = options.deviceIP || process.env.DEVICE_IP || '192.168.86.60';
    this.maxHistory = options.maxHistory || 20;
    this.history = {};       // keyed by screen id
    this.lastFired = {};     // keyed by alert id → timestamp
    this.maxNotifications = options.maxNotifications || 5;
  }

  /**
   * Register alerts from all screen modules.
   */
  registerFromModules(modules) {
    this.alerts = [];
    for (const mod of modules) {
      if (mod.alerts) {
        for (const alert of mod.alerts) {
          this.alerts.push({ ...alert, _module: mod._id });
        }
      }
    }
    if (this.alerts.length > 0) {
      console.log(`  [alerts] ${this.alerts.length} conditions registered`);
    }
  }

  /**
   * Push a data point for a screen. Evaluates all alerts for that screen.
   */
  pushData(screenId, data) {
    if (!this.history[screenId]) this.history[screenId] = [];
    this.history[screenId].push({ ...data, _ts: Date.now() });

    // Trim history
    if (this.history[screenId].length > this.maxHistory) {
      this.history[screenId].shift();
    }

    // Evaluate alerts for this screen
    this.evaluate(screenId);
  }

  /**
   * Evaluate all alerts for a given screen.
   */
  evaluate(screenId) {
    const history = this.history[screenId] || [];
    if (history.length === 0) return;

    for (const alert of this.alerts) {
      if (alert._module !== screenId) continue;

      // Check cooldown
      const lastFired = this.lastFired[alert.id] || 0;
      const cooldown = alert.cooldown || 300000; // default 5 min
      if (Date.now() - lastFired < cooldown) continue;

      // Evaluate condition
      try {
        if (alert.condition(history)) {
          this.fire(alert);
        }
      } catch (e) {
        // Condition threw — skip silently
      }
    }
  }

  /**
   * Fire a notification to the device.
   */
  fire(alert) {
    this.lastFired[alert.id] = Date.now();
    console.log(`[alert] ${alert.id}: ${alert.message}`);

    const data = JSON.stringify({
      text: alert.message,
      color: alert.color || 'FFAA00',
      beep: alert.beep || 'single',
    });

    try {
      const req = http.request(`http://${this.deviceIP}/notify`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': data.length },
      });
      req.on('error', () => {}); // ignore connection errors
      req.write(data);
      req.end();
    } catch (e) {}
  }
}

module.exports = AlertEngine;
