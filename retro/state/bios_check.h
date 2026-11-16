#pragma once

#include <stddef.h>

#define BIOS_CHECK_MAX        32
#define BIOS_CHECK_SHOWN      4
#define BIOS_CHECK_SYSTEM_ENV "PICKLES_SYSTEM"

typedef struct {
    char filename[128];
    int optional;
    int present;
} bios_entry_t;

int bios_check_scan(const char *core_file_path);

const bios_entry_t *bios_check_get(int index);

int bios_check_missing(const char *core_file_path, char *list, size_t list_size);

int bios_check_ignored(const char *core_file_path);

void bios_check_ignore(const char *core_file_path);
