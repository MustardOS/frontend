#pragma once

#include <SDL2/SDL.h>

void anim_process(void);

void anim_unload(void);

void anim_set_gradient(SDL_Texture *tex);

void anim_tick(SDL_Renderer *renderer);

int anim_is_active(void);

int anim_is_foreground(void);

int anim_frames_ready(void);
