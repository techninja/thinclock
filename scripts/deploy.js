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

import { execSync, spawnSync } from 'child_process';
import { readFileSync, writeFileSync, mkdirSync, cpSync } from 'fs';
import { fileURLToPath } from 'url';
import path from 'path';
import readline from 'readline';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const HA_HOST = process.env.HA_HOST || 'homeassistant.local';
const HA_PORT = process.env.HA_PORT || '4222';
const SSH_CMD = `ssh -p ${HA_PORT} root@${HA_HOST}`;
const SCP_CMD = `scp -P ${HA_PORT}`;

const args = process.argv.slice(2);
const scope = args[0];
const target = args[1];

const W = process.stdout.columns || 60;

// ─── progress bar ────────────────────────────────────────────────────────────

function bar(label, done, total) {
  const pct = total ? done / total : 0;
  const inner = W - label.length - 12;
  const fill = Math.round(pct * inner);
  const empty = inner - fill;
  const pctStr = String(Math.round(pct * 100)).padStart(3);
  readline.clearLine(process.stdout, 0);
  readline.cursorTo(process.stdout, 0);
  process.stdout.write(`  ${label}  [${'█'.repeat(fill)}${'░'.repeat(empty)}] ${pctStr}%`);
}

function barDone(label) {
  bar(label, 1, 1);
  process.stdout.write('\n');
}

// ─── helpers ─────────────────────────────────────────────────────────────────

function run(cmd, opts = {}) {
  execSync(cmd, { stdio: 'pipe', ...opts });
}

function sshCheck() {
  const r = spawnSync(
    'ssh',
    ['-p', HA_PORT, '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5', `root@${HA_HOST}`, 'true'],
    { stdio: 'pipe' },
  );
  if (r.status !== 0) {
    console.error(`\n✗  Cannot connect to ${HA_HOST}:${HA_PORT}`);
    console.error(`   Add your public key with:`);
    console.error(`     ssh-copy-id -p ${HA_PORT} root@${HA_HOST}\n`);
    process.exit(1);
  }
}

function help() {
  console.log(`
  ThinClock deploy

  npm run deploy                        all targets (app + integration + device)
  npm run deploy local                  app + integration, no flash
  npm run deploy local app              HA add-on server only
  npm run deploy local integration      HA custom component only
  npm run deploy device                 flash firmware only
  npm run deploy help                   show this help

  Env vars (or .env):
    HA_HOST   default: homeassistant.local
    HA_PORT   default: 4222
  `);
}

// ─── targets ─────────────────────────────────────────────────────────────────

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

  // Bump patch version so supervisor detects a change and rebuilds the image
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

  bar('copy ', 0, 3);
  const src = path.join(ROOT, 'homeassistant', 'custom_components', 'thinclock');
  const dest = `/config/custom_components/thinclock`;
  run(`${SSH_CMD} "mkdir -p ${dest}"`);
  run(`${SCP_CMD} -r ${src}/* root@${HA_HOST}:${dest}/`);
  bar('copy ', 1, 2);

  const wwwSrc = path.join(ROOT, 'homeassistant', 'www');
  run(`${SSH_CMD} "mkdir -p /config/www"`);
  run(`${SCP_CMD} -r ${wwwSrc}/* root@${HA_HOST}:/config/www/`);
  bar('copy ', 2, 2);

  execSync(`${SSH_CMD} "ha core restart" 2>/dev/null || true`, { stdio: 'pipe' });
  barDone('copy ');

  console.log('  ✓ integration done');
}

function deployDevice() {
  console.log('\n  [device] Flashing firmware');

  // PlatformIO streams its own progress — just let it through
  execSync('npm run flash', { cwd: ROOT, stdio: 'inherit' });

  console.log('  ✓ device done');
}

// ─── orchestrate ─────────────────────────────────────────────────────────────

if (scope === 'help' || scope === '--help' || scope === '-h') {
  help();
  process.exit(0);
}

if (scope !== 'device') sshCheck();

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
