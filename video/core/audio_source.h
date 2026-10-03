#pragma once

#include <limits.h>
#include <libavformat/avformat.h>
#include <common/content/content.h>

typedef struct {
    char path[PATH_MAX];
    int subsong;
    content_audio_backend backend;
} wasabi_audio_source;

void audio_source_resolve(const char *uri, wasabi_audio_source *source);
int audio_source_open(AVFormatContext **format, const char *uri, int sample_rate, AVDictionary **options);
int audio_source_subsongs(const AVFormatContext *format, const wasabi_audio_source *source, int *current);
int audio_source_resumable(const char *uri);
