/**
 * Campfire fire + spark particle layers.
 * @module api/screens/campfire-fire
 */

/** @returns {object[]} */
export function fireLayers() {
  return [
    {
      type: 'pixels',
      pattern: 'dots',
      x: 13,
      y: 6,
      color: '777777',
      points: [
        [0, 0],
        [1, 1],
        [5, 0],
        [6, 1],
        [2, 1],
        [4, 1],
      ],
    },
    {
      type: 'particles',
      gravity: -8,
      edge: 'die',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, 'FFFFFF'],
          [0.1, 'FFFF44'],
          [0.3, 'FFAA00'],
          [0.5, 'FF4400'],
          [0.7, 'CC0000'],
          [1, '330000'],
        ],
      },
      emitters: [
        {
          x: 16,
          y: 6,
          vx_min: -1.2,
          vx_max: 1.2,
          vy_min: -6,
          vy_max: -2,
          rate: 16,
          life_min: 250,
          life_max: 600,
          size: 1,
        },
      ],
    },
    {
      type: 'particles',
      gravity: -2,
      edge: 'die',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, 'FFFF88'],
          [1, 'FF8800'],
        ],
      },
      emitters: [
        {
          x: 16,
          y: 5,
          vx_min: -3,
          vx_max: 3,
          vy_min: -6,
          vy_max: -3,
          rate: 0.8,
          life_min: 400,
          life_max: 800,
          size: 1,
        },
      ],
    },
  ];
}
