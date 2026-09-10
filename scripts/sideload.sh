#!/usr/bin/env bash
# Assemble and sideload the ThinClock add-on to HA Yellow.
# Usage: npm run sideload
set -e

ADDON_DIR="$(cd "$(dirname "$0")/../thinclock-addon" && pwd)"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HA_HOST="${HA_HOST:-homeassistant.local}"
HA_PORT="${HA_PORT:-4222}"
HA_DEST="/addons/thinclock"

echo "→ Staging add-on files into thinclock-addon/"

# Copy package files, stripping dev-only scripts
node -e "
  const p = JSON.parse(require('fs').readFileSync('$ROOT/package.json'));
  delete p.scripts.postinstall;
  require('fs').writeFileSync('$ADDON_DIR/package.json', JSON.stringify(p, null, 2));
"
cp "$ROOT/package-lock.json" "$ADDON_DIR/package-lock.json"

# Sync src/ (exclude dev-only files)
rsync -a --delete \
  --exclude='*.test.js' \
  --exclude='*.spec.js' \
  "$ROOT/src/" "$ADDON_DIR/src/"

echo "→ Copying to $HA_HOST:$HA_DEST"
ssh -p "$HA_PORT" "root@$HA_HOST" "rm -rf $HA_DEST && mkdir -p $HA_DEST"
scp -P "$HA_PORT" -r "$ADDON_DIR"/* "root@$HA_HOST:$HA_DEST/"

echo "→ Reloading add-ons on HA"
ssh -p "$HA_PORT" "root@$HA_HOST" "ha addons reload"

echo ""
echo "✓ Done. In HA: Settings → Add-ons → ThinClock (Local) → Install"
