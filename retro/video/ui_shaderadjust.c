#include <stdio.h>
#include "../../module/muxshare.h"
#include "colour.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

static const char *row_labels[COLOUR_SHADER_PARAM_MAX + 1];
static const char *row_glyphs[COLOUR_SHADER_PARAM_MAX + 1];
static const char *row_help[COLOUR_SHADER_PARAM_MAX + 1];

static int param_count = 0;

static submenu self;

static int row_reset(void) {
    return param_count;
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    if (index == row_reset()) {
        buf[0] = '\0';
        return;
    }

    colour_shader_param_value_text(index, buf, buf_len);
}

static void cycle_row(const int index, const int direction) {
    colour_shader_param_cycle(index, direction);
}

static int row_is_action(const int index) {
    return index == row_reset();
}

static const char *action_label(const int index) {
    return index == row_reset() ? lang.generic.select : NULL;
}

static void row_action(const int index) {
    if (index != row_reset()) return;

    colour_shader_params_reset();
    submenu_refresh_values(&self);

    lv_obj_invalidate(ui_screen);
}

static int row_coarse_step(const int index) {
    return index == row_reset() ? 0 : 5;
}

static void closed(void) {
    colour_shader_params_save();
    shader_menu_reopen();
}

static submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_is_action = row_is_action,
    .action_label = action_label,
    .action = row_action,
    .row_coarse_step = row_coarse_step,
    .action_without_save_guard = 1,
    .closed = closed,
    .save_title = lang.muxretro.save.display_title,
    .save_desc = lang.muxretro.save.display_desc,
};

void shader_adjust_menu_init(void) {
    submenu_init(&self, &def);
}

void shader_adjust_menu_open(void) {
    param_count = colour_shader_param_count();
    if (param_count <= 0) return;

    for (int i = 0; i < param_count; i++) {
        row_labels[i] = colour_shader_param_label(i);
        row_glyphs[i] = "shader";
        row_help[i] = lang.muxretro.help.shader_adjust.parameter;
    }

    row_labels[param_count] = lang.muxretro.shader_screen.reset;
    row_glyphs[param_count] = "viewportreset";
    row_help[param_count] = lang.muxretro.help.shader_adjust.reset;

    def.row_count = param_count + 1;

    submenu_open(&self);
}

int shader_adjust_menu_is_active(void) {
    return submenu_is_active(&self);
}

void shader_adjust_menu_tick(void) {
    submenu_tick(&self);
}
