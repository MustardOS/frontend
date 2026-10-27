#pragma once

#include <SDL2/SDL.h>

int blank_init(SDL_Renderer *renderer, int screen_width, int screen_height);

void blank_update(void);

void blank_render(SDL_Renderer *renderer);

int blank_active(void);

void blank_stop(void);

void blank_shutdown(void);
