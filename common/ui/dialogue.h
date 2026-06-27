#pragma once

#include "../../lvgl/lvgl.h"
#include "../theme.h"

#define MUX_DIALOGUE_MAX_OPTIONS 8

typedef struct {
    lv_obj_t *dim;
    lv_obj_t *panel;
    lv_obj_t *title_label;
    lv_obj_t *description_label;
    lv_obj_t *options[MUX_DIALOGUE_MAX_OPTIONS];
    int option_count;
    int selected;
    lv_opa_t dim_alpha;
    struct theme_config *theme;
} mux_dialogue;

typedef enum { mux_unsaved_save = 0, mux_unsaved_discard, mux_unsaved_nope } mux_unsaved_opt;

typedef enum { mux_confirm_yep = 0, mux_confirm_nah, mux_confirm_cnt } mux_confirm_opt;

typedef enum { mux_remove_yep = 0, mux_remove_skip, mux_remove_nah, mux_remove_cnt } mux_remove_opt;

void dialogue_init(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char **options, int option_count, const char *nav_a, const char *nav_b
);

void dialogue_init_unsaved(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *save_label, const char *discard_label, const char *nav_a, const char *nav_b
);

void dialogue_init_confirm(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *confirm_label, const char *cancel_label, const char *nav_a, const char *nav_b
);

void dialogue_init_warn(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *description, const char *nav_a,
    const char *nav_b
);

void dialogue_init_remove(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *description, const char *nav_a,
    const char *nav_b
);

void dialogue_init_message(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *message, const char *nav_b
);

void dialogue_init_accept(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *nav_a
);

void dialogue_show(mux_dialogue *dlg);

void dialogue_hide(const mux_dialogue *dlg);

void dialogue_navigate(mux_dialogue *dlg, struct theme_config *t, int delta);

void dialogue_refresh(const mux_dialogue *dlg, const struct theme_config *t);
