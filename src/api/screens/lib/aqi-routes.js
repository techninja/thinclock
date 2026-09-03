/**
 * AQI data fetching and routes.
 * @module api/screens/aqi-routes
 */

/** @param {number} pm25 @returns {number} */
function pm25ToAqi(pm25) {
  const bp = [
    [0, 12, 0, 50],
    [12.1, 35.4, 51, 100],
    [35.5, 55.4, 101, 150],
    [55.5, 150.4, 151, 200],
    [150.5, 250.4, 201, 300],
    [250.5, 500, 301, 500],
  ];
  for (const [cLow, cHigh, iLow, iHigh] of bp)
    if (pm25 <= cHigh) return Math.round(((iHigh - iLow) / (cHigh - cLow)) * (pm25 - cLow) + iLow);
  return 500;
}

/** @param {number} aqi @returns {string} */
function aqiCategory(aqi) {
  if (aqi <= 50) return 'Good';
  if (aqi <= 100) return 'Moderate';
  if (aqi <= 150) return 'Unhealthy (SG)';
  if (aqi <= 200) return 'Unhealthy';
  if (aqi <= 300) return 'Very Unhealthy';
  return 'Hazardous';
}

/**
 * Register AQI fetch + data route.
 * @param {object} app
 * @param {object} config
 * @param {{ aqi: number, category: string, updated: number }} cache - shared mutable cache ref
 */
export function registerAqiRoutes(app, config, cache) {
  const API_KEY = process.env.OWM_API_KEY;
  const CITY = process.env.OWM_CITY || 'New York';

  /** @returns {Promise<void>} */
  async function fetchAQI() {
    if (!API_KEY) return;
    try {
      const geo = await (
        await fetch(
          `https://api.openweathermap.org/geo/1.0/direct?q=${encodeURIComponent(CITY)}&limit=1&appid=${API_KEY}`,
        )
      ).json();
      if (!geo.length) return;
      const { lat, lon } = geo[0];
      const data = await (
        await fetch(
          `https://api.openweathermap.org/data/2.5/air_pollution?lat=${lat}&lon=${lon}&appid=${API_KEY}`,
        )
      ).json();
      if (data.list?.[0]) {
        const epaAqi = pm25ToAqi(data.list[0].components.pm2_5);
        Object.assign(cache, { aqi: epaAqi, category: aqiCategory(epaAqi), updated: Date.now() });
        console.log(`[aqi] ${CITY}: AQI ${epaAqi} (${cache.category})`);
        if (config.pushAlert) config.pushAlert('aqi', { ...cache });
      }
    } catch (e) {
      console.error('[aqi] fetch error:', e.message);
    }
  }

  if (API_KEY) {
    fetchAQI();
    setInterval(fetchAQI, 30 * 60 * 1000);
  } else console.log('  [aqi] No OWM_API_KEY set');

  app.get('/data/aqi', (req, res) => res.json({ aqi: cache.aqi, category: cache.category }));
}
