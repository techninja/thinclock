/**
 * Server-side GIF encoder for 32×8 LED frames.
 * @module api/lib/gif
 */

import { writeLZW } from './lzw.js';

const WIDTH = 32,
  HEIGHT = 8;

const PALETTE = Buffer.alloc(768);
let idx = 0;
for (let r = 0; r < 6; r++)
  for (let g = 0; g < 6; g++)
    for (let b = 0; b < 6; b++) {
      PALETTE[idx++] = r * 51;
      PALETTE[idx++] = g * 51;
      PALETTE[idx++] = b * 51;
    }
for (let i = 216; i < 256; i++) {
  const v = (i - 216) * 6 + 3;
  PALETTE[i * 3] = v;
  PALETTE[i * 3 + 1] = v;
  PALETTE[i * 3 + 2] = v;
}

/** @param {number} gamma @returns {Uint8Array} */
function buildGammaLUT(gamma) {
  const lut = new Uint8Array(256);
  for (let i = 0; i < 256; i++) lut[i] = Math.round(Math.pow(i / 255, 1 / gamma) * 255);
  return lut;
}

/** @param {number} r @param {number} g @param {number} b @returns {number} */
function colorToIndex(r, g, b) {
  return (
    Math.min(5, Math.round(r / 51)) * 36 +
    Math.min(5, Math.round(g / 51)) * 6 +
    Math.min(5, Math.round(b / 51))
  );
}

/** @param {Buffer} rgb @param {number} scale @param {number} gap @param {Uint8Array} lut @returns {Uint8Array} */
function buildScaledFrame(rgb, scale, gap, lut) {
  const outW = WIDTH * scale + (WIDTH - 1) * gap;
  const outH = HEIGHT * scale + (HEIGHT - 1) * gap;
  const indexed = new Uint8Array(outW * outH);
  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {
      const si = (y * WIDTH + x) * 3;
      const ci = colorToIndex(lut[rgb[si]], lut[rgb[si + 1]], lut[rgb[si + 2]]);
      const sx = x * (scale + gap),
        sy = y * (scale + gap);
      for (let dy = 0; dy < scale; dy++)
        for (let dx = 0; dx < scale; dx++) indexed[(sy + dy) * outW + (sx + dx)] = ci;
    }
  }
  return indexed;
}

/**
 * Encode raw RGB frame buffers into an animated GIF.
 * @param {Buffer[]} frameBufs
 * @param {number} scale
 * @param {number} gap
 * @param {number} gamma - gamma × 10 (18 = 1.8)
 * @returns {Buffer}
 */
export function encodeGif(frameBufs, scale = 5, gap = 1, gamma = 18) {
  const lut = buildGammaLUT(gamma / 10);
  const outW = WIDTH * scale + (WIDTH - 1) * gap;
  const outH = HEIGHT * scale + (HEIGHT - 1) * gap;
  const delay = 7;
  const out = [];

  out.push(0x47, 0x49, 0x46, 0x38, 0x39, 0x61);
  out.push(outW & 0xff, (outW >> 8) & 0xff, outH & 0xff, (outH >> 8) & 0xff);
  out.push(0xf7, 0, 0);
  for (let i = 0; i < 768; i++) out.push(PALETTE[i]);
  out.push(0x21, 0xff, 0x0b, ...Buffer.from('NETSCAPE2.0'), 0x03, 0x01, 0x00, 0x00, 0x00);

  let prev = null,
    pendingDelay = delay;

  /** @param {Uint8Array} indexed @param {number} d */
  function writeFrame(indexed, d) {
    out.push(0x21, 0xf9, 0x04, 0x04, d & 0xff, (d >> 8) & 0xff, 0x00, 0x00);
    out.push(
      0x2c,
      0,
      0,
      0,
      0,
      outW & 0xff,
      (outW >> 8) & 0xff,
      outH & 0xff,
      (outH >> 8) & 0xff,
      0x00,
    );
    writeLZW(indexed, out);
  }

  for (const buf of frameBufs) {
    const indexed = buildScaledFrame(buf, scale, gap, lut);
    if (prev && Buffer.from(indexed).equals(Buffer.from(prev))) {
      pendingDelay += delay;
      continue;
    }
    if (prev) {
      writeFrame(prev, pendingDelay);
      pendingDelay = delay;
    }
    prev = indexed;
  }
  if (prev) writeFrame(prev, pendingDelay);
  out.push(0x3b);
  return Buffer.from(out);
}
