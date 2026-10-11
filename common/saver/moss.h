#pragma once

#include <SDL2/SDL.h>

int moss_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void moss_update(void);

void moss_render(SDL_Renderer *renderer);

int moss_active(void);

void moss_stop(void);

void moss_shutdown(void);
