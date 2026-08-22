#!/usr/bin/env bash
#
# Builds and runs the host-side tests against repl/src.
#
#   tests/run.sh            all of them
#   tests/run.sh arena      just one

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/repl/src"
INC="$ROOT/repl/include"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

CFLAGS="-Wall -Wextra -g -I$INC"
pass=0
fail=0

run_one() {
	local name="$1"; shift
	local test_src="$1"; shift

	if ! gcc $CFLAGS -o "$OUT/$name" "$test_src" "$@" 2>"$OUT/$name.log"; then
		printf '\n=== %s: BUILD FAILED ===\n' "$name"
		head -20 "$OUT/$name.log"
		fail=$((fail + 1))
		return
	fi

	printf '\n=== %s ===\n' "$name"
	if timeout 20 "$OUT/$name"; then
		pass=$((pass + 1))
	else
		printf '%s FAILED (exit %s)\n' "$name" "$?"
		fail=$((fail + 1))
	fi
}

want="${1:-all}"

if [ "$want" = all ] || [ "$want" = arena ]; then
	run_one arena "$ROOT/tests/src/test_arena.c" "$SRC/memory-arena.c"
fi

# These two read stdin and assert nothing, so they are smoke tests only:
# they prove the code runs on a real program without crashing.
SAMPLE="$ROOT/repl/program.txt"

if [ "$want" = all ] || [ "$want" = tokeniser ]; then
	{ cat "$SAMPLE"; printf "EOF\n"; } > "$OUT/in.txt"
	run_one tokeniser "$ROOT/tests/src/tokeniser-test.c" "$SRC/tokeniser.c" < "$OUT/in.txt"
fi

if [ "$want" = all ] || [ "$want" = parser ]; then
	{ cat "$SAMPLE"; printf "EOF\n"; } > "$OUT/in.txt"
	run_one parser "$ROOT/tests/src/parser-test.c" "$SRC/parser.c" "$SRC/tokeniser.c" "$SRC/memory-arena.c" < "$OUT/in.txt"
fi

printf '\n%s suite(s) passed, %s failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
