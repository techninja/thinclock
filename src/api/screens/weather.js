import { makeIcon } from '../lib/icons.js';

export const name = 'Weather';
export const enabled = true;
export const priority = 9;
export const tags = ['utility', 'weather'];
export const contextAction = 'refresh';

export const alerts = [
  {
    id: 'rain_started',
    condition: (history) => {
      if (history.length < 2) return false;
      const prev = history[history.length - 2];
      const curr = history[history.length - 1];
      // Previous was NOT rain/drizzle/storm, current IS
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
      const curr = history[history.length - 1];
      return curr && curr.condition >= 200 && curr.condition < 300;
    },
    message: 'Severe weather!',
    color: 'FF0000',
    icon: 'weather_storm',
    beep: 'alert',
    cooldown: 3600000, // 1 hour
  },
];

export const icons = {
  weather_sun: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '..#..#..',
      '...##...',
      '.######.',
      '.######.',
      '########',
      '.######.',
      '...##...',
      '..#..#..',
    ], 0xFF, 0xCC, 0x00)],
  },
  weather_cloud: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '........',
      '..###...',
      '.#####..',
      '########',
      '########',
      '.######.',
      '........',
      '........',
    ], 0xAA, 0xAA, 0xBB)],
  },
  weather_cloud_sun: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '.....Y..',
      '..##.Y..',
      '.####Y..',
      '########',
      '########',
      '.######.',
      '........',
      '........',
    ], 0xAA, 0xAA, 0xBB, { Y: 'FFCC00' })],
  },
  weather_storm: {
    width: 8, height: 8, fps: 2,
    data: [
      makeIcon([
        '..###...',
        '.#####..',
        '########',
        '########',
        '...Y....',
        '..Y.....',
        '.Y......',
        '........',
      ], 0x66, 0x66, 0x88, { Y: 'FFFF00' }),
      makeIcon([
        '..###...',
        '.#####..',
        '########',
        '########',
        '....Y...',
        '...Y....',
        '..Y.....',
        '........',
      ], 0x66, 0x66, 0x88, { Y: 'FFFF44' }),
    ],
  },
  weather_snow_icon: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '..###...',
      '.#####..',
      '########',
      '########',
      '.######.',
      '........',
      '........',
      '........',
    ], 0xBB, 0xBB, 0xCC)],
  },
};

