/**
 * Deploy utilities — progress bar, SSH helpers, help text.
 * @module scripts/deploy-utils
 */

import { execSync, spawnSync } from 'child_process';
import readline from 'readline';

export const W = process.stdout.columns || 60;

export function bar(label, done, total) {
  const pct = total ? done / total : 0;
  const inner = W - label.length - 12;
  const fill = Math.round(pct * inner);
  const empty = inner - fill;
  const pctStr = String(Math.round(pct * 100)).padStart(3);
  readline.clearLine(process.stdout, 0);
  readline.cursorTo(process.stdout, 0);
  process.stdout.write(`  ${label}  [${'█'.repeat(fill)}${'░'.repeat(empty)}] ${pctStr}%`);
}

export function barDone(label) {
  bar(label, 1, 1);
  process.stdout.write('\n');
}

export function run(cmd, opts = {}) {
  execSync(cmd, { stdio: 'pipe', ...opts });
}

export function sshCheck(HA_HOST, HA_PORT) {
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

export function help() {
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
