#!/usr/bin/env node

/**
 * Spec enforcement — interactive menu + CLI shortcuts.
 * Delegates all check logic to @techninja/clearstack.
 * @module scripts/spec
 */

import { loadConfig, buildChecks, buildCmds, check } from '@techninja/clearstack/lib/check.js';

const ROOT = new URL('..', import.meta.url).pathname.replace(/\/$/, '');
const [sub, subsub] = process.argv.slice(2);
const scope = subsub ? `${sub} ${subsub}` : sub;

if (scope) {
  await check(ROOT, scope);
} else {
  await interactive(ROOT);
}

/** Show interactive menu, then run the selected check. */
async function interactive(dir) {
  const cfg = loadConfig(dir);
  const checks = buildChecks(dir, cfg, buildCmds(dir));
  try {
    const { select } = await import('@inquirer/prompts');
    const action = await select({
      message: 'Spec checker — what do you want to validate?',
      choices: menuChoices(checks),
    });
    await check(dir, action);
  } catch (e) {
    if (e?.name === 'ExitPromptError') process.exit(0);
    throw e;
  }
}

/** Build interactive menu choices with hierarchy. */
function menuChoices(checks) {
  const choices = [];
  const seen = new Set();
  for (const c of checks) {
    if (c.parent && !seen.has(c.parent)) {
      seen.add(c.parent);
      const kids = checks.filter((k) => k.parent === c.parent);
      const label = kids.map((k) => k.name).join(' + ');
      choices.push({ name: `${label} [${c.parent}]`, value: c.parent });
      for (const k of kids)
        choices.push({ name: `  ${k.name} [${c.parent} ${k.key}]`, value: `${c.parent} ${k.key}` });
    } else if (!c.parent) {
      choices.push({ name: `${c.name} [${c.key}]`, value: c.key });
    }
  }
  choices.push({ name: 'All (full spec check)', value: 'all' });
  return choices;
}
