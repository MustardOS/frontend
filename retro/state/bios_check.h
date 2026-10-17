#pragma once

#include <stddef.h>

#define BIOS_CHECK_MAX 16

typedef struct {
    char desc[128];
    char filename[64];
    int optional;
    int present;
} bios_entry_t;

int bios_check_scan(const char *core_file_path);

const bios_entry_t *bios_check_get(int index);

int bios_check_missing(const char *core_file_path, char *list, size_t list_size);
