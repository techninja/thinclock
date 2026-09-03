/**
 * Rotation Manager — screens grouped by status, sequential GIF previews.
 * @module pages/rotation
 */

import { html, define, store, router } from 'hybrids';
import ScreenModel from '#store/ScreenModel.js';
import '#molecules/app-nav/index.js';
import '#atoms/app-icon/index.js';

/** Tracks screen IDs whose preview GIF has already been loaded — persists across re-renders. */
const loadedPreviews = new Set();

function screenAction(host, id, endpoint) {
  fetch(`/api/screens/${id}/${endpoint}`, { method: 'POST' }).then(() => {
    store.clear(host.screens);
    setTimeout(() => loadPreviews(host), 300);
  });
}
function scheduleLabel(schedule) {
  if (!schedule) return '';
  if (typeof schedule === 'string') return schedule;
  const s = JSON.parse(schedule);
  if (s.hours) return `${s.hours.join(', ')}`;
  if (s.months) return `${s.months.join(', ')}`;
  if (s.dateRange) return `${s.dateRange[0]}–${s.dateRange[1]}`;
  return 'Scheduled';
}

/** Load preview images sequentially, retry 202s. Skips already-loaded IDs. */
async function loadPreviews(host) {
  const imgs = [...host.querySelectorAll('img[data-src]')].filter(
    (i) => !loadedPreviews.has(i.dataset.id),
  );
  for (const img of imgs) {
    if (!host.isConnected) return;
    const src = img.dataset.src;
    const id = img.dataset.id;
    for (let attempt = 0; attempt < 10; attempt++) {
      const resp = await fetch(src);
      if (resp.ok && resp.headers.get('content-type')?.includes('image')) {
        img.src = src;
        loadedPreviews.add(id);
        await new Promise((r) => {
          img.onload = r;
          img.onerror = r;
        });
        break;
      }
      await new Promise((r) => setTimeout(r, 3000));
    }
  }
}
function renderItem(host, screen) {
  const icon = screen.schedule ? 'calendar-clock' : !screen.enabled ? 'eye-off' : null;
  return html`
    <li class="screen-item ${screen.active ? 'active' : ''} ${!screen.enabled ? 'disabled' : ''}">
      <img
        class="screen-preview"
        data-id="${screen.id}"
        data-src="/api/preview/${screen.id}.gif?seconds=1&scale=5&gap=1&gamma=18"
        src="${loadedPreviews.has(screen.id)
          ? `/api/preview/${screen.id}.gif?seconds=1&scale=5&gap=1&gamma=18`
          : ''}"
      />
      <div class="screen-info">
        <span class="screen-name">
          ${icon ? html`<app-icon name="${icon}" size="sm"></app-icon>` : html``} ${screen.name}
        </span>
        <span class="screen-tags">
          ${screen.schedule ? scheduleLabel(screen.schedule) : (screen.tags || []).join(', ')}
        </span>
      </div>
      <button
        class="btn ${screen.enabled ? 'btn-secondary' : 'btn-ghost'}"
        onclick="${(h) => screenAction(host, screen.id, screen.enabled ? 'disable' : 'enable')}"
      >
        ${screen.enabled ? 'Disable' : 'Enable'}
      </button>
      ${screen.enabled
        ? html`<button
            class="btn ${screen.pinned ? 'btn-primary' : 'btn-ghost'}"
            onclick="${(h) => screenAction(host, screen.id, screen.pinned ? 'unpin' : 'pin')}"
          >
            ${screen.pinned ? 'Unpin' : 'Add to rotation'}
          </button>`
        : html``}
    </li>
  `;
}

export default define({
  tag: 'rotation-view',
  [router.connect]: { url: '/rotation' },
  screens: store([ScreenModel]),
  _loader: {
    value: false,
    connect: (host) => {
      // Wait for render, then load previews sequentially
      const timer = setTimeout(() => loadPreviews(host), 500);
      return () => clearTimeout(timer);
    },
  },
  render: {
    value: (host) => {
      const { screens } = host;
      const ready = store.ready(screens);
      if (!ready)
        return html`<app-nav></app-nav>
          <p class="loading"><span class="spinner"></span> Loading…</p>`;

      const list = screens.filter((s) => store.ready(s));
      const active = list.filter((s) => s.active);
      const offSchedule = list.filter((s) => !s.active && s.enabled && s.schedule);
      const enabled = list.filter((s) => !s.active && s.enabled && !s.schedule);
      const disabled = list.filter((s) => !s.enabled);
      const render = (s) => renderItem(host, s);

      return html`
        <app-nav></app-nav>
        <div class="page-rotation">
          <h1>Rotation Manager</h1>
          <h2>Active Now (${active.length})</h2>
          <ul class="screen-list">
            ${active.map(render)}
          </ul>
          ${offSchedule.length
            ? html`
                <h2>Scheduled — waiting for time window</h2>
                <ul class="screen-list">
                  ${offSchedule.map(render)}
                </ul>
              `
            : html``}
          ${enabled.length
            ? html`
                <h2>Enabled</h2>
                <ul class="screen-list">
                  ${enabled.map(render)}
                </ul>
              `
            : html``}
          ${disabled.length
            ? html`
                <h2>Disabled</h2>
                <ul class="screen-list">
                  ${disabled.map(render)}
                </ul>
              `
            : html``}
        </div>
      `;
    },
    shadow: false,
  },
});
