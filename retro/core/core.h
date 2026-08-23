#pragma once

#include <limits.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "libretro.h"

struct core_cbs {
    bool initialised;
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
    bool (*retro_load_game_special)(unsigned type, const struct retro_game_info *info, size_t num_info);
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

extern char core_resolved_content_path[PATH_MAX];

extern char core_active_patches[1024];

extern int core_active_patch_count;

extern int core_restart_requested;

int core_load_subsystem_content(void);

int core_open(const char *corefile);

int core_get_name(const char *core_path, char *out, size_t out_size);

int core_content_rel_dir(const char *content_path, char *out, size_t out_size);

int core_content_save_prefix(const char *core_path_arg, const char *content_path, char *out, size_t out_size);

int core_load_content(const char *content_path);

int core_cached_system_info(struct retro_system_info *info);

int core_cached_system_av_info(struct retro_system_av_info *info);

int core_cached_api_version(unsigned *version);

void core_cache_system_av_info(const struct retro_system_av_info *info);

void core_cache_save_memory_size(size_t size);

size_t core_cached_save_memory_size(void);

void core_cache_disc_count(int count);

int core_cached_disc_count(void);

void core_prepare_content_unload(void);

void core_unload_content(void);

void core_unload(void);
