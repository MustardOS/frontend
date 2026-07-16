#pragma once

#include <stddef.h>

#define PATCH_MANUAL_MAX 64
#define PATCH_NAME_MAX   256

struct patch_manual_entry {
    char filename[PATCH_NAME_MAX];
    int enabled;
};

extern struct patch_manual_entry patch_manual_list[PATCH_MANUAL_MAX];
extern int patch_manual_count;

void patch_manual_init(const char *core_path_arg, const char *content_path);

void patch_manual_toggle(int index);

int patch_exists(const char *base_path);

int patch_apply(const char *base_path, void **data, size_t *size, char *patch_list, size_t patch_list_size);
