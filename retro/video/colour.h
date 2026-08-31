#pragma once

#include <stddef.h>
#include <SDL2/SDL.h>

#define COLOUR_FILTER_MAX       32
#define COLOUR_SHADER_MAX       64
#define COLOUR_SHADER_PARAM_MAX 16

void colour_init(void);

int colour_filter_preset_count(void);

const char *colour_filter_preset_label(int index);

int colour_shader_count(void);

const char *colour_shader_label(int index);

void colour_refresh(void);

int colour_pass_needed(void);

void colour_set_suppressed(int suppressed);

void colour_render_pass(SDL_Renderer *renderer, SDL_Texture *tex, const SDL_Rect *src_rect, const SDL_Rect *dest_rect);

int colour_shader_param_count(void);

const char *colour_shader_param_label(int index);

void colour_shader_param_value_text(int index, char *buf, size_t len);

void colour_shader_param_cycle(int index, int direction);

void colour_shader_params_reset(void);

void colour_shader_params_save(void);

