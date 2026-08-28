#!/bin/sh
# Ensure mcud-version.json matches luci-app-mcu-display host copy and PKG_RELEASE.
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
JSON="$ROOT/mcud-version.json"
PKG="${MCU_DISPLAY_PKG:-$(cd "$ROOT/../openwrt-packages/feeds/luci/luci-app-mcu-display" 2>/dev/null && pwd)}"
HOST_JSON="${HOST_JSON:-$PKG/mcud-version.json}"
MAKEFILE="${MCU_DISPLAY_MAKEFILE:-$PKG/Makefile}"
FAIL=0

if [ ! -f "$JSON" ]; then
	echo "missing $JSON" >&2
	exit 1
fi

if [ ! -f "$HOST_JSON" ]; then
	echo "SKIP: host mcud-version.json not found at $HOST_JSON" >&2
else
	if ! diff -q "$JSON" "$HOST_JSON" >/dev/null; then
		echo "mcud-version.json out of sync with host package:" >&2
		diff -u "$JSON" "$HOST_JSON" >&2 || true
		FAIL=1
	else
		echo "OK: mcud-version.json matches host package"
	fi
fi

if [ -f "$MAKEFILE" ]; then
	RELEASE="$(node -e "console.log(JSON.parse(require('fs').readFileSync('$JSON','utf8')).release)")"
	PKG_RELEASE="$(grep '^PKG_RELEASE:=' "$MAKEFILE" | sed 's/.*:=//')"
	if [ "$RELEASE" != "$PKG_RELEASE" ]; then
		echo "PKG_RELEASE ($PKG_RELEASE) != manifest release ($RELEASE)" >&2
		FAIL=1
	else
		echo "OK: PKG_RELEASE matches manifest release ($RELEASE)"
	fi
else
	echo "SKIP: Makefile not found at $MAKEFILE" >&2
fi

exit "$FAIL"
