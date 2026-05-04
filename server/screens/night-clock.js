exports.name = 'Night Clock';
exports.enabled = true;
exports.priority = 10;
exports.tags = ['utility', 'clock', 'night'];
exports.schedule = {
  hours: [22, 23, 0, 1, 2, 3, 4, 5, 6],
};

exports.screen = (config) => ({
  duration: 30000,
  layers: [
    { type: 'clock', format: config.time_format, x: 4, y: 0, color: 'AA0000', large: true, spacing: 1 },
    { type: 'particles', gravity: -0.5, edge: 'wrap', opacity: 120,
      colors: { min: 0, max: 1, stops: [[0,'440000'],[0.5,'880000'],[1,'330000']] },
      emitters: [
        { x: -1, y: 7, vx_min: -0.3, vx_max: 0.3, vy_min: -1, vy_max: -0.3, rate: 2, life_min: 4000, life_max: 8000, size: 1 },
      ],
    },
  ],
});
