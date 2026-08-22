#!/usr/bin/env bash
# Runs every .mu file under valid/ and invalid/ through the parser and
# checks that valid cases parse cleanly and invalid cases are rejected
# with a "parse error". Builds the parser-test binary directly against
# parser.c/tokeniser.c/memory-arena.c (bypassing src/interpreter.c, which
# doesn't currently compile).
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$SCRIPT_DIR/.parser-test-bin"

gcc -Wall -Wextra -g -I"$ROOT_DIR/src" -o "$BIN" \
    "$ROOT_DIR/tests/src/parser-test.c" \
    "$ROOT_DIR/src/parser.c" \
    "$ROOT_DIR/src/tokeniser.c" \
    "$ROOT_DIR/src/memory-arena.c" || exit 1

pass=0
fail=0

run_case() {
    local file="$1" expect="$2"
    local out
    out="$(printf '%s\nEOF\n' "$(cat "$file")" | "$BIN" 2>&1)"
    local code=$?

    if [ "$expect" = "valid" ]; then
        if [ $code -eq 0 ]; then
            echo "PASS  $file"
            pass=$((pass+1))
        else
            echo "FAIL  $file (expected to parse, exit $code)"
            fail=$((fail+1))
        fi
    else
        if [ $code -ne 0 ] && echo "$out" | grep -q "parse error"; then
            echo "PASS  $file"
            pass=$((pass+1))
        else
            echo "FAIL  $file (expected a parse error, exit $code)"
            fail=$((fail+1))
        fi
    fi
}

for f in "$SCRIPT_DIR"/valid/*.mu; do
    run_case "$f" valid
done

for f in "$SCRIPT_DIR"/invalid/*.mu; do
    run_case "$f" invalid
done

rm -f "$BIN"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
