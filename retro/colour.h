#pragma once

#include <SDL2/SDL.h>

#define COLOUR_FILTER_MAX 32
#define COLOUR_SHADER_MAX 64

void colour_init(void);

int colour_filter_preset_count(void);

const char *colour_filter_preset_name(int index);

const char *colour_filter_preset_label(int index);

int colour_shader_count(void);

const char *colour_shader_name(int index);

const char *colour_shader_label(int index);

void colour_refresh(void);

int colour_pass_needed(void);

void colour_render_pass(SDL_Renderer *renderer, SDL_Texture *tex, const SDL_Rect *dest_rect);
