/**
 * Preview canvas — fetches device-rendered frames and plays them back.
 * Usage: <preview-canvas screen="3" frames="30"></preview-canvas>
 * @module components/atoms/preview-canvas
 */

import { html, define } from 'hybrids';

const WIDTH = 32;
const HEIGHT = 8;
const FRAME_BYTES = WIDTH * HEIGHT * 3;
const PX = 5;
const GAP = 1;
const CANVAS_W = WIDTH * (PX + GAP) - GAP;
const CANVAS_H = HEIGHT * (PX + GAP) - GAP;

const GAMMA = 1.8;
const gammaLUT = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
  gammaLUT[i] = Math.round(Math.pow(i / 255, 1 / GAMMA) * 255);
}
function paintFrame(ctx, buf, offset) {
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);
  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {
      const i = offset + (y * WIDTH + x) * 3;
      const r = gammaLUT[buf[i]];
      const g = gammaLUT[buf[i + 1]];
      const b = gammaLUT[buf[i + 2]];
      if (r || g || b) {
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillRect(x * (PX + GAP), y * (PX + GAP), PX, PX);
      }
    }
  }
}
async function loadFrames(host) {
  try {
    const resp = await fetch(`/api/device/preview?screen=${host.screen}&frames=${host.frames}`);
    if (!resp.ok) return;
    host._buf = new Uint8Array(await resp.arrayBuffer());
    host._frameCount = Math.floor(host._buf.length / FRAME_BYTES);
    host._frameMs = parseInt(resp.headers.get('X-Frame-Ms')) || 20;
    host._frame = 0;
    startPlayback(host);
  } catch {
    /* device offline */
  }
}
function startPlayback(host) {
  if (host._interval) clearInterval(host._interval);
  if (!host._buf || host._frameCount <= 1) {
    // Static: paint single frame
    const canvas = host.querySelector('canvas');
    if (canvas && host._buf) paintFrame(canvas.getContext('2d'), host._buf, 0);
    return;
  }
  host._interval = setInterval(() => {
    const canvas = host.querySelector('canvas');
    if (!canvas) return;
    paintFrame(canvas.getContext('2d'), host._buf, host._frame * FRAME_BYTES);
    host._frame = (host._frame + 1) % host._frameCount;
  }, host._frameMs);
}

export default define({
  tag: 'preview-canvas',
  screen: 0,
  frames: 30,
  _buf: { value: null, connect: () => {} },
  _frameCount: { value: 0, connect: () => {} },
  _frameMs: { value: 20, connect: () => {} },
  _frame: { value: 0, connect: () => {} },
  _interval: {
    value: null,
    connect: (host) => {
      requestAnimationFrame(() => loadFrames(host));
      return () => {
        if (host._interval) clearInterval(host._interval);
      };
    },
  },
  render: {
    value: () => html` <canvas width="${CANVAS_W}" height="${CANVAS_H}"></canvas> `,
    shadow: false,
  },
});
