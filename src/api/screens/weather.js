import { weatherIcons } from './weather-icons.js';
import { weatherParticles, heatShimmer } from './weather-particles.js';

export const name = 'Weather';
export const enabled = true;
export const priority = 9;
export const tags = ['utility', 'weather'];
export const contextAction = 'refresh';
export const icons = weatherIcons;

export const alerts = [
  {
    id: 'rain_started',
    condition: (history) => {
      if (history.length < 2) return false;
      const prev = history[history.length - 2],
        curr = history[history.length - 1];
      const wasRaining = prev.condition >= 200 && prev.condition < 600;
      const isRaining = curr.condition >= 200 && curr.condition < 600;
      return !wasRaining && isRaining;
    },
    message: 'Rain starting',
    color: '4488FF',
    icon: 'weather_cloud',
    beep: 'single',
    cooldown: 1800000,
  },
  {
    id: 'severe_weather',
    condition: (history) => {
      const c = history[history.length - 1];
      return c && c.condition >= 200 && c.condition < 300;
    },
    message: 'Severe weather!',
    color: 'FF0000',
    icon: 'weather_storm',
    beep: 'alert',
    cooldown: 3600000,
  },
];

let weatherCache = {
  condition: 500,
  temp: 58,
  humidity: 77,
  wind_speed: 5,
  wind_deg: 270,
  updated: 0,
};

/** @param {number} temp @returns {string} */
function tempColor(temp) {
  const lo = parseInt(process.env.TEMP_COMFY_LOW) || 67;
  const hi = parseInt(process.env.TEMP_COMFY_HIGH) || 72;
  if (temp >= lo && temp <= hi) return '00CC44';
  if (temp > hi && temp <= 80) return 'AACC00';
  if (temp > 80 && temp <= 90) return 'FFAA00';
  if (temp > 90) return 'FF4400';
  if (temp < lo && temp >= 55) return '44CCAA';
  if (temp < 55 && temp >= 40) return '44AAFF';
  return '4444FF';
}

/** @param {number} condition @returns {string} */
function iconForCondition(condition) {
  if (condition >= 200 && condition < 300) return 'weather_storm';
  if (condition >= 300 && condition < 600) return 'weather_cloud';
  if (condition >= 600 && condition < 700) return 'weather_snow_icon';
  if (condition >= 700 && condition < 800) return 'weather_cloud';
  if (condition === 800) return 'weather_sun';
  return condition <= 802 ? 'weather_cloud_sun' : 'weather_cloud';
}

export const screen = (config) => ({
  duration: 30000,
  data_url: `${config.BASE}/data/weather`,
  layers: buildLayers(weatherCache),
});

/** @param {object} w */
function buildLayers(w) {
  const layers = [
    { type: 'icon', name: iconForCondition(w.condition), x: 0, y: 0 },
    {
      type: 'native',
      label: '{temp}F',
      x: 12,
      y: 0,
      color: tempColor(w.temp),
      large: true,
      spacing: 1,
    },
  ];
  const particles = weatherParticles(w.condition, w.wind_speed, w.wind_deg);
  if (particles) {
    particles.opacity = 100;
    layers.push(particles);
  }
  if (w.temp > 95) layers.push(heatShimmer(w.temp));
  return layers;
}

export const routes = (app, config) => {
  const API_KEY = process.env.OWM_API_KEY;
  const CITY = process.env.OWM_CITY || 'New York';
  const UNITS = config.temp_unit === 'C' ? 'metric' : 'imperial';

  /** @returns {Promise<void>} */
  async function fetchWeather() {
    if (!API_KEY) return;
    try {
      const url = `https://api.openweathermap.org/data/2.5/weather?q=${encodeURIComponent(CITY)}&appid=${API_KEY}&units=${UNITS}`;
      const data = await (await fetch(url)).json();
      if (data.weather && data.main) {
        weatherCache = {
          condition: data.weather[0].id,
          temp: Math.round(data.main.temp),
          humidity: data.main.humidity,
          wind_speed: data.wind?.speed || 0,
          wind_deg: data.wind?.deg || 0,
          description: data.weather[0].description,
          updated: Date.now(),
        };
        console.log(
          `[weather] ${CITY}: ${weatherCache.temp}°${config.temp_unit} (${weatherCache.description})`,
        );
        if (config.pushAlert) config.pushAlert('weather', weatherCache);
      }
    } catch (e) {
      console.error('[weather] fetch error:', e.message);
    }
  }

  if (API_KEY) {
    fetchWeather();
    setInterval(fetchWeather, 10 * 60 * 1000);
  } else console.log('  [weather] No OWM_API_KEY set, using defaults');

  app.get('/data/weather', (req, res) =>
    res.json({
      temp: weatherCache.temp,
      humidity: weatherCache.humidity,
      condition: weatherCache.condition,
      description: weatherCache.description || 'clear',
    }),
  );
};
