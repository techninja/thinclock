/**
 * mDNS advertisement — announces the ThinClock server as _thinclock._tcp.local.
 * HA's zeroconf integration picks this up and triggers the config flow.
 * @module api/lib/mdns
 */

import mdns from 'multicast-dns';
import os from 'os';

/** @param {number} port */
export function advertiseMDNS(port) {
  const host = `${os.hostname()}.local`;
  const name = 'thinclock._thinclock._tcp.local';
  const m = mdns();

  m.on('query', (query) => {
    const isOurs = query.questions.some(
      (q) => q.name === '_thinclock._tcp.local' || q.name === name,
    );
    if (!isOurs) return;

    m.respond({
      answers: [
        { type: 'PTR', name: '_thinclock._tcp.local', data: name },
        { type: 'SRV', name, data: { port, target: host, weight: 0, priority: 0 } },
        { type: 'TXT', name, data: [`path=/`, `version=0.1.0`] },
      ],
    });
  });

  console.log(`[mdns] advertising _thinclock._tcp.local → ${host}:${port}`);
  return () => m.destroy();
}
