#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="4.2.2+git20130218-muos1"
UPSTREAM_VERSION="4.2.2+git20130218"
SOURCE_TARBALL="android-tools_${UPSTREAM_VERSION}.orig.tar.xz"
SOURCE_URL="https://launchpad.net/ubuntu/+archive/primary/+files/$SOURCE_TARBALL"
SOURCE_SHA256="9bfba987e1351b12aa983787b9ae4424ab752e9e646d8e93771538dc1e5d932f"

DEBIAN_VERSION="3ubuntu41"
DEBIAN_TARBALL="android-tools_${UPSTREAM_VERSION}-${DEBIAN_VERSION}.debian.tar.gz"
DEBIAN_URL="https://launchpad.net/ubuntu/+archive/primary/+files/$DEBIAN_TARBALL"
DEBIAN_SHA256="73c3078de3e44d8a3cadf7a360863c63155d9d558c2f0933cf38ad901a3f5998"

BUILDROOT_COMMIT="e687e3815f76c1c5ea9fb52b6558bedfe53ab117"
BUILDROOT_TARBALL="buildroot-$BUILDROOT_COMMIT.tar.gz"
BUILDROOT_URL="https://github.com/buildroot/buildroot/archive/$BUILDROOT_COMMIT.tar.gz"
BUILDROOT_SHA256="50b1234a41acf2bb60504ad61ada7c953d6c44ca4f806640bad12db1c0a31204"

PATCH_FILE="$EXT_ROOT/patch/muadb-muos.patch"
PATCH_SHA=$(sha256sum "$PATCH_FILE" | cut -d' ' -f1)
ID="$VERSION|ubuntu-$DEBIAN_VERSION|buildroot-$BUILDROOT_COMMIT|$PATCH_SHA|$EXT_ARCH_FLAGS"

EXT_UP_TO_DATE muadb bin/muadb "$ID" && exit 0

EXT_FETCH "$SOURCE_URL" "$SOURCE_TARBALL" "$SOURCE_SHA256"
EXT_FETCH "$DEBIAN_URL" "$DEBIAN_TARBALL" "$DEBIAN_SHA256"
EXT_FETCH "$BUILDROOT_URL" "$BUILDROOT_TARBALL" "$BUILDROOT_SHA256"

EXT_EXTRACT "$SOURCE_TARBALL" android-tools
EXT_EXTRACT "$BUILDROOT_TARBALL" "buildroot-$BUILDROOT_COMMIT"

if [ ! -d "$EXT_SRC/android-tools/debian" ]; then
	tar xzf "$EXT_DIST/$DEBIAN_TARBALL" -C "$EXT_SRC/android-tools"
fi

EXT_RESET_WORK muadb

SOURCE="$EXT_WORK/muadb/source"
BUILD="$EXT_WORK/muadb/build"
cp -a "$EXT_SRC/android-tools" "$SOURCE"
mkdir -p "$BUILD"

while IFS= read -r PATCH_NAME; do
	case "$PATCH_NAME" in
		'' | \#*) continue ;;
	esac
	patch -s -d "$SOURCE" -p1 <"$SOURCE/debian/patches/$PATCH_NAME"
done <"$SOURCE/debian/patches/series"

for BUILDROOT_PATCH in "$EXT_SRC/buildroot-$BUILDROOT_COMMIT"/package/android-tools/*.patch; do
	patch -s -d "$SOURCE" -p1 <"$BUILDROOT_PATCH"
done

patch -s -d "$SOURCE" -p1 <"$PATCH_FILE"

printf 'Building muadb %s for %s\n' "$VERSION" "$DEVICE"

MUADB_CFLAGS="$EXT_CFLAGS -O2 -pipe -fPIE -fno-plt -fstack-protector-strong -D_FORTIFY_SOURCE=3 -DMUADB_VERSION=\\\"$VERSION\\\""
MUADB_LDFLAGS="-pie -Wl,--gc-sections -Wl,--as-needed -Wl,-z,relro,-z,now -Wl,-s"

make -C "$BUILD" -f "$SOURCE/debian/makefiles/adbd.mk" -j"$EXT_JOBS" \
	SRCDIR="$SOURCE" \
	CC="$EXT_CC" \
	CFLAGS="$MUADB_CFLAGS" \
	LDFLAGS="$MUADB_LDFLAGS" \
	LIBS="-lc -lpthread -lz -lcrypto -lcrypt" \
	adbd

mkdir -p "$EXT_PREFIX/bin" "$EXT_PREFIX/share/licenses/muadb"
cp "$BUILD/adbd" "$EXT_PREFIX/bin/muadb"
cp "$SOURCE/debian/copyright" "$EXT_PREFIX/share/licenses/muadb/"

EXT_STAMP muadb "$ID"

printf 'muadb %s installed\n' "$VERSION"
