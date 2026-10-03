#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="3.1.1"
SHA256="12b79190593bf480b2d11481b5c2de62bac07f344437a66cd8d887329875c626"
TARBALL="libsidplayfp-$VERSION.tar.gz"
SOURCE="libsidplayfp-$VERSION"
URL="https://github.com/libsidplayfp/libsidplayfp/releases/download/v$VERSION/$TARBALL"
ID="$VERSION|static|sidlite|$EXT_ARCH_FLAGS"

EXT_UP_TO_DATE libsidplayfp lib/libsidplayfp.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "$SOURCE"
EXT_RESET_WORK libsidplayfp

unset PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_PATH
PKG_CONFIG_LIBDIR="$EXT_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR

printf 'Configuring libsidplayfp %s for %s\n' "$VERSION" "$DEVICE"
cd "$EXT_WORK/libsidplayfp" || exit 1

env -u CPPFLAGS -u LDFLAGS -u LIBS \
	CC="$EXT_CC" CXX="${CROSS_COMPILE-}g++" AR="$EXT_AR" RANLIB="$EXT_RANLIB" \
	CFLAGS="$EXT_CFLAGS -O2" CXXFLAGS="$EXT_CFLAGS -O2" \
	"$EXT_SRC/$SOURCE/configure" \
	${EXT_HOST:+--host="$EXT_HOST"} \
	--prefix="$EXT_PREFIX" \
	--libdir="$EXT_PREFIX/lib" \
	--enable-static \
	--disable-shared \
	--with-usbsid=no \
	--with-exsid=no

printf 'Building libsidplayfp %s for %s\n' "$VERSION" "$DEVICE"
make -j"$EXT_JOBS" src/libsidplayfp.la
make install-libLTLIBRARIES install-src_libsidplayfp_laHEADERS install-nodist_src_libsidplayfp_laHEADERS \
	install-src_builders_sidlite_builder_libsidplayfp_sidlite_laHEADERS
rm -f "$EXT_PREFIX/lib/libsidplayfp.la" "$EXT_PREFIX/lib/libstilview.la" "$EXT_PREFIX/lib/libstilview.a"

EXT_STAMP libsidplayfp "$ID"
printf 'libsidplayfp %s installed\n' "$VERSION"
