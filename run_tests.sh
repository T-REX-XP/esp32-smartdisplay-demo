#!/bin/sh
# Host-side unit tests for esp32-smartdisplay-demo.
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$DIR/src/router"
cd "$DIR"
FAIL=0

echo ">> shell: pages manifest sync"
sh scripts/check-pages-sync.sh || FAIL=1

echo ""
echo ">> shell: mcud-version sync"
sh scripts/check-version-sync.sh || FAIL=1

echo ""
echo ">> shell: RDCP fixture sync"
sh scripts/check-rdcp-fixtures.sh || FAIL=1

echo ""
echo ">> C: proto+app host tests (100% coverage)"
sh tests/host/run-host-tests.sh || FAIL=1

echo ""
echo ">> C: test_router_data.c"
cc -std=c99 -Wall -Wextra -I"$SRC" -o test_router_data \
	tests/test_router_data.c \
	"$SRC/router_data.c" \
	"$SRC/router_pages.c" || FAIL=1
if [ "$FAIL" -eq 0 ]; then
	./test_router_data || FAIL=1
	rm -f test_router_data
fi

echo ""
echo ">> C: test_router_pages.c"
cc -std=c99 -Wall -Wextra -I"$SRC" -o test_router_pages \
	tests/test_router_pages.c \
	"$SRC/router_pages.c" || FAIL=1
if [ "$FAIL" -eq 0 ]; then
	./test_router_pages || FAIL=1
	rm -f test_router_pages
fi

echo ""
echo ">> Python: simulator protocol"
if [ ! -d .venv ]; then
	python3 -m venv .venv
	.venv/bin/pip install -q -r requirements.txt
fi
.venv/bin/python run_tests.py || FAIL=1

if [ "$FAIL" -eq 0 ]; then
	echo "All tests passed."
else
	echo "Some tests failed."
fi
exit "$FAIL"
