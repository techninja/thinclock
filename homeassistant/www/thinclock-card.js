/**
 * ThinClock Lovelace Card
 * Shows live display stream + current screen name.
 *
 * config:
 *   type: custom:thinclock-card
 *   server_url: http://192.168.86.42:3232
 *   entity: select.thinclock_192_168_86_27_current_screen
 */
class ThinClockCard extends HTMLElement {
  setConfig(config) {
    this._config = config;
    this._built = false;
  }

  set hass(hass) {
    this._hass = hass;
    if (!this._built) this._build();
    this._update();
  }

  _serverUrl() {
    if (this._config?.server_url) return this._config.server_url.replace(/\/$/, '');
    if (this._config?.entity && this._hass) {
      const s = this._hass.states[this._config.entity];
      if (s?.attributes?.server_url) return s.attributes.server_url.replace(/\/$/, '');
    }
    return '';
  }

  _build() {
    this._built = true;
    if (!this.shadowRoot) this.attachShadow({ mode: 'open' });
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
        .no-url {
          color: #666;
          padding: 24px;
          font-size: 0.85em;
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
          background: var(--success-color, #4caf50);
          flex-shrink: 0;
        }
      </style>
      <ha-card>
        <div class="stream-wrap">
          <img id="stream" alt="ThinClock live display" style="display:none">
          <div id="no-url" class="no-url">No server_url configured</div>
        </div>
        <div class="footer">
          <div class="dot"></div>
          <span id="screen-name">ThinClock</span>
        </div>
      </ha-card>
    `;
    this._img = this.shadowRoot.getElementById('stream');
    this._noUrl = this.shadowRoot.getElementById('no-url');
    this._nameEl = this.shadowRoot.getElementById('screen-name');
  }

  _update() {
    if (!this._built) return;
    const url = this._serverUrl();
    const streamUrl = url ? `${url}/api/device/stream` : '';

    // Only update img src if URL changed — avoids killing the stream
    if (streamUrl && this._img.src !== streamUrl) {
      this._img.src = streamUrl;
      this._img.style.display = '';
      this._noUrl.style.display = 'none';
    } else if (!streamUrl) {
      this._img.style.display = 'none';
      this._noUrl.style.display = '';
    }

    // Screen name from entity state
    let name = 'ThinClock';
    if (this._config?.entity && this._hass) {
      const s = this._hass.states[this._config.entity];
      if (s?.state) name = s.state;
    }
    if (this._nameEl.textContent !== name) this._nameEl.textContent = name;
  }

  getCardSize() { return 2; }

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
