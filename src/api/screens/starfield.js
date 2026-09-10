export const name = 'Starfield';
export const enabled = true;
export const priority = 1;
export const tags = ['fun', 'ambient'];

// Key insight: particles at speed 5px/s need ~3.2s to reach edge (16px).
// At speed 12px/s they need ~1.3s. Lifetime must exceed travel time.
// Color ramp: starts black, ends white — so they're invisible at center
// and bright at the edges. The built-in 25% fade-out handles exit.

export const screen = () => ({
  duration: 12000,
  layers: [
    {
      type: 'particles',
      gravity: 0,
      edge: 'die',
      warmup: 5000,
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, '000000'],
          [0.1, '000000'],
          [0.4, '333355'],
          [0.7, '8899CC'],
          [0.9, 'FFFFFF'],
          [1, 'FFFFFF'],
        ],
      },
      emitters: [
        // Mix of speeds — all live long enough to reach edges
        {
          x: 16,
          y: 4,
          vx_min: -3,
          vx_max: 3,
          vy_min: -1.5,
          vy_max: 1.5,
          rate: 3,
          life_min: 5000,
          life_max: 8000,
          size: 1,
        },
        {
          x: 16,
          y: 4,
          vx_min: -7,
          vx_max: 7,
          vy_min: -3.5,
          vy_max: 3.5,
          rate: 4,
          life_min: 2500,
          life_max: 4000,
          size: 1,
        },
        {
          x: 16,
          y: 4,
          vx_min: -14,
          vx_max: 14,
          vy_min: -7,
          vy_max: 7,
          rate: 3,
          life_min: 1500,
          life_max: 2500,
          size: 1,
        },
      ],
    },
  ],
});
