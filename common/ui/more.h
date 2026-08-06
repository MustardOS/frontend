#pragma once

#include "dialogue.h"

typedef enum {
    more_information = 0,
    more_search,
    more_sort,
    more_filter,
    more_location,
    more_random,
    more_top_level,
    more_collect,
    more_remove,
    more_help,
    more_count
} more_id;

typedef struct {
    more_id id;
    int enabled;
} more_entry;

void more_init(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const more_entry *entries, int count);

more_id more_selected(const mux_dialogue *dlg);

int more_is_enabled(const mux_dialogue *dlg, int index);

typedef struct {
    mux_dialogue dlg;
    int active;
} mux_more;

void more_open(mux_more *m, struct theme_config *t, lv_obj_t *parent, const more_entry *entries, int count);

int more_active(const mux_more *m);

void more_close(mux_more *m);

more_id more_current(const mux_more *m);

int more_dpad(mux_more *m, struct theme_config *t, int direction, int should_fire);

int more_dpad_hold(mux_more *m, struct theme_config *t, int direction, int should_fire);
