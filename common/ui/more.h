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
    more_overview,
    more_launch_count,
    more_duration,
    more_remove,
    more_help,
    more_count
} more_id;

typedef struct {
    more_id id;
    int enabled;
} more_entry;

typedef struct {
    mux_dialogue dlg;
} mux_more;

void more_open(mux_more *m, struct theme_config *t, lv_obj_t *parent, const more_entry *entries, int count);

void more_focus_last(mux_more *m);

int more_active(const mux_more *m);

more_id more_peek(const mux_more *m);

more_id more_take(mux_more *m, int chains);

void more_cancel(mux_more *m);

int more_dpad(mux_more *m, struct theme_config *t, int direction, int should_fire);

int more_dpad_hold(mux_more *m, struct theme_config *t, int direction, int should_fire);
