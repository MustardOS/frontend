#!/bin/sh
set -eu

HERE=$(CDPATH='' cd "$(dirname "$0")" && pwd)
OUT=${OUT:-"$HERE/../bin"}
GEN=${GEN:-"$HERE/.gen"}
XTOOL=${XTOOL:-"$HOME/x-tools"}

CC32="$XTOOL/arm-buildroot-linux-gnueabihf/bin/arm-buildroot-linux-gnueabihf-gcc"
CC64="$XTOOL/aarch64-buildroot-linux-gnu/bin/aarch64-buildroot-linux-gnu-gcc"
SYS64="$XTOOL/aarch64-buildroot-linux-gnu/aarch64-buildroot-linux-gnu/sysroot"

WARN="-Wall -Wextra -Wformat=2 -Werror=format-security -Wno-unused-parameter -Werror=implicit-function-declaration"
HARDEN="-pipe -ffunction-sections -fdata-sections -fstack-protector-strong -fstack-clash-protection -fno-plt"
HARDEN64="$HARDEN -D_FORTIFY_SOURCE=3"
HARDEN32="$HARDEN -D_FORTIFY_SOURCE=2"

mkdir -p "$OUT" "$GEN"
python3 "$HERE/gen.py" "$SYS64/usr/include/GLES2/gl2.h" "$GEN" >/dev/null

"$CC64" -O2 -mcpu=cortex-a53 $WARN $HARDEN64 -fPIE -DGL_GLEXT_PROTOTYPES -I"$HERE" -I"$GEN" \
	-Wl,--gc-sections -pie -Wl,-z,relro,-z,now \
	-Wl,--strip-all \
	-o "$OUT/mugl-server" "$HERE/server.c" "$HERE/server_frame.c" "$HERE/server_overlay.c" "$GEN/server_gen.c" -lSDL2 -lpthread

if [ ! -x "$CC32" ]; then
	printf 'mugl: no armhf toolchain at %s, skipping the 32-bit client\n' "$CC32"
	exit 0
fi

LIB32="$OUT/lib32"
mkdir -p "$LIB32"
rm -f "$LIB32/libmugl.so" "$LIB32/libEGL.so" "$LIB32/libGLESv2.so"

"$CC32" -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard -marm $WARN $HARDEN32 -fPIC -shared \
	-DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES -I"$HERE" -I"$GEN" \
	-Wl,--gc-sections -Wl,-z,relro,-z,now -Wl,-soname,libGLESv2.so.2 \
	-Wl,--strip-all \
	-Wl,--version-script,"$HERE/exports.map" -Wl,-Bsymbolic -Wl,--no-undefined \
	-o "$LIB32/libGLESv2.so.2" \
	"$HERE/client.c" "$HERE/client_gl.c" "$HERE/client_state.c" "$HERE/client_program.c" "$HERE/client_egl.c" "$GEN/client_gen.c" -lpthread

printf '' | "$CC32" -x c -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard -marm $HARDEN32 -fPIC -shared \
	-Wl,--gc-sections -Wl,-z,relro,-z,now -Wl,-soname,libEGL.so.1 \
	-Wl,--strip-all \
	-Wl,--no-as-needed -L"$LIB32" -l:libGLESv2.so.2 \
	-o "$LIB32/libEGL.so.1" -
