/**
 * Config route — builds and serves the device config payload.
 * Handles night mode screen filtering and brightness selection.
 */

const nightHours = () => (process.env.NIGHT_HOURS || '').split(',').map(Number).filter(Boolean);

export const isNightMode = (timezone) =>
  nightHours().includes((new Date().getUTCHours() + timezone + 24) % 24);

export const getBrightness = (config) =>
  isNightMode(config.timezone) ? parseInt(process.env.BRIGHTNESS_NIGHT) || 10 : config.brightness;

export function registerConfigRoute(app, registry, config, BASE) {
  app.get('/api/config', (req, res) => {
    let { screens, icons } = registry.build(app, config);
    if (isNightMode(config.timezone)) {
      const night = registry.getActiveModules().filter((m) => m.tags.includes('night'));
      if (night.length) {
        screens = night.map((m) => (typeof m.screen === 'function' ? m.screen(config) : m.screen));
        icons = Object.assign({}, ...night.map((m) => m.icons || {}));
      }
    }
    res.json({
      settings: {
        brightness: getBrightness(config),
        timezone: config.timezone,
        scroll_speed: 50,
        time_format: config.time_format,
        temp_unit: config.temp_unit,
        event_url: `${BASE}/api/event`,
        buttons: 'navigate',
        allow_beep: process.env.ALLOW_BEEPING !== 'false',
        transition: 12,
      },
      screens,
      icons,
    });
  });
}
