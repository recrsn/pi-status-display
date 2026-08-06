#!/usr/bin/env bash
# Build pi-status-agent_<version>_arm64.deb from the packaging/debian/
# control files. No debhelper/dpkg-buildpackage needed — dpkg-deb --build
# on a hand-assembled tree is enough for a package this small.
#
# Usage: packaging/build-deb.sh [output-dir]  (default: /tmp)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENT_DIR="$(dirname "$SCRIPT_DIR")"
OUT_DIR="${1:-/tmp}"

VERSION="$(awk '/^Version:/{print $2}' "$SCRIPT_DIR/debian/control")"
PKG="pi-status-agent_${VERSION}_arm64"
ROOT="$(mktemp -d)/$PKG"

echo "==> Building linux/arm64 binary" >&2
mkdir -p "$ROOT/usr/local/bin" "$ROOT/usr/share/pi-status-agent" "$ROOT/DEBIAN"
( cd "$AGENT_DIR" && GOOS=linux GOARCH=arm64 CGO_ENABLED=0 \
    go build -trimpath -o "$ROOT/usr/local/bin/pi-status-agent" ./cmd/pi-status-agent )

echo "==> Assembling package tree" >&2
install -m 644 "$SCRIPT_DIR/debian/control" "$ROOT/DEBIAN/control"
install -m 755 "$SCRIPT_DIR/debian/postinst" "$ROOT/DEBIAN/postinst"
install -m 755 "$SCRIPT_DIR/debian/prerm" "$ROOT/DEBIAN/prerm"
install -m 644 "$SCRIPT_DIR/systemd/pi-status-agent.service" "$ROOT/usr/share/pi-status-agent/"
install -m 644 "$SCRIPT_DIR/udev/99-pi-status.rules" "$ROOT/usr/share/pi-status-agent/"
install -m 644 "$AGENT_DIR/config.example.yaml" "$ROOT/usr/share/pi-status-agent/"

echo "==> Building $PKG.deb" >&2
mkdir -p "$OUT_DIR"
dpkg-deb --build --root-owner-group "$ROOT" "$OUT_DIR/$PKG.deb" >&2
echo "$OUT_DIR/$PKG.deb"
