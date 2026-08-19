#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="3.8.9"
SHA256="888c934f9d95648ecb9163dc8e23ab80a476ecb81a8f1154704a227b5b676dde"
TARBALL="libarchive-$VERSION.tar.xz"
URL="https://github.com/libarchive/libarchive/releases/download/v$VERSION/$TARBALL"

CODEC_ARGS=""
CODEC_LIBS=""

for CODEC in zlib:z:zlib.h bz2lib:bz2:bzlib.h lzma:lzma:lzma.h zstd:zstd:zstd.h; do
	OPT=${CODEC%%:*}
	REST=${CODEC#*:}
	LIB=${REST%%:*}
	HEADER=${REST##*:}

	# shellcheck disable=SC2086
	if printf '#include <%s>\nint main(void){return 0;}\n' "$HEADER" |
		"$EXT_CC" $EXT_SYSROOT_CFLAGS -x c - -l"$LIB" -o /dev/null >/dev/null 2>&1; then
		CODEC_ARGS="$CODEC_ARGS --with-$OPT"
		CODEC_LIBS="$CODEC_LIBS -l$LIB"
	else
		CODEC_ARGS="$CODEC_ARGS --without-$OPT"
	fi
done

printf 'libarchive codecs:%s\n' "${CODEC_LIBS:- none}"

ID="$VERSION|$EXT_ARCH_FLAGS|$CODEC_ARGS"
EXT_UP_TO_DATE libarchive lib/libarchive.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "libarchive-$VERSION"
EXT_RESET_WORK libarchive

HOST_ARG=""
[ -n "$EXT_HOST" ] && HOST_ARG="--host=$EXT_HOST"

printf 'Configuring libarchive %s for %s\n' "$VERSION" "$DEVICE"

cd "$EXT_WORK/libarchive" || exit 1

# shellcheck disable=SC2086
env -u CPPFLAGS -u CXXFLAGS -u LDFLAGS \
	CC="$EXT_CC" AR="$EXT_AR" RANLIB="$EXT_RANLIB" CFLAGS="$EXT_CFLAGS" \
	"$EXT_SRC/libarchive-$VERSION/configure" \
	--prefix="$EXT_PREFIX" \
	--libdir="$EXT_PREFIX/lib" \
	--includedir="$EXT_PREFIX/include/libarchive" \
	$HOST_ARG \
	--enable-static \
	--disable-shared \
	--disable-bsdtar \
	--disable-bsdcpio \
	--disable-bsdcat \
	--disable-bsdunzip \
	--disable-acl \
	--disable-xattr \
	--disable-rpath \
	--without-xml2 \
	--without-expat \
	--without-openssl \
	--without-nettle \
	--without-mbedtls \
	--without-libb2 \
	--without-iconv \
	--without-lz4 \
	--without-lzo2 \
	--without-cng \
	$CODEC_ARGS

printf 'Building libarchive %s for %s\n' "$VERSION" "$DEVICE"

make -j"$EXT_JOBS"
make install

printf 'LIBARCHIVE_SYSLIBS :=%s\n' "$CODEC_LIBS" >"$EXT_PREFIX/libarchive.mk"

EXT_STAMP libarchive "$ID"

printf 'libarchive %s installed\n' "$VERSION"
