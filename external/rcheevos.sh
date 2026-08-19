#!/bin/sh

# Static rcheevos for Pickles achievements minus the desktop and DLL integration

. "$(dirname -- "$0")/common.sh"

VERSION="12.4.0"
COMMIT="2ad0b8672f68a48148620164510b963039e49eb1"
SHA256="b8e3e834d15c327085154315c49dea85b081ad48592ba1740f323ac953e1bfec"
TARBALL="rcheevos-$COMMIT.tar.gz"
URL="https://github.com/RetroAchievements/rcheevos/archive/$COMMIT.tar.gz"

ID="$VERSION|$COMMIT|$EXT_ARCH_FLAGS"
EXT_UP_TO_DATE rcheevos lib/librcheevos.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "rcheevos-$COMMIT"
EXT_RESET_WORK rcheevos

SRC_DIR="$EXT_SRC/rcheevos-$COMMIT"
LIBRETRO_INC="$EXT_ROOT/../retro/core"

printf 'Building rcheevos %s for %s\n' "$VERSION" "$DEVICE"

cd "$EXT_WORK/rcheevos" || exit 1

for FILE in "$SRC_DIR"/src/*.c "$SRC_DIR"/src/rcheevos/*.c "$SRC_DIR"/src/rapi/*.c "$SRC_DIR"/src/rhash/*.c; do
	case "$FILE" in
		*/rc_client_external.c | */rc_client_raintegration.c) continue ;;
	esac

	OBJ=$(basename "$(dirname "$FILE")")_$(basename "$FILE" .c).o

	# shellcheck disable=SC2086
	"$EXT_CC" $EXT_CFLAGS -DRC_CLIENT_SUPPORTS_HASH \
		-I"$SRC_DIR/include" -I"$SRC_DIR/src" -I"$SRC_DIR/src/rcheevos" -I"$LIBRETRO_INC" \
		-c "$FILE" -o "$OBJ"
done

"$EXT_AR" rcs librcheevos.a ./*.o
"$EXT_RANLIB" librcheevos.a

mkdir -p "$EXT_PREFIX/lib" "$EXT_PREFIX/include"
cp librcheevos.a "$EXT_PREFIX/lib/"
cp "$SRC_DIR"/include/*.h "$EXT_PREFIX/include/"

cp "$SRC_DIR/src/rc_libretro.h" "$EXT_PREFIX/include/"

EXT_STAMP rcheevos "$ID"

printf 'rcheevos %s installed\n' "$VERSION"
