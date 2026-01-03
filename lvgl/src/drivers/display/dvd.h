#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int dvd_init(SDL_Renderer *renderer, const char *png_path, int screen_w, int screen_h);

void dvd_update(void);

void dvd_render(SDL_Renderer *renderer);

int dvd_active(void);

void dvd_shutdown(void);
