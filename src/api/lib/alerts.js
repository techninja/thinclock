import http from 'http';

export default class AlertEngine {
  constructor(options = {}) {
    this.deviceIP = options.deviceIP || process.env.DEVICE_IP || '192.168.86.60';
    this.timezone = options.timezone || 0;
    this.maxHistory = options.maxHistory || 20;
    this.history = {};
    this.lastFired = {};
    this.alerts = [];
  }

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

  pushData(screenId, data) {
    if (!this.history[screenId]) this.history[screenId] = [];
    this.history[screenId].push({ ...data, _ts: Date.now() });
    if (this.history[screenId].length > this.maxHistory) {
      this.history[screenId].shift();
    }
    this.evaluate(screenId);
  }

  evaluate(screenId) {
    const history = this.history[screenId] || [];
    if (history.length === 0) return;
    for (const alert of this.alerts) {
      if (alert._module !== screenId) continue;
      const lastFired = this.lastFired[alert.id] || 0;
      const cooldown = alert.cooldown || 300000;
      if (Date.now() - lastFired < cooldown) continue;
      try {
        if (alert.condition(history)) this.fire(alert);
      } catch (e) {}
    }
  }

  fire(alert) {
    this.lastFired[alert.id] = Date.now();
    console.log(`[alert] ${alert.id}: ${alert.message}`);

    const now = new Date();
    const local = new Date(now.getTime() + this.timezone * 3600000);
    const month = local.getUTCMonth() + 1;
    const day = local.getUTCDate();
    let hours = local.getUTCHours();
    const mins = String(local.getUTCMinutes()).padStart(2, '0');
    const ampm = hours >= 12 ? 'P' : 'A';
    hours = hours % 12 || 12;
    const timestamp = `${month}/${day} ${hours}:${mins}${ampm}`;
    const text = `${timestamp}: ${alert.message}`;

    const data = JSON.stringify({
      text,
      color: alert.color || 'FFAA00',
      beep: alert.beep || 'single',
      icon: alert.icon || '',
    });

    try {
      const req = http.request(`http://${this.deviceIP}/notify`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(data) },
      });
      req.on('error', () => {});
      req.write(data);
      req.end();
    } catch (e) {}
  }
}
