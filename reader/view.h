#pragma once

#include <SDL2/SDL.h>
#include "engine/document.h"

typedef struct {
    int top;
    const char *top_left;
    const char *top_right;
    const char *left;
    const char *centre;
    const char *right;
} view_status;

int view_init(SDL_Renderer *renderer, int screen_w, int screen_h);

void view_free(void);

void view_set_rotation(int rotation);

void view_logical_size(int *width, int *height);

int view_status_height(void);

void view_compose(
    const doc_image *image, int scroll_x, int scroll_y, int theme, int plain, int contrast, const view_status *status
);

void view_draw(SDL_Renderer *renderer);

void view_set_transition(int transition);

void view_prepare_transition(int direction);

int view_tick(void);

int view_animating(void);
