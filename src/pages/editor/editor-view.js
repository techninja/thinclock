/**
 * Screen Editor — create/edit custom screens with layer stack + live preview.
 * @module pages/editor
 */

import { html, define, router } from 'hybrids';
import '#molecules/app-nav/index.js';
import { LAYER_TYPES, layerSummary } from '#utils/layers.js';
import {
  addLayer,
  removeLayer,
  moveLayer,
  updateJson,
  saveScreen,
  pushToDevice,
  selectLayer,
} from './editor-actions.js';

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
              <button
                class="btn btn-ghost"
                onclick="${(h) => {
                  h._showJson = !h._showJson;
                }}"
              >
                ${_showJson ? 'Visual' : 'JSON'}
              </button>
            </div>
          </div>
          ${_status ? html`<p class="success-message">${_status}</p>` : html``}
          ${_showJson
            ? html`<div class="json-editor">
                <textarea
                  class="json-textarea"
                  value="${JSON.stringify(_screen, null, 2)}"
                  oninput="${updateJson}"
                ></textarea>
                ${_jsonError ? html`<p class="error-message">${_jsonError}</p>` : html``}
              </div>`
            : html`<div class="editor-layout">
                <div class="editor-layers">
                  <div class="form-group">
                    <label>Screen Name</label>
                    <input
                      type="text"
                      value="${_screen.name || ''}"
                      oninput="${(h, e) => {
                        h._screen = { ...h._screen, name: e.target.value };
                      }}"
                    />
                  </div>
                  <h3>Layers (${layers.length})</h3>
                  <ul class="layer-stack">
                    ${layers.map(
                      (layer, idx) =>
                        html` <li
                          class="layer-item ${idx === _selectedLayer ? 'selected' : ''}"
                          data-idx="${idx}"
                          onclick="${selectLayer}"
                        >
                          <span class="layer-type">${layer.type}</span>
                          <span class="layer-summary">${layerSummary(layer)}</span>
                          <div class="layer-controls">
                            <button
                              class="btn-icon"
                              data-idx="${idx}"
                              data-dir="-1"
                              onclick="${moveLayer}"
                            >
                              ↑
                            </button>
                            <button
                              class="btn-icon"
                              data-idx="${idx}"
                              data-dir="1"
                              onclick="${moveLayer}"
                            >
                              ↓
                            </button>
                            <button
                              class="btn-icon btn-danger-icon"
                              data-idx="${idx}"
                              onclick="${removeLayer}"
                            >
                              ×
                            </button>
                          </div>
                        </li>`,
                    )}
                  </ul>
                  <div class="add-layer">
                    <select value="${host._addType}" onchange="${html.set('_addType')}">
                      ${LAYER_TYPES.map((t) => html`<option value="${t}">${t}</option>`)}
                    </select>
                    <button class="btn btn-secondary" onclick="${addLayer}">Add Layer</button>
                  </div>
                </div>
                <div class="editor-preview card">
                  <h3>Preview</h3>
                  <p class="hint">Save and push to see on device</p>
                </div>
              </div>`}
        </div>
      `;
    },
    shadow: false,
  },
});
