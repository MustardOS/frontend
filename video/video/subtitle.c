#include "subtitle.h"

#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL2/SDL_ttf.h>
#include <libavutil/mem.h>

#include <module/muxshare.h>
#include <common/storage/fileio.h>
#include "../core/paths.h"

#define SUBTITLE_TRACK_MAX    32
#define SUBTITLE_CUE_MAX      8192
#define SUBTITLE_STREAM_CUES  256
#define SUBTITLE_FILE_MAX     ((size_t) 8U * 1024U * 1024U)
#define SUBTITLE_TEXT_MAX     2048
#define SUBTITLE_OPEN_SECONDS 10.0
#define SUBTITLE_LINE_MAX     16

typedef enum { subtitle_source_stream, subtitle_source_file } subtitle_source;

typedef enum {
    subtitle_script_latin = 0,
    subtitle_script_japanese,
    subtitle_script_korean,
    subtitle_script_chinese,
    subtitle_script_arabic,
    subtitle_script_count
} subtitle_script;

typedef struct {
    subtitle_source source;
    int stream;
    char path[PATH_MAX];
    char key[PATH_MAX];
    char label[128];
    char language[8];
} subtitle_track;

typedef struct {
    double start;
    double end;
    int open_end;
    int top;
    unsigned serial;
    char *text;
    uint32_t *pixels;
    int x;
    int y;
    int width;
    int height;
    int canvas_width;
    int canvas_height;
} subtitle_cue;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t position;
} subtitle_memory;

static subtitle_track tracks[SUBTITLE_TRACK_MAX];
static int track_count;
static int selected = -1;
static SDL_atomic_t selected_stream;
static const AVFormatContext *media;
static AVCodecContext *decoder;
static double start_offset;
static int fallback_width;
static int fallback_height;
static char content_uri[PATH_MAX];
static char choice_path[PATH_MAX];

static SDL_mutex *lock;
static subtitle_cue *cues;
static int cue_count;
static int cue_capacity;
static unsigned next_serial = 1;
static double position_now = -1.0;
static double active_until = -1.0;
static uint64_t active_signature;
static int rebuild;

static SDL_Texture *texture;
static int texture_width;
static int texture_height;
static int texture_top;
static int texture_bitmap;
static SDL_Rect texture_canvas;
static int texture_video_width;
static int texture_video_height;

static TTF_Font *fonts[subtitle_script_count];
static int font_sizes[subtitle_script_count];
static int ttf_started;

static uint64_t subtitle_hash(const char *value) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *) value; cursor && *cursor; cursor++) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void cue_free(subtitle_cue *cue) {
    free(cue->text);
    free(cue->pixels);
    memset(cue, 0, sizeof(*cue));
}

static void cues_clear_locked(void) {
    for (int index = 0; index < cue_count; index++)
        cue_free(&cues[index]);
    cue_count = 0;
    active_until = -1.0;
    active_signature = 0;
    rebuild = 1;
}

static void cues_release(void) {
    if (lock) SDL_LockMutex(lock);
    cues_clear_locked();
    free(cues);
    cues = NULL;
    cue_capacity = 0;
    if (lock) SDL_UnlockMutex(lock);
}

static void texture_release(void) {
    if (texture) SDL_DestroyTexture(texture);
    texture = NULL;
    texture_width = texture_height = 0;
    texture_bitmap = 0;
    texture_top = 0;
}

static void fonts_release(void) {
    for (int script = 0; script < subtitle_script_count; script++) {
        if (fonts[script] && TTF_WasInit()) TTF_CloseFont(fonts[script]);
        fonts[script] = NULL;
        font_sizes[script] = 0;
    }
}

static int cue_drop_oldest_locked(void) {
    if (!cue_count) return 0;
    int oldest = 0;
    for (int index = 1; index < cue_count; index++)
        if (cues[index].end < cues[oldest].end) oldest = index;
    cue_free(&cues[oldest]);
    memmove(&cues[oldest], &cues[oldest + 1], (size_t) (cue_count - oldest - 1) * sizeof(*cues));
    cue_count--;
    return 1;
}

static int cue_insert_locked(subtitle_cue *cue, const int limit) {
    for (int index = 0; index < cue_count; index++)
        if (cues[index].open_end && cues[index].start < cue->start && cues[index].end > cue->start) {
            cues[index].end = cue->start;
            cues[index].open_end = 0;
        }

    if (cue_count >= limit && !cue_drop_oldest_locked()) return 0;
    if (cue_count == cue_capacity) {
        const int capacity = cue_capacity ? cue_capacity * 2 : 64;
        subtitle_cue *grown = realloc(cues, (size_t) capacity * sizeof(*cues));
        if (!grown) return 0;
        cues = grown;
        cue_capacity = capacity;
    }

    int position = cue_count;
    while (position > 0 && cues[position - 1].start > cue->start)
        position--;
    memmove(&cues[position + 1], &cues[position], (size_t) (cue_count - position) * sizeof(*cues));
    cue->serial = next_serial++;
    if (!next_serial) next_serial = 1;
    cues[position] = *cue;
    cue_count++;
    rebuild = 1;
    return 1;
}

static void cues_close_open_locked(const double at) {
    for (int index = 0; index < cue_count; index++)
        if (cues[index].open_end && cues[index].start <= at && cues[index].end > at) {
            cues[index].end = at;
            cues[index].open_end = 0;
            rebuild = 1;
        }
}

static void append_text(char *output, const size_t size, size_t *used, const char *text, const size_t length) {
    if (*used + length + 1 > size) return;
    memcpy(output + *used, text, length);
    *used += length;
    output[*used] = '\0';
}

