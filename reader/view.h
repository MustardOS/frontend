#pragma once

#include <SDL2/SDL.h>
#include "engine/document.h"

typedef struct {
    int filter_active;
    float matrix[9];
    int brightness;
    int contrast;
    int saturation;
    int hue_shift;
    int gamma;
    int sharpness;
} view_colour;

int view_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void view_free(void);

void view_set_rotation(int rotation);

void view_logical_size(int *width, int *height);

void view_compose(const doc_image *image, int scroll_x, int scroll_y, int theme, int plain, const view_colour *colour);

void view_draw(SDL_Renderer *renderer);

int view_save_png(const char *path);

void view_set_transition(int transition);

void view_prepare_transition(int direction);

int view_tick(void);

int view_animating(void);
