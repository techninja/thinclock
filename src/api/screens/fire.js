export const name = 'Fire';
export const enabled = true;
export const priority = 2;
export const tags = ['fun', 'ambient'];

// Fire: dense particles rising from the bottom with warm colors.
// Young particles are bright white/yellow (base of flame),
// aging to orange then red then dark (tips).
// Slight random horizontal drift for flicker.
export const screen = () => ({
  duration: 12000,
  layers: [
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
        // Wide base of fire across bottom
        {
          x: -1,
          y: 7,
          vx_min: -1.5,
          vx_max: 1.5,
          vy_min: -6,
          vy_max: -2,
          rate: 25,
          life_min: 300,
          life_max: 800,
          size: 1,
        },
        // Occasional larger sparks
        {
          x: -1,
          y: 7,
          vx_min: -2,
          vx_max: 2,
          vy_min: -8,
          vy_max: -4,
          rate: 3,
          life_min: 400,
          life_max: 1000,
          size: 1,
        },
      ],
    },
  ],
});
