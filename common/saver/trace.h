#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int trace_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void trace_update(void);

void trace_render(SDL_Renderer *renderer);

int trace_active(void);

void trace_stop(void);

void trace_shutdown(void);
