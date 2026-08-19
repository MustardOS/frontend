#!/bin/sh

# shellcheck shell=sh disable=SC2034
set -eu

EXT_ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

EXT_DIST="$EXT_ROOT/dist"
EXT_SRC="$EXT_ROOT/src"

: "${DEVICE:?DEVICE not set - build via ./build.sh make}"

EXT_WORK="$EXT_ROOT/work/$DEVICE"
EXT_PREFIX="$EXT_ROOT/prefix/$DEVICE"

if [ -z "${EXT_ARCH_FLAGS-}" ]; then
	printf 'Error: EXT_ARCH_FLAGS not set - build via "./build.sh make" so external.mk can supply it\n' 1>&2
	exit 1
fi

if [ "$DEVICE" != "NATIVE" ] && [ -z "${CROSS_COMPILE-}" ]; then
	printf 'Error: CROSS_COMPILE not set - build via "./build.sh make" so the toolchain is on the path\n' 1>&2
	exit 1
fi

EXT_JOBS=$(nproc 2>/dev/null || echo 4)

if [ "$DEVICE" = "NATIVE" ]; then
	EXT_HOST=""
	EXT_CC="gcc"
else
	EXT_HOST=$(basename "$CROSS_COMPILE" | sed 's/-$//')
	EXT_CC="${CROSS_COMPILE}gcc"
fi

EXT_TRIPLE=$("$EXT_CC" -dumpmachine 2>/dev/null || true)

if [ -z "$EXT_TRIPLE" ]; then
	printf 'Error: could not run %s -dumpmachine\n' "$EXT_CC" 1>&2
	exit 1
fi

EXT_ARCH=${EXT_TRIPLE%%-*}

EXT_PTR_SIZE=$(echo | "$EXT_CC" -E -dM - 2>/dev/null | awk '/__SIZEOF_POINTER__/{print $3}')
[ -n "$EXT_PTR_SIZE" ] || EXT_PTR_SIZE=8

EXT_AR="${CROSS_COMPILE-}ar"
EXT_RANLIB="${CROSS_COMPILE-}ranlib"

EXT_SYSROOT_CFLAGS=""
[ -n "${SYSROOT-}" ] && EXT_SYSROOT_CFLAGS="--sysroot=$SYSROOT"

EXT_CFLAGS="$EXT_ARCH_FLAGS $EXT_SYSROOT_CFLAGS -fPIC -ffunction-sections -fdata-sections"
mkdir -p "$EXT_DIST" "$EXT_SRC" "$EXT_PREFIX"

EXT_UP_TO_DATE() {
	STAMP_FILE="$EXT_PREFIX/.stamp-$1"
	STAMP_MARKER="$EXT_PREFIX/$2"

	[ -f "$STAMP_FILE" ] || return 1
	[ -e "$STAMP_MARKER" ] || return 1
	[ "$(cat "$STAMP_FILE")" = "$3" ] || return 1

	printf '%s already built for %s\n' "$1" "$DEVICE"
	return 0
}

EXT_STAMP() {
	printf '%s' "$2" >"$EXT_PREFIX/.stamp-$1"
}

EXT_FETCH() {
	FETCH_URL="$1"
	FETCH_FILE="$EXT_DIST/$2"
	FETCH_SHA="$3"

	if [ ! -f "$FETCH_FILE" ]; then
		printf 'Fetching %s\n' "$2"

		if command -v curl >/dev/null 2>&1; then
			curl -fsSL -o "$FETCH_FILE.part" "$FETCH_URL"
		elif command -v wget >/dev/null 2>&1; then
			wget -q -O "$FETCH_FILE.part" "$FETCH_URL"
		else
			printf 'Error: neither curl nor wget is available\n' 1>&2
			exit 1
		fi

		mv "$FETCH_FILE.part" "$FETCH_FILE"
	fi

	FETCH_GOT=$(sha256sum "$FETCH_FILE" | cut -d' ' -f1)

	if [ "$FETCH_GOT" != "$FETCH_SHA" ]; then
		printf 'Error: checksum mismatch for %s\n  expected %s\n  got      %s\n' "$2" "$FETCH_SHA" "$FETCH_GOT" 1>&2
		rm -f "$FETCH_FILE"
		exit 1
	fi
}

EXT_EXTRACT() {
	FETCH_FILE="$EXT_DIST/$1"
	ARCHIVE_DIR="$EXT_SRC/$2"

	[ -d "$ARCHIVE_DIR" ] && return 0

	printf 'Extracting %s\n' "$1"

	case "$1" in
		*.tar.xz) tar xJf "$FETCH_FILE" -C "$EXT_SRC" ;;
		*.tar.gz | *.tgz) tar xzf "$FETCH_FILE" -C "$EXT_SRC" ;;
		*)
			printf 'Error: unknown archive type: %s\n' "$1" 1>&2
			exit 1
			;;
	esac
}

EXT_RESET_WORK() {
	rm -rf "${EXT_WORK:?}/${1:?}"
	mkdir -p "$EXT_WORK/$1"
}
