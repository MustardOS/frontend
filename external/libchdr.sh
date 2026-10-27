#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="0.3.0"
SHA256="313f6bf5537e2494daa3daa8a931a2536fc4f2b8312c07e3c3d5324d2052fa20"
TARBALL="libchdr-$VERSION.tar.gz"
SOURCE="libchdr-$VERSION"
URL="https://github.com/rtissera/libchdr/archive/refs/tags/v$VERSION.tar.gz"
ID="$VERSION|system-zlib-zstd|$EXT_ARCH_FLAGS"

EXT_UP_TO_DATE libchdr lib/libchdr.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "$SOURCE"
EXT_RESET_WORK libchdr

SRC_DIR="$EXT_SRC/$SOURCE"

printf 'Building libchdr %s for %s\n' "$VERSION" "$DEVICE"
cd "$EXT_WORK/libchdr" || exit 1

for FILE in "$SRC_DIR"/src/*.c "$SRC_DIR"/deps/lzma-25.01/src/LzmaDec.c; do
	OBJ=$(basename "$FILE" .c).o
	# shellcheck disable=SC2086
	"$EXT_CC" $EXT_CFLAGS -DCHDR_SYSTEM_ZLIB -DCHDR_SYSTEM_ZSTD \
		-DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 \
		-I"$SRC_DIR/include" -I"$SRC_DIR/deps/lzma-25.01/include" \
		-c "$FILE" -o "$OBJ"
done

"$EXT_AR" rcs libchdr.a ./*.o
"$EXT_RANLIB" libchdr.a

mkdir -p "$EXT_PREFIX/lib" "$EXT_PREFIX/include/libchdr"
cp libchdr.a "$EXT_PREFIX/lib/"
cp "$SRC_DIR"/include/libchdr/*.h "$EXT_PREFIX/include/libchdr/"

EXT_STAMP libchdr "$ID"
printf 'libchdr %s installed\n' "$VERSION"
