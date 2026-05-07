/**
 * Create an icon from ASCII art rows.
 * Characters: '#' = primary color, 'W' = white, 'T' = brown, 'Y' = gold,
 *             'B' = black (explicit), '.' = transparent (black)
 * Additional colors can be passed as a colorMap object.
 */
export function makeIcon(rows, r, g, b, colorMap = {}) {
  const primary = [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
  const defaults = { '#': primary, W: 'FFFFFF', T: '664400', Y: 'FFCC00', B: '000000', '.': '000000' };
  const map = { ...defaults, ...colorMap };

  let hex = '';
  for (const row of rows) {
    for (const ch of row) {
      hex += map[ch] || '000000';
    }
  }
  return hex;
}

/**
 * Create a solid color line icon of given width.
 */
export function makeLine(width, color = 'FFFFFF') {
  return color.repeat(width);
}

/**
 * Color range helper — creates the stops format.
 */
export function colorRange(min, max, stops) {
  return { min, max, stops };
}
