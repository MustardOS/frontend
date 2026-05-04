#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int firefly_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void firefly_update(void);

void firefly_render(SDL_Renderer *renderer);

int firefly_active(void);

void firefly_stop(void);

void firefly_shutdown(void);