static void ass_override(const char *block, const size_t length, int *top, int *drawing) {
    for (size_t index = 0; index + 1 < length; index++) {
        if (block[index] != '\\') continue;
        if (block[index + 1] == 'a' && index + 3 < length && block[index + 2] == 'n') {
            const char digit = block[index + 3];
            if (digit >= '7' && digit <= '9') *top = 1;
            if (digit >= '1' && digit <= '3') *top = 0;
        } else if (block[index + 1] == 'p' && index + 2 < length && block[index + 2] >= '0'
                   && block[index + 2] <= '9') {
            *drawing = block[index + 2] != '0';
        }
    }
}

static void ass_to_text(const char *ass, char *output, const size_t size, int *top) {
    const char *text = ass;
    for (int commas = 0; commas < 8; commas++) {
        const char *next = strchr(text, ',');
        if (!next) {
            text = ass;
            break;
        }
        text = next + 1;
    }

    size_t used = 0;
    int drawing = 0;
    output[0] = '\0';
    for (const char *cursor = text; *cursor;) {
        if (*cursor == '{') {
            const char *close = strchr(cursor, '}');
            if (!close) break;
            ass_override(cursor + 1, (size_t) (close - cursor - 1), top, &drawing);
            cursor = close + 1;
            continue;
        }
        if (cursor[0] == '\\' && (cursor[1] == 'N' || cursor[1] == 'n')) {
            if (!drawing) append_text(output, size, &used, "\n", 1);
            cursor += 2;
            continue;
        }
        if (cursor[0] == '\\' && cursor[1] == 'h') {
            if (!drawing) append_text(output, size, &used, " ", 1);
            cursor += 2;
            continue;
        }
        if (*cursor == '\r') {
            cursor++;
            continue;
        }
        if (!drawing) append_text(output, size, &used, cursor, 1);
        cursor++;
    }

    while (used > 0 && (output[used - 1] == '\n' || output[used - 1] == ' ' || output[used - 1] == '\t'))
        output[--used] = '\0';
    size_t start = 0;
    while (output[start] == '\n' || output[start] == ' ')
        start++;
    if (start) memmove(output, output + start, used - start + 1);
}

static void plain_text(const char *source, char *output, const size_t size) {
    size_t used = 0;
    output[0] = '\0';
    for (const char *cursor = source; *cursor; cursor++)
        if (*cursor != '\r') append_text(output, size, &used, cursor, 1);
    while (used > 0 && (output[used - 1] == '\n' || output[used - 1] == ' '))
        output[--used] = '\0';
}

static uint32_t *bitmap_compose(const AVSubtitle *subtitle, int *x, int *y, int *width, int *height) {
    int left = INT_MAX;
    int top = INT_MAX;
    int right = INT_MIN;
    int bottom = INT_MIN;
    for (unsigned index = 0; index < subtitle->num_rects; index++) {
        const AVSubtitleRect *rect = subtitle->rects[index];
        if (rect->type != SUBTITLE_BITMAP || rect->w <= 0 || rect->h <= 0 || !rect->data[0] || !rect->data[1]) continue;
        if (rect->x < left) left = rect->x;
        if (rect->y < top) top = rect->y;
        if (rect->x + rect->w > right) right = rect->x + rect->w;
        if (rect->y + rect->h > bottom) bottom = rect->y + rect->h;
    }
    if (left == INT_MAX || right <= left || bottom <= top || (right - left) > 4096 || (bottom - top) > 4096)
        return NULL;

    const int composed_width = right - left;
    const int composed_height = bottom - top;
    uint32_t *pixels = calloc((size_t) composed_width * (size_t) composed_height, sizeof(*pixels));
    if (!pixels) return NULL;

    for (unsigned index = 0; index < subtitle->num_rects; index++) {
        const AVSubtitleRect *rect = subtitle->rects[index];
        if (rect->type != SUBTITLE_BITMAP || rect->w <= 0 || rect->h <= 0 || !rect->data[0] || !rect->data[1]) continue;
        const uint32_t *palette = (const uint32_t *) rect->data[1];
        for (int row = 0; row < rect->h; row++) {
            const uint8_t *line = rect->data[0] + (size_t) row * (size_t) rect->linesize[0];
            uint32_t *target = pixels + (size_t) (rect->y - top + row) * (size_t) composed_width + (rect->x - left);
            for (int column = 0; column < rect->w; column++) {
                const uint8_t entry = line[column];
                if (entry < rect->nb_colors) target[column] = palette[entry];
            }
        }
    }

    *x = left;
    *y = top;
    *width = composed_width;
    *height = composed_height;
    return pixels;
}

