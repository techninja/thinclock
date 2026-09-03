export const name = 'Air Quality';
export const enabled = true;
export const priority = 7;
export const tags = ['utility', 'weather'];

export const alerts = [
  {
    id: 'aqi_unhealthy',
    condition: (history) => {
      if (history.length < 2) return false;
      const prev = history[history.length - 2];
      const curr = history[history.length - 1];
      return prev.aqi <= 100 && curr.aqi > 100;
    },
    message: 'AQI unhealthy',
    color: 'FF8800',
    beep: 'single',
    cooldown: 3600000, // 1 hour
  },
  {
    id: 'aqi_dangerous',
    condition: (history) => {
      const curr = history[history.length - 1];
      return curr && curr.aqi > 200;
    },
    message: 'AQI dangerous!',
    color: 'FF0000',
    beep: 'alert',
    cooldown: 3600000,
  },
];

const aqiCache = { aqi: 42, category: 'Good', updated: 0 };

export const screen = (config) => {
  const aqi = aqiCache.aqi;
  let textColor = '00CC44';
  if (aqi > 300) textColor = '880044';
  else if (aqi > 200) textColor = '880088';
  else if (aqi > 150) textColor = 'FF0000';
  else if (aqi > 100) textColor = 'FF8800';
  else if (aqi > 50) textColor = 'FFCC00';

  // Position of indicator on the 32px bar (AQI 0-300 mapped to x 0-31)
  const indicatorX = Math.min(31, Math.round((aqi / 300) * 31));

  return {
    duration: 12000,
    data_url: `${config.BASE}/data/aqi`,
    layers: [
      // Full-width EPA color gradient bar across bottom rows
      {
        type: 'gradient',
        x: 0,
        y: 5,
        width: 32,
        height: 3,
        direction: 'horizontal',
        colors: {
          min: 0,
          max: 1,
          stops: [
            [0, '00CC44'],
            [0.17, 'FFCC00'],
            [0.33, 'FF8800'],
            [0.5, 'FF0000'],
            [0.67, '880088'],
            [1, '880044'],
          ],
        },
      },
      // White indicator line at current AQI position
      { type: 'pixels', pattern: 'vline', x: indicatorX, y: 5, color: 'FFFFFF' },
      // AQI value + label (centered)
      {
        type: 'native',
        label: 'AQI {aqi}',
        x: 0,
        y: 0,
        color: textColor,
        large: false,
        spacing: 1,
        align: 'center',
      },
    ],
  };
};

import { registerAqiRoutes } from './lib/aqi-routes.js';
export const routes = (app, config) => registerAqiRoutes(app, config, aqiCache);
