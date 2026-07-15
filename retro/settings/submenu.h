#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../../common/ui/dialogue.h"
#include "../input/nav_repeat.h"
#include "settings.h"

typedef struct {
    const char *const *labels;
    const char *const *glyphs;

    int row_count;

    void (*value_text)(int index, char *buf, size_t len);
    void (*cycle)(int index, int direction);
    int (*row_is_action)(int index);
    void (*action)(int index);
    int (*child_tick)(void);
    void (*closed)(void);

    const char *save_title;
    const char *save_desc;
} submenu_def;

typedef struct {
    const submenu_def *def;
    int active;
    uint64_t prev_nav_mask;
    nav_repeat_t rpt_up, rpt_down, rpt_left, rpt_right;
    int nav_row_class;
    int save_dialogue_active;
    mux_dialogue save_dlg;
    struct session_settings_t entry_snapshot;
    int pending_action_row;
} submenu;

void submenu_init(submenu *m, const submenu_def *def);

void submenu_open(submenu *m);

void submenu_reopen_at(submenu *m, int row);

int submenu_is_active(const submenu *m);

void submenu_tick(submenu *m);

void submenu_refresh_values(const submenu *m);

void submenu_stack_resync(void);
