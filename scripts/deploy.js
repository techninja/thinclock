#!/usr/bin/env node
/**
 * ThinClock deploy orchestrator
 *
 * npm run deploy                        — all targets (app + integration + device)
 * npm run deploy local                  — app + integration (no flash)
 * npm run deploy local app              — HA add-on server only
 * npm run deploy local integration      — HA custom component only
 * npm run deploy device                 — flash firmware only
 * npm run deploy help                   — show this help
 */

import { readFileSync, writeFileSync, mkdirSync, cpSync } from 'fs';
import { fileURLToPath } from 'url';
import path from 'path';
import { bar, barDone, run, sshCheck, help } from './deploy-utils.js';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const HA_HOST = process.env.HA_HOST || 'homeassistant.local';
const HA_PORT = process.env.HA_PORT || '4222';
const SSH_CMD = `ssh -p ${HA_PORT} root@${HA_HOST}`;
const SCP_CMD = `scp -P ${HA_PORT}`;

const args = process.argv.slice(2);
const scope = args[0];
const target = args[1];

function deployApp() {
  console.log('\n  [app] Deploying HA add-on server');

  bar('stage  ', 0, 3);
  const addonDir = path.join(ROOT, 'thinclock-addon');
  mkdirSync(addonDir, { recursive: true });
  const pkg = JSON.parse(readFileSync(path.join(ROOT, 'package.json'), 'utf8'));
  delete pkg.scripts.postinstall;
  writeFileSync(path.join(addonDir, 'package.json'), JSON.stringify(pkg, null, 2));
  run(`cp ${ROOT}/package-lock.json ${addonDir}/package-lock.json`);
  cpSync(path.join(ROOT, 'src'), path.join(addonDir, 'src'), {
    recursive: true,
    filter: (s) => !s.endsWith('.test.js') && !s.endsWith('.spec.js'),
  });

  const cfgPath = path.join(addonDir, 'config.yaml');
  const cfg = readFileSync(cfgPath, 'utf8');
  const bumped = cfg.replace(
    /^version:\s*"(\d+)\.(\d+)\.(\d+)"/m,
    (_, ma, mi, pa) => `version: "${ma}.${mi}.${parseInt(pa) + 1}"`,
  );
  writeFileSync(cfgPath, bumped);
  bar('stage  ', 1, 3);

  run(`${SSH_CMD} "mkdir -p /addons/thinclock"`);
  run(`${SCP_CMD} -r ${addonDir}/* root@${HA_HOST}:/addons/thinclock/`);
  bar('stage  ', 2, 3);
  barDone('stage  ');

  bar('rebuild', 0, 1);
  try {
    run(`${SSH_CMD} "ha apps update local_thinclock"`);
    barDone('rebuild');
    console.log('  ✓ app done');
  } catch (e) {
    const msg = e.stderr?.toString() || e.message || '';
    if (msg.includes('No update available')) {
      barDone('rebuild');
      console.log('  ✓ app already up to date (no version change)');
    } else {
      throw e;
    }
  }
}

function deployIntegration() {
  console.log('\n  [integration] Deploying HA custom component');

  bar('copy ', 0, 2);
  const src = path.join(ROOT, 'homeassistant', 'custom_components', 'thinclock');
  const dest = `/config/custom_components/thinclock`;
  run(`${SSH_CMD} "mkdir -p ${dest}"`);
  run(`${SCP_CMD} -r ${src}/* root@${HA_HOST}:${dest}/`);
  bar('copy ', 1, 2);

  const wwwSrc = path.join(ROOT, 'homeassistant', 'www');
  run(`${SSH_CMD} "mkdir -p /config/www"`);
  run(`${SCP_CMD} -r ${wwwSrc}/* root@${HA_HOST}:/config/www/`);
  bar('copy ', 2, 2);

  try { run(`${SSH_CMD} "ha core restart"`); } catch { /* non-fatal */ }
  barDone('copy ');
  console.log('  ✓ integration done');
}

function deployDevice() {
  console.log('\n  [device] Flashing firmware');
  execSync('npm run flash', { cwd: ROOT, stdio: 'inherit' });
  console.log('  ✓ device done');
}

if (scope === 'help' || scope === '--help' || scope === '-h') {
  help();
  process.exit(0);
}

if (scope !== 'device') sshCheck(HA_HOST, HA_PORT);

if (!scope) {
  deployApp();
  deployIntegration();
  deployDevice();
} else if (scope === 'local') {
  if (!target || target === 'app') deployApp();
  if (!target || target === 'integration') deployIntegration();
} else if (scope === 'device') {
  deployDevice();
} else {
  console.error(`  Unknown target: "${scope}". Run: npm run deploy help`);
  process.exit(1);
}

console.log('\n  ✓ Deploy complete\n');
