/**
 * Project-level clearstack spec extensions.
 * Adds firmware C++ linting via cppcheck.
 * @module clearstack.spec
 */

import { runExtCmd } from '@techninja/clearstack/lib/check.js';

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
];
