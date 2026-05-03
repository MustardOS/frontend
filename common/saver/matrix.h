#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int matrix_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void matrix_update(void);

void matrix_render(SDL_Renderer *renderer);

int matrix_active(void);

void matrix_stop(void);

void matrix_shutdown(void);
