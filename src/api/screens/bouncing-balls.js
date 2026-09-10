export const name = 'Bouncing Balls';
export const enabled = true;
export const priority = 2;
export const tags = ['fun', 'ambient'];

export const screen = () => ({
  duration: 10000,
  layers: [
    {
      type: 'particles',
      gravity: 20,
      edge: 'bounce',
      colors: {
        min: 0,
        max: 1,
        stops: [
          [0, 'FF0000'],
          [0.16, 'FF8800'],
          [0.33, 'FFFF00'],
          [0.5, '00FF00'],
          [0.66, '0088FF'],
          [0.83, '8800FF'],
          [1, 'FF00FF'],
        ],
      },
      emitters: [
        {
          x: 8,
          y: 2,
          vx_min: -6,
          vx_max: 6,
          vy_min: -8,
          vy_max: 2,
          rate: 1.5,
          life_min: 5000,
          life_max: 10000,
          size: 2,
        },
        {
          x: 24,
          y: 2,
          vx_min: -6,
          vx_max: 6,
          vy_min: -8,
          vy_max: 2,
          rate: 1.5,
          life_min: 5000,
          life_max: 10000,
          size: 2,
        },
      ],
    },
  ],
});
