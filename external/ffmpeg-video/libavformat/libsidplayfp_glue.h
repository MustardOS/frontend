#ifndef AVFORMAT_LIBSIDPLAYFP_GLUE_H
#define AVFORMAT_LIBSIDPLAYFP_GLUE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SidplayGlue SidplayGlue;

typedef struct SidplayGlueInfo {
    unsigned int songs;
    unsigned int song;
    int chips;
    int64_t length_ms;
    const char *title;
    const char *author;
    const char *released;
    const char *format;
    const char *model;
} SidplayGlueInfo;

SidplayGlue *sidplay_glue_open(
    const char *path, unsigned int song, unsigned int sample_rate, const char *database, char *error, size_t error_size
);
void sidplay_glue_close(SidplayGlue *glue);
void sidplay_glue_info(const SidplayGlue *glue, SidplayGlueInfo *info);
int sidplay_glue_render(SidplayGlue *glue, int16_t *output, int frames);
int sidplay_glue_restart(SidplayGlue *glue);
int64_t sidplay_glue_skip(SidplayGlue *glue, int64_t frames, int (*interrupted)(void *), void *opaque);

#ifdef __cplusplus
}
#endif

#endif
