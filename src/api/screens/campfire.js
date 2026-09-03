import { makeIcon } from '../lib/icons.js';
import { campfireLayers } from './lib/campfire-layers.js';

export const name = 'Campfire';
export const enabled = true;
export const priority = 2;
export const tags = ['fun', 'ambient'];
export const schedule = 'evening';

/** @returns {number} 0-7 moon phase index */
function getMoonPhase() {
  const knownNew = new Date(2000, 0, 6).getTime();
  const daysSince = (Date.now() - knownNew) / 86400000;
  return Math.floor(((daysSince % 29.53) / 29.53) * 8) % 8;
}

/** @param {number} phase @returns {string} */
function moonIcon(phase) {
  const moons = [
    ['.....', '.###.', '#####', '.###.', '.....'],
    ['...#.', '..##.', '.###.', '..##.', '...#.'],
    ['..###', '..###', '..###', '..###', '..###'],
    ['.####', '.####', '#####', '.####', '.####'],
    ['.###.', '#####', '#####', '#####', '.###.'],
    ['####.', '####.', '#####', '####.', '####.'],
    ['###..', '###..', '###..', '###..', '###..'],
    ['.#...', '.##..', '.###.', '.##..', '.#...'],
  ];
  return makeIcon(moons[phase], 0xff, 0xff, 0xcc);
}

export const icons = {};

export const screen = (_config) => {
  icons.moon_phase = { width: 5, height: 5, fps: 0, data: [moonIcon(getMoonPhase())] };
  return { duration: 12000, layers: campfireLayers() };
};
