const fs = require('fs');
const path = require('path');

class ScreenRegistry {
  constructor(options = {}) {
    this.modules = [];
    this.mode = options.mode || process.env.SCREEN_MODE || 'auto';
    this.allowlist = (options.allowlist || process.env.SCREEN_ALLOWLIST || '')
      .split(',').map(s => s.trim()).filter(Boolean);
    this.blocklist = (options.blocklist || process.env.SCREEN_BLOCKLIST || '')
      .split(',').map(s => s.trim()).filter(Boolean);
    this.maxScreens = parseInt(options.maxScreens || process.env.MAX_SCREENS || '8');
  }

  /**
   * Load all .js screen modules from a directory.
   */
  loadDir(dir) {
    const files = fs.readdirSync(dir).filter(f => f.endsWith('.js')).sort();
    for (const file of files) {
      try {
        const mod = require(path.join(dir, file));
        mod._file = file;
        mod._id = file.replace('.js', '');
        mod.priority = mod.priority || 0;
        mod.tags = mod.tags || [];
        mod.schedule = mod.schedule || null;
        mod.contextAction = mod.contextAction || null;
        this.modules.push(mod);
        console.log(`  [screen] ${mod.enabled ? '✓' : '✗'} ${mod.name || file} (${mod._id})`);
      } catch (e) {
        console.error(`  [screen] ✗ ${file}: ${e.message}`);
      }
    }
  }

  /**
   * Check if a module passes its schedule filter for the current time.
   */
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
      // 0=Sun, 1=Mon, ...
      if (!s.days.includes(now.getDay())) return false;
    }
    if (s.dateRange) {
      const mmdd = (now.getMonth() + 1) * 100 + now.getDate();
      if (mmdd < s.dateRange[0] || mmdd > s.dateRange[1]) return false;
    }
    return true;
  }

  /**
   * Get the filtered, sorted list of active modules.
   */
  getActiveModules() {
    return this.modules
      .filter(mod => {
        // Must be enabled
        if (!mod.enabled) return false;
        // Blocklist
        if (this.blocklist.includes(mod._id)) return false;
        // Allowlist (manual mode)
        if (this.mode === 'manual' && this.allowlist.length > 0) {
          if (!this.allowlist.includes(mod._id)) return false;
        }
        // Schedule (auto mode)
        if (this.mode === 'auto') {
          if (!this.passesSchedule(mod)) return false;
        }
        // 'all' mode: just enabled + not blocked
        return true;
      })
      .sort((a, b) => b.priority - a.priority)
      .slice(0, this.maxScreens);
  }

  /**
   * Register all routes (call once at startup).
   */
  registerRoutes(app, config) {
    for (const mod of this.modules) {
      if (mod.routes) mod.routes(app, config);
    }
  }

  /**
   * Build screens and icons for config response.
   */
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

  /**
   * Enable/disable a screen by id.
   */
  setEnabled(id, enabled) {
    const mod = this.modules.find(m => m._id === id || m._file === id);
    if (mod) mod.enabled = enabled;
  }

  /**
   * Get the context action for a screen index.
   */
  getContextAction(screenIndex) {
    const active = this.getActiveModules();
    if (screenIndex >= 0 && screenIndex < active.length) {
      return active[screenIndex].contextAction || 'pause';
    }
    return 'pause';
  }

  /**
   * List all loaded modules with status and schedule info.
   */
  list() {
    const active = this.getActiveModules();
    const activeIds = active.map(m => m._id);

    return this.modules.map(m => ({
      id: m._id,
      file: m._file,
      name: m.name || m._file,
      enabled: m.enabled,
      active: activeIds.includes(m._id),
      priority: m.priority,
      tags: m.tags,
      schedule: m.schedule,
      contextAction: m.contextAction,
    }));
  }
}

module.exports = ScreenRegistry;
