exports.name = 'Rain';
exports.enabled = false;  // use weather screen's dynamic rain instead
exports.priority = 2;
exports.tags = ['ambient', 'weather'];

exports.screen = () => ({
  duration: 10000,
  layers: [
    { type: 'particles', gravity: 10, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'4444FF'],[0.5,'88CCFF'],[1,'FFFFFF']] },
      emitters: [
        { x: -1, y: 0, vx_min: -0.3, vx_max: 0.3, vy_min: 4, vy_max: 10, rate: 14, life_min: 600, life_max: 1500, size: 1 },
      ],
    },
  ],
});
