#!/bin/sh

set -euf

DEVICE=${DEVICE:-ARM64_A53}
PLATFORM=${PLATFORM:-unix}
ARCH=${ARCH:-arm64}

XTOOL=${XTOOL:-"$HOME/x-tools"}
XHOST=${XHOST:-"aarch64-buildroot-linux-gnu"}

CPU_CFLAGS_DEFAULT='-march=armv8-a+simd -mtune=cortex-a53'

USAGE() {
	printf 'muOS Frontend Builder + Cross Compile Tool\n'
	printf '\n'
	printf '%s\n' "Usage:"
	printf '  %s [print|make [args...]]\n' "$0"
	printf '\n'
	printf '%s\n' "Examples:"
	printf '  %s print\n' "$0"
	printf '  %s make -j4\n' "$0"
	printf '\n'
	printf 'Keep Shell Variables:\n'
	printf '  source %s\n' "$0"
	printf '\n'

	exit 0
}

DETECT_HOST() {
	# Only attempt if XHOST is empty and XTOOL exists
	[ -n "${XHOST-}" ] && [ "$XHOST" != "" ] && return 0
	[ -d "$XTOOL" ] || return 0

	# Guard against empty glob
	set -- "$XTOOL"/*
	[ -e "${1-}" ] || return 0

	for D in "$XTOOL"/*; do
		[ -d "$D/bin" ] || continue
		set -- "$D"/bin/*-gcc

		[ -e "${1-}" ] || continue
		XHOST=$(basename "$D")

		export XHOST
		return 0
	done
}

# Choose a sysroot path that matches common buildroot or ct-ng layouts
DETECT_SYSROOT() {
	# Allow override by env exports
	[ -n "${SYSROOT-}" ] && [ "$SYSROOT" != "" ] && return 0

	C1="$XTOOL/$XHOST/$XHOST/sysroot"
	C2="$XTOOL/$XHOST/$XHOST"

	if [ -d "$C1" ]; then
		SYSROOT="$C1"
	elif [ -d "$C2/sysroot" ]; then
		SYSROOT="$C2/sysroot"
	elif [ -d "$C2/usr" ]; then
		SYSROOT="$C2"
	else
		SYSROOT=""
	fi

	export SYSROOT
}

CHECK_TOOLS() {
	if [ ! -d "$XBIN" ]; then
		printf 'Error: XBIN not found: %s\n' "$XBIN" 1>&2
		exit 1
	fi

	for T in "${CROSS_COMPILE}gcc" "${CROSS_COMPILE}g++" "${CROSS_COMPILE}ar" "${CROSS_COMPILE}strip"; do
		if ! command -v "$T" >/dev/null 2>&1; then
			printf 'Error: required tool not found in PATH: %s\n' "$T" 1>&2
			exit 1
		fi
	done

	# Prefer pkgconf in toolchain bin, else fall back to system pkgconf/pkg-config
	if [ -x "$XBIN/pkgconf" ]; then
		PKG_CONFIG="$XBIN/pkgconf"
	elif command -v pkgconf >/dev/null 2>&1; then
		PKG_CONFIG=pkgconf
	else
		PKG_CONFIG=pkg-config
	fi

	export PKG_CONFIG
}

DETECT_HOST

if [ -z "$XHOST" ]; then
	printf '%s\n' "Error: could not detect XHOST automatically. Set XHOST (e.g. aarch64-buildroot-linux-gnu)." 1>&2
	exit 1
fi

XBIN="$XTOOL/$XHOST/bin"
export XBIN

PATH="$XBIN:$PATH"
export PATH

DETECT_SYSROOT

if [ -z "$SYSROOT" ]; then
	printf 'Warning: could not detect SYSROOT under %s\n' "$XTOOL/$XHOST" 1>&2
	# Continue without sysroot if user wants purely prefix-based includes/libs,
	# but most setups do have a sysroot. You can set SYSROOT manually.
fi

# Canonical CROSS_COMPILE prefix (with path so callers can just use ${CROSS_COMPILE}gcc, etc.)
CROSS_COMPILE="$XBIN/$XHOST-"
export CROSS_COMPILE

# Tool variables (use the prefix to avoid duplication)
CC="${CROSS_COMPILE}gcc"
CXX="${CROSS_COMPILE}g++"
AR="${CROSS_COMPILE}ar"
LD="${CROSS_COMPILE}ld"
STRIP="${CROSS_COMPILE}strip"
export CC CXX AR LD STRIP

# Core flags: minimal and override-friendly.
CPPFLAGS=${CPPFLAGS:-}
CFLAGS=${CFLAGS:-$CPU_CFLAGS_DEFAULT}
CXXFLAGS=${CXXFLAGS:-}
LDFLAGS=${LDFLAGS:-}

if [ -n "$SYSROOT" ]; then
	CPPFLAGS="${CPPFLAGS} --sysroot=$SYSROOT -I$SYSROOT/usr/include"
	CXXFLAGS="${CXXFLAGS} --sysroot=$SYSROOT -I$SYSROOT/usr/include"
	CFLAGS="${CFLAGS} --sysroot=$SYSROOT -I$SYSROOT/usr/include"
	LDFLAGS="${LDFLAGS} --sysroot=$SYSROOT -L$SYSROOT/lib -L$SYSROOT/usr/lib -L$SYSROOT/usr/local/lib"
fi

export CPPFLAGS CFLAGS CXXFLAGS LDFLAGS

# pkg-config stuff; tell it about the sysroot and where .pc files live - this is annoying
if [ -n "$SYSROOT" ]; then
	PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
	PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig:$SYSROOT/usr/local/lib/pkgconfig"
	export PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR
fi

ARMABI="$XHOST"
TOOLCHAIN_DIR="$XTOOL/$XHOST"
DESTDIR="${SYSROOT:-}"
export DEVICE PLATFORM ARCH ARMABI TOOLCHAIN_DIR DESTDIR

# Optional legacy-style INC_DIR/LIB_DIR for build systems that read them
INC_DIR="$CPPFLAGS"
LIB_DIR="$LDFLAGS"
export INC_DIR LIB_DIR

# SDL/Freetype/ALSA via pkg-config — callers should prefer: $(pkg-config --cflags --libs sdl2 freetype2 alsa)
# Kept for compatibility if some Makefiles look for them:
if command -v "${PKG_CONFIG:-pkg-config}" >/dev/null 2>&1; then
	if "${PKG_CONFIG:-pkg-config}" --exists sdl2 >/dev/null 2>&1; then
		SDL2CONFIG="$SYSROOT/usr/bin/sdl2-config"
		export SDL2CONFIG
	fi
	if "${PKG_CONFIG:-pkg-config}" --exists alsa >/dev/null 2>&1; then
		ALSA_CFLAGS=$("${PKG_CONFIG:-pkg-config}" --cflags alsa 2>/dev/null || printf '')
		ALSA_LIBS=$("${PKG_CONFIG:-pkg-config}" --libs alsa 2>/dev/null || printf '')
		export ALSA_CFLAGS ALSA_LIBS
	fi
fi

CHECK_TOOLS

PRINT_ENV() {
	printf 'Device:        %s\n' "$DEVICE"
	printf 'Platform:      %s\n' "$PLATFORM"
	printf 'Arch:          %s\n' "$ARCH"
	printf 'Cross Root:    %s\n' "$XTOOL"
	printf 'Host Tuple:    %s\n' "$XHOST"
	printf 'Bin Path:      %s\n' "$XBIN"

	if [ -n "$SYSROOT" ]; then
		printf 'Sysroot:       %s\n' "$SYSROOT"
	else
		printf 'Sysroot:       (not detected)\n'
	fi

	printf 'CC:            %s\n' "$CC"
	printf 'CFLAGS:        %s\n' "$CFLAGS"
	printf 'CXXFLAGS:      %s\n' "$CXXFLAGS"
	printf 'CPPFLAGS:      %s\n' "$CPPFLAGS"
	printf 'LDFLAGS:       %s\n' "$LDFLAGS"

	if [ -n "${PKG_CONFIG-}" ]; then
		printf 'PKG_CONFIG:    %s\n' "$PKG_CONFIG"
	fi
}

RUN_MAKE() {
	if command -v make >/dev/null 2>&1; then
		exec make "${@-}"
	fi

	printf '%s\n' "Error: 'make' not found on PATH." 1>&2
	exit 1
}

CMD=${1-}
case "$CMD" in
	make) shift && RUN_MAKE "$@" ;;
	print) PRINT_ENV ;;
	*) USAGE ;;
esac
