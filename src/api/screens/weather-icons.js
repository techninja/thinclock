/**
 * Weather screen icon definitions.
 * @module api/screens/weather-icons
 */

import { makeIcon } from '../lib/icons.js';

export const weatherIcons = {
  weather_sun: {
    width: 8,
    height: 8,
    fps: 0,
    data: [
      makeIcon(
        [
          '..#..#..',
          '...##...',
          '.######.',
          '.######.',
          '########',
          '.######.',
          '...##...',
          '..#..#..',
        ],
        0xff,
        0xcc,
        0x00,
      ),
    ],
  },
  weather_cloud: {
    width: 8,
    height: 8,
    fps: 0,
    data: [
      makeIcon(
        [
          '........',
          '..###...',
          '.#####..',
          '########',
          '########',
          '.######.',
          '........',
          '........',
        ],
        0xaa,
        0xaa,
        0xbb,
      ),
    ],
  },
  weather_cloud_sun: {
    width: 8,
    height: 8,
    fps: 0,
    data: [
      makeIcon(
        [
          '...Y....',
          '..##.Y..',
          '.####Y..',
          '########',
          '########',
          '.######.',
          '........',
          '........',
        ],
        0xaa,
        0xaa,
        0xbb,
        { Y: 'FFCC00' },
      ),
    ],
  },
  weather_storm: {
    width: 8,
    height: 8,
    fps: 2,
    data: [
      makeIcon(
        [
          '..###...',
          '.#####..',
          '########',
          '########',
          '...Y....',
          '..Y.....',
          '.Y......',
          '........',
        ],
        0x66,
        0x66,
        0x88,
        { Y: 'FFFF00' },
      ),
      makeIcon(
        [
          '..###...',
          '.#####..',
          '########',
          '########',
          '....Y...',
          '...Y....',
          '..Y.....',
          '........',
        ],
        0x66,
        0x66,
        0x88,
        { Y: 'FFFF44' },
      ),
    ],
  },
  weather_snow_icon: {
    width: 8,
    height: 8,
    fps: 0,
    data: [
      makeIcon(
        [
          '..###...',
          '.#####..',
          '########',
          '########',
          '.######.',
          '........',
          '........',
          '........',
        ],
        0xbb,
        0xbb,
        0xcc,
      ),
    ],
  },
};
