/**
 * Project-level clearstack spec extensions.
 * Adds firmware C++ linting via cppcheck and Python linting via ruff.
 * @module clearstack.spec
 */

import { runExtCmd, elapsed } from '@techninja/clearstack/lib/check.js';
import { execSync } from 'child_process';
import { readFileSync, readdirSync } from 'fs';
import path from 'path';

const CPP_MAX_LINES = 400;
const PY_MAX_LINES = 150;

// ── Line size checks ──────────────────────────────────────────────────────────

function checkLineLengths(dir, exts, max) {
  const files = readdirSync(dir).filter(f => exts.some(e => f.endsWith(e)));
  const violations = [];
  for (const f of files) {
    const lines = readFileSync(path.join(dir, f), 'utf8').split('\n').length;
    if (lines > max) violations.push(`${f}: ${lines} lines (max ${max})`);
  }
  if (!violations.length) return { pass: true };
  return { pass: false, errors: violations };
}

function sizeCheck(label, dir, exts, max, opts) {
  const start = performance.now();
  const suffix = () => ` (${elapsed(start)})`;
  const result = checkLineLengths(dir, exts, max);
  if (!opts?.quiet) {
    if (result.pass) {
      console.log(`  ✅ ${label}${suffix()}`);
    } else {
      console.log(`  ❌ ${label}${suffix()}`);
      for (const line of result.errors) console.log(`     ${line}`);
    }
  }
  return result;
}

// ── Ruff ──────────────────────────────────────────────────────────────────────

/** Run ruff with JSON output, group by file, separate fixable from non-fixable. */
function runRuff(target, opts) {
  const start = performance.now();
  const label = 'Python (ruff)';
  const suffix = () => ` (${elapsed(start)})`;

  let raw;
  try {
    raw = execSync(`ruff check ${target} --output-format=json`, { encoding: 'utf-8', stdio: 'pipe' });
  } catch (err) {
    raw = err.stdout || '[]';
  }

  let diags;
  try { diags = JSON.parse(raw); } catch { diags = []; }

  if (!diags.length) {
    if (!opts?.quiet) console.log(`  ✅ ${label}${suffix()}`);
    return { pass: true, label, time: elapsed(start) };
  }

  const byFile = new Map();
  for (const d of diags) {
    const rel = d.filename.replace(process.cwd() + '/', '');
    if (!byFile.has(rel)) byFile.set(rel, { fixable: [], errors: [] });
    const entry = { code: d.code, row: d.location.row, message: d.message };
    if (d.fix?.applicability === 'safe') byFile.get(rel).fixable.push(entry);
    else byFile.get(rel).errors.push(entry);
  }

  const violations = [];
  for (const [file, { fixable, errors }] of byFile) {
    const lines = [];
    if (fixable.length) lines.push(`  [*] ${fixable.length} auto-fixable — press f to fix`);
    for (const e of errors) lines.push(`  ${e.code} L${e.row}: ${e.message}`);
    violations.push({ file, lines });
  }

  if (!opts?.quiet) {
    console.log(`  ❌ ${label}${suffix()}`);
    for (const { file, lines } of violations) {
      console.log(`     ${file}`);
      for (const l of lines) console.log(`     ${l}`);
    }
  }
  return { pass: false, label, time: elapsed(start), violations };
}

// ── Checks ────────────────────────────────────────────────────────────────────

/** @type {import('@techninja/clearstack/lib/check.js').Check[]} */
export default [
  {
    key: 'firmware',
    name: 'Firmware C++ (cppcheck)',
    aliases: ['cpp', 'fw'],
    parent: 'lint',
    watchExts: ['.cpp', '.h'],
    watchPaths: ['firmware/src/', 'firmware/include/'],
    run: (opts) => runExtCmd(
      'Firmware C++ (cppcheck)',
      'cppcheck --enable=warning,style,performance --suppress=missingIncludeSystem --inline-suppr --suppressions-list=firmware/.cppcheck-suppress --error-exitcode=1 -I firmware/include firmware/src/ 2>&1',
      { ...opts, ignorePaths: ['firmware/.pio/'], lineFilter: (l) => !/^(Checking |\d+\/\d+ files checked)/.test(l) },
    ),
  },
  {
    key: 'firmware-size',
    name: 'Firmware C++ file size',
    aliases: ['cpp-size'],
    parent: 'lint',
    watchExts: ['.cpp'],
    watchPaths: ['firmware/src/'],
    run: (opts) => sizeCheck('Firmware C++ file size', 'firmware/src', ['.cpp', '.h'], CPP_MAX_LINES, opts),
  },
  {
    key: 'python-size',
    name: 'Python file size',
    aliases: ['py-size'],
    parent: 'lint',
    watchExts: ['.py'],
    watchPaths: ['homeassistant/'],
    run: (opts) => sizeCheck('Python file size', 'homeassistant/custom_components/thinclock', ['.py'], PY_MAX_LINES, opts),
  },
  {
    key: 'python-lint',
    name: 'Python (ruff)',
    aliases: ['py', 'ruff'],
    parent: 'lint',
    watchExts: ['.py'],
    watchPaths: ['homeassistant/'],
    fix: () => { try { execSync('ruff check --fix homeassistant/', { stdio: 'pipe' }); } catch { /* fixes applied even on exit 1 */ } },
    run: (opts) => runRuff('homeassistant/', opts),
  },
];
