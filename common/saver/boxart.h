#pragma once

#include <SDL2/SDL.h>

int boxart_init(SDL_Renderer *renderer, const char *catalogue_path, int screen_w, int screen_h);

void boxart_update(void);

void boxart_render(SDL_Renderer *renderer);

int boxart_active(void);

void boxart_stop(void);

void boxart_shutdown(void);
