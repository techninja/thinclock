import fs from 'fs';
import path from 'path';
import { pathToFileURL } from 'url';

export default class ScreenRegistry {
  constructor(options = {}) {
    this.modules = [];
    this.mode = options.mode || process.env.SCREEN_MODE || 'auto';
    this.allowlist = (options.allowlist || process.env.SCREEN_ALLOWLIST || '')
      .split(',').map(s => s.trim()).filter(Boolean);
    this.blocklist = (options.blocklist || process.env.SCREEN_BLOCKLIST || '')
      .split(',').map(s => s.trim()).filter(Boolean);
    this.maxScreens = parseInt(options.maxScreens || process.env.MAX_SCREENS || '8');
  }

  async loadDir(dir) {
    const files = fs.readdirSync(dir).filter(f => f.endsWith('.js')).sort();
    for (const file of files) {
      try {
        const mod = await import(pathToFileURL(path.join(dir, file)).href);
        const m = { ...mod };
        m._file = file;
        m._id = file.replace('.js', '');
        m.priority = m.priority || 0;
        m.tags = m.tags || [];
        m.schedule = m.schedule || null;
        m.contextAction = m.contextAction || null;
        this.modules.push(m);
        console.log(`  [screen] ${m.enabled ? '✓' : '✗'} ${m.name || file} (${m._id})`);
      } catch (e) {
        console.error(`  [screen] ✗ ${file}: ${e.message}`);
      }
    }
  }

  passesSchedule(mod) {
    if (!mod.schedule) return true;
    const now = new Date();
    const s = mod.schedule;
    if (s.months && s.months.length > 0) {
      if (!s.months.includes(now.getMonth() + 1)) return false;
    }
    if (s.hours && s.hours.length > 0) {
      if (!s.hours.includes(now.getHours())) return false;
    }
    if (s.days && s.days.length > 0) {
      if (!s.days.includes(now.getDay())) return false;
    }
    if (s.dateRange) {
      const mmdd = (now.getMonth() + 1) * 100 + now.getDate();
      if (mmdd < s.dateRange[0] || mmdd > s.dateRange[1]) return false;
    }
    return true;
  }

  getActiveModules() {
    return this.modules
      .filter(mod => {
        if (!mod.enabled) return false;
        if (this.blocklist.includes(mod._id)) return false;
        if (this.mode === 'manual' && this.allowlist.length > 0) {
          if (!this.allowlist.includes(mod._id)) return false;
        }
        if (this.mode === 'auto') {
          if (!this.passesSchedule(mod)) return false;
        }
        return true;
      })
      .sort((a, b) => b.priority - a.priority)
      .slice(0, this.maxScreens);
  }

  registerRoutes(app, config) {
    for (const mod of this.modules) {
      if (mod.routes) mod.routes(app, config);
    }
  }

  build(app, config) {
    const icons = {};
    const screens = [];
    const active = this.getActiveModules();
    for (const mod of active) {
      if (mod.icons) Object.assign(icons, mod.icons);
      if (mod.screen) {
        const scr = typeof mod.screen === 'function' ? mod.screen(config) : mod.screen;
        screens.push(scr);
      }
    }
    return { screens, icons };
  }

  setEnabled(id, enabled) {
    const mod = this.modules.find(m => m._id === id || m._file === id);
    if (mod) mod.enabled = enabled;
  }

  getContextAction(screenIndex) {
    const active = this.getActiveModules();
    if (screenIndex >= 0 && screenIndex < active.length) {
      return active[screenIndex].contextAction || 'pause';
    }
    return 'pause';
  }

  list() {
    const active = this.getActiveModules();
    const activeIds = active.map(m => m._id);
    return this.modules.map(m => ({
      id: m._id, file: m._file, name: m.name || m._file,
      enabled: m.enabled, active: activeIds.includes(m._id),
      priority: m.priority, tags: m.tags, schedule: m.schedule,
      contextAction: m.contextAction,
    }));
  }
}
