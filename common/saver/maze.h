#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int maze_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void maze_update(void);

void maze_render(SDL_Renderer *renderer);

int maze_active(void);

void maze_stop(void);

void maze_shutdown(void);
