#pragma once

#include <SDL2/SDL.h>

void video_geometry_snap_integer(SDL_Rect *rect, int source_w, int source_h);

int video_geometry_is_integer(const SDL_Rect *rect, int source_w, int source_h);
