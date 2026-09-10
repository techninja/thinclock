/**
 * Weather data layer — cache, HA entity subscription, OWM fallback.
 * Consumed by weather.js screen module.
 */

export let weatherCache = {
  condition: 500,
  temp: 58,
  humidity: 77,
  wind_speed: 5,
  wind_deg: 270,
  updated: 0,
};

const HA_CONDITION_MAP = {
  'clear-night': 800,
  sunny: 800,
  partlycloudy: 802,
  cloudy: 804,
  fog: 741,
  rainy: 500,
  pouring: 502,
  snowy: 601,
  'snowy-rainy': 611,
  windy: 771,
  'windy-variant': 771,
  hail: 511,
  lightning: 211,
  'lightning-rainy': 211,
  exceptional: 900,
};

export function fetchFromHA(config, haEntity) {
  if (!config.haAdapter) return false;
  const e = config.haAdapter.entities[haEntity];
  if (!e) return false;
  const attr = e.attributes || {};
  const tempRaw = attr.temperature ?? weatherCache.temp;
  const tempF =
    attr.temperature_unit === '°C' ? Math.round((tempRaw * 9) / 5 + 32) : Math.round(tempRaw);
  weatherCache = {
    condition: HA_CONDITION_MAP[e.state] ?? 800,
    temp: config.temp_unit === 'C' ? Math.round(tempRaw) : tempF,
    humidity: attr.humidity ?? weatherCache.humidity,
    wind_speed: attr.wind_speed ?? weatherCache.wind_speed,
    wind_deg: attr.wind_bearing ?? weatherCache.wind_deg,
    description: e.state,
    updated: Date.now(),
  };
  return true;
}

export async function fetchFromOWM(apiKey, city, units, config) {
  if (!apiKey) return;
  try {
    const url = `https://api.openweathermap.org/data/2.5/weather?q=${encodeURIComponent(city)}&appid=${apiKey}&units=${units}`;
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
      console.log(`[weather] OWM ${city}: ${weatherCache.temp}° (${weatherCache.description})`);
      if (config.pushAlert) config.pushAlert('weather', weatherCache);
    }
  } catch (e) {
    console.error('[weather] fetch error:', e.message);
  }
}

/** Wire up polling and HA/OWM subscriptions. Returns fetchWeather for immediate use. */
export function setupWeatherFetching(config) {
  const API_KEY = process.env.OWM_API_KEY;
  const CITY = process.env.OWM_CITY || 'New York';
  const UNITS = config.temp_unit === 'C' ? 'metric' : 'imperial';
  const HA_ENTITY = process.env.WEATHER_ENTITY || 'weather.home';

  async function fetchWeather() {
    if (fetchFromHA(config, HA_ENTITY)) {
      console.log(`[weather] HA ${HA_ENTITY}: ${weatherCache.temp}° (${weatherCache.description})`);
      if (config.pushAlert) config.pushAlert('weather', weatherCache);
    } else {
      await fetchFromOWM(API_KEY, CITY, UNITS, config);
    }
  }

  if (config.haAdapter || API_KEY) {
    setInterval(fetchWeather, 10 * 60 * 1000);
    if (config.haAdapter) config.haAdapter.onEntity(HA_ENTITY, fetchWeather);
    else fetchWeather();
  } else {
    console.log('  [weather] No HA adapter or OWM_API_KEY — using defaults');
  }

  return fetchWeather;
}
