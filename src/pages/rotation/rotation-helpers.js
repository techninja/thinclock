/**
 * Rotation Manager helpers — preview loader, item renderer.
 * @module pages/rotation/rotation-helpers
 */

import { html } from 'hybrids';
import '#atoms/app-icon/index.js';

export const loadedPreviews = new Set();

export async function loadPreviews(host) {
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

function scheduleLabel(schedule) {
  if (!schedule) return '';
  if (typeof schedule === 'string') return schedule;
  const s = JSON.parse(schedule);
  if (s.hours) return s.hours.join(', ');
  if (s.months) return s.months.join(', ');
  if (s.dateRange) return `${s.dateRange[0]}–${s.dateRange[1]}`;
  return 'Scheduled';
}

const previewSrc = (id) => `/api/preview/${id}.gif?seconds=1&scale=5&gap=1&gamma=18`;

export function renderItem(host, screen, action) {
  const icon = screen.schedule ? 'calendar-clock' : !screen.enabled ? 'eye-off' : null;
  return html`
    <li class="screen-item ${screen.active ? 'active' : ''} ${!screen.enabled ? 'disabled' : ''}">
      <img
        class="screen-preview"
        data-id="${screen.id}"
        data-src="${previewSrc(screen.id)}"
        src="${loadedPreviews.has(screen.id) ? previewSrc(screen.id) : ''}"
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
        onclick="${() => action(screen.id, screen.enabled ? 'disable' : 'enable')}"
      >
        ${screen.enabled ? 'Disable' : 'Enable'}
      </button>
      ${screen.enabled
        ? html`<button
            class="btn ${screen.pinned ? 'btn-primary' : 'btn-ghost'}"
            onclick="${() => action(screen.id, screen.pinned ? 'unpin' : 'pin')}"
          >
            ${screen.pinned ? 'Unpin' : 'Add to rotation'}
          </button>`
        : html``}
    </li>
  `;
}
