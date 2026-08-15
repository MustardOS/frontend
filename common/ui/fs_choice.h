#pragma once

#include "../../lvgl/lvgl.h"
#include "../theme.h"
#include "dialogue.h"

#define FS_CHOICE_COUNT 3

typedef struct {
    const char *title;
    const char *description;
    const char *no_tooling;
    const char *name[FS_CHOICE_COUNT];
    const char *about[FS_CHOICE_COUNT];
} fs_choice_text;

int fs_choice_open(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const fs_choice_text *text);

void fs_choice_describe(mux_dialogue *dlg, const fs_choice_text *text);

const char *fs_choice_name(int slot);
