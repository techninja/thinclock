import { makeIcon } from '../lib/icons.js';

export const name = 'Awtrix Clock';
export const enabled = true;
export const priority = 10; // always show first
export const tags = ['utility', 'clock'];
export const contextAction = 'pause';

export const icons = {
  calendar: {
    width: 9,
    height: 8,
    fps: 0,
    data: [
      makeIcon(
        [
          '#########',
          '#########',
          'WWWWWWWWW',
          'WWWWWWWWW',
          'WWWWWWWWW',
          'WWWWWWWWW',
          'WWWWWWWWW',
          'WWWWWWWWW',
        ],
        0xff,
        0x22,
        0x22,
      ),
    ],
  },
};

export const screen = (config) => ({
  duration: 30000,
  data_url: `${config.BASE}/data/datetime`,
  layers: [
    {
      type: 'gradient',
      x: 1,
      y: 2,
      width: 7,
      height: 6,
      direction: 'vertical',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, 'FFFFFF'],
          [1, 'DDCCAA'],
        ],
      },
    },
    { type: 'icon', name: 'calendar', x: 0, y: 0 },
    { type: 'native', label: '{day}', x: 1, y: 2, color: '000000', large: false, spacing: 1, align: 'center', align_width: 7 },
    {
      type: 'clock',
      format: config.time_format,
      x: 12,
      y: 1,
      color: '4488FF',
      large: false,
      spacing: 1,
    },
    { type: 'pixels', pattern: 'week_dots', x: 10, y: 7, color: '4488FF', dim_color: '112244' },
  ],
});

export const routes = (app, config) => {
  app.get('/data/datetime', (req, res) => {
    const now = new Date();
    const local = new Date(now.getTime() + config.timezone * 3600000);
    res.json({
      day: local.getUTCDate(),
      weekday: local.getUTCDay(),
      month: local.getUTCMonth() + 1,
    });
  });
};
