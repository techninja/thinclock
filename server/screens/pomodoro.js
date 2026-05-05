const { makeIcon } = require('../lib/icons');

exports.name = 'Pomodoro';
exports.enabled = true;
exports.priority = 5;
exports.tags = ['utility', 'timer'];
exports.contextAction = 'pomodoro';

exports.icons = {
  tomato: {
    width: 8, height: 8, fps: 0,
    data: [makeIcon([
      '..GGGG..',
      '..G##G..',
      '.######.',
      '########',
      '########',
      '########',
      '.######.',
      '..####..',
    ], 0xCC, 0x22, 0x00, { G: '22AA00' })],
  },
};

// Pomodoro state
let state = {
  phase: 'idle',     // 'idle', 'work', 'break', 'long_break'
  cycle: 0,
  startedAt: null,
};

const WORK_MS = 25 * 60 * 1000;
const BREAK_MS = 5 * 60 * 1000;
const LONG_BREAK_MS = 15 * 60 * 1000;

function getPhaseColor() {
  switch (state.phase) {
    case 'work': return 'FF8800';
    case 'break': return '00CC44';
    case 'long_break': return '0088FF';
    default: return '888888';
  }
}

function getNextPhaseLabel() {
  // What happens when you press the button
  if (state.phase === 'idle' || state.phase === 'break' || state.phase === 'long_break') {
    return '25:00';  // next is work
  } else {
    // Currently working — next is break
    return state.cycle % 4 === 3 ? '15:00' : '05:00';
  }
}

exports.screen = (config) => {
  const color = getPhaseColor();

  return {
    duration: 30000,
    layers: [
      { type: 'icon', name: 'tomato', x: 0, y: 0 },
      // Progress bar across bottom
      { type: 'gauge', style: 'hbar', x: 0, y: 7, width: 32, height: 1,
        value_key: 'progress',
        range: { min: 0, max: 100, stops: [[0, color], [1, color]] } },
      // Timer countdown (rendered from device timer, updates every frame)
      { type: 'clock', format: 'timer', x: 11, y: 1, color, large: false, spacing: 1 },
    ],
  };
};

exports.routes = (app, config) => {
  function startNext() {
    if (state.phase === 'idle' || state.phase === 'break' || state.phase === 'long_break') {
      state.phase = 'work';
      state.startedAt = Date.now();
    } else if (state.phase === 'work') {
      state.cycle++;
      if (state.cycle % 4 === 0) {
        state.phase = 'long_break';
      } else {
        state.phase = 'break';
      }
      state.startedAt = Date.now();
    }
    console.log(`[pomodoro] ${state.phase} (cycle ${state.cycle})`);

    // Also push a timer to the device for the breathing dot
    const duration = state.phase === 'work' ? WORK_MS :
                     state.phase === 'long_break' ? LONG_BREAK_MS : BREAK_MS;
    const color = getPhaseColor();
    try {
      const http = require('http');
      const data = JSON.stringify({ duration, color });
      const req = http.request('http://192.168.86.60/timer', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': data.length },
      });
      req.write(data);
      req.end();
    } catch (e) {}
  }

  function getRemaining() {
    if (!state.startedAt || state.phase === 'idle') return getNextPhaseLabel();
    let duration;
    switch (state.phase) {
      case 'work': duration = WORK_MS; break;
      case 'break': duration = BREAK_MS; break;
      case 'long_break': duration = LONG_BREAK_MS; break;
      default: return '00:00';
    }
    const elapsed = Date.now() - state.startedAt;
    const remaining = Math.max(0, duration - elapsed);

    if (remaining === 0 && state.phase !== 'idle') {
      startNext();
    }

    const mins = Math.floor(remaining / 60000);
    const secs = Math.floor((remaining / 1000) % 60);
    return `${String(mins).padStart(2,'0')}:${String(secs).padStart(2,'0')}`;
  }

  function getProgress() {
    if (!state.startedAt || state.phase === 'idle') return 0;
    let duration;
    switch (state.phase) {
      case 'work': duration = WORK_MS; break;
      case 'break': duration = BREAK_MS; break;
      case 'long_break': duration = LONG_BREAK_MS; break;
      default: return 0;
    }
    const elapsed = Date.now() - state.startedAt;
    return Math.min(100, (elapsed / duration) * 100);
  }

  app.get('/data/pomodoro', (req, res) => {
    res.json({
      phase: state.phase,
      display: getRemaining(),
      progress: Math.round(getProgress()),
      cycle: state.cycle,
    });
  });

  app.post('/pomodoro/toggle', (req, res) => {
    startNext();
    res.json({ ok: true, phase: state.phase, cycle: state.cycle });
  });
};
