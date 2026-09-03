/**
 * Icon/sprite renderer — draws icons from hex RGB data.
 * @module utils/renderer/icon
 */

import { setPixel } from './core.js';

/**
 * Render an icon layer from the icon registry.
 * @param {Uint8Array} buf
 * @param {object} layer - {name, x, y}
 * @param {object} icons - {name: {width, height, data: [hexString]}}
 */
export function renderIcon(buf, layer, icons) {
  const { name, x = 0, y = 0 } = layer;
  const icon = icons[name];
  if (!icon || !icon.data || !icon.data[0]) return;

  const { width, height } = icon;
  const hex = icon.data[0]; // first frame

  for (let row = 0; row < height; row++) {
    for (let col = 0; col < width; col++) {
      const offset = (row * width + col) * 6; // 6 hex chars per pixel (RRGGBB)
      const r = parseInt(hex.slice(offset, offset + 2), 16) || 0;
      const g = parseInt(hex.slice(offset + 2, offset + 4), 16) || 0;
      const b = parseInt(hex.slice(offset + 4, offset + 6), 16) || 0;
      // Black = transparent
      if (r || g || b) {
        setPixel(buf, x + col, y + row, r, g, b);
      }
    }
  }
}
