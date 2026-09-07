/**
 * Device registry — persists approved device IPs, gates WS connections.
 */

import { readFileSync, writeFileSync, existsSync } from 'fs';

const REGISTRY_PATH = './data/devices.json';

// { [ip]: { ip, name, approvedAt } }
let registry = {};

export function loadRegistry() {
  try {
    if (existsSync(REGISTRY_PATH)) registry = JSON.parse(readFileSync(REGISTRY_PATH, 'utf8'));
  } catch {
    registry = {};
  }
}

function save() {
  try {
    writeFileSync(REGISTRY_PATH, JSON.stringify(registry, null, 2));
  } catch {
    /* /data may not exist in dev */
  }
}

export function isApproved(ip) {
  return !!registry[ip];
}

export function approveDevice(ip, name = '') {
  registry[ip] = { ip, name, approvedAt: new Date().toISOString() };
  save();
  console.log(`[registry] approved device ${ip} (${name})`);
}

export function removeDevice(ip) {
  delete registry[ip];
  save();
}

export function listDevices() {
  return Object.values(registry);
}
