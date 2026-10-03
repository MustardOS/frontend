#!/bin/sh

. "$(dirname -- "$0")/common.sh"

VERSION="9.0.1"
SHA256="cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635"
TARBALL="ffmpeg-$VERSION.tar.xz"
URL="https://ffmpeg.org/releases/$TARBALL"

BASE_PREFIX="$EXT_PREFIX"
EXT_WORK="$EXT_ROOT/work/$DEVICE/ffmpeg-video"
EXT_PREFIX="$BASE_PREFIX/video"
OVERLAY="$EXT_ROOT/ffmpeg-video"
SOURCE_ROOT="$EXT_ROOT/work/$DEVICE/ffmpeg-video-src"
SOURCE="$SOURCE_ROOT/ffmpeg-$VERSION"
mkdir -p "$EXT_WORK" "$EXT_PREFIX"

ASM_ARG=""
case "$EXT_ARCH" in
	x86_64 | i?86)
		if ! command -v nasm >/dev/null 2>&1 && ! command -v yasm >/dev/null 2>&1; then
			ASM_ARG="--disable-x86asm"
		fi
		;;
esac

PROTOCOLS="file,http,https,tcp,tls,udp,crypto,cache,async,concat,pipe,rtp"

VIDEO_DEMUXERS="mov,matroska,avi,mpegts,mpegps,mpegvideo,h263,hls,flv,gif,apng,mjpeg,bink,smacker,roq,flic,ipmovie,nut,webm_dash_manifest,rtsp,sdp,image2,concat"
SUBTITLE_DEMUXERS="srt,ass,webvtt"
AUDIO_DEMUXERS="ogg,wav,w64,aiff,au,caf,ircam,sox,voc,mp3,aac,ac3,eac3,dts,dtshd,mlp,truehd,loas,flac,asf,rm,oma,ape,wv,mpc,mpc8,tta,tak,shorten,qoa,amr,amrnb,amrwb,dsf,wsd,nistsphere,pvf,sdx,sln,qcp,g723_1,g729"
GAME_AUDIO_DEMUXERS="adp,ads,adx,aea,afc,aix,apc,ast,bfstm,brstm,fsb,hca,msf,nsp,redspark,rsd,sds,svag,svs,vag,vpk,xmd,xvag,xwma"
LIBRARY_DEMUXERS="libopenmpt,libgme,libsidplayfp,libvgmstream"

VIDEO_DECODERS="h264,hevc,mpeg1video,mpeg2video,mpeg4,h263,h263i,h263p,flv,vp6,vp6a,vp6f,vp8,vp9,av1,mjpeg,png,apng,bmp,gif,tiff,webp,rawvideo,theora,vc1,wmv3,prores,dvvideo,cinepak,indeo3,indeo4,indeo5,msmpeg4v1,msmpeg4v2,msmpeg4v3,msvideo1,qtrle,rpza,svq1,svq3,rv10,rv20,rv30,rv40,bink,smacker,roq,flic,interplay_video"
AUDIO_DECODERS="aac,aac_latm,mp1,mp2,mp3,ac3,eac3,dca,mlp,truehd,opus,vorbis,flac,alac,ape,wavpack,mpc7,mpc8,tta,tak,shorten,qoa,speex,amrnb,amrwb,wmapro,wmav1,wmav2,wmalossless,wmavoice,cook,sipr,ra_144,ra_288,atrac1,atrac3,atrac3al,atrac3p,atrac3pal,atrac9,binkaudio_dct,binkaudio_rdft,smackaud,roq_dpcm,interplay_acm,interplay_dpcm,evrc,g723_1,g729,hca,nellymoser,qcelp,qdm2,truespeech,xma1,xma2,dsd_lsbf,dsd_lsbf_planar,dsd_msbf,dsd_msbf_planar"
PCM_DECODERS="pcm_s8,pcm_s8_planar,pcm_s16le,pcm_s16le_planar,pcm_s16be,pcm_s16be_planar,pcm_s24le,pcm_s24be,pcm_s24daud,pcm_s32le,pcm_s32be,pcm_f32le,pcm_f32be,pcm_f64le,pcm_f64be,pcm_u8,pcm_u16le,pcm_u16be,pcm_u24le,pcm_u24be,pcm_u32le,pcm_u32be,pcm_alaw,pcm_mulaw"
ADPCM_DECODERS="adpcm_adx,adpcm_afc,adpcm_ct,adpcm_dtk,adpcm_g726,adpcm_ima_apc,adpcm_ima_rad,adpcm_ima_wav,adpcm_ima_qt,adpcm_ms,adpcm_sbpro_2,adpcm_sbpro_3,adpcm_sbpro_4,adpcm_psx,adpcm_thp,adpcm_thp_le,adpcm_xa,adpcm_xmd,adpcm_yamaha"
SUBTITLE_DECODERS="subrip,ass,ssa,movtext,webvtt,dvdsub,pgssub"
HARDWARE_DECODERS="h264_v4l2m2m,hevc_v4l2m2m,mpeg1_v4l2m2m,mpeg2_v4l2m2m,mpeg4_v4l2m2m,vc1_v4l2m2m,vp8_v4l2m2m,vp9_v4l2m2m"

