#pragma once

#include <stddef.h>
#include <common/base/options.h>
#include <common/display/theme.h>

#define COLOUR_SCHEME_PRESETS OPT_SHARE_PATH "colour"
#define COLOUR_SCHEME_USER    RUN_STORAGE_PATH "colour"
#define COLOUR_SCHEME_NAME    64
#define COLOUR_VALUE_SIZE     32

typedef struct {
    char name[COLOUR_SCHEME_NAME];
    char path[MAX_BUFFER_SIZE];
    int user;
    int folder;
} colour_scheme;

int colour_scheme_read(const char *path, char values[colour_role_count][COLOUR_VALUE_SIZE]);

int colour_scheme_scan(const char *dir, int user, int folders, colour_scheme *out, int max);

int colour_scheme_names(char names[][MAX_BUFFER_SIZE], int max);

int colour_scheme_path(const char *name, char *path, size_t size);
