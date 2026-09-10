/** @param {number} port */
export function advertiseMDNS(port, host) {
  // Server no longer advertises via mDNS — devices are paired by HA pushing config URL at confirm time
  console.log(`[mdns] server running at ${host || 'auto'}:${port} (no mDNS advertisement)`);
}