// Map OWM condition codes to icon + particle config
function getWeatherLayers(condition, temp, config, wind_speed, wind_deg) {
  const layers = [];
  let icon = 'weather_sun';
  let particles = null;

  // Wind → horizontal drift for rain/snow. Simplify to left/right.
  // OWM wind_deg is where wind comes FROM. Convert to push direction.
  // 0/360=N (pushes south, no horizontal), 90=E (pushes west/left), 270=W (pushes east/right)
  const windRad = (wind_deg || 0) * Math.PI / 180;
  const windDrift = -Math.sin(windRad) * Math.min(wind_speed || 0, 15) * 0.3;

  if (condition >= 200 && condition < 300) {
    // Thunderstorm
    icon = 'weather_storm';
    particles = {
      type: 'particles', gravity: 12, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'8888FF'],[0.5,'AACCFF'],[1,'4466AA']] },
      emitters: [
        { x: -1, y: 0, vx_min: windDrift - 0.5, vx_max: windDrift + 0.5, vy_min: 6, vy_max: 12, rate: 10, life_min: 400, life_max: 800, size: 1 },
      ],
    };
  } else if (condition >= 300 && condition < 400) {
    // Drizzle
    icon = 'weather_cloud';
    particles = {
      type: 'particles', gravity: 6, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'6688CC'],[1,'4466AA']] },
      emitters: [
        { x: -1, y: 0, vx_min: windDrift - 0.3, vx_max: windDrift + 0.3, vy_min: 3, vy_max: 6, rate: 6, life_min: 800, life_max: 1500, size: 1 },
      ],
    };
  } else if (condition >= 500 && condition < 600) {
    // Rain — intensity varies by code
    icon = 'weather_cloud';
    let rate = 6, vyMin = 4, vyMax = 8;
    if (condition === 500) { rate = 4; vyMin = 3; vyMax = 6; }       // light
    else if (condition === 501) { rate = 8; vyMin = 4; vyMax = 8; }  // moderate
    else if (condition === 502) { rate = 14; vyMin = 6; vyMax = 11; } // heavy
    else if (condition >= 503) { rate = 20; vyMin = 8; vyMax = 14; }  // very heavy/extreme
    particles = {
      type: 'particles', gravity: 10, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'4444FF'],[0.5,'88CCFF'],[1,'FFFFFF']] },
      emitters: [
        { x: -1, y: 0, vx_min: windDrift - 0.5, vx_max: windDrift + 0.5, vy_min: vyMin, vy_max: vyMax, rate, life_min: 400, life_max: 1000, size: 1 },
      ],
    };
  } else if (condition >= 600 && condition < 700) {
    // Snow
    icon = 'weather_snow_icon';
    particles = {
      type: 'particles', gravity: 2, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'FFFFFF'],[0.5,'CCDDFF'],[1,'8899BB']] },
      emitters: [
        { x: -1, y: 0, vx_min: windDrift - 0.8, vx_max: windDrift + 0.8, vy_min: 1, vy_max: 3, rate: 6, life_min: 2000, life_max: 4000, size: 1 },
      ],
    };
  } else if (condition >= 700 && condition < 800) {
    // Atmosphere (fog, mist)
    icon = 'weather_cloud';
  } else if (condition === 800) {
    // Clear
    icon = 'weather_sun';
  } else if (condition > 800) {
    // Cloudy
    icon = condition <= 802 ? 'weather_cloud_sun' : 'weather_cloud';
  }

  // Build layers: icon and text first, then particles on top with opacity
  layers.push({ type: 'icon', name: icon, x: 0, y: 0 });

  // Temperature: large font, centered, with degree symbol
  // Temperature color: blue(cold) → green(nice 67-72) → yellow → red(hot)
  const comfyLow = parseInt(process.env.TEMP_COMFY_LOW) || 67;
  const comfyHigh = parseInt(process.env.TEMP_COMFY_HIGH) || 72;
  let tempColor;
  if (temp >= comfyLow && temp <= comfyHigh) tempColor = '00CC44';      // green = nice
  else if (temp > comfyHigh && temp <= 80) tempColor = 'AACC00';        // yellow-green
  else if (temp > 80 && temp <= 90) tempColor = 'FFAA00';               // orange
  else if (temp > 90) tempColor = 'FF4400';                              // red = hot
  else if (temp < comfyLow && temp >= 55) tempColor = '44CCAA';         // teal
  else if (temp < 55 && temp >= 40) tempColor = '44AAFF';               // blue
  else tempColor = '4444FF';                                             // deep blue = cold
  layers.push({
    type: 'native', label: '{temp}F', x: 12, y: 0,
    color: tempColor, large: true, spacing: 1,
  });

  // Particles on top with transparency
  if (particles) {
    particles.opacity = 100;
    layers.push(particles);
  }

  // Heat shimmer when hot (>95F)
  if (temp > 95) {
    const heatRate = Math.min(20, 8 + (temp - 95) * 1.2);
    layers.push({
      type: 'particles', gravity: -3, edge: 'die', opacity: 80,
      colors: { min: 0, max: 1, stops: [[0,'FF4400'],[0.5,'FF8800'],[1,'FFAA00']] },
      emitters: [
        { x: -1, y: 7, vx_min: -0.5, vx_max: 0.5, vy_min: -2, vy_max: -0.5, rate: heatRate, life_min: 500, life_max: 1200, size: 1 },
      ],
    });
  }

  return layers;
}

// Cache weather data
let weatherCache = { condition: 500, temp: 58, humidity: 77, wind_speed: 5, wind_deg: 270, updated: 0 };

export const screen = (config) => ({
  duration: 30000,
  data_url: `${config.BASE}/data/weather`,
  layers: getWeatherLayers(weatherCache.condition, weatherCache.temp, config, weatherCache.wind_speed, weatherCache.wind_deg),
});

export const routes = (app, config) => {
  const API_KEY = process.env.OWM_API_KEY;
  const CITY = process.env.OWM_CITY || 'New York';
  const UNITS = config.temp_unit === 'C' ? 'metric' : 'imperial';

  // Fetch weather periodically
  async function fetchWeather() {
    if (!API_KEY) return;
    try {
      const url = `https://api.openweathermap.org/data/2.5/weather?q=${encodeURIComponent(CITY)}&appid=${API_KEY}&units=${UNITS}`;
      const resp = await fetch(url);
      const data = await resp.json();
      if (data.weather && data.main) {
        weatherCache = {
          condition: data.weather[0].id,
          temp: Math.round(data.main.temp),
          humidity: data.main.humidity,
          wind_speed: data.wind ? data.wind.speed : 0,
          wind_deg: data.wind ? data.wind.deg : 0,
          description: data.weather[0].description,
          updated: Date.now(),
        };
        console.log(`[weather] ${CITY}: ${weatherCache.temp}°${config.temp_unit} (${weatherCache.description})`);
        // Push to alert engine
        if (config.pushAlert) config.pushAlert('weather', weatherCache);
      }
    } catch (e) {
      console.error('[weather] fetch error:', e.message);
    }
  }

  // Fetch on start and every 10 minutes
  if (API_KEY) {
    fetchWeather();
    setInterval(fetchWeather, 10 * 60 * 1000);
  } else {
    console.log('  [weather] No OWM_API_KEY set, using defaults');
  }

  // Data endpoint for the device
  app.get('/data/weather', (req, res) => {
    res.json({
      temp: weatherCache.temp,
      humidity: weatherCache.humidity,
      condition: weatherCache.condition,
      description: weatherCache.description || 'clear',
    });
  });
};
