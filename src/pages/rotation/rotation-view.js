/**
 * Rotation Manager — screens grouped by status, sequential GIF previews.
 * @module pages/rotation
 */

import { html, define, store, router } from 'hybrids';
import ScreenModel from '#store/ScreenModel.js';
import '#molecules/app-nav/index.js';
import { loadPreviews, renderItem } from './rotation-helpers.js';

function screenAction(host, id, endpoint) {
  fetch(`/api/screens/${id}/${endpoint}`, { method: 'POST' }).then(() => {
    store.clear(host.screens);
    setTimeout(() => loadPreviews(host), 300);
  });
}

function renderSection(title, list, render) {
  return list.length
    ? html`<h2>${title}</h2>
        <ul class="screen-list">
          ${list.map(render)}
        </ul>`
    : html``;
}

export default define({
  tag: 'rotation-view',
  [router.connect]: { url: '/rotation' },
  screens: store([ScreenModel]),
  _loader: {
    value: false,
    connect: (host) => {
      const timer = setTimeout(() => loadPreviews(host), 500);
      return () => clearTimeout(timer);
    },
  },
  render: {
    value: (host) => {
      const { screens } = host;
      if (!store.ready(screens))
        return html`<app-nav></app-nav>
          <p class="loading"><span class="spinner"></span> Loading…</p>`;

      const list = screens.filter((s) => store.ready(s));
      const action = (id, ep) => screenAction(host, id, ep);
      const render = (s) => renderItem(host, s, action);
      const active = list.filter((s) => s.active);
      const offSchedule = list.filter((s) => !s.active && s.enabled && s.schedule);
      const enabled = list.filter((s) => !s.active && s.enabled && !s.schedule);
      const disabled = list.filter((s) => !s.enabled);

      return html`
        <app-nav></app-nav>
        <div class="page-rotation">
          <h1>Rotation Manager</h1>
          ${renderSection(`Active Now (${active.length})`, active, render)}
          ${renderSection('Scheduled — waiting for time window', offSchedule, render)}
          ${renderSection('Enabled', enabled, render)}
          ${renderSection('Disabled', disabled, render)}
        </div>
      `;
    },
    shadow: false,
  },
});
