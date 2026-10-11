#pragma once

#include <SDL2/SDL.h>

int glitch_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void glitch_update(void);

void glitch_render(SDL_Renderer *renderer);

int glitch_active(void);

void glitch_stop(void);

void glitch_shutdown(void);
