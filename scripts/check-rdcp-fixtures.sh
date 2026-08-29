#!/bin/sh
# Compare shared RDCP golden traces with the host mcudd tree when present.
set -eu
DIR="$(cd "$(dirname "$0")/.." && pwd)"
FW="$DIR/testdata/rdcp"
HOST="$(cd "$DIR/../openwrt-packages/feeds/packages/mcudd/testdata/rdcp" 2>/dev/null && pwd)" || HOST=""
if [ -z "$HOST" ] || [ ! -d "$HOST" ]; then
	echo "host testdata missing (skip sibling diff)"
	exit 0
fi
fail=0
for f in handshake.jsonl ping.jsonl echo.jsonl screen.jsonl gesture.jsonl metrics-system.jsonl; do
	if ! cmp -s "$FW/$f" "$HOST/$f"; then
		echo "MISMATCH: $f"
		diff -u "$FW/$f" "$HOST/$f" || true
		fail=1
	fi
done
if [ "$fail" -ne 0 ]; then
	echo "RDCP fixtures are out of sync"
	exit 1
fi
echo "RDCP fixtures match host testdata"
