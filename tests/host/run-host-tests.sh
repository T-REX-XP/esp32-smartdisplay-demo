#!/bin/sh
# Host-compile proto + app with gcov; require 100% line coverage.
set -eu
DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$DIR"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cc -std=c99 -Wall -Wextra -Werror -O0 --coverage \
	-I"$DIR/src/proto" -I"$DIR/src/app" \
	-o "$TMP/test_rdcp" \
	tests/host/test_rdcp.c src/proto/rdcp.c
"$TMP/test_rdcp"

cc -std=c99 -Wall -Wextra -Werror -O0 --coverage \
	-I"$DIR/src/proto" -I"$DIR/src/app" \
	-o "$TMP/test_router_engine" \
	tests/host/test_router_engine.c src/app/router_engine.c src/proto/rdcp.c
"$TMP/test_router_engine"

# gcov files land next to objects in TMP if we compile there — recompile in TMP
# so .gcda/.gcno stay together.
cd "$TMP"
cc -std=c99 -Wall -Wextra -Werror -O0 --coverage \
	-I"$DIR/src/proto" -I"$DIR/src/app" \
	-c "$DIR/src/proto/rdcp.c" -o rdcp.o
cc -std=c99 -Wall -Wextra -Werror -O0 --coverage \
	-I"$DIR/src/proto" -I"$DIR/src/app" \
	-c "$DIR/src/app/router_engine.c" -o router_engine.o
cc -std=c99 -Wall -Wextra -Werror -O0 --coverage \
	-I"$DIR/src/proto" -I"$DIR/src/app" \
	-c "$DIR/tests/host/test_rdcp.c" -o test_rdcp.o
cc -std=c99 -Wall -Wextra -Werror -O0 --coverage \
	-I"$DIR/src/proto" -I"$DIR/src/app" \
	-c "$DIR/tests/host/test_router_engine.c" -o test_router_engine.o
cc --coverage -o test_rdcp test_rdcp.o rdcp.o
cc --coverage -o test_router_engine test_router_engine.o router_engine.o rdcp.o
./test_rdcp
./test_router_engine

GCOV=gcov
if ! command -v gcov >/dev/null 2>&1; then
	if command -v llvm-cov >/dev/null 2>&1; then
		GCOV="llvm-cov gcov"
	else
		echo "gcov not found"
		exit 1
	fi
fi
$GCOV -b rdcp.c router_engine.c >/dev/null

check_gcov() {
	file="$1"
	if [ ! -f "$file" ]; then
		echo "missing $file"
		return 1
	fi
	# ##### = uncovered executable line
	if grep -E '^[[:space:]]*#####:' "$file" >/dev/null; then
		echo "FAIL: uncovered lines in $file"
		grep -E '^[[:space:]]*#####:' "$file"
		return 1
	fi
	echo "OK: 100% lines $file"
}

check_gcov rdcp.c.gcov
check_gcov router_engine.c.gcov
