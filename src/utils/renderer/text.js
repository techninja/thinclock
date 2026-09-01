/**
 * Native text renderer — 3×5 and 5×7 pixel fonts.
 * Ported from firmware display.cpp bitmask fonts.
 * @module utils/renderer/text
 */

import { setPixel, parseColor } from './core.js';

// 3x5 digits: columns as bitmask, LSB = top row
const FONT_3X5 = [
  [0x1f, 0x11, 0x1f],
  [0x00, 0x1f, 0x00],
  [0x1d, 0x15, 0x17],
  [0x15, 0x15, 0x1f],
  [0x07, 0x04, 0x1f],
  [0x17, 0x15, 0x1d],
  [0x1f, 0x15, 0x1d],
  [0x01, 0x01, 0x1f],
  [0x1f, 0x15, 0x1f],
  [0x17, 0x15, 0x1f],
];

// 3x5 alpha A-Z
const FONT_3X5_ALPHA = [
  [0x1e, 0x05, 0x1e],
  [0x1f, 0x15, 0x0a],
  [0x0e, 0x11, 0x11],
  [0x1f, 0x11, 0x0e],
  [0x1f, 0x15, 0x11],
  [0x1f, 0x05, 0x01],
  [0x0e, 0x11, 0x19],
  [0x1f, 0x04, 0x1f],
  [0x11, 0x1f, 0x11],
  [0x08, 0x10, 0x0f],
  [0x1f, 0x04, 0x1b],
  [0x1f, 0x10, 0x10],
  [0x1f, 0x02, 0x1f],
  [0x1f, 0x06, 0x1f],
  [0x0e, 0x11, 0x0e],
  [0x1f, 0x05, 0x02],
  [0x0e, 0x19, 0x1e],
  [0x1f, 0x05, 0x1a],
  [0x12, 0x15, 0x09],
  [0x01, 0x1f, 0x01],
  [0x0f, 0x10, 0x0f],
  [0x07, 0x18, 0x07],
  [0x1f, 0x08, 0x1f],
  [0x1b, 0x04, 0x1b],
  [0x03, 0x1c, 0x03],
  [0x19, 0x15, 0x13],
];

// 5x7 digits
const FONT_5X7 = [
  [0x7f, 0x41, 0x41, 0x41, 0x7f],
  [0x00, 0x42, 0x7f, 0x40, 0x00],
  [0x79, 0x49, 0x49, 0x49, 0x4f],
  [0x41, 0x49, 0x49, 0x49, 0x7f],
  [0x0f, 0x08, 0x08, 0x08, 0x7f],
  [0x4f, 0x49, 0x49, 0x49, 0x79],
  [0x7f, 0x49, 0x49, 0x49, 0x79],
  [0x01, 0x01, 0x01, 0x01, 0x7f],
  [0x7f, 0x49, 0x49, 0x49, 0x7f],
  [0x4f, 0x49, 0x49, 0x49, 0x7f],
];

/**
 *
 */
function drawGlyph(buf, x, y, cols, rows, r, g, b) {
  for (let col = 0; col < cols.length; col++) {
    for (let row = 0; row < rows; row++) {
      if (cols[col] & (1 << row)) setPixel(buf, x + col, y + row, r, g, b);
    }
  }
}

/**
 *
 */
function drawChar(buf, ch, cx, y, r, g, b, large) {
  if (ch >= '0' && ch <= '9') {
    const d = ch.charCodeAt(0) - 48;
    const font = large ? FONT_5X7[d] : FONT_3X5[d];
    drawGlyph(buf, cx, y, font, large ? 7 : 5, r, g, b);
    return large ? 6 : 4;
  }
  const upper = ch.toUpperCase();
  if (upper >= 'A' && upper <= 'Z' && !large) {
    drawGlyph(buf, cx, y, FONT_3X5_ALPHA[upper.charCodeAt(0) - 65], 5, r, g, b);
    return 4;
  }
  if (ch === ':') {
    setPixel(buf, cx, y + (large ? 2 : 1), r, g, b);
    setPixel(buf, cx, y + (large ? 4 : 3), r, g, b);
    return 2;
  }
  if (ch === '.') {
    setPixel(buf, cx, y + (large ? 6 : 4), r, g, b);
    return 2;
  }
  if (ch === ' ') return 2;
  if (ch === '-') {
    setPixel(buf, cx, y + 2, r, g, b);
    setPixel(buf, cx + 1, y + 2, r, g, b);
    setPixel(buf, cx + 2, y + 2, r, g, b);
    return 4;
  }
  return 2;
}

/** Render a native text layer */
export function renderText(buf, layer) {
  const { label = '', x = 0, y = 0, color = 'FFFFFF', large = false, spacing = 1 } = layer;
  const [r, g, b] = parseColor(color);
  let cx = x;
  for (const ch of label) {
    cx += drawChar(buf, ch, cx, y, r, g, b, large) + spacing - 1;
  }
}

/** Render a clock layer (uses current time) */
export function renderClock(buf, layer) {
  const { x = 0, y = 0, color = 'FFFFFF', large = false, spacing = 1, format = '12h' } = layer;
  const now = new Date();
  let h = now.getHours();
  if (format === '12h') h = h % 12 || 12;
  const timeStr = `${h}:${String(now.getMinutes()).padStart(2, '0')}`;
  renderText(buf, { label: timeStr, x, y, color, large, spacing });
}
