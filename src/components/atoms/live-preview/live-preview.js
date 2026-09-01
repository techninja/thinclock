/**
 * Live preview — receives framebuffer via WebSocket, renders with LED-style gaps.
 * @module components/atoms/live-preview
 */

import { html, define } from 'hybrids';

const WIDTH = 32;
const HEIGHT = 8;
const BYTES = WIDTH * HEIGHT * 3;
const PX = 9; // pixel size
const GAP = 1; // gap between pixels
const BEZEL = PX * 2 + GAP; // 2 "LED-sized" pixels as bezel
const CANVAS_W = WIDTH * PX + (WIDTH - 1) * GAP + BEZEL * 2;
const CANVAS_H = HEIGHT * PX + (HEIGHT - 1) * GAP + BEZEL * 2;

// Gamma LUT — boost low-mid tones to match LED perceived brightness
const GAMMA = 1.8;
const gammaLUT = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
  gammaLUT[i] = Math.round(Math.pow(i / 255, 1 / GAMMA) * 255);
}

/**
 *
 */
function paint(host, buf) {
  const canvas = host.querySelector('canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);

  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {
      const i = (y * WIDTH + x) * 3;
      const r = gammaLUT[buf[i]];
      const g = gammaLUT[buf[i + 1]];
      const b = gammaLUT[buf[i + 2]];
      if (r || g || b) {
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillRect(BEZEL + x * (PX + GAP), BEZEL + y * (PX + GAP), PX, PX);
      }
    }
  }
}

/**
 *
 */
function connect(host) {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const ws = new WebSocket(`${proto}//${location.host}/ws/framebuffer`);
  ws.binaryType = 'arraybuffer';
  ws.onmessage = (e) => {
    const buf = new Uint8Array(e.data);
    if (buf.length === BYTES) paint(host, buf);
  };
  ws.onclose = () => {
    if (host.isConnected) setTimeout(() => connect(host), 2000);
  };
  host._ws = ws;
}

export default define({
  tag: 'live-preview',
  _ws: {
    value: null,
    connect: (host) => {
      connect(host);
      return () => {
        if (host._ws) host._ws.close();
      };
    },
  },
  render: {
    value: () => html` <canvas width="${CANVAS_W}" height="${CANVAS_H}"></canvas> `,
    shadow: false,
  },
});
