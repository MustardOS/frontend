#pragma once

#include <stddef.h>

enum soundfont_state { soundfont_idle = 0, soundfont_loading, soundfont_playing, soundfont_failed };

int soundfont_scan(char ***names, size_t *count);

int soundfont_resolve(const char *name, char *path, size_t path_size);

void soundfont_preview_start(const char *name);

void soundfont_preview_stop(void);

void soundfont_preview_poll(void);

int soundfont_preview_state(void);

const char *soundfont_preview_name(void);
