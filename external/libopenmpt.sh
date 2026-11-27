#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="0.8.10"
SHA256="976d259288c40cd285771f77d09eff3d02514511d5677ecfcfb53be5013317e2"
TARBALL="libopenmpt-$VERSION+release.autotools.tar.gz"
SOURCE="libopenmpt-$VERSION+release.autotools"
URL="https://lib.openmpt.org/files/libopenmpt/src/$TARBALL"
ID="$VERSION|minimal|$EXT_ARCH_FLAGS"

EXT_UP_TO_DATE libopenmpt lib/libopenmpt.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "$SOURCE"
EXT_RESET_WORK libopenmpt

printf 'Configuring libopenmpt %s for %s\n' "$VERSION" "$DEVICE"
cd "$EXT_WORK/libopenmpt" || exit 1

"$EXT_SRC/$SOURCE/configure" \
	${EXT_HOST:+--host="$EXT_HOST"} \
	--prefix="$EXT_PREFIX" \
	--enable-static \
	--disable-shared \
	--disable-openmpt123 \
	--disable-examples \
	--disable-tests \
	--without-zlib \
	--without-mpg123 \
	--without-ogg \
	--without-vorbis \
	--without-vorbisfile \
	--without-flac \
	CC="$EXT_CC" \
	CXX="${CROSS_COMPILE-}g++" \
	AR="$EXT_AR" \
	RANLIB="$EXT_RANLIB" \
	CFLAGS="$EXT_CFLAGS" \
	CXXFLAGS="$EXT_CFLAGS"

printf 'Building libopenmpt %s for %s\n' "$VERSION" "$DEVICE"
make -j"$EXT_JOBS"
mkdir -p "$EXT_PREFIX/lib" "$EXT_PREFIX/lib/pkgconfig" "$EXT_PREFIX/include/libopenmpt"
cp .libs/libopenmpt.a "$EXT_PREFIX/lib/libopenmpt.a"
cp libopenmpt/libopenmpt.pc "$EXT_PREFIX/lib/pkgconfig/libopenmpt.pc"
cp "$EXT_SRC/$SOURCE"/libopenmpt/*.h "$EXT_PREFIX/include/libopenmpt/"

EXT_STAMP libopenmpt "$ID"
printf 'libopenmpt %s installed\n' "$VERSION"
