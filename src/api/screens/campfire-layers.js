/**
 * Campfire scene layer definitions — sky, stars, ground, tent.
 * @module api/screens/campfire-layers
 */

import { fireLayers } from './campfire-fire.js';

/** @returns {object[]} */
export function campfireLayers() {
  return [
    {
      type: 'gradient',
      x: 0,
      y: 0,
      width: 32,
      height: 6,
      direction: 'vertical',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, '030308'],
          [0.5, '080818'],
          [1, '101028'],
        ],
      },
    },
    {
      type: 'particles',
      gravity: 0,
      edge: 'wrap',
      blend: 'add',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, '000000'],
          [0.3, '444466'],
          [0.7, '444466'],
          [1, '000000'],
        ],
      },
      emitters: [
        {
          x: -1,
          y: -1,
          vx_min: 0,
          vx_max: 0,
          vy_min: 0,
          vy_max: 0,
          rate: 1.5,
          life_min: 2000,
          life_max: 4000,
          size: 1,
        },
      ],
    },
    { type: 'icon', name: 'moon_phase', x: 26, y: 0 },
    {
      type: 'gradient',
      x: 0,
      y: 6,
      width: 32,
      height: 2,
      direction: 'horizontal',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, '332200'],
          [0.2, '2A1A00'],
          [0.4, '3A2800'],
          [0.6, '2A1A00'],
          [0.8, '332200'],
          [1, '2A1A00'],
        ],
      },
    },
    {
      type: 'pixels',
      pattern: 'dots',
      x: 3,
      y: 2,
      color: '886644',
      points: [
        [2, 0],
        [1, 1],
        [3, 1],
        [0, 2],
        [4, 2],
        [0, 3],
        [1, 3],
        [2, 3],
        [3, 3],
        [4, 3],
      ],
    },
    ...fireLayers(),
  ];
}
