#!/usr/bin/env bash

# pipe all contents of a specified file to specified port in batch mode and
# stream the reply to stdout as it arrives

set -u

# fix baud rate at 115200
BAUD="${3:-115200}"

# check correct num args
if [ $# -lt 2 ]; then
	echo "usage: $0 <file> <port> [baud]" >&2
	exit 2
fi

# alias args
FILE="$1"
PORT="$2"

# check if given file is readable
if [ ! -r "$FILE" ]; then
	echo "$0: cannot read $FILE" >&2
	exit 1
fi

# check if given port is character special device file
if [ ! -c "$PORT" ]; then
	echo "$0: $PORT is not a valid serial device" >&2
	exit 1
fi

# check if port is busy
if ! stty -F "$PORT" "$BAUD" raw -echo clocal -hupcl 2>/dev/null; then
	echo "$0: $PORT is busy" >&2
	exit 1
fi

exec 3<>"$PORT"

# ctrl c has to stop the program on the board, not just this script, otherwise
# an ep loop keeps running and the next upload lands on top of it
cleanup() {
	printf '\x03' >&3 2>/dev/null || true
	exec 3<&- 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT

timeout 1 cat <&3 >/dev/null 2>&1    # let the port settle, early writes get lost

printf '\x03' >&3                    # cancel anything left over from last time
printf '\x05' >&3                    # start batch mode
cat "$FILE" >&3
printf '\x04' >&3                    # end batch mode

# print from the input header onward and stop at the end header. a program that
# returns itself never prints one, so ctrl c is how you leave it.
#
# awk reads the port itself instead of sitting behind cat. cat would block in a
# read and only notice awk had gone the next time it wrote, so the script hung
# on after the end header. awk also strips the carriage returns, where piping
# through tr block buffers and hands over nothing until it is killed.
#
# the markers can carry the terminal reset the repl emits before them, and that
# has no newline of its own, so match on a copy with the escapes stripped and
# print the untouched line so program colours survive
filter='
{
	line = $0
	sub(/\r$/, "", line)
	probe = line
	gsub(/\033\[[0-9;?]*[a-zA-Z]/, "", probe)
}
probe ~ /^=+ input =+$/ { seen = 1 }
seen                    { print line; fflush() }
probe ~ /^=+ end =+$/   { exit }
'

# unbounded by default so a running program can be watched, set UPLOAD_TIMEOUT
# to put a ceiling on it for scripted use
if [ -n "${UPLOAD_TIMEOUT:-}" ]; then
	timeout "$UPLOAD_TIMEOUT" awk "$filter" <&3
else
	awk "$filter" <&3
fi
