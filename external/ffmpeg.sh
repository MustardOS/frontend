#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="9.0.1"
SHA256="cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635"
TARBALL="ffmpeg-$VERSION.tar.xz"
URL="https://ffmpeg.org/releases/$TARBALL"

SMALL="${FFMPEG_SMALL:-1}"

COMPONENTS="
	--enable-protocol=file
	--enable-demuxer=mov
	--enable-decoder=h264,hevc,mpeg4,aac,mp3,pcm_s16le,pcm_s16be,pcm_u8
	--enable-parser=h264,hevc,mpeg4video,aac,mpegaudio
"

ASM_ARG=""

case "$EXT_ARCH" in
	x86_64 | i?86)
		if ! command -v nasm >/dev/null 2>&1 && ! command -v yasm >/dev/null 2>&1; then
			ASM_ARG="--disable-x86asm"
			printf 'Warning: nasm and yasm are both missing, building ffmpeg without x86 assembly\n' 1>&2
			printf '         Decoding will be noticeably slower - install nasm to avoid this\n' 1>&2
		fi
		;;
esac

ID="$VERSION|$SMALL|$EXT_ARCH_FLAGS|$ASM_ARG|$(printf '%s' "$COMPONENTS" | tr -s '[:space:]' ' ')"
EXT_UP_TO_DATE ffmpeg lib/libavcodec.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
EXT_EXTRACT "$TARBALL" "ffmpeg-$VERSION"
EXT_RESET_WORK ffmpeg

SMALL_ARG=""
[ "$SMALL" = "1" ] && SMALL_ARG="--enable-small"

CROSS=""
if [ "$DEVICE" != "NATIVE" ]; then
	CROSS="--enable-cross-compile --cross-prefix=$CROSS_COMPILE --target-os=linux --arch=$EXT_ARCH"
	[ -n "${SYSROOT-}" ] && CROSS="$CROSS --sysroot=$SYSROOT"
fi

printf 'Configuring ffmpeg %s for %s\n' "$VERSION" "$DEVICE"

cd "$EXT_WORK/ffmpeg" || exit 1

# shellcheck disable=SC2086
env -u CC -u CFLAGS -u CPPFLAGS -u CXXFLAGS -u LDFLAGS \
	"$EXT_SRC/ffmpeg-$VERSION/configure" \
	--prefix="$EXT_PREFIX" \
	--libdir="$EXT_PREFIX/lib" \
	--incdir="$EXT_PREFIX/include" \
	$CROSS \
	--enable-static \
	--disable-shared \
	--enable-pic \
	--enable-pthreads \
	--disable-autodetect \
	--disable-everything \
	--disable-programs \
	--disable-doc \
	--disable-network \
	--disable-avdevice \
	--disable-avfilter \
	--disable-debug \
	--disable-zlib \
	--disable-bzlib \
	--disable-lzma \
	--disable-iconv \
	$SMALL_ARG \
	$ASM_ARG \
	$COMPONENTS \
	--extra-cflags="$EXT_ARCH_FLAGS -ffunction-sections -fdata-sections"

printf 'Building ffmpeg %s for %s\n' "$VERSION" "$DEVICE"

make -j"$EXT_JOBS"
make install

EXT_STAMP ffmpeg "$ID"

printf 'ffmpeg %s installed\n' "$VERSION"
