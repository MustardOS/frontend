#include "audio.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <common/display/screenshot.h>
#include <common/content/catalogue.h>
#include <common/content/core/common.h>
#include <common/base/strutil.h>
#include <common/display/theme.h>
#include <common/runtime/init.h>
#include <common/storage/fileio.h>

#define ARTWORK_MAX_DIMENSION 1024

static void tag_copy(char *output, const size_t size, const AVDictionary *metadata, const char *key) {
    const AVDictionaryEntry *entry = av_dict_get(metadata, key, NULL, AV_DICT_IGNORE_SUFFIX);
    snprintf(output, size, "%s", entry && entry->value ? entry->value : "");
}

static void tag_copy_fallback(char *output, const size_t size, const AVDictionary *metadata,
                              const char *key) {
    if (!output[0]) tag_copy(output, size, metadata, key);
}

static int neighbouring_artwork(const char *uri, char *output, const size_t size) {
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s", uri ? uri : "");
    char *separator = strrchr(directory, '/');
    if (!separator) return 0;
    *separator = '\0';
    static const char *const names[] = {
        "cover.jpg", "cover.jpeg", "cover.png", "cover.webp", "folder.jpg", "folder.jpeg", "folder.png",
        "folder.webp", "front.jpg", "front.jpeg", "front.png", "album.jpg", "album.png"
    };
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
        if (snprintf(output, size, "%s/%s", directory, names[index]) >= (int) size) continue;
        if (file_exist(output)) return 1;
    }
    output[0] = '\0';
    return 0;
}

static int catalogue_artwork(
    const char *uri, const char *fallback_title, char *output, const size_t size
) {
    char catalogue[MAX_BUFFER_SIZE];
    get_catalogue_name_for_content(uri, catalogue, sizeof(catalogue));
    if (!catalogue[0]) return 0;

    char *program = strip_ext(get_file_name(uri));
    char *alternate = strip_ext(get_file_name(fallback_title ? fallback_title : ""));
    const int found = program && load_image_catalogue(
        catalogue, program, alternate ? alternate : "", "default", mux_dim, "box", output, size
    );
    free(program);
    free(alternate);
    return found;
}

static int attached_artwork(AVFormatContext *format, char *output, const size_t size) {
    if (!format) return 0;
    for (unsigned int index = 0; index < format->nb_streams; index++) {
        AVStream *stream = format->streams[index];
        if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO
            || !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
            || stream->attached_pic.size <= 0)
            continue;

        const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *context = codec ? avcodec_alloc_context3(codec) : NULL;
        AVFrame *frame = av_frame_alloc();
        struct SwsContext *scale = NULL;
        uint8_t *rgb = NULL;
        int okay = 0;
        if (!context || !frame || avcodec_parameters_to_context(context, stream->codecpar) < 0
            || avcodec_open2(context, codec, NULL) < 0
            || avcodec_send_packet(context, &stream->attached_pic) < 0
            || avcodec_receive_frame(context, frame) < 0 || frame->width <= 0 || frame->height <= 0)
            goto done;

        if (frame->width > 4096 || frame->height > 4096) goto done;
        int output_width = frame->width;
        int output_height = frame->height;
        if (output_width > ARTWORK_MAX_DIMENSION || output_height > ARTWORK_MAX_DIMENSION) {
            if (output_width >= output_height) {
                output_height = (int) ((int64_t) output_height * ARTWORK_MAX_DIMENSION / output_width);
                output_width = ARTWORK_MAX_DIMENSION;
            } else {
                output_width = (int) ((int64_t) output_width * ARTWORK_MAX_DIMENSION / output_height);
                output_height = ARTWORK_MAX_DIMENSION;
            }
            if (output_width < 1) output_width = 1;
            if (output_height < 1) output_height = 1;
        }
        const size_t bytes = (size_t) output_width * (size_t) output_height * 3U;
        rgb = av_malloc(bytes);
        if (!rgb) goto done;
        scale = sws_getContext(frame->width, frame->height, (enum AVPixelFormat) frame->format,
                               output_width, output_height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR,
                               NULL, NULL, NULL);
        if (!scale) goto done;
        uint8_t *planes[] = {rgb, NULL, NULL, NULL};
        const int strides[] = {output_width * 3, 0, 0, 0};
        if (sws_scale(scale, (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height,
                      planes, strides) <= 0)
            goto done;
        mkdir("/tmp/mustardos", 0755);
        if (snprintf(output, size, "/tmp/mustardos/wasabi-art-%ld.png", (long) getpid()) >= (int) size)
            goto done;
        okay = screenshot_write_rgb(output, rgb, (uint32_t) output_width, (uint32_t) output_height) == 0;
        if (!okay) output[0] = '\0';

    done:
        sws_freeContext(scale);
        av_free(rgb);
        av_frame_free(&frame);
        avcodec_free_context(&context);
        return okay;
    }
    return 0;
}

