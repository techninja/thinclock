/**
 * Device info store — fetches via server proxy to device.
 * @module store/DeviceModel
 */

import { store } from 'hybrids';

/**
 * @typedef {Object} DeviceInfo
 * @property {string} ip
 * @property {string} version
 * @property {string} build
 * @property {number} uptime
 * @property {number} freeRam
 * @property {string} ssid
 * @property {number} rssi
 * @property {number} temp
 * @property {number} humidity
 * @property {number} light
 */

/** @type {import('hybrids').Model<DeviceInfo>} */
const DeviceModel = {
  ip: '',
  version: '',
  build: '',
  uptime: 0,
  freeRam: 0,
  ssid: '',
  rssi: 0,
  temp: 0,
  humidity: 0,
  light: 0,
  [store.connect]: {
    get: async () => {
      const { ip } = await fetch('/api/device-ip').then((r) => r.json());
      if (!ip) return { ip: '' };
      const [info, sensors] = await Promise.all([
        fetch('/api/device/info').then((r) => r.json()).catch(() => ({})),
        fetch('/api/device/sensors').then((r) => r.json()).catch(() => ({})),
      ]);
      return {
        ip,
        version: info.version || '',
        build: info.build || '',
        uptime: info.uptime || 0,
        freeRam: info.free_heap || 0,
        ssid: info.wifi_ssid || '',
        rssi: info.rssi || 0,
        temp: sensors.temperature || 0,
        humidity: sensors.humidity || 0,
        light: sensors.light || 0,
      };
    },
  },
};

export default DeviceModel;
