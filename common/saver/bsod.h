#pragma once

#include <SDL2/SDL.h>

int bsod_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void bsod_update(void);

void bsod_render(SDL_Renderer *renderer);

int bsod_active(void);

void bsod_stop(void);

void bsod_shutdown(void);
