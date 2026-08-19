#!/bin/sh

set -euf

HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

LIBS="ffmpeg libarchive mojibake openssl rcheevos"

[ $# -gt 0 ] && LIBS="$*"

for LIB in $LIBS; do
	if [ ! -x "$HERE/$LIB.sh" ]; then
		printf 'Error: no such external library: %s\n' "$LIB" 1>&2
		exit 1
	fi

	"$HERE/$LIB.sh" || {
		printf 'Error: %s failed to build\n' "$LIB" 1>&2
		exit 1
	}
done
