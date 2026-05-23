#pragma once

#include "../lvgl/lvgl.h"
#include "theme.h"

#define MUX_DIALOGUE_MAX_OPTIONS 8

typedef struct {
    lv_obj_t *dim;
    lv_obj_t *panel;
    lv_obj_t *title_label;
    lv_obj_t *options[MUX_DIALOGUE_MAX_OPTIONS];
    int option_count;
    int selected;
} mux_dialogue;

void dialogue_init(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                   const char *title, const char **options, int option_count,
                   const char *nav_a, const char *nav_b);

void dialogue_show(mux_dialogue *dlg);

void dialogue_hide(mux_dialogue *dlg);

void dialogue_navigate(mux_dialogue *dlg, struct theme_config *t, int delta);

void dialogue_refresh(mux_dialogue *dlg, struct theme_config *t);
