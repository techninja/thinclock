# Spec Workflow: LLM Sessions & Compliance

## The Problem

LLM coding sessions move fast. Features emerge through rapid iteration,
and stopping to run `npm run spec` after every file edit creates friction
that kills creative velocity. But ignoring the spec entirely leads to
a costly batch refactor at the end — splitting files, fixing lint, and
chasing type errors across dozens of changes.

## The Balance: Feature Checkpoints

Don't run spec after every change. Don't ignore it until deploy. Instead,
run spec at **natural feature boundaries**:

```
Feature unit complete → npm run spec code
Section complete → npm run spec all
Pre-deploy → npm run spec all + types
```

A "feature unit" is a logical chunk: "media grid view works," "portfolio
section complete," "name correction system done." Not every file save.

## Project Rules for LLM Context

The biggest spec compliance gap is **the LLM not knowing the rules exist.**
Spec docs live in `docs/` but aren't automatically loaded into session
context. Fix this with a project-level rules file:

```
.amazonq/rules/clearstack.md   # Loaded every session automatically
```

This file should state:

- The 150-line limit and what to do at 120 lines
- Decomposition patterns (what to extract when)
- Code style enforcement (so the LLM writes compliant code from the start)
- The assumption that spec watch may be running

When an LLM knows the constraint exists, it naturally writes smaller
files, adds `SPLIT CANDIDATE` comments, and avoids patterns that will
need refactoring later.

## The SPLIT CANDIDATE Pattern

When a file passes ~120 lines during development:

```javascript
// SPLIT CANDIDATE: keyboard handler → utils/mediaKeys.js
// SPLIT CANDIDATE: carousel template → utils/mediaCarousel.js
```

These comments serve two purposes:

1. Signal to the current session where to split if the file goes over
2. Signal to a future session exactly how to decompose without re-reading

## Spec Watch Mode (Proposed)

A dashboard that runs continuously during development:

```bash
npm run spec --watch
```

Displays a compact status board:

```
  ✅ code (67 files)
  ✅ lint (44 files)
  ⚠️  types (1 warning)
  ✅ docs (12 files)
```

When a violation occurs, it shows the error inline and provides a
copy-pasteable block the developer can drop into their LLM session:

```
  ❌ code: src/pages/media/media-detail-view.js (167 lines, max 150)
     Split candidates: L45 data loading, L89 carousel template, L140 related grid
```

This gives the LLM everything it needs to fix the issue without running
the check itself, re-reading the file, or guessing at the structure.

## Automated Ignore Configuration

The spec checker should manage ignore paths centrally rather than
requiring manual duplication across eslint, prettier, stylelint, and
jsconfig. A single `SPEC_IGNORE_DIRS` in `.env` should propagate to
all tools automatically, or the spec checker should inject ignores
when running each tool.

Current pain: adding `src/space/` required editing 5 different config
files. It should require editing one.

## Decomposition Patterns for Views

View components (page-level) are the most common files to exceed 150
lines because they combine: imports, data loading, state management,
keyboard handlers, and large template literals.

Extraction priority (most reusable → most specific):

| What to extract                | Where to put it                  | When                         |
| ------------------------------ | -------------------------------- | ---------------------------- |
| Data loading/caching           | `src/utils/{feature}Loader.js`   | >20 lines of fetch/transform |
| Keyboard/event handlers        | `src/utils/{feature}Keys.js`     | >10 lines of handler logic   |
| Repeated template fragments    | `src/utils/{feature}Template.js` | Used in 2+ views             |
| Render sub-sections            | `src/utils/{feature}Render.js`   | Single view, but >30 lines   |
| Shared UI (header, breadcrumb) | `src/components/`                | Used site-wide               |

The goal isn't to have zero logic in view files — it's to keep each
file answerable with "what does this do?" in one pass.
