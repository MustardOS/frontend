#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void render_init(TTF_Font *fonts[8], int cell_w, int cell_h, SDL_Color def_fg, SDL_Color def_bg);

void render_glyph_cache_clear(void);

void render_screen(
    SDL_Renderer *ren, SDL_Texture *target, SDL_Texture *bg_tex, int screen_w, int vis_rows, SDL_Color solid_fg,
    int use_solid_fg, int use_solid_bg, SDL_Color solid_bg, int readonly
);
