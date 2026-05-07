export const name = 'Lava Lamp';
export const enabled = true;
export const priority = 1;
export const tags = ['fun', 'ambient'];

export const screen = () => ({
  duration: 10000,
  layers: [
    { type: 'particles', gravity: -2, edge: 'wrap',
      colors: { min: 0, max: 1, stops: [[0,'FF2200'],[0.3,'FF8800'],[0.6,'FFCC00'],[1,'FF4400']] },
      emitters: [
        { x: -1, y: 7, vx_min: -1, vx_max: 1, vy_min: -4, vy_max: -1, rate: 4, life_min: 2000, life_max: 5000, size: 2 },
      ],
    },
  ],
});
