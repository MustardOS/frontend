#pragma once

#include <stdint.h>
#include <SDL2/SDL.h>

int datetime_init(SDL_Renderer *renderer, int screen_w, int screen_h, uint8_t col_r, uint8_t col_g, uint8_t col_b, uint8_t col_a);

void datetime_update(void);

void datetime_render(SDL_Renderer *renderer);

int datetime_active(void);

void datetime_stop(void);

void datetime_shutdown(void);
