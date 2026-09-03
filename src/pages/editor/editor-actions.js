/**
 * Editor action handlers — layer mutations, save, push.
 * @module pages/editor/editor-actions
 */

import { devicePost } from '#utils/device.js';
import { newLayer } from '#utils/layers.js';

/** @param {object} host */
export function addLayer(host) {
  host._screen = {
    ...host._screen,
    layers: [...host._screen.layers, newLayer(host._addType || 'native')],
  };
}

/** @param {object} host @param {Event} event */
export function removeLayer(host, event) {
  const idx = parseInt(event.currentTarget.dataset.idx);
  host._screen = { ...host._screen, layers: host._screen.layers.filter((_, i) => i !== idx) };
}

/** @param {object} host @param {Event} event */
export function moveLayer(host, event) {
  const idx = parseInt(event.currentTarget.dataset.idx);
  const dir = parseInt(event.currentTarget.dataset.dir);
  const layers = [...host._screen.layers];
  const newIdx = idx + dir;
  if (newIdx < 0 || newIdx >= layers.length) return;
  [layers[idx], layers[newIdx]] = [layers[newIdx], layers[idx]];
  host._screen = { ...host._screen, layers };
}

/** @param {object} host @param {Event} event */
export function updateJson(host, event) {
  try {
    host._screen = JSON.parse(event.target.value);
    host._jsonError = '';
  } catch (e) {
    host._jsonError = e.message;
  }
}

/** @param {object} host */
export function saveScreen(host) {
  const id = host._screenId || `screen-${Date.now()}`;
  const data = { ...host._screen, id, name: host._screen.name || id };
  fetch(`/api/custom-screens/${id}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  }).then(() => {
    host._status = 'Saved!';
    host._screenId = id;
  });
}

/** @param {object} host */
export function pushToDevice(host) {
  devicePost('/render', { ...host._screen, frames: 1, display: true }).then(() => {
    host._status = 'Pushed!';
  });
}

/** @param {object} host @param {Event} event */
export function selectLayer(host, event) {
  host._selectedLayer = parseInt(event.currentTarget.dataset.idx);
}
