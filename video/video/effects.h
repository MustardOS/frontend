#pragma once

#include <SDL2/SDL.h>
#include <stddef.h>

int video_effects_render(
    SDL_Renderer *renderer, SDL_Texture *source, SDL_Texture *next, Uint8 next_alpha,
    const SDL_Rect *source_rect, const SDL_Rect *destination, double rotation, SDL_RendererFlip flip
);

void video_effects_changed(void);
void video_effects_close(void);
int video_effects_parameter_count(void);
const char *video_effects_parameter_label(int index);
void video_effects_parameter_value(int index, char *value, size_t size);
int video_effects_parameter_cycle(int index, int direction);
void video_effects_parameters_reset(void);
