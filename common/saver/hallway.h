#pragma once

#include <SDL2/SDL.h>

int hallway_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void hallway_update(void);

void hallway_render(SDL_Renderer *renderer);

int hallway_active(void);

void hallway_stop(void);

void hallway_shutdown(void);
