/**
 * Pomodoro HTTP routes — data endpoint and toggle.
 * @module api/screens/pomodoro-routes
 */

import http from 'http';

/**
 * Register pomodoro routes against shared state.
 * @param {object} app
 * @param {object} state
 * @param {Function} getPhaseColor
 * @param {Function} getNextPhaseLabel
 */
export function registerPomodoroRoutes(app, state, getPhaseColor, getNextPhaseLabel) {
  const WORK_MS = 25 * 60 * 1000;
  const BREAK_MS = 5 * 60 * 1000;
  const LONG_BREAK_MS = 15 * 60 * 1000;
  /** @returns {void} */
  function startNext() {
    if (state.phase === 'idle' || state.phase === 'break' || state.phase === 'long_break') {
      state.phase = 'work';
    } else {
      state.cycle++;
      state.phase = state.cycle % 4 === 0 ? 'long_break' : 'break';
    }
    state.startedAt = Date.now();
    console.log(`[pomodoro] ${state.phase} (cycle ${state.cycle})`);
    const duration =
      state.phase === 'work' ? WORK_MS : state.phase === 'long_break' ? LONG_BREAK_MS : BREAK_MS;
    const data = JSON.stringify({ duration, color: getPhaseColor() });
    try {
      const req = http.request(`http://${process.env.DEVICE_IP || '192.168.86.60'}/timer`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': data.length },
      });
      req.write(data);
      req.end();
    } catch {
      /* device unreachable */
    }
  }

  /** @returns {string} */
  function getRemaining() {
    if (!state.startedAt || state.phase === 'idle') return getNextPhaseLabel();
    const durations = { work: WORK_MS, break: BREAK_MS, long_break: LONG_BREAK_MS };
    const duration = durations[state.phase];
    if (!duration) return '00:00';
    const remaining = Math.max(0, duration - (Date.now() - state.startedAt));
    if (remaining === 0) startNext();
    return `${String(Math.floor(remaining / 60000)).padStart(2, '0')}:${String(Math.floor((remaining / 1000) % 60)).padStart(2, '0')}`;
  }

  /** @returns {number} */
  function getProgress() {
    if (!state.startedAt || state.phase === 'idle') return 0;
    const durations = { work: WORK_MS, break: BREAK_MS, long_break: LONG_BREAK_MS };
    const duration = durations[state.phase];
    return duration ? Math.min(100, ((Date.now() - state.startedAt) / duration) * 100) : 0;
  }

  app.get('/data/pomodoro', (req, res) =>
    res.json({
      phase: state.phase,
      display: getRemaining(),
      progress: Math.round(getProgress()),
      cycle: state.cycle,
    }),
  );

  app.post('/pomodoro/toggle', (req, res) => {
    startNext();
    res.json({ ok: true, phase: state.phase, cycle: state.cycle });
  });
}
