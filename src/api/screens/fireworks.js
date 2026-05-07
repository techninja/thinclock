export const name = 'Fireworks';
export const enabled = true;
export const priority = 3;
export const tags = ['fun', 'seasonal', 'holiday'];
export const schedule = {
  dateRange: [701, 704],  // July 1-4
};

export const screen = () => ({
  duration: 12000,
  layers: [
    { type: 'particles', gravity: 14, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'FFFFFF'],[0.15,'FFFF00'],[0.3,'FF8800'],[0.5,'FF0044'],[0.7,'FF00FF'],[0.85,'4400FF'],[1,'0088FF']] },
      emitters: [
        { x: -1, y: 8, vx_min: -1.5, vx_max: 1.5, vy_min: -11, vy_max: -9, rate: 0.7, life_min: 800, life_max: 1200, size: 1, rocket: true },
      ],
    },
  ],
});
