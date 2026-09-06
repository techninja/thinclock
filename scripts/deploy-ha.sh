#!/usr/bin/env bash
# Deploy the ThinClock custom component to HA and reload the integration.
# Usage: npm run deploy:ha
#
# Reads HA_HOST and HA_PORT from environment (or .env).
# Requires your SSH public key to be authorized on the HA host.
# To add it:  ssh-copy-id -p $HA_PORT root@$HA_HOST
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HA_HOST="${HA_HOST:-homeassistant.local}"
HA_PORT="${HA_PORT:-4222}"
SRC="$ROOT/homeassistant/custom_components/thinclock"
DEST="/config/custom_components/thinclock"
SSH="ssh -p $HA_PORT root@$HA_HOST"
SCP="scp -P $HA_PORT"

# Friendly error if SSH auth fails
if ! $SSH true 2>/dev/null; then
  echo ""
  echo "✗ Cannot connect to $HA_HOST:$HA_PORT"
  echo ""
  echo "  Add your public key with:"
  echo "    ssh-copy-id -p $HA_PORT root@$HA_HOST"
  echo ""
  echo "  Or set HA_HOST / HA_PORT in your .env if the address differs."
  exit 1
fi

echo "→ Copying custom component to $HA_HOST:$DEST"
$SSH "mkdir -p $DEST"
$SCP -r "$SRC"/* "root@$HA_HOST:$DEST/"

echo "→ Reloading ThinClock integration"
$SSH "ha core restart" 2>/dev/null || $SSH "killall -HUP python3" 2>/dev/null || true

echo ""
echo "✓ Done — integration reloaded on $HA_HOST"
