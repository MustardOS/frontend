#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "avformat.h"
#include "demux.h"
#include "internal.h"
#include "url.h"
#include "libsidplayfp_glue.h"

#define SIDPLAY_PACKET_FRAMES 1024
#define SIDPLAY_SEARCH_DEPTH  6
#define SIDPLAY_CHANNELS      2
#define SIDPLAY_FRAME_BYTES   (SIDPLAY_CHANNELS * 2)

typedef struct SidplayContext {
    const AVClass *class;
    SidplayGlue *glue;
    int64_t position;
    int64_t duration;

    int subsong;
    int sample_rate;
    int default_length;
    char *songlengths;
} SidplayContext;

#define OFFSET(x) offsetof(SidplayContext, x)
#define A         AV_OPT_FLAG_AUDIO_PARAM
#define D         AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
    {"subsong", "subsong to play, 0 for the default", OFFSET(subsong), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 256, A | D},
    {"sample_rate", "output sample rate", OFFSET(sample_rate), AV_OPT_TYPE_INT, {.i64 = 48000}, 8000, 192000, A | D},
    {"default_length", "seconds when unknown", OFFSET(default_length), AV_OPT_TYPE_INT, {.i64 = 180}, 1, 86400, A | D},
    {"songlengths", "HVSC song length database", OFFSET(songlengths), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, A | D},
    {NULL}
};

static const char *const songlength_names[] = {
    "Songlengths.md5",
    "DOCUMENTS/Songlengths.md5",
    "C64Music/DOCUMENTS/Songlengths.md5",
};

static int find_songlengths(const char *path, char *output, size_t size) {
    char directory[PATH_MAX];
    av_strlcpy(directory, path, sizeof(directory));
    for (int depth = 0; depth < SIDPLAY_SEARCH_DEPTH; depth++) {
        char *separator = strrchr(directory, '/');
        if (!separator) break;
        *separator = '\0';
        for (size_t index = 0; index < FF_ARRAY_ELEMS(songlength_names); index++) {
            if (snprintf(output, size, "%s/%s", directory, songlength_names[index]) >= (int) size) continue;
            if (access(output, R_OK) == 0) return 1;
        }
    }
    output[0] = '\0';
    return 0;
}

static void add_meta(AVFormatContext *s, const char *name, const char *value) {
    if (value && value[0]) av_dict_set(&s->metadata, name, value, 0);
}

static int read_close_sidplay(AVFormatContext *s) {
    SidplayContext *c = s->priv_data;
    sidplay_glue_close(c->glue);
    c->glue = NULL;
    return 0;
}

static int read_header_sidplay(AVFormatContext *s) {
    SidplayContext *c = s->priv_data;
    const char *path = s->url;
    av_strstart(path, "file:", &path);

    char database[PATH_MAX] = "";
    if (c->songlengths && c->songlengths[0])
        av_strlcpy(database, c->songlengths, sizeof(database));
    else
        find_songlengths(path, database, sizeof(database));

    char error[256] = "";
    c->glue = sidplay_glue_open(
        path, (unsigned int) c->subsong, (unsigned int) c->sample_rate, database, error, sizeof(error)
    );
    if (!c->glue) {
        av_log(s, AV_LOG_ERROR, "Unable to open SID tune: %s\n", error);
        return AVERROR_INVALIDDATA;
    }

    SidplayGlueInfo info;
    sidplay_glue_info(c->glue, &info);
    const int64_t length_ms = info.length_ms > 0 ? info.length_ms : (int64_t) c->default_length * 1000;
    c->duration = av_rescale(length_ms, c->sample_rate, 1000);

    AVStream *st = avformat_new_stream(s, NULL);
    if (!st) return AVERROR(ENOMEM);
    st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    st->codecpar->codec_id = AV_NE(AV_CODEC_ID_PCM_S16BE, AV_CODEC_ID_PCM_S16LE);
    st->codecpar->sample_rate = c->sample_rate;
    st->codecpar->bits_per_coded_sample = 16;
    st->codecpar->block_align = SIDPLAY_FRAME_BYTES;
    av_channel_layout_default(&st->codecpar->ch_layout, SIDPLAY_CHANNELS);
    avpriv_set_pts_info(st, 64, 1, c->sample_rate);
    st->start_time = 0;
    st->duration = c->duration;

    char text[256];
    add_meta(s, "title", info.title);
    add_meta(s, "artist", info.author);
    add_meta(s, "copyright", info.released);
    if (info.format) {
        snprintf(text, sizeof(text), "%s%s%s", info.format, info.model ? ", " : "", info.model ? info.model : "");
        add_meta(s, "format", text);
    }
    if (info.songs > 1) {
        snprintf(text, sizeof(text), "%u", info.songs);
        add_meta(s, "tracks", text);
        snprintf(text, sizeof(text), "%u/%u", info.song, info.songs);
        add_meta(s, "track", text);
    }
    return 0;
}

static int read_packet_sidplay(AVFormatContext *s, AVPacket *pkt) {
    SidplayContext *c = s->priv_data;
    if (c->position >= c->duration) return AVERROR_EOF;

    int frames = SIDPLAY_PACKET_FRAMES;
    if (c->duration - c->position < frames) frames = (int) (c->duration - c->position);
    int ret = av_new_packet(pkt, frames * SIDPLAY_FRAME_BYTES);
    if (ret < 0) return ret;

    const int rendered = sidplay_glue_render(c->glue, (int16_t *) pkt->data, frames);
    if (rendered < 0) return AVERROR_EXTERNAL;
    if (rendered == 0) return AVERROR_EOF;
    if (rendered < frames) av_shrink_packet(pkt, rendered * SIDPLAY_FRAME_BYTES);
    pkt->pts = c->position;
    pkt->duration = rendered;
    c->position += rendered;
    return 0;
}

static int sidplay_interrupted(void *opaque) {
    AVFormatContext *s = opaque;
    return ff_check_interrupt(&s->interrupt_callback);
}

static int read_seek_sidplay(AVFormatContext *s, int stream_index, int64_t timestamp, int flags) {
    SidplayContext *c = s->priv_data;
    if (timestamp < 0) timestamp = 0;
    if (timestamp > c->duration) timestamp = c->duration;
    if (timestamp < c->position) {
        if (sidplay_glue_restart(c->glue) < 0) return AVERROR_EXTERNAL;
        c->position = 0;
    }
    const int64_t skipped = sidplay_glue_skip(c->glue, timestamp - c->position, sidplay_interrupted, s);
    if (skipped < 0) return AVERROR_EXTERNAL;
    c->position += skipped;
    return c->position < timestamp && ff_check_interrupt(&s->interrupt_callback) ? AVERROR_EXIT : 0;
}

static const AVClass class_sidplay = {
    .class_name = "libsidplayfp demuxer",
    .item_name = av_default_item_name,
    .option = options,
    .version = LIBAVUTIL_VERSION_INT,
};

const FFInputFormat ff_libsidplayfp_demuxer = {
    .p.name = "libsidplayfp",
    .p.long_name = NULL_IF_CONFIG_SMALL("Commodore 64 SID music"),
    .p.priv_class = &class_sidplay,
    .p.flags = AVFMT_NOFILE | AVFMT_NO_BYTE_SEEK,
    .priv_data_size = sizeof(SidplayContext),
    .flags_internal = FF_INFMT_FLAG_INIT_CLEANUP,
    .read_header = read_header_sidplay,
    .read_packet = read_packet_sidplay,
    .read_close = read_close_sidplay,
    .read_seek = read_seek_sidplay,
};
