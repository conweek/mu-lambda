#!/usr/bin/env bash

# pipe all contents of a specified file to specified port in batch mode and print output to stdout

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

# give some time for everything to print out
SIZE=$(wc -c < "$FILE")
WAIT=$(( SIZE / 2000 + 4 ))

exec 3<>"$PORT"

timeout 1 cat <&3 >/dev/null 2>&1    # let the port settle, early writes get lost

printf '\x03' >&3                    # cancel anything left over from last time
printf '\x05' >&3                    # start batch mode
cat "$FILE" >&3
printf '\x04' >&3                    # end batch mode

# only show input and output sections unless headers not detected in output
RAW=$(timeout "$WAIT" cat <&3 | tr -d '\r') 
OUT=$(printf '%s\n' "$RAW" | sed -n '/^=* input =*$/,/^=* end =*$/p')

if [ -n "$OUT" ]; then
	printf '%s\n' "$OUT"
else
	echo "$0: no headers in reply, dumping raw transcript" >&2
	printf '%s\n' "$RAW"
fi

exec 3<&-
