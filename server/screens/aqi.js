exports.name = 'Air Quality';
exports.enabled = true;
exports.priority = 7;
exports.tags = ['utility', 'weather'];

exports.alerts = [
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

let aqiCache = { aqi: 42, category: 'Good', updated: 0 };

exports.screen = (config) => {
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
    duration: 30000,
    data_url: `${config.BASE}/data/aqi`,
    layers: [
      // Full-width EPA color gradient bar across bottom rows
      { type: 'gradient', x: 0, y: 5, width: 32, height: 3, direction: 'horizontal',
        colors: { min: 0, max: 1, stops: [[0,'00CC44'],[0.17,'FFCC00'],[0.33,'FF8800'],[0.5,'FF0000'],[0.67,'880088'],[1,'880044']] } },
      // White indicator line at current AQI position
      { type: 'pixels', pattern: 'vline', x: indicatorX, y: 5, color: 'FFFFFF' },
      // AQI value + label (centered)
      { type: 'native', label: 'AQI {aqi}', x: 7, y: 0, color: textColor, large: false, spacing: 1 },
    ],
  };
};

exports.routes = (app, config) => {
  const API_KEY = process.env.OWM_API_KEY;
  const CITY = process.env.OWM_CITY || 'New York';

  async function fetchAQI() {
    if (!API_KEY) return;
    try {
      const geoUrl = `https://api.openweathermap.org/geo/1.0/direct?q=${encodeURIComponent(CITY)}&limit=1&appid=${API_KEY}`;
      const geoResp = await fetch(geoUrl);
      const geoData = await geoResp.json();
      if (!geoData.length) return;

      const { lat, lon } = geoData[0];
      const aqiUrl = `https://api.openweathermap.org/data/2.5/air_pollution?lat=${lat}&lon=${lon}&appid=${API_KEY}`;
      const resp = await fetch(aqiUrl);
      const data = await resp.json();
      if (data.list && data.list[0]) {
        const pm25 = data.list[0].components.pm2_5;
        const epaAqi = pm25ToAqi(pm25);
        aqiCache = { aqi: epaAqi, category: aqiCategory(epaAqi), updated: Date.now() };
        console.log(`[aqi] ${CITY}: AQI ${epaAqi} (${aqiCache.category})`);
        if (config.pushAlert) config.pushAlert('aqi', aqiCache);
      }
    } catch (e) {
      console.error('[aqi] fetch error:', e.message);
    }
  }

  function pm25ToAqi(pm25) {
    const bp = [[0,12,0,50],[12.1,35.4,51,100],[35.5,55.4,101,150],[55.5,150.4,151,200],[150.5,250.4,201,300],[250.5,500,301,500]];
    for (const [cLow, cHigh, iLow, iHigh] of bp) {
      if (pm25 <= cHigh) return Math.round((iHigh - iLow) / (cHigh - cLow) * (pm25 - cLow) + iLow);
    }
    return 500;
  }

  function aqiCategory(aqi) {
    if (aqi <= 50) return 'Good';
    if (aqi <= 100) return 'Moderate';
    if (aqi <= 150) return 'Unhealthy (SG)';
    if (aqi <= 200) return 'Unhealthy';
    if (aqi <= 300) return 'Very Unhealthy';
    return 'Hazardous';
  }

  if (API_KEY) {
    fetchAQI();
    setInterval(fetchAQI, 30 * 60 * 1000);
  } else {
    console.log('  [aqi] No OWM_API_KEY set');
  }

  app.get('/data/aqi', (req, res) => {
    res.json({ aqi: aqiCache.aqi, category: aqiCache.category });
  });
};
