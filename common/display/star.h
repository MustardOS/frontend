#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int star_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void star_update(void);

void star_render(SDL_Renderer *renderer);

int star_active(void);

void star_stop(void);

void star_shutdown(void);
