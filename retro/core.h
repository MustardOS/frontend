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
};

extern struct core_cbs current_core;

extern char core_content_path[PATH_MAX];

extern char core_content_load_method[32];

int core_open(const char *corefile);

int core_load_content(const char *content_path);

void core_unload_content(void);

void core_unload(void);
