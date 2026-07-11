#pragma once

#include <SDL2/SDL.h>

void overlay_bridge_init(const char *core_path_arg, const char *content_path);

void overlay_bridge_apply(void);

void overlay_bridge_render(SDL_Renderer *renderer, int canvas_w, int canvas_h);

void overlay_bridge_shutdown(void);
