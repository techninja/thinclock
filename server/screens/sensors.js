exports.name = 'Sensor Dashboard';
exports.enabled = true;
exports.priority = 8;
exports.tags = ['utility', 'sensors'];

exports.screen = () => ({
  duration: 8000,
  data_url: 'self://sensors',
  layers: [
    { type: 'gauge', style: 'vbar', x: 0, y: 0, width: 8, height: 8,
      value_key: 'light',
      range: { min: 0, max: 100, stops: [[0,'222244'],[0.3,'FFAA00'],[1,'FFFF88']] } },
    { type: 'native', label: '{temperature}F', x: 10, y: 1, color: 'FF8800', large: false, spacing: 1 },
  ],
});
