/**
 * Screen Editor — create/edit custom screens with layer stack + live preview.
 * @module pages/editor
 */

import { html, define, router } from 'hybrids';
import '#molecules/app-nav/index.js';
import { devicePost } from '#utils/device.js';
import { LAYER_TYPES, newLayer, layerSummary } from '#utils/layers.js';

function addLayer(host) {
  const screen = { ...host._screen };
  screen.layers = [...screen.layers, newLayer(host._addType || 'native')];
  host._screen = screen;
}

function removeLayer(host, event) {
  const idx = parseInt(event.currentTarget.dataset.idx);
  const screen = { ...host._screen };
  screen.layers = screen.layers.filter((_, i) => i !== idx);
  host._screen = screen;
}

function moveLayer(host, event) {
  const idx = parseInt(event.currentTarget.dataset.idx);
  const dir = parseInt(event.currentTarget.dataset.dir);
  const screen = { ...host._screen };
  const layers = [...screen.layers];
  const newIdx = idx + dir;
  if (newIdx < 0 || newIdx >= layers.length) return;
  [layers[idx], layers[newIdx]] = [layers[newIdx], layers[idx]];
  screen.layers = layers;
  host._screen = screen;
}

function updateJson(host, event) {
  try {
    host._screen = JSON.parse(event.target.value);
    host._jsonError = '';
  } catch (e) { host._jsonError = e.message; }
}

function saveScreen(host) {
  const id = host._screenId || `screen-${Date.now()}`;
  const data = { ...host._screen, id, name: host._screen.name || id };
  fetch(`/api/custom-screens/${id}`, {
    method: 'PUT', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  }).then(() => { host._status = 'Saved!'; host._screenId = id; });
}

function pushToDevice(host) {
  devicePost('/render', { ...host._screen, frames: 1, display: true })
    .then(() => { host._status = 'Pushed!'; });
}

function selectLayer(host, event) {
  host._selectedLayer = parseInt(event.currentTarget.dataset.idx);
}

export default define({
  tag: 'editor-view',
  [router.connect]: { url: '/editor' },
  _screen: { value: { name: 'New Screen', duration: 10000, layers: [] }, connect: () => {} },
  _screenId: '',
  _selectedLayer: -1,
  _addType: 'native',
  _jsonError: '',
  _status: '',
  _showJson: false,
  render: {
    value: (host) => {
      const { _screen, _selectedLayer, _jsonError, _status, _showJson } = host;
      const layers = _screen.layers || [];
      return html`
        <app-nav></app-nav>
        <div class="page-editor">
          <div class="editor-header">
            <h1>Screen Editor</h1>
            <div class="editor-actions">
              <button class="btn btn-primary" onclick="${saveScreen}">Save</button>
              <button class="btn btn-secondary" onclick="${pushToDevice}">Push to Device</button>
              <button class="btn btn-ghost" onclick="${(h) => { h._showJson = !h._showJson; }}">
                ${_showJson ? 'Visual' : 'JSON'}
              </button>
            </div>
          </div>
          ${_status ? html`<p class="success-message">${_status}</p>` : html``}
          ${_showJson ? html`
            <div class="json-editor">
              <textarea class="json-textarea"
                value="${JSON.stringify(_screen, null, 2)}"
                oninput="${updateJson}"></textarea>
              ${_jsonError ? html`<p class="error-message">${_jsonError}</p>` : html``}
            </div>
          ` : html`
            <div class="editor-layout">
              <div class="editor-layers">
                <div class="form-group">
                  <label>Screen Name</label>
                  <input type="text" value="${_screen.name || ''}"
                    oninput="${(h, e) => { h._screen = { ...h._screen, name: e.target.value }; }}" />
                </div>
                <h3>Layers (${layers.length})</h3>
                <ul class="layer-stack">
                  ${layers.map((layer, idx) => html`
                    <li class="layer-item ${idx === _selectedLayer ? 'selected' : ''}"
                        data-idx="${idx}" onclick="${selectLayer}">
                      <span class="layer-type">${layer.type}</span>
                      <span class="layer-summary">${layerSummary(layer)}</span>
                      <div class="layer-controls">
                        <button class="btn-icon" data-idx="${idx}" data-dir="-1" onclick="${moveLayer}">↑</button>
                        <button class="btn-icon" data-idx="${idx}" data-dir="1" onclick="${moveLayer}">↓</button>
                        <button class="btn-icon btn-danger-icon" data-idx="${idx}" onclick="${removeLayer}">×</button>
                      </div>
                    </li>
                  `)}
                </ul>
                <div class="add-layer">
                  <select value="${host._addType}" onchange="${html.set('_addType')}">
                    ${LAYER_TYPES.map(t => html`<option value="${t}">${t}</option>`)}
                  </select>
                  <button class="btn btn-secondary" onclick="${addLayer}">Add Layer</button>
                </div>
              </div>
              <div class="editor-preview card">
                <h3>Preview</h3>
                <p class="hint">Save and push to see on device</p>
              </div>
            </div>
          `}
        </div>
      `;
    },
    shadow: false,
  },
});