static void store_subtitle(
    const AVSubtitle *subtitle, const double packet_start, const double packet_duration, const double offset,
    const int limit
) {
    double start = packet_start;
    if (subtitle->pts != AV_NOPTS_VALUE) start = (double) subtitle->pts / AV_TIME_BASE - offset;
    if (start < -1.0) return;

    const double begin = start + subtitle->start_display_time / 1000.0;
    if (!subtitle->num_rects) {
        SDL_LockMutex(lock);
        cues_close_open_locked(begin);
        SDL_UnlockMutex(lock);
        return;
    }

    subtitle_cue cue = {0};
    cue.start = begin;
    if (subtitle->end_display_time && subtitle->end_display_time != UINT32_MAX
        && subtitle->end_display_time > subtitle->start_display_time) {
        cue.end = start + subtitle->end_display_time / 1000.0;
    } else if (packet_duration > 0.0) {
        cue.end = start + packet_duration;
    } else {
        cue.end = begin + SUBTITLE_OPEN_SECONDS;
        cue.open_end = 1;
    }

    char text[SUBTITLE_TEXT_MAX];
    size_t used = 0;
    text[0] = '\0';
    for (unsigned index = 0; index < subtitle->num_rects; index++) {
        const AVSubtitleRect *rect = subtitle->rects[index];
        char piece[SUBTITLE_TEXT_MAX];
        piece[0] = '\0';
        if (rect->type == SUBTITLE_ASS && rect->ass)
            ass_to_text(rect->ass, piece, sizeof(piece), &cue.top);
        else if (rect->type == SUBTITLE_TEXT && rect->text)
            plain_text(rect->text, piece, sizeof(piece));
        if (!piece[0]) continue;
        if (used) append_text(text, sizeof(text), &used, "\n", 1);
        append_text(text, sizeof(text), &used, piece, strlen(piece));
    }

    if (text[0]) {
        cue.text = strdup(text);
        if (!cue.text) return;
    } else {
        cue.pixels = bitmap_compose(subtitle, &cue.x, &cue.y, &cue.width, &cue.height);
        if (!cue.pixels) return;
        cue.canvas_width = decoder && decoder->width > 0 ? decoder->width : fallback_width;
        cue.canvas_height = decoder && decoder->height > 0 ? decoder->height : fallback_height;
        if (cue.canvas_width <= 0 || cue.canvas_height <= 0) {
            free(cue.pixels);
            return;
        }
    }

    SDL_LockMutex(lock);
    if (!cue_insert_locked(&cue, limit)) cue_free(&cue);
    SDL_UnlockMutex(lock);
}

static void decode_into_cues(
    AVCodecContext *context, const AVPacket *packet, const AVRational time_base, const double offset, const int limit
) {
    AVSubtitle subtitle;
    int got = 0;
    if (avcodec_decode_subtitle2(context, &subtitle, &got, (AVPacket *) packet) < 0 || !got) return;

    double packet_start = 0.0;
    if (packet->pts != AV_NOPTS_VALUE) packet_start = (double) packet->pts * av_q2d(time_base) - offset;
    const double packet_duration = packet->duration > 0 ? (double) packet->duration * av_q2d(time_base) : 0.0;
    store_subtitle(&subtitle, packet_start, packet_duration, offset, limit);
    avsubtitle_free(&subtitle);
}

static AVCodecContext *open_subtitle_decoder(const AVStream *stream) {
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) return NULL;
    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context) return NULL;
    if (avcodec_parameters_to_context(context, stream->codecpar) < 0) {
        avcodec_free_context(&context);
        return NULL;
    }
    context->pkt_timebase = stream->time_base;
    if (avcodec_open2(context, codec, NULL) < 0) {
        avcodec_free_context(&context);
        return NULL;
    }
    return context;
}

static int utf8_valid(const unsigned char *text, const size_t size) {
    size_t index = 0;
    while (index < size) {
        const unsigned char lead = text[index];
        size_t extra;
        if (lead < 0x80)
            extra = 0;
        else if ((lead & 0xE0) == 0xC0 && lead >= 0xC2)
            extra = 1;
        else if ((lead & 0xF0) == 0xE0)
            extra = 2;
        else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4)
            extra = 3;
        else
            return 0;
        if (index + extra >= size && extra) return 0;
        for (size_t step = 1; step <= extra; step++)
            if ((text[index + step] & 0xC0) != 0x80) return 0;
        index += extra + 1;
    }
    return 1;
}

static size_t utf8_put(unsigned char *output, const uint32_t codepoint) {
    if (codepoint < 0x80) {
        output[0] = (unsigned char) codepoint;
        return 1;
    }
    if (codepoint < 0x800) {
        output[0] = (unsigned char) (0xC0 | (codepoint >> 6));
        output[1] = (unsigned char) (0x80 | (codepoint & 0x3F));
        return 2;
    }
    if (codepoint < 0x10000) {
        output[0] = (unsigned char) (0xE0 | (codepoint >> 12));
        output[1] = (unsigned char) (0x80 | ((codepoint >> 6) & 0x3F));
        output[2] = (unsigned char) (0x80 | (codepoint & 0x3F));
        return 3;
    }
    output[0] = (unsigned char) (0xF0 | (codepoint >> 18));
    output[1] = (unsigned char) (0x80 | ((codepoint >> 12) & 0x3F));
    output[2] = (unsigned char) (0x80 | ((codepoint >> 6) & 0x3F));
    output[3] = (unsigned char) (0x80 | (codepoint & 0x3F));
    return 4;
}

static unsigned char *
utf16_to_utf8(const unsigned char *data, const size_t size, const int big_endian, size_t *output_size) {
    unsigned char *output = malloc(size / 2 * 3 + 1);
    if (!output) return NULL;
    size_t used = 0;
    for (size_t index = 0; index + 1 < size; index += 2) {
        uint32_t unit = big_endian ? (uint32_t) (data[index] << 8 | data[index + 1])
                                   : (uint32_t) (data[index + 1] << 8 | data[index]);
        if (unit >= 0xD800 && unit <= 0xDBFF && index + 3 < size) {
            const uint32_t low = big_endian ? (uint32_t) (data[index + 2] << 8 | data[index + 3])
                                            : (uint32_t) (data[index + 3] << 8 | data[index + 2]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                index += 2;
            }
        }
        used += utf8_put(output + used, unit);
    }
    *output_size = used;
    return output;
}

