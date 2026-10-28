#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="1.9.1-muos1"
COMMIT="b174b03a3c2641a578b9299e540994a35b2f03a9"
SHA256="4a4ec439335b452a2b81752cc6531c1429031662e563801b965ab7bf38d5df88"
TARBALL="umtprd-$COMMIT.tar.gz"
URL="https://github.com/viveris/uMTP-Responder/archive/$COMMIT.tar.gz"
PATCH_FILE="$EXT_ROOT/patch/umtprd-muos.patch"
PATCH_SHA=$(sha256sum "$PATCH_FILE" | cut -d' ' -f1)
MUOS_SHA=$(sha256sum \
	"$EXT_ROOT/../module/mumtp.c" \
	"$EXT_ROOT/../common/storage/mount.c" \
	"$EXT_ROOT/../common/storage/view.c" | sha256sum | cut -d' ' -f1)

ID="$VERSION|$COMMIT|$PATCH_SHA|$MUOS_SHA|$EXT_ARCH_FLAGS"
EXT_UP_TO_DATE umtprd bin/mumtp "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "uMTP-Responder-$COMMIT"
EXT_RESET_WORK umtprd

SOURCE="$EXT_WORK/umtprd/source"
FRONTEND="$EXT_ROOT/.."
cp -a "$EXT_SRC/uMTP-Responder-$COMMIT" "$SOURCE"
patch -d "$SOURCE" -p1 <"$PATCH_FILE"

printf 'Building uMTP Responder %s for %s\n' "$VERSION" "$DEVICE"

cd "$EXT_WORK/umtprd" || exit 1

MTP_CFLAGS="$EXT_CFLAGS -O2 -pipe -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=3 -D_GNU_SOURCE -I$SOURCE/inc -I$FRONTEND"

for FILE in "$SOURCE"/src/*.c; do
	OBJ="upstream_$(basename "$FILE" .c).o"
	if [ "$(basename "$FILE")" = umtprd.c ]; then
		# shellcheck disable=SC2086
		"$EXT_CC" $MTP_CFLAGS -Dmain=umtprd_main -c "$FILE" -o "$OBJ"
	else
		# shellcheck disable=SC2086
		"$EXT_CC" $MTP_CFLAGS -c "$FILE" -o "$OBJ"
	fi
done

for FILE in "$SOURCE"/src/mtp_operations/*.c; do
	OBJ="operation_$(basename "$FILE" .c).o"
	# shellcheck disable=SC2086
	"$EXT_CC" $MTP_CFLAGS -c "$FILE" -o "$OBJ"
done

for FILE in "$FRONTEND/module/mumtp.c" "$FRONTEND/common/storage/mount.c" "$FRONTEND/common/storage/view.c"; do
	OBJ="muos_$(basename "$FILE" .c).o"
	# shellcheck disable=SC2086
	"$EXT_CC" $MTP_CFLAGS -c "$FILE" -o "$OBJ"
done

"$EXT_CC" -o mumtp ./*.o -pthread -lrt -Wl,--gc-sections -Wl,-z,relro,-z,now -pie -s

mkdir -p "$EXT_PREFIX/bin" "$EXT_PREFIX/share/licenses/umtprd"
cp mumtp "$EXT_PREFIX/bin/"
cp "$SOURCE/LICENSE" "$EXT_PREFIX/share/licenses/umtprd/"

EXT_STAMP umtprd "$ID"

printf 'uMTP Responder %s installed\n' "$VERSION"
