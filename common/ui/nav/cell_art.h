#pragma once

#include <lvgl/lvgl.h>

void cell_art_apply(lv_obj_t *panel, const char *item, int w, int h, int slant, int focused, int native);

void cell_art_item(lv_obj_t *panel, const char *item);

void cell_art_resize(lv_obj_t *panel, int w, int h);

lv_opa_t cell_opa(const lv_obj_t *panel);

void cell_set_opa(lv_obj_t *panel, lv_opa_t opa);

void cell_art_forget(void);
