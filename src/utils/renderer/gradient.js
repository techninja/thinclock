/**
 * Gradient layer renderer.
 * @module utils/renderer/gradient
 */

import { setPixel, colorFromStops } from './core.js';

/**
 * Render a gradient layer.
 * @param {Uint8Array} buf
 * @param {object} layer - {x, y, width, height, direction, colors: {stops}}
 */
export function renderGradient(buf, layer) {
  const { x = 0, y = 0, width = 32, height = 8, direction = 'horizontal', colors } = layer;
  if (!colors?.stops) return;
  const stops = colors.stops;
  const isHoriz = direction === 'horizontal';
  const span = isHoriz ? width : height;

  for (let row = 0; row < height; row++) {
    for (let col = 0; col < width; col++) {
      const t = span > 1 ? (isHoriz ? col : row) / (span - 1) : 0;
      const [r, g, b] = colorFromStops(stops, t);
      setPixel(buf, x + col, y + row, r, g, b);
    }
  }
}
