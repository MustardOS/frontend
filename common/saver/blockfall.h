#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int blockfall_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void blockfall_update(void);

void blockfall_render(SDL_Renderer *renderer);

int blockfall_active(void);

void blockfall_stop(void);

void blockfall_shutdown(void);