static unsigned char *windows1252_to_utf8(const unsigned char *data, const size_t size, size_t *output_size) {
    static const uint16_t high[32] = {0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                      0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
                                      0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                      0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
    unsigned char *output = malloc(size * 3 + 1);
    if (!output) return NULL;
    size_t used = 0;
    for (size_t index = 0; index < size; index++) {
        const unsigned char value = data[index];
        const uint32_t codepoint = value >= 0x80 && value < 0xA0 ? high[value - 0x80] : value;
        used += utf8_put(output + used, codepoint);
    }
    *output_size = used;
    return output;
}

static unsigned char *load_text_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long length = ftell(file);
    if (length <= 0 || (size_t) length > SUBTITLE_FILE_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    unsigned char *raw = malloc((size_t) length);
    if (!raw || fread(raw, 1, (size_t) length, file) != (size_t) length) {
        free(raw);
        fclose(file);
        return NULL;
    }
    fclose(file);

    const size_t raw_size = (size_t) length;
    unsigned char *converted = NULL;
    size_t converted_size = 0;
    if (raw_size >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        converted = utf16_to_utf8(raw + 2, raw_size - 2, 0, &converted_size);
    } else if (raw_size >= 2 && raw[0] == 0xFE && raw[1] == 0xFF) {
        converted = utf16_to_utf8(raw + 2, raw_size - 2, 1, &converted_size);
    } else {
        const size_t skip = raw_size >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF ? 3 : 0;
        if (utf8_valid(raw + skip, raw_size - skip)) {
            converted = malloc(raw_size - skip + 1);
            if (converted) {
                memcpy(converted, raw + skip, raw_size - skip);
                converted_size = raw_size - skip;
            }
        } else {
            converted = windows1252_to_utf8(raw, raw_size, &converted_size);
        }
    }
    free(raw);
    if (!converted) return NULL;
    converted[converted_size] = '\0';
    *size = converted_size;
    return converted;
}

static int memory_read(void *opaque, uint8_t *buffer, const int size) {
    subtitle_memory *memory = opaque;
    const size_t remaining = memory->size - memory->position;
    if (!remaining) return AVERROR_EOF;
    const size_t count = (size_t) size < remaining ? (size_t) size : remaining;
    memcpy(buffer, memory->data + memory->position, count);
    memory->position += count;
    return (int) count;
}

static int64_t memory_seek(void *opaque, const int64_t offset, const int whence) {
    subtitle_memory *memory = opaque;
    int64_t target;
    switch (whence & ~AVSEEK_FORCE) {
        case AVSEEK_SIZE:
            return (int64_t) memory->size;
        case SEEK_SET:
            target = offset;
            break;
        case SEEK_CUR:
            target = (int64_t) memory->position + offset;
            break;
        case SEEK_END:
            target = (int64_t) memory->size + offset;
            break;
        default:
            return AVERROR(EINVAL);
    }
    if (target < 0 || target > (int64_t) memory->size) return AVERROR(EINVAL);
    memory->position = (size_t) target;
    return target;
}

static const char *file_demuxer(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    if (strcasecmp(dot, ".srt") == 0) return "srt";
    if (strcasecmp(dot, ".ass") == 0 || strcasecmp(dot, ".ssa") == 0) return "ass";
    if (strcasecmp(dot, ".vtt") == 0) return "webvtt";
    return NULL;
}

static int load_file_track(const subtitle_track *track) {
    const char *demuxer = file_demuxer(track->path);
    const AVInputFormat *input = demuxer ? av_find_input_format(demuxer) : NULL;
    if (!input) return 0;

    size_t size = 0;
    unsigned char *text = load_text_file(track->path, &size);
    if (!text) return 0;

    subtitle_memory memory = {text, size, 0};
    unsigned char *buffer = av_malloc(4096);
    if (!buffer) {
        free(text);
        return 0;
    }
    AVIOContext *io = avio_alloc_context(buffer, 4096, 0, &memory, memory_read, NULL, memory_seek);
    if (!io) {
        av_free(buffer);
        free(text);
        return 0;
    }
    AVFormatContext *format = avformat_alloc_context();
    if (!format) {
        av_freep(&io->buffer);
        avio_context_free(&io);
        free(text);
        return 0;
    }
    format->pb = io;
    format->flags |= AVFMT_FLAG_CUSTOM_IO;

    int loaded = 0;
    if (avformat_open_input(&format, NULL, input, NULL) == 0 && format->nb_streams > 0) {
        AVCodecContext *context = open_subtitle_decoder(format->streams[0]);
        AVPacket *packet = context ? av_packet_alloc() : NULL;
        if (packet) {
            const AVRational time_base = format->streams[0]->time_base;
            while (av_read_frame(format, packet) >= 0) {
                if (packet->stream_index == 0) decode_into_cues(context, packet, time_base, 0.0, SUBTITLE_CUE_MAX);
                av_packet_unref(packet);
            }
            loaded = 1;
        }
        av_packet_free(&packet);
        avcodec_free_context(&context);
    }
    avformat_close_input(&format);
    av_freep(&io->buffer);
    avio_context_free(&io);
    free(text);
    return loaded;
}

static void stream_label(const AVStream *stream, const int number, subtitle_track *track) {
    const AVDictionaryEntry *title = av_dict_get(stream->metadata, "title", NULL, 0);
    const AVDictionaryEntry *language = av_dict_get(stream->metadata, "language", NULL, 0);
    char code[8] = "";
    if (language && language->value && strcasecmp(language->value, "und") != 0) {
        snprintf(code, sizeof(code), "%s", language->value);
        for (char *cursor = code; *cursor; cursor++)
            if (*cursor >= 'a' && *cursor <= 'z') *cursor = (char) (*cursor - 'a' + 'A');
        snprintf(track->language, sizeof(track->language), "%s", language->value);
    }

    if (title && title->value && title->value[0] && code[0])
        snprintf(track->label, sizeof(track->label), "%s (%s)", title->value, code);
    else if (title && title->value && title->value[0])
        snprintf(track->label, sizeof(track->label), "%s", title->value);
    else if (code[0])
        snprintf(track->label, sizeof(track->label), "%s", code);
    else
        snprintf(track->label, sizeof(track->label), "%s %d", lang.muxmedia.track, number);
}

static void add_stream_tracks(const AVFormatContext *format) {
    int number = 0;
    for (unsigned index = 0; index < format->nb_streams && track_count < SUBTITLE_TRACK_MAX; index++) {
        const AVStream *stream = format->streams[index];
        if (stream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) continue;
        if (!avcodec_find_decoder(stream->codecpar->codec_id)) continue;
        subtitle_track *track = &tracks[track_count];
        memset(track, 0, sizeof(*track));
        track->source = subtitle_source_stream;
        track->stream = (int) index;
        snprintf(track->key, sizeof(track->key), "stream:%u", index);
        stream_label(stream, ++number, track);
        track_count++;
    }
}

static int compare_tracks(const void *left, const void *right) {
    const subtitle_track *a = left;
    const subtitle_track *b = right;
    return strcasecmp(a->path, b->path);
}

static void add_file_tracks(const char *uri) {
    if (!uri || strstr(uri, "://")) return;

    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s", uri);
    char *separator = strrchr(directory, '/');
    if (!separator) return;
    *separator = '\0';

    char base[PATH_MAX];
    snprintf(base, sizeof(base), "%s", separator + 1);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    const size_t base_length = strlen(base);
    if (!base_length) return;

    DIR *folder = opendir(directory[0] ? directory : "/");
    if (!folder) return;

    const int first = track_count;
    const struct dirent *entry;
    while ((entry = readdir(folder)) && track_count < SUBTITLE_TRACK_MAX) {
        const char *name = entry->d_name;
        if (strncmp(name, base, base_length) != 0 || name[base_length] != '.' || !file_demuxer(name)) continue;

        subtitle_track *track = &tracks[track_count];
        memset(track, 0, sizeof(*track));
        track->source = subtitle_source_file;
        track->stream = -1;
        if ((size_t) snprintf(track->path, sizeof(track->path), "%s/%s", directory, name) >= sizeof(track->path))
            continue;
        snprintf(track->key, sizeof(track->key), "file:%s", name);

        const char *middle = name + base_length + 1;
        const char *extension = strrchr(name, '.');
        if (extension > middle) {
            snprintf(track->label, sizeof(track->label), "%.*s", (int) (extension - middle), middle);
            for (char *cursor = track->label; *cursor && *cursor != '.'; cursor++)
                if (*cursor >= 'a' && *cursor <= 'z') *cursor = (char) (*cursor - 'a' + 'A');
            const char *language_end = strchr(middle, '.');
            const size_t language_length = language_end ? (size_t) (language_end - middle) : 0;
            if (language_length >= 2 && language_length < sizeof(track->language))
                snprintf(track->language, sizeof(track->language), "%.*s", (int) language_length, middle);
        } else {
            snprintf(track->label, sizeof(track->label), "%s", extension + 1);
            for (char *cursor = track->label; *cursor; cursor++)
                if (*cursor >= 'a' && *cursor <= 'z') *cursor = (char) (*cursor - 'a' + 'A');
        }
        track_count++;
    }
    closedir(folder);

    if (track_count - first > 1) qsort(&tracks[first], (size_t) (track_count - first), sizeof(*tracks), compare_tracks);
}

static void choice_save(void) {
    if (!choice_path[0]) return;
    if (selected < 0) {
        remove(choice_path);
        return;
    }
    create_directories(choice_path, 1);
    write_text_to_file(choice_path, "w", CHAR, tracks[selected].key);
}

static int choice_load(void) {
    if (!choice_path[0] || !file_exist(choice_path)) return -1;
    char *value = read_line_char_from(choice_path, 1);
    if (!value) return -1;
    int found = -1;
    for (int index = 0; index < track_count; index++)
        if (strcmp(tracks[index].key, value) == 0) {
            found = index;
            break;
        }
    free(value);
    return found;
}

static void decoder_release(void) {
    SDL_AtomicSet(&selected_stream, -1);
    if (lock) SDL_LockMutex(lock);
    avcodec_free_context(&decoder);
    cues_clear_locked();
    if (lock) SDL_UnlockMutex(lock);
}

static int activate(const int index) {
    decoder_release();
    selected = -1;
    if (index < 0 || index >= track_count) return 0;

    const subtitle_track *track = &tracks[index];
    if (track->source == subtitle_source_file) {
        if (!load_file_track(track)) return 0;
        selected = index;
        return 1;
    }

    if (!media || track->stream < 0 || (unsigned) track->stream >= media->nb_streams) return 0;
    AVCodecContext *context = open_subtitle_decoder(media->streams[track->stream]);
    if (!context) return 0;
    SDL_LockMutex(lock);
    decoder = context;
    SDL_UnlockMutex(lock);
    selected = index;
    SDL_AtomicSet(&selected_stream, track->stream);
    return 1;
}

void subtitle_open(const AVFormatContext *format, const char *uri, const int video_width, const int video_height) {
    subtitle_close();
    if (!lock) lock = SDL_CreateMutex();
    if (!lock) return;

    media = format;
    fallback_width = video_width;
    fallback_height = video_height;
    start_offset = format && format->start_time != AV_NOPTS_VALUE ? (double) format->start_time / AV_TIME_BASE : 0.0;
    snprintf(content_uri, sizeof(content_uri), "%s", uri ? uri : "");
    snprintf(
        choice_path, sizeof(choice_path), WASABI_SHARE_PATH "settings/subtitle/%016llx",
        (unsigned long long) subtitle_hash(content_uri)
    );

    if (format) add_stream_tracks(format);
    add_file_tracks(uri);

    const int remembered = choice_load();
    if (remembered >= 0) activate(remembered);
}

void subtitle_close(void) {
    decoder_release();
    cues_release();
    texture_release();
    fonts_release();
    selected = -1;
    track_count = 0;
    media = NULL;
    position_now = -1.0;
    content_uri[0] = '\0';
    choice_path[0] = '\0';
}

int subtitle_track_count(void) {
    return track_count;
}

const char *subtitle_track_label(const int index) {
    return index >= 0 && index < track_count ? tracks[index].label : "";
}

int subtitle_track_selected(void) {
    return selected;
}

int subtitle_select(const int index) {
    if (index == selected) return 0;
    const int embedded = activate(index) && tracks[index].source == subtitle_source_stream;
    choice_save();
    rebuild = 1;
    return embedded;
}

int subtitle_stream(void) {
    return SDL_AtomicGet(&selected_stream);
}

void subtitle_packet(const AVPacket *packet) {
    if (!lock || !packet) return;
    SDL_LockMutex(lock);
    const int stream = SDL_AtomicGet(&selected_stream);
    if (decoder && media && stream >= 0 && packet->stream_index == stream)
        decode_into_cues(decoder, packet, media->streams[stream]->time_base, start_offset, SUBTITLE_STREAM_CUES);
    SDL_UnlockMutex(lock);
}

void subtitle_flush(void) {
    if (!lock || SDL_AtomicGet(&selected_stream) < 0) return;
    SDL_LockMutex(lock);
    if (decoder) avcodec_flush_buffers(decoder);
    cues_clear_locked();
    SDL_UnlockMutex(lock);
}

int subtitle_set_position(const double playback) {
    if (!lock || selected < 0) return 0;
    const double position = playback - config.video.subtitle_delay / 10.0;
    SDL_LockMutex(lock);
    // A sorted cue set cannot change between its next start or active end.
    if (!rebuild && position >= position_now && (active_until < 0.0 || position < active_until)) {
        position_now = position;
        SDL_UnlockMutex(lock);
        return 0;
    }

    position_now = position;
    uint64_t signature = 0;
    double next_change = -1.0;
    for (int index = 0; index < cue_count; index++) {
        if (cues[index].start > position) {
            if (next_change < 0.0 || cues[index].start < next_change) next_change = cues[index].start;
            break;
        }
        if (cues[index].end <= position) continue;
        signature = signature * UINT64_C(1099511628211) + cues[index].serial;
        if (next_change < 0.0 || cues[index].end < next_change) next_change = cues[index].end;
    }
    const int changed = signature != active_signature || rebuild;
    active_until = next_change;
    active_signature = signature;
    if (changed) rebuild = 1;
    SDL_UnlockMutex(lock);
    return changed;
}

void subtitle_settings_changed(void) {
    rebuild = 1;
}

static subtitle_script detect_script(const char *text, const char *language) {
    const unsigned char *cursor = (const unsigned char *) text;
    int han = 0;
    while (*cursor) {
        uint32_t codepoint = *cursor;
        size_t length = 1;
        if ((codepoint & 0xE0) == 0xC0 && cursor[1]) {
            codepoint = ((codepoint & 0x1F) << 6) | (cursor[1] & 0x3F);
            length = 2;
        } else if ((codepoint & 0xF0) == 0xE0 && cursor[1] && cursor[2]) {
            codepoint = ((codepoint & 0x0F) << 12) | ((cursor[1] & 0x3F) << 6) | (cursor[2] & 0x3F);
            length = 3;
        } else if ((codepoint & 0xF8) == 0xF0 && cursor[1] && cursor[2] && cursor[3]) {
            codepoint = ((codepoint & 0x07) << 18) | ((cursor[1] & 0x3F) << 12) | ((cursor[2] & 0x3F) << 6)
                        | (cursor[3] & 0x3F);
            length = 4;
        }
        if ((codepoint >= 0xAC00 && codepoint <= 0xD7AF) || (codepoint >= 0x1100 && codepoint <= 0x11FF)
            || (codepoint >= 0x3130 && codepoint <= 0x318F))
            return subtitle_script_korean;
        if (codepoint >= 0x3040 && codepoint <= 0x30FF) return subtitle_script_japanese;
        if (codepoint >= 0x0600 && codepoint <= 0x06FF) return subtitle_script_arabic;
        if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) han = 1;
        cursor += length;
    }
    if (!han) return subtitle_script_latin;
    if (language
        && (strncasecmp(language, "zh", 2) == 0 || strcasecmp(language, "chi") == 0
            || strcasecmp(language, "zho") == 0))
        return subtitle_script_chinese;
    return subtitle_script_japanese;
}

