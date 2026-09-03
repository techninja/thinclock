/**
 * Dashboard page — device status, active rotation, quick actions.
 * @module pages/home
 */

import { html, define, store, router } from 'hybrids';
import DeviceModel from '#store/DeviceModel.js';
import '#molecules/app-nav/index.js';
import '#atoms/live-preview/index.js';
import RotationView from '#pages/rotation/rotation-view.js';
import SettingsView from '#pages/settings/settings-view.js';
import NotifyView from '#pages/notify/notify-view.js';
import EditorView from '#pages/editor/editor-view.js';
function formatUptime(seconds) {
  if (!seconds) return '—';
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return `${h}h ${m}m`;
}

/** @param {HTMLElement} host */
function loadActive(host) {
  fetch('/api/active')
    .then((r) => r.json())
    .then((list) => {
      host._active = list;
    })
    .catch(() => {});
}

/** @param {HTMLElement} host */
function handleBeep(_host) {
  import('#utils/device.js').then(({ devicePost }) => {
    devicePost('/beep', { pattern: 'default' });
  });
}

export default define({
  tag: 'home-view',
  [router.connect]: { url: '/', stack: [RotationView, SettingsView, NotifyView, EditorView] },
  device: store(DeviceModel),
  _active: {
    value: [],
    connect: (host) => {
      loadActive(host);
    },
  },
  render: {
    value: ({ device, _active }) => {
      const ready = store.ready(device);
      const active = Array.isArray(_active) ? _active : [];
      return html`
        <app-nav></app-nav>
        <div class="page-dashboard">
          <h1>Dashboard</h1>
          ${ready
            ? html`
                <section class="card">
                  <h2>Live Display</h2>
                  <live-preview fps="10"></live-preview>
                </section>
                <section class="card">
                  <h2>Device</h2>
                  <dl class="info-grid">
                    <dt>IP</dt>
                    <dd>${device.ip || 'Not connected'}</dd>
                    <dt>Version</dt>
                    <dd>${device.version || '—'}</dd>
                    <dt>Uptime</dt>
                    <dd>${formatUptime(device.uptime)}</dd>
                    <dt>WiFi</dt>
                    <dd>${device.ssid} (${device.rssi}dBm)</dd>
                    <dt>RAM</dt>
                    <dd>${device.freeRam ? `${device.freeRam}B` : '—'}</dd>
                    <dt>Temp</dt>
                    <dd>${device.temp}°</dd>
                    <dt>Humidity</dt>
                    <dd>${device.humidity}%</dd>
                    <dt>Light</dt>
                    <dd>${device.light}</dd>
                  </dl>
                </section>
              `
            : html`<p class="loading"><span class="spinner"></span> Loading device…</p>`}
          <section class="card">
            <h2>Active Rotation (${active.length})</h2>
            <ul class="rotation-list">
              ${active.map((s) => html`<li>${s.name}</li>`)}
            </ul>
          </section>
          <section class="card">
            <h2>Quick Actions</h2>
            <div class="actions">
              <a href="/notify" class="btn btn-primary">Send Notification</a>
              <button class="btn btn-secondary" onclick="${handleBeep}">Test Beep</button>
            </div>
          </section>
        </div>
      `;
    },
    shadow: false,
  },
});
