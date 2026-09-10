/**
 * Device communication helpers — routes through server proxy.
 * @module utils/device
 */

/**
 * POST JSON to device via server proxy.
 * @param {string} endpoint - e.g. '/notify'
 * @param {object} body
 */
export async function devicePost(endpoint, body) {
  return fetch(`/api/device${endpoint}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
}

/**
 * GET from device via server proxy.
 * @param {string} endpoint
 */
export async function deviceGet(endpoint) {
  return fetch(`/api/device${endpoint}`).then((r) => r.json());
}
