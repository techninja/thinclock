import { makeIcon } from '../lib/icons.js';

export const name = 'Network Monitor';
export const enabled = true;
export const priority = 6;
export const tags = ['utility', 'network'];

export const alerts = [
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

export const icons = {
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

export const screen = (config) => ({
  duration: 15000,
  data_url: 'self://ping',
  layers: [
    { type: 'icon', name: 'wifi_net', x: 0, y: 0 },
    // Ping value in ms
    { type: 'native', label: '{ping}', x: 10, y: 1, color: 'AAAAAA', large: false, spacing: 1 },
    // Signal bars (5 bars using dots, filled based on RSSI)
    // RSSI: -30=excellent, -50=good, -70=ok, -80=weak, -90=terrible
    // We'll render this as 5 vertical bars of increasing height
    { type: 'pixels', pattern: 'dots', x: 24, y: 7, color: '00FF44', points: [[0,0],[1,0]] },
    { type: 'pixels', pattern: 'dots', x: 26, y: 6, color: '00FF44', points: [[0,0],[1,0],[0,1],[1,1]] },
    { type: 'pixels', pattern: 'dots', x: 28, y: 5, color: '88FF00', points: [[0,0],[1,0],[0,1],[1,1],[0,2],[1,2]] },
    { type: 'pixels', pattern: 'dots', x: 30, y: 4, color: 'FFCC00', points: [[0,0],[1,0],[0,1],[1,1],[0,2],[1,2],[0,3],[1,3]] },
  ],
});