void wasabi_audio_metadata(AVFormatContext *format, const char *uri, const char *fallback_title,
                           wasabi_audio_info *information) {
    if (!information) return;
    memset(information, 0, sizeof(*information));
    if (!format) {
        snprintf(information->title, sizeof(information->title), "%s", fallback_title ? fallback_title : "");
        if (!catalogue_artwork(uri, fallback_title, information->artwork, sizeof(information->artwork)))
            neighbouring_artwork(uri, information->artwork, sizeof(information->artwork));
        return;
    }
    tag_copy(information->title, sizeof(information->title), format->metadata, "title");
    tag_copy(information->artist, sizeof(information->artist), format->metadata, "artist");
    tag_copy(information->album, sizeof(information->album), format->metadata, "album");
    tag_copy(information->album_artist, sizeof(information->album_artist), format->metadata, "album_artist");
    tag_copy(information->composer, sizeof(information->composer), format->metadata, "composer");
    tag_copy(information->genre, sizeof(information->genre), format->metadata, "genre");
    tag_copy(information->year, sizeof(information->year), format->metadata, "date");
    tag_copy(information->track, sizeof(information->track), format->metadata, "track");
    tag_copy(information->disc, sizeof(information->disc), format->metadata, "disc");
    tag_copy(information->comment, sizeof(information->comment), format->metadata, "comment");
    tag_copy(information->copyright, sizeof(information->copyright), format->metadata, "copyright");
    tag_copy(information->encoder, sizeof(information->encoder), format->metadata, "encoder");
    tag_copy_fallback(information->title, sizeof(information->title), format->metadata, "song");
    tag_copy_fallback(information->artist, sizeof(information->artist), format->metadata, "author");
    tag_copy_fallback(information->album, sizeof(information->album), format->metadata, "game");
    tag_copy_fallback(information->genre, sizeof(information->genre), format->metadata, "system");
    if (!information->title[0])
        snprintf(information->title, sizeof(information->title), "%s", fallback_title ? fallback_title : "");
    tag_copy(information->format, sizeof(information->format), format->metadata, "format");
    if (!information->format[0] && format->iformat && format->iformat->long_name)
        snprintf(information->format, sizeof(information->format), "%s", format->iformat->long_name);
    information->bitrate = format->bit_rate > 0 && format->bit_rate <= INT_MAX ? (int) format->bit_rate : 0;
    information->artwork_temporary = attached_artwork(format, information->artwork, sizeof(information->artwork));
    if (!information->artwork_temporary
        && !catalogue_artwork(uri, fallback_title, information->artwork, sizeof(information->artwork)))
        neighbouring_artwork(uri, information->artwork, sizeof(information->artwork));
}

void wasabi_audio_cleanup(wasabi_audio_info *information) {
    if (!information) return;
    if (information->artwork_temporary && information->artwork[0]) unlink(information->artwork);
    information->artwork[0] = '\0';
    information->artwork_temporary = 0;
}
