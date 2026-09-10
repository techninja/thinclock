#!/usr/bin/env node

/**
 * Release helper — bumps version, updates changelog, commits, and tags.
 * Platform-aware: runs sync-vendor.js if it exists.
 * Usage: node scripts/release.js [patch|minor|major]
 * @module scripts/release
 */

import { execSync } from 'node:child_process';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const bump = process.argv[2] || 'patch';

if (!['patch', 'minor', 'major'].includes(bump)) {
  console.error('Usage: node scripts/release.js [patch|minor|major]');
  process.exit(1);
}

const run = (cmd) => execSync(cmd, { cwd: ROOT, stdio: 'inherit' });
const out = (cmd) => execSync(cmd, { cwd: ROOT, encoding: 'utf-8' }).trim();

// Gate: clean working tree (allow staged changes)
const dirty = out('git diff --name-only HEAD').split('\n').filter(Boolean);
if (dirty.length > 0) {
  console.error('\n❌ Uncommitted changes — commit or stash first:\n');
  dirty.forEach((f) => console.error(`   ${f}`));
  process.exit(1);
}

// Bump version
const pkgPath = resolve(ROOT, 'package.json');
const pkg = JSON.parse(readFileSync(pkgPath, 'utf-8'));
const [ma, mi, pa] = pkg.version.split('.').map(Number);
const next =
  bump === 'major'
    ? `${ma + 1}.0.0`
    : bump === 'minor'
      ? `${ma}.${mi + 1}.0`
      : `${ma}.${mi}.${pa + 1}`;
pkg.version = next;
writeFileSync(pkgPath, JSON.stringify(pkg, null, 2) + '\n');
console.log(`\n📦 ${pkg.name} → v${next}\n`);

// Platform: sync vendor if applicable
const syncScript = resolve(ROOT, 'scripts/sync-vendor.js');
if (existsSync(syncScript)) {
  console.log('🔄 Syncing vendor...');
  run('node scripts/sync-vendor.js');
}

// Spec check
console.log('\n🔍 Running spec checks...\n');
run('npm run spec -- check all');

// Changelog
const changelogPath = resolve(ROOT, 'CHANGELOG.md');
const lastTag = out('git tag --sort=-v:refname').split('\n')[0] || '';
const range = lastTag ? `${lastTag}..HEAD` : 'HEAD';
const log = out(`git log ${range} --pretty=format:"- %s"`);
const date = new Date().toISOString().split('T')[0];
const entry = `## [${next}] - ${date}\n\n${log || '- Initial release'}\n`;

if (existsSync(changelogPath)) {
  const cl = readFileSync(changelogPath, 'utf-8');
  const marker = '## [Unreleased]';
  const updated = cl.includes(marker)
    ? cl.replace(marker, `${marker}\n\n${entry}`)
    : `${entry}\n\n${cl}`;
  writeFileSync(changelogPath, updated);
} else {
  const header = '# Changelog\n\n## [Unreleased]\n\n';
  writeFileSync(changelogPath, header + entry);
}
console.log('📝 Updated CHANGELOG.md');

// Commit + tag
run('git add -A');
run(`git commit -m "release: v${next}"`);
run(`git tag v${next}`);
console.log(`\n🏷️  Tagged v${next}`);
console.log(`\n   git push && git push --tags\n`);
