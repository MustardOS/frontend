#pragma once

#include <stddef.h>
#include <stdint.h>
#include <common/base/options.h>
#include "../core/paths.h"
#include <stdio.h>
#include <SDL2/SDL.h>

#define COLOUR_SHADER_PARAM_MAX 16
#define COLOUR_PRESET_FILE_MAX  (128 * 1024)
#define COLOUR_FILTER_DIR       OPT_SHARE_PATH "filter/"
#define COLOUR_SHADER_DIR       OPT_SHARE_PATH "shader/"
#define COLOUR_FILTER_USER_DIR  RETRO_SHARE_PATH "filter/"
#define COLOUR_SHADER_USER_DIR  RETRO_SHARE_PATH "shader/"

int colour_filter_path(const char *stem, char *out, size_t out_size);

int colour_shader_path(const char *stem, char *out, size_t out_size);

enum colour_shader_cost {
    colour_shader_cost_unknown = 0,
    colour_shader_cost_low,
    colour_shader_cost_medium,
    colour_shader_cost_high
};

enum colour_shader_compatibility {
    colour_shader_compatibility_all = 0,
    colour_shader_compatibility_software,
    colour_shader_compatibility_hardware
};

void colour_init(void);

int colour_filter_preset_count(void);

const char *colour_filter_preset_label(int index);

const char *colour_filter_preset_key(int index);

int colour_filter_preset_index(const char *key);

int colour_filter_file_valid(const char *path);

int colour_shader_count(void);

const char *colour_shader_label(int index);

const char *colour_shader_key(int index);

int colour_shader_index(const char *key);

int colour_shader_file_valid(const char *path);

int colour_filter_is_user(int index);

int colour_shader_is_user(int index);

int colour_filter_delete(int index);

int colour_shader_delete(int index);

enum colour_shader_cost colour_shader_cost_for_output(int index, int width, int height);

enum colour_shader_compatibility colour_shader_compatibility_for_index(int index);

void colour_refresh(void);

int colour_pass_needed(void);

void colour_set_suppressed(int suppressed);

void colour_render_pass(SDL_Renderer *renderer, SDL_Texture *tex, const SDL_Rect *src_rect, const SDL_Rect *dest_rect);

void colour_render_pass_area_scaled(
    SDL_Renderer *renderer, SDL_Texture *tex, const SDL_Rect *src_rect, const SDL_Rect *dest_rect
);

int colour_shader_param_count(void);

const char *colour_shader_param_label(int index);

void colour_shader_param_value_text(int index, char *buf, size_t len);

void colour_shader_param_cycle(int index, int direction);

void colour_shader_params_reset(void);

void colour_shader_params_save(void);

void colour_shader_export_contract(FILE *stream);

unsigned colour_shader_render_operations(void);

uint64_t colour_shader_processed_pixels(void);
