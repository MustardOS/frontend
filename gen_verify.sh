#!/bin/sh

INTERNAL_BASE="${1:?usage: $0 <internal repo>}"

TARGET_BASE="/opt/muos/script"
OUT="./common/verify.h"

INTERNAL_BASE=${INTERNAL_BASE%/}
DIRS="device init mount mux package system var web"

command -v xxhsum >/dev/null 2>&1 || {
	echo "xxhsum not found"
	exit 1
}

{
	echo "#pragma once"
	echo
	echo "struct int_script_hash {"
	echo "    const char *path;"
	echo "    const char *hash;"
	echo "};"
	echo
	echo "int script_hash_check(void);"
	echo
	echo "static const struct int_script_hash int_scripts[] = {"

	for d in $DIRS; do
		[ -d "$INTERNAL_BASE/$d" ] || continue
		find "$INTERNAL_BASE/$d" -type f
	done |
		LC_ALL=C sort |
		while IFS= read -r FILE; do
			HASH=$(xxhsum "$FILE" | awk '{print $1}')
			TARGET_PATH=$(printf '%s\n' "$FILE" | sed "s|^$INTERNAL_BASE|$TARGET_BASE|")
			printf "    { \"%s\", \"%s\" },\n" "$TARGET_PATH" "$HASH"
		done

	echo "};"
} >"$OUT"
