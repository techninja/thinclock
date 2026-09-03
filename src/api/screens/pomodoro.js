import { makeIcon } from '../lib/icons.js';

export const name = 'Pomodoro';
export const enabled = true;
export const priority = 5;
export const tags = ['utility', 'timer'];
export const contextAction = 'pomodoro';
export const schedule = 'work';

export const icons = {
  tomato: {
    width: 8,
    height: 8,
    fps: 0,
    data: [
      makeIcon(
        [
          '..GGGG..',
          '..G##G..',
          '.######.',
          '########',
          '########',
          '########',
          '.######.',
          '..####..',
        ],
        0xcc,
        0x22,
        0x00,
        { G: '22AA00' },
      ),
    ],
  },
};

// Pomodoro state
const state = {
  phase: 'idle', // 'idle', 'work', 'break', 'long_break'
  cycle: 0,
  startedAt: null,
};

const WORK_MS = 25 * 60 * 1000;
const BREAK_MS = 5 * 60 * 1000;
const LONG_BREAK_MS = 15 * 60 * 1000;
function getPhaseColor() {
  switch (state.phase) {
    case 'work':
      return 'FF8800';
    case 'break':
      return '00CC44';
    case 'long_break':
      return '0088FF';
    default:
      return '888888';
  }
}
function getNextPhaseLabel() {
  if (state.phase === 'idle' || state.phase === 'break' || state.phase === 'long_break') {
    return '25:00';
  } else {
    return state.cycle % 4 === 3 ? '15:00' : '05:00';
  }
}
export const screen = (_config) => {
  const color = getPhaseColor();

  // Build phase indicator bar: 8 phases, 3px each + 1px black gap
  const phaseColors = [
    'FF8800',
    '00CC44',
    'FF8800',
    '00CC44',
    'FF8800',
    '00CC44',
    'FF8800',
    '0088FF',
  ];
  const currentPhaseIdx =
    state.phase === 'idle'
      ? -1
      : (() => {
          const cyclePos = state.cycle % 4;
          if (state.phase === 'work') return cyclePos * 2;
          if (state.phase === 'break') return (cyclePos - 1) * 2 + 1;
          if (state.phase === 'long_break') return 7;
          return -1;
        })();

  const layers = [
    { type: 'icon', name: 'tomato', x: 0, y: 0 },
    { type: 'clock', format: 'timer', x: 11, y: 1, color, large: false, spacing: 1 },
    // Black bar to cover tomato bottom bleeding through
    {
      type: 'pixels',
      pattern: 'dots',
      x: 0,
      y: 7,
      color: '000000',
      points: Array.from({ length: 32 }, (_, i) => [i, 0]),
    },
  ];

  // Add phase segments
  for (let i = 0; i < 8; i++) {
    const baseColor = phaseColors[i];
    let segColor;
    if (i === currentPhaseIdx) {
      segColor = baseColor;
    } else if (currentPhaseIdx >= 0 && i < currentPhaseIdx) {
      const r = parseInt(baseColor.slice(0, 2), 16) >> 2;
      const g = parseInt(baseColor.slice(2, 4), 16) >> 2;
      const b = parseInt(baseColor.slice(4, 6), 16) >> 2;
      segColor = [r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('');
    } else {
      const r = parseInt(baseColor.slice(0, 2), 16) >> 1;
      const g = parseInt(baseColor.slice(2, 4), 16) >> 1;
      const b = parseInt(baseColor.slice(4, 6), 16) >> 1;
      segColor = [r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('');
    }

    const points = [
      [0, 0],
      [1, 0],
      [2, 0],
    ].map(([px, py]) => [i * 4 + px, py]);
    const layer = { type: 'pixels', pattern: 'dots', x: 0, y: 7, color: segColor, points };

    // Current phase breathes
    if (i === currentPhaseIdx) {
      layer.opacity = 255;
      layer.tweens = [
        { prop: 'opacity', from: 80, to: 255, duration: 1500, easing: 'sine', loop: 'pingpong' },
      ];
    }

    layers.push(layer);
  }

  return { duration: 30000, layers };
};

import { registerPomodoroRoutes } from './lib/pomodoro-routes.js';
export const routes = (app, _config) =>
  registerPomodoroRoutes(app, state, getPhaseColor, getNextPhaseLabel);
