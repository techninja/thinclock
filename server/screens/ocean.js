exports.name = 'Ocean Waves';
exports.enabled = true;
exports.priority = 2;
exports.tags = ['ambient', 'fun'];

exports.screen = () => ({
  duration: 12000,
  layers: [
    // Sky
    { type: 'gradient', x: 0, y: 0, width: 32, height: 3, direction: 'vertical',
      colors: { min: 0, max: 1, stops: [[0,'112244'],[1,'224466']] } },
    // Deep water
    { type: 'gradient', x: 0, y: 3, width: 32, height: 5, direction: 'vertical',
      colors: { min: 0, max: 1, stops: [[0,'003366'],[0.4,'002244'],[1,'001122']] } },
    // Wave crest (light blue line that oscillates up/down)
    { type: 'gradient', x: 0, y: 3, width: 32, height: 1, direction: 'horizontal',
      colors: { min: 0, max: 1, stops: [[0,'2266AA'],[0.3,'3388CC'],[0.6,'2266AA'],[1,'3388CC']] },
      tweens: [
        { prop: 'y', from: 2, to: 4, duration: 3000, easing: 'sine', loop: 'pingpong' },
      ] },
    // Foam/spray particles at the wave line
    { type: 'particles', gravity: 1, edge: 'die', blend: 'add',
      colors: { min: 0, max: 1, stops: [[0,'FFFFFF'],[0.3,'AADDFF'],[1,'000000']] },
      emitters: [
        { x: -1, y: 3, vx_min: -1, vx_max: 1, vy_min: -1.5, vy_max: 0.5, rate: 4, life_min: 500, life_max: 1200, size: 1 },
      ],
    },
    // Deeper bubbles
    { type: 'particles', gravity: -0.5, edge: 'die', blend: 'add',
      colors: { min: 0, max: 1, stops: [[0,'000000'],[0.3,'224488'],[0.7,'224488'],[1,'000000']] },
      emitters: [
        { x: -1, y: 7, vx_min: -0.3, vx_max: 0.3, vy_min: -1, vy_max: -0.3, rate: 2, life_min: 1500, life_max: 3000, size: 1 },
      ],
    },
  ],
});
