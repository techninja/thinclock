/**
 * Realtime sync via Server-Sent Events.
 * Debounces rapid updates to avoid clearing stores mid-render.
 * @module utils/realtimeSync
 */

import { store } from 'hybrids';

/**
 * Connect to the SSE endpoint and clear store caches on entity updates.
 * Debounces clears — multiple events within 300ms trigger one clear.
 * @param {string} url - SSE endpoint, e.g. '/api/events'
 * @param {Record<string, import('hybrids').Model<any>>} modelMap
 * @returns {() => void} Disconnect function
 */
export function connectRealtime(url, modelMap) {
  const source = new EventSource(url);
  /** @type {Record<string, ReturnType<typeof setTimeout>>} */
  const timers = {};

  source.addEventListener('open', () => {
    console.log('[SSE] Connected to', url);
  });

  source.addEventListener('update', (event) => {
    const { type, action } = JSON.parse(event.data);
    const Model = modelMap[type];
    if (!Model) return;

    // Debounce: batch rapid events (e.g. reorder sends N PUTs)
    clearTimeout(timers[type]);
    timers[type] = setTimeout(() => {
      console.log(`[SSE] ${type} ${action} — clearing store cache`);
      try {
        store.clear([Model]);
      } catch (e) {
        console.warn(`[SSE] clear list failed for ${type}:`, e.message);
      }
      try {
        store.clear(Model);
      } catch (e) {
        console.warn(`[SSE] clear singular failed for ${type}:`, e.message);
      }
    }, 300);
  });

  source.addEventListener('error', () => {
    console.log('[SSE] Connection lost, reconnecting in 5s...');
    source.close();
    setTimeout(() => connectRealtime(url, modelMap), 5000);
  });

  return () => source.close();
}
