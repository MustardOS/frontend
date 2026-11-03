#pragma once

#include <lvgl/lvgl.h>

typedef struct {
    int found;
    int x, y, w, h, radius, opacity, angle;
    int rotate_x, rotate_y, depth, perspective, reflect;
    int label_below, label_width, label_gap;
} grid_layout_entry;

const char *grid_layout_json(void);

grid_layout_entry find_grid_layout(int item_index, int panel_index);

void place_grid_layout(lv_obj_t *cell_panel, const grid_layout_entry *entry, int step);
