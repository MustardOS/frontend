#pragma once

#include <SDL2/SDL.h>

void vis_fill_circle(SDL_Renderer *renderer, float x, float y, float radius, SDL_Color colour);
void vis_ring(SDL_Renderer *renderer, float x, float y, float inner, float outer, SDL_Color colour);
void vis_sector(
    SDL_Renderer *renderer, float x, float y, float inner, float outer, float start, float end, SDL_Color colour
);
void vis_line(SDL_Renderer *renderer, float x0, float y0, float x1, float y1, float width, SDL_Color colour);
void vis_round_rect(
    SDL_Renderer *renderer, float x, float y, float width, float height, float radius, SDL_Color colour
);
void vis_quad(
    SDL_Renderer *renderer, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
    SDL_Color colour
);
SDL_Color vis_alpha(SDL_Color colour, int alpha);
