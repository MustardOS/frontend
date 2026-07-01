#pragma once

#include <SDL2/SDL.h>

int slideshow_init(SDL_Renderer *renderer, const char *const *dirs, int dir_count, int screen_w, int screen_h);

void slideshow_update(void);

void slideshow_render(SDL_Renderer *renderer);

int slideshow_active(void);

void slideshow_stop(void);

void slideshow_shutdown(void);
