exports.name = 'Water Fill';
exports.enabled = true;
exports.priority = 1;
exports.tags = ['fun', 'ambient'];

// Water drops fall and accumulate at the bottom.
// Uses a mask that builds up as the "water level" — but since we can't
// dynamically update the mask, we simulate accumulation by using very long
// lifetime + bounce at bottom with heavy damping so particles settle.
exports.screen = () => ({
  duration: 15000,
  layers: [
    { type: 'particles', gravity: 8, edge: 'bounce',
      colors: { min: 0, max: 1, stops: [[0,'2244FF'],[0.3,'44AAFF'],[0.7,'2266FF'],[1,'1133AA']] },
      emitters: [
        // Drops from top
        { x: -1, y: 0, vx_min: -0.2, vx_max: 0.2, vy_min: 2, vy_max: 5, rate: 10, life_min: 12000, life_max: 15000, size: 1 },
      ],
      // Mask: floor at bottom to catch particles
      mask: '................................' +
            '................................' +
            '................................' +
            '................................' +
            '................................' +
            '................................' +
            '................................' +
            '################################',
    },
  ],
});
