#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "libretro.h"

struct core_cbs {
    bool initialized;
    void *handle;

    bool need_fullpath;
    bool block_extract;
    const char *valid_extensions;

    void (*retro_init)(void);
    void (*retro_deinit)(void);
    unsigned (*retro_api_version)(void);
    void (*retro_get_system_info)(struct retro_system_info *info);
    void (*retro_get_system_av_info)(struct retro_system_av_info *info);
    void (*retro_set_controller_port_device)(unsigned port, unsigned device);
    void (*retro_reset)(void);
    void (*retro_run)(void);
    size_t (*retro_serialize_size)(void);
    bool (*retro_serialize)(void *data, size_t size);
    bool (*retro_unserialize)(const void *data, size_t size);
    bool (*retro_load_game)(const struct retro_game_info *game);
    void (*retro_unload_game)(void);
    void *(*retro_get_memory_data)(unsigned id);
    size_t (*retro_get_memory_size)(unsigned id);
    void (*retro_cheat_reset)(void);
    void (*retro_cheat_set)(unsigned index, bool enabled, const char *code);
};

extern struct core_cbs current_core;

extern char core_content_path[PATH_MAX];

extern char core_file_path[PATH_MAX];

extern char core_content_load_method[32];

extern char core_active_patches[1024];

extern int core_active_patch_count;

int core_open(const char *corefile);

void core_get_name(const char *core_path, char *out, size_t out_size);

int core_load_content(const char *content_path);

void core_unload_content(void);

void core_unload(void);