static TTF_Font *font_for(const subtitle_script script, const int size) {
    if (fonts[script] && font_sizes[script] == size) return fonts[script];
    if (!ttf_started) {
        if (!TTF_WasInit() && TTF_Init() != 0) return NULL;
        ttf_started = 1;
    }
    if (fonts[script]) TTF_CloseFont(fonts[script]);
    fonts[script] = NULL;

    static const char *const files[subtitle_script_count] = {
        INTERNAL_FONTS "/Noto Sans.ttf",           INTERNAL_FONTS "/Japanese/Noto Sans JP.ttf",
        INTERNAL_FONTS "/Korean/Noto Sans KR.ttf", INTERNAL_FONTS "/Chinese (Simplified)/Noto Sans SC.ttf",
        INTERNAL_FONTS "/Arabic/Noto Sans AR.ttf",
    };
    fonts[script] = TTF_OpenFont(files[script], size);
    if (!fonts[script] && script != subtitle_script_latin) return font_for(subtitle_script_latin, size);
    font_sizes[script] = fonts[script] ? size : 0;
    return fonts[script];
}

static int wrap_lines(TTF_Font *font, const char *text, const int limit, char lines[][SUBTITLE_TEXT_MAX]) {
    int count = 0;
    const char *cursor = text;
    while (*cursor && count < SUBTITLE_LINE_MAX) {
        const char *end = strchr(cursor, '\n');
        const size_t length = end ? (size_t) (end - cursor) : strlen(cursor);
        char paragraph[SUBTITLE_TEXT_MAX];
        snprintf(paragraph, sizeof(paragraph), "%.*s", (int) length, cursor);

        char line[SUBTITLE_TEXT_MAX] = "";
        char *word = paragraph;
        while (word && count < SUBTITLE_LINE_MAX) {
            char *space = strchr(word, ' ');
            if (space) *space = '\0';
            char candidate[SUBTITLE_TEXT_MAX];
            snprintf(candidate, sizeof(candidate), "%s%s%s", line, line[0] ? " " : "", word);
            int width = 0;
            TTF_SizeUTF8(font, candidate, &width, NULL);
            if (width <= limit) {
                snprintf(line, sizeof(line), "%s", candidate);
            } else if (line[0]) {
                snprintf(lines[count++], SUBTITLE_TEXT_MAX, "%s", line);
                line[0] = '\0';
                if (space) *space = ' ';
                continue;
            } else {
                size_t taken = 0;
                const size_t length = strlen(word);
                while (taken < length) {
                    size_t next = taken + 1;
                    while (next < length && ((unsigned char) word[next] & 0xC0) == 0x80)
                        next++;
                    char piece[SUBTITLE_TEXT_MAX];
                    snprintf(piece, sizeof(piece), "%.*s", (int) next, word);
                    TTF_SizeUTF8(font, piece, &width, NULL);
                    if (width > limit && taken) break;
                    taken = next;
                }
                snprintf(line, sizeof(line), "%.*s", (int) taken, word);
                if (taken < length) {
                    snprintf(lines[count++], SUBTITLE_TEXT_MAX, "%s", line);
                    line[0] = '\0';
                    memmove(word, word + taken, length - taken + 1);
                    if (space) *space = ' ';
                    continue;
                }
            }
            word = space ? space + 1 : NULL;
        }
        if (count < SUBTITLE_LINE_MAX && line[0]) snprintf(lines[count++], SUBTITLE_TEXT_MAX, "%s", line);

        if (!end) break;
        cursor = end + 1;
    }
    return count;
}

