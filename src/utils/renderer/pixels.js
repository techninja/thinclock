/**
 * Pixels/dots layer renderer.
 * @module utils/renderer/pixels
 */

import { setPixel, parseColor } from './core.js';

/**
 * Render a pixels layer.
 * @param {Uint8Array} buf
 * @param {object} layer - {pattern, x, y, color, dim_color}
 */
export function renderPixels(buf, layer) {
  const { pattern, x = 0, y = 0, color = 'FFFFFF', dim_color } = layer;
  const [r, g, b] = parseColor(color);

  if (pattern === 'vline') {
    for (let row = 0; row < 3; row++) {
      setPixel(buf, x, y + row, r, g, b);
    }
  } else if (pattern === 'week_dots') {
    const dow = new Date().getDay();
    const [dr, dg, db] = dim_color ? parseColor(dim_color) : [r >> 2, g >> 2, b >> 2];
    for (let i = 0; i < 7; i++) {
      const px = x + i * 3;
      if (i <= dow) {
        setPixel(buf, px, y, r, g, b);
      } else {
        setPixel(buf, px, y, dr, dg, db);
      }
    }
  } else if (pattern === 'hline') {
    for (let col = 0; col < 3; col++) {
      setPixel(buf, x + col, y, r, g, b);
    }
  } else if (pattern === 'dot') {
    setPixel(buf, x, y, r, g, b);
  }
}
