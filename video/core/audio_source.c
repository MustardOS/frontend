#include "audio_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define AUDIO_SOURCE_SUBSONG_LIMIT 4096

static int header_matches(const char *path, const char *magic, const size_t length) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    char header[16];
    const size_t count = length <= sizeof(header) ? fread(header, 1, length, file) : 0;
    fclose(file);
    return count == length && memcmp(header, magic, length) == 0;
}

static int extension_is(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    return dot && !strchr(dot, '/') && strcasecmp(dot, extension) == 0;
}

void audio_source_resolve(const char *uri, wasabi_audio_source *source) {
    memset(source, 0, sizeof(*source));
    snprintf(source->path, sizeof(source->path), "%s", uri ? uri : "");
    source->backend = content_audio_stream;
    if (!uri || strstr(uri, "://")) return;

    char *fragment = strrchr(source->path, '#');
    if (fragment && fragment[1] && strspn(fragment + 1, "0123456789") == strlen(fragment + 1)) {
        *fragment = '\0';
        if (access(source->path, F_OK) == 0)
            source->subsong = atoi(fragment + 1);
        else
            *fragment = '#';
    }

    const content_audio_backend backend = content_path_audio_backend(source->path);
    if (backend != content_audio_none) source->backend = backend;
    if (source->backend == content_audio_stream && extension_is(source->path, ".awb")
        && header_matches(source->path, "AFS2", 4))
        source->backend = content_audio_game;
}

int audio_source_open(AVFormatContext **format, const char *uri, const int sample_rate, AVDictionary **options) {
    wasabi_audio_source source;
    audio_source_resolve(uri, &source);

    const char *name = NULL;
    char value[32];
    switch (source.backend) {
        case content_audio_chiptune:
            name = "libgme";
            if (source.subsong > 0) {
                snprintf(value, sizeof(value), "%d", source.subsong - 1);
                av_dict_set(options, "track_index", value, 0);
            }
            break;
        case content_audio_sid:
            name = "libsidplayfp";
            if (source.subsong > 0) {
                snprintf(value, sizeof(value), "%d", source.subsong);
                av_dict_set(options, "subsong", value, 0);
            }
            break;
        case content_audio_game:
            name = "libvgmstream";
            if (source.subsong > 0) {
                snprintf(value, sizeof(value), "%d", source.subsong);
                av_dict_set(options, "subsong", value, 0);
            }
            break;
        default:
            break;
    }
    if ((source.backend == content_audio_chiptune || source.backend == content_audio_sid) && sample_rate > 0) {
        snprintf(value, sizeof(value), "%d", sample_rate);
        av_dict_set(options, "sample_rate", value, 0);
    }

    const AVInputFormat *input = name ? av_find_input_format(name) : NULL;
    if (name && !input) return AVERROR_DEMUXER_NOT_FOUND;
    return avformat_open_input(format, source.path, input, options);
}

int audio_source_subsongs(const AVFormatContext *format, const wasabi_audio_source *source, int *current) {
    if (!format || !source
        || (source->backend != content_audio_chiptune && source->backend != content_audio_sid
            && source->backend != content_audio_game))
        return 0;

    const AVDictionaryEntry *tracks = av_dict_get(format->metadata, "tracks", NULL, AV_DICT_MATCH_CASE);
    int count = tracks && tracks->value ? atoi(tracks->value) : 0;
    if (count < 2) return 0;
    if (count > AUDIO_SOURCE_SUBSONG_LIMIT) count = AUDIO_SOURCE_SUBSONG_LIMIT;

    int playing = source->subsong;
    if (playing <= 0) {
        const AVDictionaryEntry *track = av_dict_get(format->metadata, "track", NULL, AV_DICT_MATCH_CASE);
        playing = track && track->value ? atoi(track->value) : 1;
    }
    if (playing < 1 || playing > count) playing = 1;
    if (current) *current = playing;
    return count;
}

int audio_source_resumable(const char *uri) {
    wasabi_audio_source source;
    audio_source_resolve(uri, &source);
    return source.backend != content_audio_sid;
}