static SDL_Surface *render_text(const char *text, const char *language, const int video_width, const int video_height) {
    static const int sizes[] = {48, 62, 78};
    const int size_index =
        config.video.subtitle_size >= 0 && config.video.subtitle_size < 3 ? config.video.subtitle_size : 1;
    int font_size = video_height * sizes[size_index] / 1000;
    if (font_size < 12) font_size = 12;

    TTF_Font *font = font_for(detect_script(text, language), font_size);
    if (!font) return NULL;

    static char lines[SUBTITLE_LINE_MAX][SUBTITLE_TEXT_MAX];
    const int count = wrap_lines(font, text, video_width * 9 / 10, lines);
    if (!count) return NULL;

    const int boxed = config.video.subtitle_style == 1;
    const int outline = boxed ? 0 : (font_size / 14 > 1 ? font_size / 14 : 1);
    const int padding = boxed ? font_size / 4 : 0;
    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color black = {0, 0, 0, 255};

    SDL_Surface *rendered[SUBTITLE_LINE_MAX] = {0};
    int widest = 0;
    int line_height = TTF_FontLineSkip(font) + outline * 2;
    for (int index = 0; index < count; index++) {
        TTF_SetFontOutline(font, 0);
        SDL_Surface *fill = TTF_RenderUTF8_Blended(font, lines[index][0] ? lines[index] : " ", white);
        if (!fill) continue;
        SDL_Surface *line = SDL_CreateRGBSurfaceWithFormat(
            0, fill->w + outline * 2 + padding * 2, fill->h + outline * 2, 32, SDL_PIXELFORMAT_ARGB8888
        );
        if (!line) {
            SDL_FreeSurface(fill);
            continue;
        }
        SDL_FillRect(line, NULL, SDL_MapRGBA(line->format, 0, 0, 0, boxed ? 160 : 0));
        if (outline) {
            TTF_SetFontOutline(font, outline);
            SDL_Surface *edge = TTF_RenderUTF8_Blended(font, lines[index][0] ? lines[index] : " ", black);
            TTF_SetFontOutline(font, 0);
            if (edge) {
                SDL_SetSurfaceBlendMode(edge, SDL_BLENDMODE_BLEND);
                SDL_Rect at = {padding, 0, edge->w, edge->h};
                SDL_BlitSurface(edge, NULL, line, &at);
                SDL_FreeSurface(edge);
            }
        }
        SDL_SetSurfaceBlendMode(fill, SDL_BLENDMODE_BLEND);
        SDL_Rect at = {padding + outline, outline, fill->w, fill->h};
        SDL_BlitSurface(fill, NULL, line, &at);
        SDL_FreeSurface(fill);
        rendered[index] = line;
        if (line->w > widest) widest = line->w;
        if (line->h > line_height) line_height = line->h;
    }
    if (!widest) {
        for (int index = 0; index < count; index++)
            if (rendered[index]) SDL_FreeSurface(rendered[index]);
        return NULL;
    }

    SDL_Surface *output = SDL_CreateRGBSurfaceWithFormat(0, widest, line_height * count, 32, SDL_PIXELFORMAT_ARGB8888);
    if (output) {
        SDL_FillRect(output, NULL, SDL_MapRGBA(output->format, 0, 0, 0, 0));
        SDL_SetSurfaceBlendMode(output, SDL_BLENDMODE_NONE);
        for (int index = 0; index < count; index++) {
            if (!rendered[index]) continue;
            SDL_SetSurfaceBlendMode(rendered[index], SDL_BLENDMODE_NONE);
            SDL_Rect at = {
                (widest - rendered[index]->w) / 2, index * line_height, rendered[index]->w, rendered[index]->h
            };
            SDL_BlitSurface(rendered[index], NULL, output, &at);
        }
    }
    for (int index = 0; index < count; index++)
        if (rendered[index]) SDL_FreeSurface(rendered[index]);
    return output;
}

