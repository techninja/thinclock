import fs from 'fs';
import path from 'path';
import { pathToFileURL, fileURLToPath } from 'url';
import { isScheduleActive } from './schedules.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OVERRIDES_FILE = path.join(__dirname, '..', '..', '..', 'data', 'screen-overrides.json');

function loadOverrides() {
  try {
    return JSON.parse(fs.readFileSync(OVERRIDES_FILE, 'utf8'));
  } catch {
    return {};
  }
}

function saveOverrides(overrides) {
  fs.mkdirSync(path.dirname(OVERRIDES_FILE), { recursive: true });
  fs.writeFileSync(OVERRIDES_FILE, JSON.stringify(overrides, null, 2));
}

export default class ScreenRegistry {
  constructor(options = {}) {
    this.modules = [];
    this.mode = options.mode || process.env.SCREEN_MODE || 'auto';
    this.allowlist = (options.allowlist || process.env.SCREEN_ALLOWLIST || '')
      .split(',')
      .map((s) => s.trim())
      .filter(Boolean);
    this.blocklist = (options.blocklist || process.env.SCREEN_BLOCKLIST || '')
      .split(',')
      .map((s) => s.trim())
      .filter(Boolean);
    this.maxScreens = parseInt(options.maxScreens || process.env.MAX_SCREENS || '8');
  }

  async loadDir(dir) {
    const overrides = loadOverrides();
    const files = fs
      .readdirSync(dir)
      .filter((f) => f.endsWith('.js'))
      .sort();
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
        if (overrides[m._id] !== undefined)
          m.enabled = overrides[m._id].enabled ?? overrides[m._id];
        m.pinned = overrides[m._id]?.pinned ?? false;
        this.modules.push(m);
        console.log(`  [screen] ${m.enabled ? '✓' : '✗'} ${m.name || file} (${m._id})`);
      } catch (e) {
        console.error(`  [screen] ✗ ${file}: ${e.message}`);
      }
    }
  }

  getActiveModules() {
    const pinned = this.modules.filter(
      (mod) => mod.enabled && mod.pinned && !this.blocklist.includes(mod._id),
    );
    const pinnedIds = new Set(pinned.map((m) => m._id));
    const rest = this.modules
      .filter((mod) => {
        if (!mod.enabled || pinnedIds.has(mod._id)) return false;
        if (this.blocklist.includes(mod._id)) return false;
        if (this.mode === 'manual' && this.allowlist.length > 0) {
          if (!this.allowlist.includes(mod._id)) return false;
        }
        if (this.mode === 'auto') {
          if (!isScheduleActive(mod.schedule)) return false;
        }
        return true;
      })
      .sort((a, b) => b.priority - a.priority)
      .slice(0, Math.max(0, this.maxScreens - pinned.length));
    return [...pinned, ...rest];
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
    const mod = this.modules.find((m) => m._id === id || m._file === id);
    if (!mod) return;
    mod.enabled = enabled;
    const overrides = loadOverrides();
    overrides[mod._id] = { enabled, pinned: mod.pinned ?? false };
    saveOverrides(overrides);
  }

  setPinned(id, pinned) {
    const mod = this.modules.find((m) => m._id === id || m._file === id);
    if (!mod) return;
    mod.pinned = pinned;
    const overrides = loadOverrides();
    overrides[mod._id] = { enabled: mod.enabled, pinned };
    saveOverrides(overrides);
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
    const activeIds = active.map((m) => m._id);
    return this.modules.map((m) => ({
      id: m._id,
      file: m._file,
      name: m.name || m._file,
      enabled: m.enabled,
      active: activeIds.includes(m._id),
      pinned: m.pinned ?? false,
      priority: m.priority,
      tags: m.tags,
      schedule: m.schedule,
      contextAction: m.contextAction,
    }));
  }
}
