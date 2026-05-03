#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int constellation_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void constellation_update(void);

void constellation_render(SDL_Renderer *renderer);

int constellation_active(void);

void constellation_stop(void);

void constellation_shutdown(void);