static void rebuild_texture(SDL_Renderer *renderer, const int video_width, const int video_height) {
    texture_release();
    texture_video_width = video_width;
    texture_video_height = video_height;

    char text[SUBTITLE_TEXT_MAX];
    size_t used = 0;
    text[0] = '\0';
    int top = 0;
    int bitmap = -1;
    SDL_LockMutex(lock);
    for (int index = 0; index < cue_count; index++) {
        const subtitle_cue *cue = &cues[index];
        if (cue->start > position_now) break;
        if (cue->end <= position_now) continue;
        if (cue->text) {
            if (used) append_text(text, sizeof(text), &used, "\n", 1);
            append_text(text, sizeof(text), &used, cue->text, strlen(cue->text));
            if (cue->top) top = 1;
        } else if (bitmap < 0 && cue->pixels) {
            bitmap = index;
        }
    }

    if (!text[0] && bitmap >= 0) {
        const subtitle_cue *cue = &cues[bitmap];
        texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, cue->width, cue->height);
        if (texture) {
            SDL_UpdateTexture(texture, NULL, cue->pixels, cue->width * (int) sizeof(uint32_t));
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            texture_bitmap = 1;
            texture_width = cue->width;
            texture_height = cue->height;
            texture_canvas = (SDL_Rect) {cue->x, cue->y, cue->canvas_width, cue->canvas_height};
        }
    }
    SDL_UnlockMutex(lock);
    rebuild = 0;
    if (!text[0]) return;

    SDL_Surface *surface = render_text(text, selected >= 0 ? tracks[selected].language : "", video_width, video_height);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        texture_width = surface->w;
        texture_height = surface->h;
        texture_top = top;
    }
    SDL_FreeSurface(surface);
}

