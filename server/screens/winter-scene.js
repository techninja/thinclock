const { makeIcon, makeLine } = require('../lib/icons');

exports.name = 'Winter Scene';
exports.enabled = true;
exports.priority = 3;
exports.tags = ['seasonal', 'ambient', 'holiday'];
exports.schedule = {
  months: [11, 12, 1, 2],  // Nov through Feb
};

exports.icons = {
  snowman: {
    width: 6, height: 8, fps: 0,
    data: [makeIcon([
      '..WW..',
      '.WWWW.',
      '.WWWW.',
      '..WW..',
      '.WWWW.',
      'WWWWWW',
      'WWWWWW',
      '.WWWW.',
    ], 0xFF, 0xFF, 0xFF)],
  },
  tree: {
    width: 8, height: 8, fps: 3,
    data: [
      makeIcon([
        '...Y....',
        '..G#G...',
        '.GG#GG..',
        'GG#GG#G.',
        '.GGGGGG.',
        'GG#GG#G.',
        '...T....',
        'WWWWWWWW',
      ], 0x00, 0xAA, 0x00, { '#': 'FF0000', G: '00AA00' }),
      makeIcon([
        '...Y....',
        '..GGG...',
        '.G#GG#..',
        'GGG#GGG.',
        '.G#GG#G.',
        'GGGGGGG.',
        '...T....',
        'WWWWWWWW',
      ], 0x00, 0xAA, 0x00, { '#': '0044FF', G: '00AA00' }),
      makeIcon([
        '...Y....',
        '..G#G...',
        '.GGGGG..',
        'G#GGGG#.',
        '.GGGGG..',
        'G#GG#GG.',
        '...T....',
        'WWWWWWWW',
      ], 0x00, 0xAA, 0x00, { '#': 'FF00FF', G: '00AA00' }),
    ],
  },
  ground: {
    width: 32, height: 1, fps: 0,
    data: [makeLine(32)],
  },
};

exports.screen = () => ({
  duration: 12000,
  layers: [
    { type: 'gradient', x: 0, y: 0, width: 32, height: 7, direction: 'vertical',
      colors: { min: 0, max: 1, stops: [[0,'112244'],[0.6,'334466'],[1,'445566']] } },
    { type: 'icon', name: 'ground', x: 0, y: 7 },
    { type: 'icon', name: 'snowman', x: 2, y: 0 },
    { type: 'icon', name: 'tree', x: 22, y: 0 },
    { type: 'particles', gravity: 3, edge: 'die',
      colors: { min: 0, max: 1, stops: [[0,'FFFFFF'],[0.5,'CCDDFF'],[1,'8899CC']] },
      emitters: [
        { x: -1, y: 0, vx_min: -0.3, vx_max: 0.3, vy_min: 1, vy_max: 3, rate: 6, life_min: 2000, life_max: 4000, size: 1 },
      ],
    },
  ],
});
