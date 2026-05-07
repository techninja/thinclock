export const name = 'Night Clock';
export const enabled = true;
export const priority = 10;
export const tags = ['utility', 'clock', 'night'];
export const schedule = {
  hours: [22, 23, 0, 1, 2, 3, 4, 5, 6],
};

export const screen = (config) => {
  // Calculate sunrise progress during the last night hour
  // Night hours end at 7am (hour 6 is the last). During hour 6,
  // gradually shift from night to dawn.
  const now = new Date();
  const localHour = (now.getUTCHours() + config.timezone + 24) % 24;
  const localMin = now.getUTCMinutes();

  let sunriseProgress = 0; // 0 = full night, 1 = full dawn
  if (localHour === 6) {
    sunriseProgress = 0.3 + (localMin / 60) * 0.7; // 30%-100% during hour 6
  } else if (localHour === 5 && localMin >= 30) {
    sunriseProgress = ((localMin - 30) / 30) * 0.3; // 0%-30% in last half of hour 5
  }

  // Interpolate colors based on progress
  function lerpColor(c1, c2, t) {
    const r1 = parseInt(c1.slice(0,2),16), g1 = parseInt(c1.slice(2,4),16), b1 = parseInt(c1.slice(4,6),16);
    const r2 = parseInt(c2.slice(0,2),16), g2 = parseInt(c2.slice(2,4),16), b2 = parseInt(c2.slice(4,6),16);
    const r = Math.round(r1 + (r2-r1)*t), g = Math.round(g1 + (g2-g1)*t), b = Math.round(b1 + (b2-b1)*t);
    return [r,g,b].map(v => v.toString(16).padStart(2,'0')).join('');
  }

  // Night: pure black. Dawn: warm gradient from bottom
  const skyTop = lerpColor('000000', '1a0a2e', sunriseProgress);
  const skyMid = lerpColor('000000', '2d1b4e', sunriseProgress);
  const skyBot = lerpColor('000000', 'ff6b35', sunriseProgress);

  // Clock color shifts from deep red to warm orange at dawn
  const clockColor = lerpColor('AA0000', 'FF8844', sunriseProgress);

  // Particle color shifts warmer
  const particleBase = lerpColor('440000', '663300', sunriseProgress);
  const particleMid = lerpColor('880000', 'FF6600', sunriseProgress);

  const layers = [];

  // Sunrise gradient (fades in from bottom)
  if (sunriseProgress > 0) {
    layers.push({
      type: 'gradient', x: 0, y: 0, width: 32, height: 8, direction: 'vertical',
      colors: { min: 0, max: 1, stops: [[0, skyTop], [0.5, skyMid], [1, skyBot]] },
    });
  }

  // Clock
  layers.push({
    type: 'clock', format: config.time_format, x: 4, y: 0,
    color: clockColor, large: true, spacing: 1,
  });

  // Particles (shift warmer at dawn)
  layers.push({
    type: 'particles', gravity: -0.5, edge: 'wrap', opacity: 120, blend: 'add',
    colors: { min: 0, max: 1, stops: [[0, particleBase], [0.5, particleMid], [1, particleBase]] },
    emitters: [
      { x: -1, y: 7, vx_min: -0.3, vx_max: 0.3, vy_min: -1, vy_max: -0.3, rate: 2, life_min: 4000, life_max: 8000, size: 1 },
    ],
  });

  return { duration: 30000, layers };
};
