#!/bin/sh
set -eu

HERE=$(CDPATH='' cd "$(dirname "$0")" && pwd)
OUT=${OUT:-"$HERE/../bin"}
GEN=${GEN:-"$HERE/.gen"}
XTOOL=${XTOOL:-"$HOME/x-tools"}
CC64=${MUGL_CC64:-}
SYS64=${MUGL_SYSROOT:-}
CC32=${MUGL_CC32:-}

if [ -z "$CC64" ] || [ ! -x "$CC64" ]; then
	printf 'mugl: MUGL_CC64 does not name an executable compiler: %s\n' "$CC64" 1>&2
	exit 1
fi

if [ -z "$SYS64" ]; then
	SYS64=$("$CC64" -print-sysroot 2>/dev/null || true)
fi

if [ ! -f "$SYS64/usr/include/GLES2/gl2.h" ]; then
	printf 'mugl: GLES2 headers not found in compiler sysroot: %s\n' "$SYS64" 1>&2
	exit 1
fi

if [ -z "$CC32" ]; then
	set +f
	set -- "$XTOOL"/*/bin/*-gcc
	set -f
	for CANDIDATE in "$@"; do
		[ -x "$CANDIDATE" ] || continue
		MACHINE=$("$CANDIDATE" -dumpmachine 2>/dev/null || true)
		case "$MACHINE" in
			arm*-*gnueabihf*)
				CC32=$CANDIDATE
				break
				;;
		esac
	done
fi

WARN="-Wall -Wextra -Wformat=2 -Werror=format-security -Wno-unused-parameter -Werror=implicit-function-declaration"
HARDEN="-pipe -ffunction-sections -fdata-sections -fstack-protector-strong -fstack-clash-protection -fno-plt"
HARDEN64="$HARDEN -D_FORTIFY_SOURCE=3"
HARDEN32="$HARDEN -D_FORTIFY_SOURCE=2"

mkdir -p "$OUT" "$GEN"
python3 "$HERE/gen.py" "$SYS64/usr/include/GLES2/gl2.h" "$GEN" >/dev/null

# WARN and HARDEN64 deliberately expand into separate compiler arguments.
# shellcheck disable=SC2086
"$CC64" -O2 -mcpu=cortex-a53 $WARN $HARDEN64 -fPIE -DGL_GLEXT_PROTOTYPES -I"$HERE" -I"$GEN" \
	-Wl,--gc-sections -pie -Wl,-z,relro,-z,now \
	-Wl,--strip-all \
	-o "$OUT/mugl-server" "$HERE/server.c" "$HERE/server_frame.c" "$HERE/server_overlay.c" "$GEN/server_gen.c" -lSDL2 -lpthread

if [ -z "$CC32" ] || [ ! -x "$CC32" ]; then
	printf 'mugl: no ARM hard-float compiler found under %s; skipping the 32-bit client\n' "$XTOOL"
	exit 0
fi

LIB32="$OUT/lib32"
mkdir -p "$LIB32"
rm -f "$LIB32/libmugl.so" "$LIB32/libEGL.so" "$LIB32/libGLESv2.so"

# WARN and HARDEN32 deliberately expand into separate compiler arguments.
# shellcheck disable=SC2086
"$CC32" -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard -marm $WARN $HARDEN32 -fPIC -shared \
	-DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES -I"$HERE" -I"$GEN" \
	-Wl,--gc-sections -Wl,-z,relro,-z,now -Wl,-soname,libGLESv2.so.2 \
	-Wl,--strip-all \
	-Wl,--version-script,"$HERE/exports.map" -Wl,-Bsymbolic -Wl,--no-undefined \
	-o "$LIB32/libGLESv2.so.2" \
	"$HERE/client.c" "$HERE/client_gl.c" "$HERE/client_state.c" "$HERE/client_program.c" "$HERE/client_egl.c" "$GEN/client_gen.c" -lpthread

# HARDEN32 deliberately expands into separate compiler arguments.
# shellcheck disable=SC2086
printf '' | "$CC32" -x c -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard -marm $HARDEN32 -fPIC -shared \
	-Wl,--gc-sections -Wl,-z,relro,-z,now -Wl,-soname,libEGL.so.1 \
	-Wl,--strip-all \
	-Wl,--no-as-needed -L"$LIB32" -l:libGLESv2.so.2 \
	-o "$LIB32/libEGL.so.1" -
