const fs = require('fs');
const path = require('path');

class ScreenRegistry {
  constructor() {
    this.modules = [];
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
        this.modules.push(mod);
        console.log(`  [screen] ${mod.enabled ? '✓' : '✗'} ${mod.name || file}`);
      } catch (e) {
        console.error(`  [screen] ✗ ${file}: ${e.message}`);
      }
    }
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

    for (const mod of this.modules) {
      if (!mod.enabled) continue;

      // Collect icons
      if (mod.icons) Object.assign(icons, mod.icons);

      // Build screen config
      if (mod.screen) {
        const scr = typeof mod.screen === 'function' ? mod.screen(config) : mod.screen;
        screens.push(scr);
      }
    }

    return { screens, icons };
  }

  /**
   * Enable/disable a screen by filename.
   */
  setEnabled(file, enabled) {
    const mod = this.modules.find(m => m._file === file);
    if (mod) mod.enabled = enabled;
  }

  /**
   * List all loaded modules with status.
   */
  list() {
    return this.modules.map(m => ({
      file: m._file,
      name: m.name || m._file,
      enabled: m.enabled,
    }));
  }
}

module.exports = ScreenRegistry;
