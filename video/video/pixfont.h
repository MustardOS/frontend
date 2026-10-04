#pragma once

#include <SDL2/SDL.h>

int pixfont_width(const char *text, int scale);
void pixfont_draw(SDL_Renderer *renderer, const char *text, int x, int y, int scale);
