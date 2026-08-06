#!/usr/bin/env bash
# Build the agent .deb and install it on a Pi over SSH.
#
# Usage: packaging/deploy.sh user@host
# Requires passwordless (key-based) SSH — this script does not prompt for a
# password.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 user@host" >&2
    exit 1
fi
HOST="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DEB="$("$SCRIPT_DIR/build-deb.sh")"
echo "==> Built $DEB"

echo "==> Copying to $HOST"
scp -q "$DEB" "$HOST:/tmp/"

echo "==> Installing on $HOST"
ssh "$HOST" "sudo dpkg -i /tmp/$(basename "$DEB") && rm -f /tmp/$(basename "$DEB") && systemctl status --no-pager pi-status-agent"

echo "==> Done. Tail logs with: ssh $HOST journalctl -u pi-status-agent -f"
