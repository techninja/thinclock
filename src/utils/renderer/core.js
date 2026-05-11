/**
 * Core renderer — composites layers onto a 32×8 pixel buffer.
 * @module utils/renderer/core
 */

import { renderGradient } from './gradient.js';
import { renderPixels } from './pixels.js';
import { renderText, renderClock } from './text.js';
import { renderIcon } from './icon.js';
import { renderParticles } from './particles.js';

export const WIDTH = 32;
export const HEIGHT = 8;

/** Create a blank framebuffer (32×8 RGB array) */
export function createBuffer() {
  return new Uint8Array(WIDTH * HEIGHT * 3);
}

/** Set a pixel in the buffer (black = transparent for sprites) */
export function setPixel(buf, x, y, r, g, b) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  const i = (y * WIDTH + x) * 3;
  buf[i] = r;
  buf[i + 1] = g;
  buf[i + 2] = b;
}

/** Additive blend a pixel */
export function addPixel(buf, x, y, r, g, b) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  const i = (y * WIDTH + x) * 3;
  buf[i] = Math.min(255, buf[i] + r);
  buf[i + 1] = Math.min(255, buf[i + 1] + g);
  buf[i + 2] = Math.min(255, buf[i + 2] + b);
}

/** Parse hex color string to [r, g, b] */
export function parseColor(hex) {
  if (!hex) return [0, 0, 0];
  hex = hex.replace('#', '');
  return [
    parseInt(hex.slice(0, 2), 16) || 0,
    parseInt(hex.slice(2, 4), 16) || 0,
    parseInt(hex.slice(4, 6), 16) || 0,
  ];
}

/** Interpolate color from gradient stops */
export function colorFromStops(stops, val) {
  if (!stops || stops.length === 0) return [255, 255, 255];
  if (val <= stops[0][0]) return parseColor(stops[0][1]);
  if (val >= stops[stops.length - 1][0]) return parseColor(stops[stops.length - 1][1]);
  for (let i = 0; i < stops.length - 1; i++) {
    const [p0, c0] = stops[i];
    const [p1, c1] = stops[i + 1];
    if (val >= p0 && val <= p1) {
      const t = (val - p0) / (p1 - p0);
      const a = parseColor(c0), b = parseColor(c1);
      return [
        Math.round(a[0] + (b[0] - a[0]) * t),
        Math.round(a[1] + (b[1] - a[1]) * t),
        Math.round(a[2] + (b[2] - a[2]) * t),
      ];
    }
  }
  return parseColor(stops[stops.length - 1][1]);
}

const RENDERERS = {
  gradient: renderGradient,
  pixels: renderPixels,
  native: renderText,
  clock: renderClock,
  icon: renderIcon,
  particles: renderParticles,
};

/**
 * Render all layers of a screen onto a buffer.
 * @param {object} screen - Screen definition with layers[]
 * @param {object} icons - Icon registry {name: {width, height, data}}
 * @param {object} [state] - Particle state for animation
 * @returns {Uint8Array} 32×8×3 RGB buffer
 */
export function renderScreen(screen, icons = {}, state = {}) {
  const buf = createBuffer();
  if (!screen?.layers) return buf;
  for (const layer of screen.layers) {
    const fn = RENDERERS[layer.type];
    if (fn) fn(buf, layer, icons, state);
  }
  return buf;
}
