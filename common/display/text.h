#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void sdl_text_die(const char *msg);

float sdl_smoothstep01(float t);

SDL_Surface *sdl_render_text_surface(TTF_Font *font, const char *text, SDL_Color col, int shadow_offset);

SDL_Surface *
sdl_render_text_wrapped_surface(TTF_Font *font, const char *text, SDL_Color col, int wrap_w, int shadow_offset);

SDL_Texture *sdl_render_text(
    SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color col, int shadow_offset, int *out_w, int *out_h
);

SDL_Texture *sdl_render_text_wrapped(
    SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color col, int wrap_w, int shadow_offset, int *out_w,
    int *out_h
);
