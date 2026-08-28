#!/bin/sh
# Verify router_pages.c order matches host pages.json (when sibling repo present).
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
FW="$ROOT/src/router/router_pages.c"
PKG="${MCU_DISPLAY_PKG:-$(cd "$ROOT/../openwrt-packages/feeds/luci/luci-app-mcu-display" 2>/dev/null && pwd)}"
JSON="${PAGES_JSON:-$PKG/root/etc/mcud/pages.json}"
FAIL=0

extract_json_ids() {
	node -e "
const j = require(process.argv[1]);
for (const s of j.screens || [])
	if (s.enabled !== false) process.stdout.write(s.id + ' ');
" "$1"
}

extract_c_page_ids() {
	sed -n '/PAGE_IDS\[/,/\};/p' "$1" | sed -n 's/^[[:space:]]*"\([^"]*\)",/\1/p' | tr '\n' ' '
}

if [ ! -f "$FW" ]; then
	echo "missing $FW" >&2
	exit 1
fi

FW_IDS="$(extract_c_page_ids "$FW")"
echo "router_pages.c: $FW_IDS"

if [ ! -f "$JSON" ]; then
	echo "SKIP: pages.json not found at $JSON" >&2
	exit 0
fi

JSON_IDS="$(extract_json_ids "$JSON")"
echo "pages.json:     $JSON_IDS"

if [ "$JSON_IDS" != "$FW_IDS" ]; then
	echo "pages.json != router_pages.c" >&2
	FAIL=1
else
	echo "OK: page id order matches pages.json"
fi

exit "$FAIL"
