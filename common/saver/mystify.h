#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int mystify_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void mystify_update(void);

void mystify_render(SDL_Renderer *renderer);

int mystify_active(void);

void mystify_stop(void);

void mystify_shutdown(void);
