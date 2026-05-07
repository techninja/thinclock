import { makeIcon } from '../lib/icons.js';

export const name = 'Campfire';
export const enabled = true;
export const priority = 2;
export const tags = ['fun', 'ambient'];
export const schedule = {
  hours: [18, 19, 20, 21, 22, 23],
};

// Moon phase calculation (0-7: new, waxing crescent, first quarter, waxing gibbous, full, waning gibbous, last quarter, waning crescent)
function getMoonPhase() {
  const now = new Date();
  // Simple approximation: lunar cycle ~29.53 days
  // Known new moon: Jan 6, 2000
  const knownNew = new Date(2000, 0, 6).getTime();
  const cycle = 29.53;
  const daysSince = (now.getTime() - knownNew) / (1000 * 60 * 60 * 24);
  const phase = ((daysSince % cycle) / cycle);
  return Math.floor(phase * 8) % 8;
}

function moonIcon(phase) {
  // 5x5 moon icons for different phases
  const moons = [
    // 0: new (dark)
    ['.....', '.###.', '#####', '.###.', '.....'],
    // 1: waxing crescent
    ['...#.', '..##.', '.###.', '..##.', '...#.'],
    // 2: first quarter
    ['..###', '..###', '..###', '..###', '..###'],
    // 3: waxing gibbous
    ['.####', '.####', '#####', '.####', '.####'],
    // 4: full
    ['.###.', '#####', '#####', '#####', '.###.'],
    // 5: waning gibbous
    ['####.', '####.', '#####', '####.', '####.'],
    // 6: last quarter
    ['###..', '###..', '###..', '###..', '###..'],
    // 7: waning crescent
    ['.#...', '.##..', '.###.', '.##..', '.#...'],
  ];
  return makeIcon(moons[phase], 0xFF, 0xFF, 0xCC);
}

export const icons = {};

export const screen = (config) => {
  const phase = getMoonPhase();
  // Dynamically set moon icon
  exports.icons.moon_phase = {
    width: 5, height: 5, fps: 0,
    data: [moonIcon(phase)],
  };

  return {
    duration: 12000,
    layers: [
      // Night sky gradient (full top area)
      { type: 'gradient', x: 0, y: 0, width: 32, height: 6, direction: 'vertical',
        colors: { min: 0, max: 1, stops: [[0,'030308'],[0.5,'080818'],[1,'101028']] } },
      // Twinkling stars (additive blend — black=transparent, light adds to sky)
      { type: 'particles', gravity: 0, edge: 'wrap', blend: 'add',
        colors: { min: 0, max: 1, stops: [[0,'000000'],[0.3,'444466'],[0.7,'444466'],[1,'000000']] },
        emitters: [
          { x: -1, y: -1, vx_min: 0, vx_max: 0, vy_min: 0, vy_max: 0, rate: 1.5, life_min: 2000, life_max: 4000, size: 1 },
        ],
      },
      // Moon (phase-accurate)
      { type: 'icon', name: 'moon_phase', x: 26, y: 0 },
      // Bumpy ground
      { type: 'gradient', x: 0, y: 6, width: 32, height: 2, direction: 'horizontal',
        colors: { min: 0, max: 1, stops: [[0,'332200'],[0.2,'2A1A00'],[0.4,'3A2800'],[0.6,'2A1A00'],[0.8,'332200'],[1,'2A1A00']] } },
      // Tent (triangle on the left)
      { type: 'pixels', pattern: 'dots', x: 3, y: 2, color: '886644',
        points: [[2,0],[1,1],[3,1],[0,2],[4,2],[0,3],[1,3],[2,3],[3,3],[4,3]] },
      // Stone ring around fire
      { type: 'pixels', pattern: 'dots', x: 13, y: 6, color: '777777',
        points: [[0,0],[1,1],[5,0],[6,1],[2,1],[4,1]] },
      // Fire
      { type: 'particles', gravity: -8, edge: 'die',
        colors: { min: 0, max: 1, stops: [[0,'FFFFFF'],[0.1,'FFFF44'],[0.3,'FFAA00'],[0.5,'FF4400'],[0.7,'CC0000'],[1,'330000']] },
        emitters: [
          { x: 16, y: 6, vx_min: -1.2, vx_max: 1.2, vy_min: -6, vy_max: -2, rate: 16, life_min: 250, life_max: 600, size: 1 },
        ],
      },
      // Sparks
      { type: 'particles', gravity: -2, edge: 'die',
        colors: { min: 0, max: 1, stops: [[0,'FFFF88'],[1,'FF8800']] },
        emitters: [
          { x: 16, y: 5, vx_min: -3, vx_max: 3, vy_min: -6, vy_max: -3, rate: 0.8, life_min: 400, life_max: 800, size: 1 },
        ],
      },
    ],
  };
};
