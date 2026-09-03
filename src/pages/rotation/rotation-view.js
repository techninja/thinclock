/**
 * Rotation Manager — screens grouped by status, sequential GIF previews.
 * @module pages/rotation
 */

import { html, define, store, router } from 'hybrids';
import ScreenModel from '#store/ScreenModel.js';
import '#molecules/app-nav/index.js';
function toggleScreen(host, event) {
  const id = event.currentTarget.dataset.id;
  const enabled = event.currentTarget.dataset.enabled === 'true';
  const endpoint = enabled ? 'disable' : 'enable';
  fetch(`/api/screens/${id}/${endpoint}`, { method: 'POST' }).then(() =>
    store.clear([ScreenModel]),
  );
}
function scheduleLabel(schedule) {
  if (!schedule) return '';
  if (typeof schedule === 'string') return schedule;
  const s = JSON.parse(schedule);
  if (s.hours) return `Hours: ${s.hours.join(', ')}`;
  if (s.months) return `Months: ${s.months.join(', ')}`;
  if (s.dateRange) return `Dates: ${s.dateRange[0]}–${s.dateRange[1]}`;
  return 'Scheduled';
}

/** Load preview images sequentially, retry 202s. Skips already-loaded imgs. */
async function loadPreviews(host) {
  const imgs = [...host.querySelectorAll('img[data-src]')].filter(
    (i) => !i.src || i.src === window.location.href,
  );
  for (const img of imgs) {
    if (!host.isConnected) return;
    const src = img.dataset.src;
    for (let attempt = 0; attempt < 10; attempt++) {
      const resp = await fetch(src);
      if (resp.ok && resp.headers.get('content-type')?.includes('image')) {
        img.src = src;
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
function renderItem(screen) {
  return html`
    <li class="screen-item ${screen.active ? 'active' : ''} ${!screen.enabled ? 'disabled' : ''}">
      <img
        class="screen-preview"
        data-src="/api/preview/${screen.id}.gif?seconds=1&scale=5&gap=1&gamma=18"
      />
      <div class="screen-info">
        <span class="screen-name">${screen.name}</span>
        <span class="screen-tags">
          ${screen.schedule ? scheduleLabel(screen.schedule) : (screen.tags || []).join(', ')}
        </span>
      </div>
      <button
        class="btn ${screen.enabled
          ? screen.active
            ? 'btn-success'
            : 'btn-secondary'
          : 'btn-ghost'}"
        data-id="${screen.id}"
        data-enabled="${screen.enabled}"
        onclick="${toggleScreen}"
      >
        ${screen.active ? '▶ In rotation' : screen.enabled ? 'On' : 'Off'}
      </button>
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
    value: ({ screens }) => {
      const ready = store.ready(screens);
      if (!ready)
        return html`<app-nav></app-nav>
          <p class="loading"><span class="spinner"></span> Loading…</p>`;

      const list = screens.filter((s) => store.ready(s));
      const active = list.filter((s) => s.active);
      const offSchedule = list.filter((s) => !s.active && s.enabled && s.schedule);
      const enabled = list.filter((s) => !s.active && s.enabled && !s.schedule);
      const disabled = list.filter((s) => !s.enabled);

      return html`
        <app-nav></app-nav>
        <div class="page-rotation">
          <h1>Rotation Manager</h1>
          <h2>Active Now (${active.length})</h2>
          <ul class="screen-list">
            ${active.map(renderItem)}
          </ul>
          ${offSchedule.length
            ? html`
                <h2>Scheduled — waiting for time window</h2>
                <ul class="screen-list">
                  ${offSchedule.map(renderItem)}
                </ul>
              `
            : html``}
          ${enabled.length
            ? html`
                <h2>Enabled</h2>
                <ul class="screen-list">
                  ${enabled.map(renderItem)}
                </ul>
              `
            : html``}
          ${disabled.length
            ? html`
                <h2>Disabled</h2>
                <ul class="screen-list">
                  ${disabled.map(renderItem)}
                </ul>
              `
            : html``}
        </div>
      `;
    },
    shadow: false,
  },
});
