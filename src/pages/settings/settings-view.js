/**
 * Settings page — device config, firmware info, reboot.
 * @module pages/settings
 */

import { html, define, store, router } from 'hybrids';
import DeviceModel from '#store/DeviceModel.js';
import '#molecules/app-nav/index.js';

/**
 *
 */
function handleReboot(host) {
  if (!confirm('Reboot device?')) return;
  import('#utils/device.js').then(({ devicePost }) => {
    devicePost('/reboot', {}).catch(() => {});
  });
}

/**
 *
 */
function formatUptime(seconds) {
  if (!seconds) return '—';
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return `${h}h ${m}m`;
}

export default define({
  tag: 'settings-view',
  [router.connect]: { url: '/settings' },
  device: store(DeviceModel),
  render: {
    value: ({ device }) => {
      const ready = store.ready(device);
      return html`
        <app-nav></app-nav>
        <div class="page-settings">
          <h1>Settings</h1>
          ${!ready
            ? html`<p class="loading"><span class="spinner"></span> Loading…</p>`
            : html`
                <section class="card">
                  <h2>Firmware</h2>
                  <dl class="info-grid">
                    <dt>Version</dt>
                    <dd>${device.version || '—'}</dd>
                    <dt>Build</dt>
                    <dd>${device.build || '—'}</dd>
                    <dt>Free RAM</dt>
                    <dd>${device.freeRam ? `${device.freeRam}B` : '—'}</dd>
                    <dt>Uptime</dt>
                    <dd>${formatUptime(device.uptime)}</dd>
                  </dl>
                </section>
                <section class="card">
                  <h2>Network</h2>
                  <dl class="info-grid">
                    <dt>Device IP</dt>
                    <dd>${device.ip}</dd>
                    <dt>WiFi SSID</dt>
                    <dd>${device.ssid || '—'}</dd>
                    <dt>Signal</dt>
                    <dd>${device.rssi}dBm</dd>
                  </dl>
                </section>
                <section class="card">
                  <h2>Sensors</h2>
                  <dl class="info-grid">
                    <dt>Temperature</dt>
                    <dd>${device.temp}°</dd>
                    <dt>Humidity</dt>
                    <dd>${device.humidity}%</dd>
                    <dt>Light</dt>
                    <dd>${device.light}</dd>
                  </dl>
                </section>
                <section class="card">
                  <h2>Actions</h2>
                  <button class="btn btn-danger" onclick="${handleReboot}">Reboot Device</button>
                </section>
              `}
        </div>
      `;
    },
    shadow: false,
  },
});
