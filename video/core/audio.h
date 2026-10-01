#pragma once

#include <stddef.h>
#include <libavformat/avformat.h>

typedef struct {
    char title[256];
    char artist[256];
    char album[256];
    char album_artist[256];
    char composer[256];
    char genre[128];
    char year[32];
    char track[32];
    char disc[32];
    char comment[512];
    char copyright[256];
    char encoder[128];
    char format[128];
    char artwork[4096];
    int bitrate;
    int artwork_temporary;
} wasabi_audio_info;

void wasabi_audio_metadata(AVFormatContext *format, const char *uri, const char *fallback_title,
                           wasabi_audio_info *information);
void wasabi_audio_cleanup(wasabi_audio_info *information);
