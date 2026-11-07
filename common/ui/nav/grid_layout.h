#pragma once

#include <lvgl/lvgl.h>

enum { layout_shape_hexagon = 1 };

typedef struct {
    int found;
    int x, y, w, h, radius, opacity, angle, glyph;
    int rotate_x, rotate_y, depth, perspective, reflect;
    int label_below, label_none, label_beside, label_width, label_gap, hexagon, text_size, art, slant, text_mode;
    int text_x, text_y, text_angle, text_bg;
    int shadow, border;
} grid_layout_entry;

const char *grid_layout_json(void);

int grid_layout_full_names(void);

grid_layout_entry find_grid_layout(int item_index, int panel_index);

void grid_layout_forget(void);

void place_grid_layout(lv_obj_t *cell_panel, const grid_layout_entry *entry, int step);
