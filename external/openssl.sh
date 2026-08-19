#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="3.5.7"
SHA256="a8c0d28a529ca480f9f36cf5792e2cd21984552a3c8e4aa11a24aa31aeac98e8"
TARBALL="openssl-$VERSION.tar.gz"
URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION/$TARBALL"

if ! command -v perl >/dev/null 2>&1; then
	printf 'Error: OpenSSL is built by perl, which was not found on the build host\n' 1>&2
	exit 1
fi

# Time::Piece is core perl but often a separate package so fall back to our shim
if ! perl -MTime::Piece -e '' >/dev/null 2>&1; then
	PERL5LIB="$EXT_ROOT/perl${PERL5LIB:+:$PERL5LIB}"
	export PERL5LIB

	if ! perl -MTime::Piece -e '' >/dev/null 2>&1; then
		printf 'Error: neither the real Time::Piece nor the local shim would load\n' 1>&2
		exit 1
	fi

	printf 'Using the local Time::Piece shim - install perl-Time-Piece for the real one\n'
fi

case "$EXT_ARCH" in
	aarch64) TARGET="linux-aarch64" ;;
	arm*) TARGET="linux-armv4" ;;
	x86_64) TARGET="linux-x86_64" ;;
	i?86) TARGET="linux-x86" ;;
	riscv64) TARGET="linux64-riscv64" ;;
	riscv32) TARGET="linux32-riscv32" ;;
	powerpc64le | ppc64le) TARGET="linux-ppc64le" ;;
	loongarch64) TARGET="linux64-loongarch64" ;;
	mips64*) TARGET="linux64-mips64" ;;
	*)
		if [ "$EXT_PTR_SIZE" = "4" ]; then
			TARGET="linux-generic32"
		else
			TARGET="linux-generic64"
		fi

		printf 'Warning: no tuned OpenSSL target for %s, falling back to %s\n' "$EXT_ARCH" "$TARGET" 1>&2
		;;
esac

ID="$VERSION|$TARGET|$EXT_ARCH_FLAGS"
EXT_UP_TO_DATE openssl lib/libcrypto.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "openssl-$VERSION"
EXT_RESET_WORK openssl

CROSS_ARG=""
[ "$DEVICE" != "NATIVE" ] && CROSS_ARG="--cross-compile-prefix=$CROSS_COMPILE"

printf 'Configuring OpenSSL %s for %s\n' "$VERSION" "$DEVICE"

cd "$EXT_WORK/openssl" || exit 1

# shellcheck disable=SC2086
env -u CC -u CFLAGS -u CPPFLAGS -u CXXFLAGS -u LDFLAGS \
	"$EXT_SRC/openssl-$VERSION/Configure" \
	"$TARGET" \
	$CROSS_ARG \
	--prefix="$EXT_PREFIX" \
	--libdir=lib \
	--openssldir=/etc/ssl \
	no-shared \
	no-tests \
	no-apps \
	no-docs \
	no-comp \
	no-zlib \
	no-ssl3 \
	no-weak-ssl-ciphers \
	$EXT_CFLAGS

printf 'Building OpenSSL %s for %s\n' "$VERSION" "$DEVICE"

make -j"$EXT_JOBS"
make install_sw

EXT_STAMP openssl "$ID"

printf 'OpenSSL %s installed\n' "$VERSION"
