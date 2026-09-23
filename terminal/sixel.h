#pragma once

#include <stddef.h>
#include <SDL2/SDL.h>

void sixel_decode(const char *data, size_t len, int cell_x, int cell_y);

int sixel_count(void);

const Uint32 *sixel_get(int idx, int *out_x, int *out_y, int *out_w, int *out_h);

const Uint32 *sixel_pixels(int *out_x, int *out_y, int *out_w, int *out_h);

void sixel_free(void);

void sixel_scroll(int lines);

void sixel_set_cell_h(int cell_h);

int sixel_cell_h(void);

void sixel_set_cell_w(int cell_w);

int sixel_cell_w(void);

void sixel_set_cell_y(int new_y);
