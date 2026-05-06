const { makeIcon } = require('../lib/icons');

exports.name = 'Network Monitor';
exports.enabled = true;
exports.priority = 6;
exports.tags = ['utility', 'network'];

exports.alerts = [
  {
    id: 'internet_slow',
    condition: (history) => {
      if (history.length < 5) return false;
      return history.slice(-5).every(d => d.ping > 600 && d.ping > 0);
    },
    message: 'Slow internet',
    color: 'FFAA00',
    icon: 'wifi_net',
    beep: 'single',
    cooldown: 300000, // 5 min
  },
  {
    id: 'internet_down',
    condition: (history) => {
      if (history.length < 3) return false;
      return history.slice(-3).every(d => d.ping === 0 || d.status === 0);
    },
    message: 'Internet down!',
    color: 'FF0000',
    icon: 'wifi_net',
    beep: 'alert',
    cooldown: 60000, // 1 min
  },
];

exports.icons = {
  wifi_net: {
    width: 8, height: 8, fps: 0,
    // Key color FF8800 will be remapped based on ping value
    remap_key: 'FF8800',
    remap_value_key: 'ping',
    remap_range: {
      min: 0, max: 300,
      stops: [[0,'FF0000'],[0.01,'00FF44'],[0.1,'00FF44'],[0.27,'88FF00'],[0.5,'FFCC00'],[0.8,'FF8800'],[1,'FF2200']],
    },
    data: [makeIcon([
      '..####..',
      '.######.',
      '#......#',
      '..####..',
      '.#....#.',
      '...##...',
      '........',
      '...##...',
    ], 0xFF, 0x88, 0x00)],
  },
};

exports.screen = (config) => ({
  duration: 30000,
  data_url: 'self://ping',
  layers: [
    { type: 'icon', name: 'wifi_net', x: 0, y: 0 },
    { type: 'native', label: '{ping}', x: 10, y: 0, color: 'AAAAAA', large: false, spacing: 1 },
    { type: 'native', label: '{rssi}', x: 21, y: 0, color: '666688', large: false, spacing: 1 },
    // Signal strength bar along bottom
    { type: 'gauge', style: 'hbar', x: 9, y: 6, width: 22, height: 2,
      value_key: 'rssi',
      range: { min: -90, max: -30, stops: [[0,'FF2200'],[0.3,'FF8800'],[0.5,'FFCC00'],[0.7,'88FF00'],[1,'00FF44']] } },
  ],
});
