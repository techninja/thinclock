/**
 * Layer helpers — factory defaults and display summaries.
 * @module utils/layers
 */

const LAYER_TYPES = ['gradient', 'native', 'icon', 'pixels', 'clock', 'particles'];

/** Create a new layer with sensible defaults for the given type */
function newLayer(type) {
  const base = { type, x: 0, y: 0 };
  if (type === 'gradient') return { ...base, width: 32, height: 8, direction: 'horizontal', colors: { min: 0, max: 1, stops: [[0, 'FF0000'], [1, '0000FF']] } };
  if (type === 'native') return { ...base, label: 'Hello', color: 'FFFFFF', large: false, spacing: 1 };
  if (type === 'clock') return { ...base, x: 8, y: 1, format: '12h', color: '4488FF', large: false, spacing: 1 };
  if (type === 'pixels') return { ...base, pattern: 'dot', color: 'FFFFFF' };
  if (type === 'icon') return { ...base, name: '' };
  if (type === 'particles') return { ...base, gravity: 0, edge: 'die', colors: { min: 0, max: 1, stops: [[0, 'FFFFFF'], [1, '000000']] }, emitters: [{ x: -1, y: -1, vx_min: -1, vx_max: 1, vy_min: -1, vy_max: 1, rate: 3, life_min: 500, life_max: 1500, size: 1 }] };
  return base;
}

/** Short display summary for a layer in the stack */
function layerSummary(layer) {
  if (layer.type === 'native') return layer.label || '';
  if (layer.type === 'clock') return layer.format || '12h';
  if (layer.type === 'gradient') return layer.direction || '';
  if (layer.type === 'icon') return layer.name || '';
  if (layer.type === 'pixels') return layer.pattern || '';
  if (layer.type === 'particles') return `${layer.emitters?.length || 0} emitters`;
  return '';
}

export { LAYER_TYPES, newLayer, layerSummary };
