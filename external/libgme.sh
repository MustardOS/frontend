#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="0.6.5"
SHA256="a133f19278222136ba0d8c27b64a07987ba05fec9d2e6d293ccd8cabdd97ddbb"
TARBALL="libgme-$VERSION-src.tar.gz"
SOURCE="libgme-$VERSION"
URL="https://github.com/libgme/game-music-emu/releases/download/$VERSION/$TARBALL"
ID="$VERSION|static|$EXT_ARCH_FLAGS"

EXT_UP_TO_DATE libgme lib/libgme.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "$SOURCE"
EXT_RESET_WORK libgme

printf 'Configuring libgme %s for %s\n' "$VERSION" "$DEVICE"
cd "$EXT_WORK/libgme" || exit 1

cmake "$EXT_SRC/$SOURCE" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$EXT_PREFIX" \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_C_COMPILER="$EXT_CC" \
	-DCMAKE_CXX_COMPILER="${CROSS_COMPILE-}g++" \
	-DCMAKE_AR="$EXT_AR" \
	-DCMAKE_RANLIB="$EXT_RANLIB" \
	-DCMAKE_C_FLAGS="$EXT_CFLAGS" \
	-DCMAKE_CXX_FLAGS="$EXT_CFLAGS" \
	-DBUILD_TESTING=OFF \
	-DGME_BUILD_SHARED=OFF \
	-DGME_BUILD_STATIC=ON \
	-DGME_BUILD_TESTING=OFF \
	-DGME_BUILD_EXAMPLES=OFF \
	-DGME_ZLIB=ON

printf 'Building libgme %s for %s\n' "$VERSION" "$DEVICE"
cmake --build . --parallel "$EXT_JOBS"
env DESTDIR= cmake --install .

EXT_STAMP libgme "$ID"
printf 'libgme %s installed\n' "$VERSION"
