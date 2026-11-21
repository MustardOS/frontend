#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="1.28.6"
SHA256="dc85000640a45d90ca9b77b061f6ca67cce0c4d4fbecf8819647baf573eef101"
TARBALL="mupdf-$VERSION-source.tar.gz"
SOURCE="mupdf-$VERSION-source"
URL="https://mupdf.com/downloads/archive/$TARBALL"

SYS_INC="${SYSROOT:+$SYSROOT}/usr/include"

MUPDF_DEFINES="-DTOFU_EMOJI -DTOFU_HISTORIC -DTOFU_SIL -DTOFU_SYMBOL \
-DFZ_ENABLE_OFFICE=0 -DFZ_ENABLE_FB2=0 -DFZ_ENABLE_MD=0 -DFZ_ENABLE_MOBI=0 \
-DFZ_ENABLE_OCR_OUTPUT=0 -DFZ_ENABLE_ODT_OUTPUT=0"

ID="$VERSION|static|system-ft-hb-jpeg-z|$EXT_ARCH_FLAGS|$MUPDF_DEFINES"

EXT_UP_TO_DATE mupdf lib/libmupdf.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "$SOURCE"
EXT_RESET_WORK mupdf

printf 'Building MuPDF %s for %s\n' "$VERSION" "$DEVICE"

make -C "$EXT_SRC/$SOURCE" -j"$EXT_JOBS" libs \
	OUT="$EXT_WORK/mupdf" \
	build=release \
	OS=Linux \
	CC="$EXT_CC" \
	CXX="${CROSS_COMPILE-}g++" \
	AR="$EXT_AR" \
	LD="${CROSS_COMPILE-}ld" \
	shared=no \
	HAVE_X11=no HAVE_GLUT=no HAVE_WAYLAND=no HAVE_LIBCRYPTO=no HAVE_CURL=no \
	mujs=no xps=no svg=no extract=no brotli=no barcode=no tofu_cjk=yes \
	USE_SYSTEM_FREETYPE=yes USE_SYSTEM_HARFBUZZ=yes USE_SYSTEM_LIBJPEG=yes USE_SYSTEM_ZLIB=yes \
	SYS_FREETYPE_CFLAGS="-I$SYS_INC/freetype2" SYS_FREETYPE_LIBS="-lfreetype" \
	SYS_HARFBUZZ_CFLAGS="-I$SYS_INC/harfbuzz" SYS_HARFBUZZ_LIBS="-lharfbuzz" \
	SYS_LIBJPEG_CFLAGS="" SYS_LIBJPEG_LIBS="-ljpeg" \
	SYS_ZLIB_CFLAGS="" SYS_ZLIB_LIBS="-lz" \
	XCFLAGS="$EXT_CFLAGS $MUPDF_DEFINES"

mkdir -p "$EXT_PREFIX/include" "$EXT_PREFIX/lib"
rm -rf "$EXT_PREFIX/include/mupdf"
cp -R "$EXT_SRC/$SOURCE/include/mupdf" "$EXT_PREFIX/include/"
cp "$EXT_WORK/mupdf/libmupdf.a" "$EXT_WORK/mupdf/libmupdf-third.a" "$EXT_PREFIX/lib/"

EXT_STAMP mupdf "$ID"
printf 'MuPDF %s installed\n' "$VERSION"
