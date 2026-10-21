#pragma once

#include <stdint.h>
#include <SDL2/SDL.h>

typedef void (*crt_effect_draw_fn)(SDL_Renderer *renderer, const SDL_Rect *rect, void *userdata);

uint32_t crt_effect_power_off_duration(void);

void crt_effect_render_power_off(
    SDL_Renderer *renderer, const SDL_Rect *frame, uint32_t started, uint32_t now, crt_effect_draw_fn draw,
    void *userdata
);
