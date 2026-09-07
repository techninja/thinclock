/**
 * ThinClock Lovelace Card
 * Shows live display stream + current screen name.
 *
 * config:
 *   type: custom:thinclock-card
 *   server_url: http://homeassistant.local:3232   # optional, auto-detected from entity
 *   entity: select.thinclock_current_screen       # optional
 */
class ThinClockCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    this._render();
  }

  setConfig(config) {
    this._config = config;
    this.attachShadow({ mode: 'open' });
    this._render();
  }

  _serverUrl() {
    if (this._config?.server_url) return this._config.server_url;
    // Try to read from entity attributes
    if (this._config?.entity && this._hass) {
      const state = this._hass.states[this._config.entity];
      if (state?.attributes?.server_url) return state.attributes.server_url;
    }
    return '';
  }

  _render() {
    if (!this.shadowRoot) return;
    const url = this._serverUrl();
    const streamUrl = url ? `${url}/api/device/stream` : '';

    // Current screen name from entity
    let screenName = '';
    if (this._config?.entity && this._hass) {
      const state = this._hass.states[this._config.entity];
      screenName = state?.state || '';
    }

    this.shadowRoot.innerHTML = `
      <style>
        :host { display: block; }
        ha-card { overflow: hidden; }
        .stream-wrap {
          background: #000;
          display: flex;
          align-items: center;
          justify-content: center;
          padding: 12px;
        }
        img {
          image-rendering: pixelated;
          width: 100%;
          max-width: 320px;
          height: auto;
          display: block;
        }
        .footer {
          padding: 8px 12px;
          font-size: 0.85em;
          color: var(--secondary-text-color);
          display: flex;
          align-items: center;
          gap: 6px;
        }
        .dot {
          width: 8px; height: 8px; border-radius: 50%;
          background: ${streamUrl ? 'var(--success-color, #4caf50)' : 'var(--error-color, #f44336)'};
          flex-shrink: 0;
        }
      </style>
      <ha-card>
        <div class="stream-wrap">
          ${streamUrl
            ? `<img src="${streamUrl}" alt="ThinClock live display">`
            : `<div style="color:#666;padding:24px;font-size:0.85em">No server_url configured</div>`
          }
        </div>
        <div class="footer">
          <div class="dot"></div>
          <span>${screenName || 'ThinClock'}</span>
        </div>
      </ha-card>
    `;
  }

  getCardSize() { return 2; }

  static getConfigElement() {
    return document.createElement('thinclock-card-editor');
  }

  static getStubConfig() {
    return { type: 'custom:thinclock-card', server_url: 'http://homeassistant.local:3232' };
  }
}

customElements.define('thinclock-card', ThinClockCard);
window.customCards = window.customCards || [];
window.customCards.push({
  type: 'thinclock-card',
  name: 'ThinClock',
  description: 'Live display preview for ThinClock',
  preview: true,
});
