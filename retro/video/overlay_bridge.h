#pragma once

#include <SDL2/SDL.h>

void overlay_bridge_init(const char *core_path_arg, const char *content_path);

void overlay_bridge_apply(void);

int overlay_bridge_active(void);

void overlay_bridge_set_suppressed(int suppressed);

void overlay_bridge_render(SDL_Renderer *renderer, int canvas_w, int canvas_h);

void overlay_bridge_shutdown(void);
