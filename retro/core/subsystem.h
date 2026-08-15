#pragma once

#include "libretro.h"

#define SUBSYSTEM_MAX       8
#define SUBSYSTEM_ROM_MAX   4
#define SUBSYSTEM_IDENT_LEN 64
#define SUBSYSTEM_DESC_LEN  128
#define SUBSYSTEM_EXT_LEN   128

struct subsystem_rom {
    char desc[SUBSYSTEM_DESC_LEN];
    char valid_extensions[SUBSYSTEM_EXT_LEN];
    int required;
};

struct subsystem_entry {
    char ident[SUBSYSTEM_IDENT_LEN];
    char desc[SUBSYSTEM_DESC_LEN];
    unsigned id;
    int rom_count;
    struct subsystem_rom roms[SUBSYSTEM_ROM_MAX];
};

extern struct subsystem_entry subsystem_list[SUBSYSTEM_MAX];
extern int subsystem_count;

void subsystem_reset(void);

void subsystem_store(const struct retro_subsystem_info *info);

int subsystem_find(const char *ident);

void subsystem_log_resolved(void);

int subsystem_pending_count(void);

int subsystem_select(const char *ident, const char *const *paths, int count);

void subsystem_clear_pending(void);

const char *subsystem_pending_path(int index);

int subsystem_pending_entry(void);
