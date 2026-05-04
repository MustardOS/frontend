#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

int pulse_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void pulse_update(void);

void pulse_render(SDL_Renderer *renderer);

int pulse_active(void);

void pulse_stop(void);

void pulse_shutdown(void);