void subtitle_render(SDL_Renderer *renderer, const SDL_Rect *video, const double angle) {
    if (!lock || selected < 0 || !renderer || !video || video->w <= 0 || video->h <= 0) return;
    const int quarter = (int) (angle / 90.0 + 0.5) % 4;
    if (rebuild || video->w != texture_video_width || video->h != texture_video_height)
        rebuild_texture(renderer, video->w, video->h);
    if (!texture) return;

    const SDL_Rect area = *video;

    SDL_Rect target;
    if (texture_bitmap) {
        if (texture_canvas.w <= 0 || texture_canvas.h <= 0) return;
        target.x = area.x + (int) ((long long) texture_canvas.x * area.w / texture_canvas.w);
        target.y = area.y + (int) ((long long) texture_canvas.y * area.h / texture_canvas.h);
        target.w = (int) ((long long) texture_width * area.w / texture_canvas.w);
        target.h = (int) ((long long) texture_height * area.h / texture_canvas.h);
    } else {
        int bottom = area.y + area.h;
        int upper = area.y;
        if (config.video.subtitle_position == 1 && !quarter) {
            int screen_width = 0;
            int screen_height = 0;
            SDL_GetRendererOutputSize(renderer, &screen_width, &screen_height);
            if (screen_height > 0) {
                bottom = screen_height;
                upper = 0;
            }
        }
        const int margin = area.h / 24;
        target.w = texture_width;
        target.h = texture_height;
        target.x = area.x + (area.w - texture_width) / 2;
        target.y = texture_top ? upper + margin : bottom - margin - texture_height;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (!quarter) {
        SDL_RenderCopy(renderer, texture, NULL, &target);
        return;
    }
    const SDL_Point centre = {area.x + area.w / 2 - target.x, area.y + area.h / 2 - target.y};
    SDL_RenderCopyEx(renderer, texture, NULL, &target, quarter * 90.0, &centre, SDL_FLIP_NONE);
}
