#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="0.8.0"
SHA256="c4ecdbc413d5752ebc5be59be0c6c2a9dd7e3dec4a0abbed22aa4a68d04d1627"
TARBALL="mojibake-$VERSION.tar.gz"
URL="https://github.com/zaerl/mojibake/archive/refs/tags/v$VERSION.tar.gz"

ID="$VERSION|$EXT_ARCH_FLAGS"
EXT_UP_TO_DATE mojibake lib/libmojibake.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "mojibake-$VERSION"
EXT_RESET_WORK mojibake

SRC_DIR="$EXT_SRC/mojibake-$VERSION/src"

printf 'Building mojibake %s for %s\n' "$VERSION" "$DEVICE"

cd "$EXT_WORK/mojibake" || exit 1

for FILE in "$SRC_DIR"/*.c; do
	OBJ=$(basename "$FILE" .c).o
	# shellcheck disable=SC2086
	"$EXT_CC" $EXT_CFLAGS -I"$SRC_DIR" -c "$FILE" -o "$OBJ"
done

"$EXT_AR" rcs libmojibake.a ./*.o
"$EXT_RANLIB" libmojibake.a

mkdir -p "$EXT_PREFIX/lib" "$EXT_PREFIX/include/mojibake"
cp libmojibake.a "$EXT_PREFIX/lib/"
cp "$SRC_DIR"/*.h "$EXT_PREFIX/include/mojibake/"

EXT_STAMP mojibake "$ID"

printf 'mojibake %s installed\n' "$VERSION"
