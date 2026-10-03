#include <stdio.h>
#include <libvgmstream/libvgmstream.h>
#include <libvgmstream/libvgmstream_streamfile.h>
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "avformat.h"
#include "demux.h"
#include "internal.h"

#define VGMSTREAM_MAX_CHANNELS 8

typedef struct VGMStreamContext {
    const AVClass *class;
    libvgmstream_t *lib;
    int64_t position;
    int bytes_per_frame;

    int subsong;
    double loop_count;
    double fade_time;
    int play_forever;
} VGMStreamContext;

#define OFFSET(x) offsetof(VGMStreamContext, x)
#define A         AV_OPT_FLAG_AUDIO_PARAM
#define D         AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
    {"subsong", "subsong to play, 0 for the default", OFFSET(subsong), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, A | D},
    {"loop_count", "times looped audio repeats", OFFSET(loop_count), AV_OPT_TYPE_DOUBLE, {.dbl = 2.0}, 0, 100, A | D},
    {"fade_time", "fade after the last loop", OFFSET(fade_time), AV_OPT_TYPE_DOUBLE, {.dbl = 10.0}, 0, 60, A | D},
    {"play_forever", "keep repeating looped audio", OFFSET(play_forever), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, A | D},
    {NULL}
};

static void vgmstream_log(int level, const char *text) {
    av_log(NULL, level >= LIBVGMSTREAM_LOG_LEVEL_INFO ? AV_LOG_VERBOSE : AV_LOG_DEBUG, "%s", text);
}

static void add_meta(AVFormatContext *s, const char *name, const char *value) {
    if (value && value[0]) av_dict_set(&s->metadata, name, value, 0);
}

static int read_close_vgmstream(AVFormatContext *s) {
    VGMStreamContext *c = s->priv_data;
    libvgmstream_free(c->lib);
    c->lib = NULL;
    return 0;
}

static int read_header_vgmstream(AVFormatContext *s) {
    VGMStreamContext *c = s->priv_data;
    const char *path = s->url;
    av_strstart(path, "file:", &path);

    libvgmstream_set_log(LIBVGMSTREAM_LOG_LEVEL_INFO, vgmstream_log);
    libstreamfile_t *file = libstreamfile_open_from_stdio(path);
    if (!file) return AVERROR(ENOENT);

    libvgmstream_config_t config = {
        .allow_play_forever = c->play_forever,
        .play_forever = c->play_forever,
        .loop_count = c->loop_count,
        .fade_time = c->fade_time,
        .auto_downmix_channels = VGMSTREAM_MAX_CHANNELS,
        .force_sfmt = LIBVGMSTREAM_SFMT_PCM16,
    };
    c->lib = libvgmstream_create(file, c->subsong, &config);
    libstreamfile_close(file);
    if (!c->lib) return AVERROR_INVALIDDATA;

    const libvgmstream_format_t *format = c->lib->format;
    if (format->channels <= 0 || format->channels > VGMSTREAM_MAX_CHANNELS || format->sample_rate <= 0
        || format->sample_size != 2)
        return AVERROR_INVALIDDATA;
    c->bytes_per_frame = format->channels * format->sample_size;

    AVStream *st = avformat_new_stream(s, NULL);
    if (!st) return AVERROR(ENOMEM);
    st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    st->codecpar->codec_id = AV_NE(AV_CODEC_ID_PCM_S16BE, AV_CODEC_ID_PCM_S16LE);
    st->codecpar->sample_rate = format->sample_rate;
    st->codecpar->bits_per_coded_sample = 16;
    st->codecpar->block_align = c->bytes_per_frame;
    if (format->channel_layout && av_popcount(format->channel_layout) == format->channels)
        av_channel_layout_from_mask(&st->codecpar->ch_layout, format->channel_layout);
    else
        av_channel_layout_default(&st->codecpar->ch_layout, format->channels);
    avpriv_set_pts_info(st, 64, 1, format->sample_rate);
    st->start_time = 0;
    if (!format->play_forever && format->play_samples > 0) st->duration = format->play_samples;

    char text[64];
    add_meta(s, "title", format->stream_name);
    add_meta(s, "format", format->meta_name);
    add_meta(s, "encoder", format->codec_name);
    if (format->subsong_count > 1) {
        snprintf(text, sizeof(text), "%d", format->subsong_count);
        add_meta(s, "tracks", text);
        snprintf(
            text, sizeof(text), "%d/%d", format->subsong_index > 0 ? format->subsong_index : 1, format->subsong_count
        );
        add_meta(s, "track", text);
    }
    if (format->loop_flag) {
        snprintf(text, sizeof(text), "%" PRId64, format->loop_start);
        add_meta(s, "loop_start", text);
        snprintf(text, sizeof(text), "%" PRId64, format->loop_end);
        add_meta(s, "loop_end", text);
    }
    return 0;
}

static int read_packet_vgmstream(AVFormatContext *s, AVPacket *pkt) {
    VGMStreamContext *c = s->priv_data;
    libvgmstream_decoder_t *decoder = c->lib->decoder;

    while (!decoder->done) {
        if (libvgmstream_render(c->lib) < 0) return decoder->done ? AVERROR_EOF : AVERROR_EXTERNAL;
        if (decoder->buf_samples <= 0) continue;

        const int bytes = decoder->buf_samples * c->bytes_per_frame;
        int ret = av_new_packet(pkt, bytes);
        if (ret < 0) return ret;
        memcpy(pkt->data, decoder->buf, bytes);
        pkt->pts = c->position;
        pkt->duration = decoder->buf_samples;
        c->position += decoder->buf_samples;
        return 0;
    }
    return AVERROR_EOF;
}

static int read_seek_vgmstream(AVFormatContext *s, int stream_index, int64_t timestamp, int flags) {
    VGMStreamContext *c = s->priv_data;
    if (timestamp < 0) timestamp = 0;
    libvgmstream_seek(c->lib, timestamp);
    const int64_t position = libvgmstream_get_play_position(c->lib);
    c->position = position >= 0 ? position : timestamp;
    return 0;
}

static const AVClass class_vgmstream = {
    .class_name = "vgmstream demuxer",
    .item_name = av_default_item_name,
    .option = options,
    .version = LIBAVUTIL_VERSION_INT,
};

const FFInputFormat ff_libvgmstream_demuxer = {
    .p.name = "libvgmstream",
    .p.long_name = NULL_IF_CONFIG_SMALL("vgmstream video game audio"),
    .p.priv_class = &class_vgmstream,
    .p.flags = AVFMT_NOFILE | AVFMT_NO_BYTE_SEEK,
    .priv_data_size = sizeof(VGMStreamContext),
    .flags_internal = FF_INFMT_FLAG_INIT_CLEANUP,
    .read_header = read_header_vgmstream,
    .read_packet = read_packet_vgmstream,
    .read_close = read_close_vgmstream,
    .read_seek = read_seek_vgmstream,
};
