/**
 * Project-level clearstack spec extensions.
 * Adds firmware C++ linting via cppcheck.
 * @module clearstack.spec
 */

import { runExtCmd } from '@techninja/clearstack/lib/check.js';
import { readFileSync, readdirSync } from 'fs';
import path from 'path';

const CPP_MAX_LINES = 300;

function checkCppLineLengths() {
  const dir = 'firmware/src';
  const files = readdirSync(dir).filter(f => f.endsWith('.cpp') || f.endsWith('.h'));
  const violations = [];
  for (const f of files) {
    const lines = readFileSync(path.join(dir, f), 'utf8').split('\n').length;
    if (lines > CPP_MAX_LINES) violations.push(`${f}: ${lines} lines (max ${CPP_MAX_LINES})`);
  }
  if (violations.length) throw new Error('C++ files exceed line limit:\n' + violations.join('\n'));
}

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
      { ...opts, ignorePaths: ['firmware/.pio/'] },
    ),
  },
  {
    key: 'firmware-size',
    name: 'Firmware C++ file size',
    aliases: ['cpp-size'],
    parent: 'lint',
    watchExts: ['.cpp'],
    watchPaths: ['firmware/src/'],
    run: () => checkCppLineLengths(),
  },
];
