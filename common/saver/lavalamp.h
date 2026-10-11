#pragma once

#include <SDL2/SDL.h>

int lavalamp_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void lavalamp_update(void);

void lavalamp_render(SDL_Renderer *renderer);

int lavalamp_active(void);

void lavalamp_stop(void);

void lavalamp_shutdown(void);
