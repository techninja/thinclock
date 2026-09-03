/**
 * Weather particle layer builders by condition code.
 * @module api/screens/weather-particles
 */

/**
 * Build a particle layer for rain/snow/storm based on OWM condition + wind.
 * @param {number} condition - OWM condition code
 * @param {number} windSpeed
 * @param {number} windDeg
 * @returns {object|null}
 */
export function weatherParticles(condition, windSpeed, windDeg) {
  const windRad = ((windDeg || 0) * Math.PI) / 180;
  const drift = -Math.sin(windRad) * Math.min(windSpeed || 0, 15) * 0.3;

  if (condition >= 200 && condition < 300) {
    return rain('8888FF', 'AACCFF', '4466AA', drift - 0.5, drift + 0.5, 6, 12, 10, 400, 800);
  } else if (condition >= 300 && condition < 400) {
    return rain('6688CC', '4466AA', '4466AA', drift - 0.3, drift + 0.3, 3, 6, 6, 800, 1500);
  } else if (condition >= 500 && condition < 600) {
    const [rate, vyMin, vyMax] = rainIntensity(condition);
    return rain(
      '4444FF',
      '88CCFF',
      'FFFFFF',
      drift - 0.5,
      drift + 0.5,
      vyMin,
      vyMax,
      rate,
      400,
      1000,
    );
  } else if (condition >= 600 && condition < 700) {
    return {
      type: 'particles',
      gravity: 2,
      edge: 'die',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, 'FFFFFF'],
          [0.5, 'CCDDFF'],
          [1, '8899BB'],
        ],
      },
      emitters: [
        {
          x: -1,
          y: 0,
          vx_min: drift - 0.8,
          vx_max: drift + 0.8,
          vy_min: 1,
          vy_max: 3,
          rate: 6,
          life_min: 2000,
          life_max: 4000,
          size: 1,
        },
      ],
    };
  }
  return null;
}

/**
 * Heat shimmer particle layer for temps above 95°F.
 * @param {number} temp
 * @returns {object}
 */
export function heatShimmer(temp) {
  const rate = Math.min(20, 8 + (temp - 95) * 1.2);
  return {
    type: 'particles',
    gravity: -3,
    edge: 'die',
    opacity: 80,
    colors: {
      min: 0,
      max: 1,
      stops: [
        [0, 'FF4400'],
        [0.5, 'FF8800'],
        [1, 'FFAA00'],
      ],
    },
    emitters: [
      {
        x: -1,
        y: 7,
        vx_min: -0.5,
        vx_max: 0.5,
        vy_min: -2,
        vy_max: -0.5,
        rate,
        life_min: 500,
        life_max: 1200,
        size: 1,
      },
    ],
  };
}

/** @param {number} code @returns {[number,number,number]} */
function rainIntensity(code) {
  if (code === 500) return [4, 3, 6];
  if (code === 501) return [8, 4, 8];
  if (code === 502) return [14, 6, 11];
  return [20, 8, 14];
}

/** @returns {object} */
function rain(c1, c2, c3, vxMin, vxMax, vyMin, vyMax, rate, lifeMin, lifeMax) {
  return {
    type: 'particles',
    gravity: 10,
    edge: 'die',
    colors: {
      min: 0,
      max: 1,
      stops: [
        [0, c1],
        [0.5, c2],
        [1, c3],
      ],
    },
    emitters: [
      {
        x: -1,
        y: 0,
        vx_min: vxMin,
        vx_max: vxMax,
        vy_min: vyMin,
        vy_max: vyMax,
        rate,
        life_min: lifeMin,
        life_max: lifeMax,
        size: 1,
      },
    ],
  };
}
