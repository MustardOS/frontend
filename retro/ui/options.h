#pragma once

#include <stdbool.h>
#include "../core/libretro.h"

#define OPTIONS_MAX            64
#define OPTIONS_MAX_VALUES     32
#define OPTIONS_MAX_CATEGORIES 32

struct core_option_entry {
    char key[64];
    char label[128];
    char category_key[64];
    char values[OPTIONS_MAX_VALUES][64];
    int value_count;
    int current_index;
};

struct core_option_category {
    char key[64];
    char label[128];
};

extern struct core_option_entry options_list[OPTIONS_MAX];
extern int options_count;
extern struct core_option_category options_categories[OPTIONS_MAX_CATEGORIES];
extern int options_category_count;
extern bool options_dirty;

void options_reset(void);

void options_store_v1(const struct retro_core_option_definition *defs);

void options_store_v2(const struct retro_core_options_v2 *opts);

void options_store_legacy(const struct retro_variable *vars);

const char *options_get_value(const char *key);

void options_cycle(int index, int direction);

void options_init_paths(const char *core_path_arg, const char *content_path);

void options_capture_baseline(void);

int options_is_dirty(void);

void options_discard(void);

void options_save_content(void);

void options_save_core(void);

void options_save_directory(void);