PARSERS="aac,aac_latm,ac3,av1,dca,flac,h264,hevc,mlp,mpeg4video,mpegaudio,mpegvideo,opus,vc1,vorbis,vp8,vp9"
FILTERS="buffer,buffersink,bwdif,lutyuv,hue,format,colorchannelmixer"

COMPONENTS="
	--enable-protocol=$PROTOCOLS
	--enable-demuxer=$VIDEO_DEMUXERS,$SUBTITLE_DEMUXERS,$AUDIO_DEMUXERS,$GAME_AUDIO_DEMUXERS,$LIBRARY_DEMUXERS
	--enable-decoder=$VIDEO_DECODERS,$AUDIO_DECODERS,$PCM_DECODERS,$ADPCM_DECODERS,$SUBTITLE_DECODERS,$HARDWARE_DECODERS
	--enable-parser=$PARSERS
	--enable-filter=$FILTERS
"

OVERLAY_ID=$(cd "$OVERLAY" && find . -type f | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -d' ' -f1)
ID="$VERSION|video-14-openmpt-gme-sid-vgm-$VERSION|$OVERLAY_ID|$EXT_ARCH_FLAGS|$ASM_ARG|$(printf '%s' "$COMPONENTS" | tr -s '[:space:]' ' ')"
EXT_UP_TO_DATE ffmpeg-video lib/libavcodec.a "$ID" && exit 0

EXT_FETCH "$URL" "$TARBALL" "$SHA256"
rm -rf "$SOURCE_ROOT" "$EXT_WORK"
mkdir -p "$SOURCE_ROOT" "$EXT_WORK"
printf 'Preparing video ffmpeg %s source\n' "$VERSION"
tar xJf "$EXT_DIST/$TARBALL" -C "$SOURCE_ROOT"
patch -d "$SOURCE" -p1 --forward --quiet <"$OVERLAY/wasabi-demuxers.patch"
cp "$OVERLAY"/libavformat/* "$SOURCE/libavformat/"

CROSS=""
if [ "$DEVICE" != "NATIVE" ]; then
	CROSS="--enable-cross-compile --cross-prefix=$CROSS_COMPILE --target-os=linux --arch=$EXT_ARCH"
	[ -n "${SYSROOT-}" ] && CROSS="$CROSS --sysroot=$SYSROOT"
fi

printf 'Configuring video ffmpeg %s for %s\n' "$VERSION" "$DEVICE"
cd "$EXT_WORK" || exit 1

env -u CC -u CFLAGS -u CPPFLAGS -u CXXFLAGS -u LDFLAGS -u PKG_CONFIG_SYSROOT_DIR -u PKG_CONFIG_LIBDIR \
	PKG_CONFIG_PATH="$BASE_PREFIX/lib/pkgconfig" \
	"$SOURCE/configure" \
	--prefix="$EXT_PREFIX" \
	--pkg-config=pkg-config \
	--pkg-config-flags=--static \
	--libdir="$EXT_PREFIX/lib" \
	--incdir="$EXT_PREFIX/include" \
	$CROSS \
	--enable-static \
	--disable-shared \
	--enable-pic \
	--enable-pthreads \
	--disable-everything \
	--enable-network \
	--enable-openssl \
	--enable-libopenmpt \
	--enable-libgme \
	--enable-libsidplayfp \
	--enable-libvgmstream \
	--enable-gpl \
	--enable-version3 \
	--enable-zlib \
	--enable-v4l2-m2m \
	--disable-autodetect \
	--disable-programs \
	--disable-doc \
	--disable-avdevice \
	--disable-encoders \
	--disable-muxers \
	--disable-debug \
	--disable-bzlib \
	--disable-lzma \
	--disable-iconv \
	$ASM_ARG \
	$COMPONENTS \
	--extra-cflags="$EXT_ARCH_FLAGS -I$BASE_PREFIX/include -ffunction-sections -fdata-sections" \
	--extra-cxxflags="$EXT_ARCH_FLAGS -I$BASE_PREFIX/include -ffunction-sections -fdata-sections" \
	--extra-ldflags="-L$BASE_PREFIX/lib" \
	--extra-libs="-lssl -lcrypto -lstdc++ -lz -ldl -lpthread -lm"

printf 'Building video ffmpeg %s for %s\n' "$VERSION" "$DEVICE"
make -j"$EXT_JOBS"
make DESTDIR= install-libs install-headers

EXT_STAMP ffmpeg-video "$ID"
printf 'video ffmpeg %s installed\n' "$VERSION"
