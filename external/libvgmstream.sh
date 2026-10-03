#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="r2117"
SHA256="b9ad0ffabb5919e1c9ec2912416e3ce37916417a8c410196822ce6c610dcde66"
TARBALL="vgmstream-$VERSION.tar.gz"
SOURCE="vgmstream-$VERSION"
URL="https://github.com/vgmstream/vgmstream/archive/refs/tags/$VERSION.tar.gz"
ID="$VERSION|static|internal-codecs|$EXT_ARCH_FLAGS"

EXT_UP_TO_DATE libvgmstream lib/libvgmstream.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "$SOURCE"
EXT_RESET_WORK libvgmstream

printf 'Configuring vgmstream %s for %s\n' "$VERSION" "$DEVICE"
cd "$EXT_WORK/libvgmstream" || exit 1

cmake "$EXT_SRC/$SOURCE" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER="$EXT_CC" \
	-DCMAKE_CXX_COMPILER="${CROSS_COMPILE-}g++" \
	-DCMAKE_AR="$EXT_AR" \
	-DCMAKE_RANLIB="$EXT_RANLIB" \
	-DCMAKE_C_FLAGS="$EXT_CFLAGS" \
	-DCMAKE_CXX_FLAGS="$EXT_CFLAGS" \
	-DUSE_MPEG=OFF \
	-DUSE_VORBIS=OFF \
	-DUSE_FFMPEG=OFF \
	-DUSE_G7221=ON \
	-DUSE_G719=OFF \
	-DUSE_ATRAC9=OFF \
	-DUSE_CELT=OFF \
	-DUSE_SPEEX=OFF \
	-DBUILD_CLI=OFF \
	-DBUILD_V123=OFF \
	-DBUILD_AUDACIOUS=OFF \
	-DBUILD_SHARED_LIBS=OFF

printf 'Building vgmstream %s for %s\n' "$VERSION" "$DEVICE"
cmake --build . --target libvgmstream --parallel "$EXT_JOBS"

mkdir -p "$EXT_PREFIX/lib" "$EXT_PREFIX/include/libvgmstream"
cp src/libvgmstream.a "$EXT_PREFIX/lib/libvgmstream.a"
cp "$EXT_SRC/$SOURCE/src/libvgmstream.h" "$EXT_SRC/$SOURCE/src/libvgmstream_streamfile.h" "$EXT_PREFIX/include/libvgmstream/"
cp "$EXT_SRC/$SOURCE/COPYING" "$EXT_PREFIX/include/libvgmstream/COPYING"

EXT_STAMP libvgmstream "$ID"
printf 'vgmstream %s installed\n' "$VERSION"
