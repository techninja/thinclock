/**
 * Device config URL reconciliation.
 * On server startup, checks each approved device and pushes the correct
 * config URL if it's missing or pointing elsewhere — handles re-flash.
 */

const TIMEOUT = 4000;

async function tryFetch(url, opts = {}) {
  try {
    return await fetch(url, { signal: AbortSignal.timeout(TIMEOUT), ...opts });
  } catch {
    return null;
  }
}

export async function reconcileDevices(configUrl, listDevices) {
  const devices = listDevices();
  if (!devices.length) return;

  for (const { ip } of devices) {
    const info = await tryFetch(`http://${ip}/info`);
    if (!info?.ok) continue;

    const { config_url } = await info.json().catch(() => ({}));
    if (config_url === configUrl) continue;

    console.log(
      `[registry] ${ip} config_url ${config_url ? `wrong (${config_url})` : 'empty'} — correcting`,
    );
    await tryFetch(`http://${ip}/config_url`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ config_url: configUrl }),
    });
  }
}
