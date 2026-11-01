#include <stdio.h>
#include <common/platform/input.h>
#include <common/ui/common.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

static const char *row_labels[PORT_SOURCE_COUNT] = {
    lang.muxretro.settings_screen.target_a,         lang.muxretro.settings_screen.target_b,
    lang.muxretro.settings_screen.target_x,         lang.muxretro.settings_screen.target_y,
    lang.muxretro.settings_screen.target_l1,        lang.muxretro.settings_screen.target_r1,
    lang.muxretro.settings_screen.target_l2,        lang.muxretro.settings_screen.target_r2,
    lang.muxretro.settings_screen.target_l3,        lang.muxretro.settings_screen.target_r3,
    lang.muxretro.settings_screen.target_select,    lang.muxretro.settings_screen.target_start,
    lang.muxretro.settings_screen.target_dpad_up,   lang.muxretro.settings_screen.target_dpad_down,
    lang.muxretro.settings_screen.target_dpad_left, lang.muxretro.settings_screen.target_dpad_right,
    lang.muxretro.settings_screen.stick_ls_up,      lang.muxretro.settings_screen.stick_ls_down,
    lang.muxretro.settings_screen.stick_ls_left,    lang.muxretro.settings_screen.stick_ls_right,
    lang.muxretro.settings_screen.stick_rs_up,      lang.muxretro.settings_screen.stick_rs_down,
    lang.muxretro.settings_screen.stick_rs_left,    lang.muxretro.settings_screen.stick_rs_right,
};

// TODO: Make these individual or at least a bit different one day, too tired today!
static const char *row_glyphs[PORT_SOURCE_COUNT] = {
    "controller", "controller", "controller", "controller", "controller", "controller", "controller", "controller",
    "controller", "controller", "controller", "controller", "controller", "controller", "controller", "controller",
    "controller", "controller", "controller", "controller", "controller", "controller", "controller", "controller",
};

#define GRID_COLS 4

#define CELL_RESET   (-1)
#define CELL_UNBOUND (-2)

static const int grid_cells[] = {
    0, 1, 2, 3, 12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 20, 21, 22, 23, CELL_RESET, CELL_UNBOUND,
};

#define GRID_CELL_COUNT ((int) (sizeof(grid_cells) / sizeof(grid_cells[0])))
#define GRID_ROWS_MAX   (PORT_TARGET_COUNT / GRID_COLS + 1)
#define GRID_MAP_SIZE   (GRID_CELL_COUNT + GRID_ROWS_MAX + 1)

static const char *grid_map[GRID_MAP_SIZE];
static int picker_cells[GRID_CELL_COUNT];
static int picker_cell_count = 0;
static int picker_full_count = 0;
static int picker_core = 0;
static lv_obj_t *picker_grid = NULL;
static int grid_index = 0;

static int picker_active = 0;
static int picker_row = -1;
static uint64_t picker_prev_mask = 0;

static nav_repeat_t rpt_pick_up = {0};
static nav_repeat_t rpt_pick_down = {0};
static nav_repeat_t rpt_pick_left = {0};
static nav_repeat_t rpt_pick_right = {0};

static int active_port = 0;
static int device_row_count = PORT_SOURCE_COUNT;
static int core_row_count = 0;

static const char *bm_labels[PORT_SOURCE_COUNT + PORT_TARGET_COUNT];
static const char *bm_glyphs[PORT_SOURCE_COUNT + PORT_TARGET_COUNT];
static char core_labels[PORT_TARGET_COUNT][MAX_BUFFER_SIZE];
static int core_row_target[PORT_TARGET_COUNT];
static list_frame bm_frames[2];

static nav_repeat_t rpt_turbo_left = {0};
static nav_repeat_t rpt_turbo_right = {0};
static uint64_t turbo_cycle_prev_mask = 0;

static submenu bm_self[MUX_INPUT_PORT_COUNT];

static int is_device_row(const int index) {
    return index >= 0 && index < device_row_count;
}

static int is_core_row(const int index) {
    return index >= device_row_count && index < device_row_count + core_row_count;
}

static int core_row_target_of(const int index) {
    return is_core_row(index) ? core_row_target[index - device_row_count] : -1;
}

static int core_bound(const int source, const int target) {
    return session_settings_source_macro(active_port)[source] < 0
           && session_settings_source_target(active_port)[source] == target;
}

static int core_first_bound(const int target) {
    for (int source = 0; source < device_row_count; source++) {
        if (core_bound(source, target)) return source;
    }

    return -1;
}

static void core_sync_turbo(const int target, const int from) {
    const int rate = session_settings_source_turbo(active_port)[from];

    for (int source = 0; source < device_row_count; source++) {
        if (source != from && core_bound(source, target)) session_settings_set_source_turbo(active_port, source, rate);
    }
}

static void core_row_value(const int index, char *buf, const size_t buf_len) {
    const int target = core_row_target[index - device_row_count];
    const int *turbo = session_settings_source_turbo(active_port);

    size_t used = 0;
    buf[0] = '\0';

    for (int source = 0; source < device_row_count && used < buf_len; source++) {
        if (!core_bound(source, target)) continue;

        const char *gap = used ? ", " : "";
        const int written = turbo[source] > 0 ? snprintf(
                                                    buf + used, buf_len - used, "%s%s (%s)", gap, row_labels[source],
                                                    session_settings_turbo_rate_name(turbo[source])
                                                )
                                              : snprintf(buf + used, buf_len - used, "%s%s", gap, row_labels[source]);
        if (written < 0) break;
        used += (size_t) written;
    }

    if (!used) snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.unbound);
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    if (is_device_row(index)) {
        session_settings_source_value(active_port, index, buf, buf_len);
    } else if (index >= device_row_count && index < device_row_count + core_row_count) {
        core_row_value(index, buf, buf_len);
    } else if (buf_len) {
        buf[0] = '\0';
    }
}

static int row_is_action(const int index) {
    return is_device_row(index) || is_core_row(index);
}

static void picker_open(int row);

static int turbo_style_source(const int index) {
    if (is_core_row(index)) return core_first_bound(core_row_target_of(index));
    if (!is_device_row(index)) return -1;
    if (session_settings_source_macro(active_port)[index] >= 0) return -1;

    return session_settings_source_target(active_port)[index] >= 0 ? index : -1;
}

static int turbo_style_available(const int index) {
    return turbo_style_source(index) >= 0;
}

static const char *row_y_label(const int index) {
    return turbo_style_available(index) ? lang.muxretro.settings_screen.turbo_style : NULL;
}

static void row_y_action(const int index) {
    if (!turbo_style_available(index)) return;

    const int source = turbo_style_source(index);
    const int frames = session_settings_switch_source_turbo_style(active_port, source);
    if (is_core_row(index)) core_sync_turbo(core_row_target_of(index), source);

    pause_menu_show_toast(
        frames ? lang.muxretro.settings_screen.turbo_style_frames : lang.muxretro.settings_screen.turbo_style_time
    );
    submenu_refresh_values(&bm_self[active_port]);
}

static void row_action(const int index) {
    if (!row_is_action(index)) return;

    picker_open(index);
}

static void closed(void) {
    lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

    input_port_menu_reopen_button_mapping(active_port);
}

static submenu_def bm_def = {
    .labels = bm_labels,
    .glyphs = bm_glyphs,
    .help_title = lang.muxretro.settings_screen.button_mapping,
    .help_all = lang.muxretro.help.screen.button_mapping,
    .row_count = PORT_SOURCE_COUNT,
    .frames = bm_frames,
    .value_text = row_value_text,
    .row_is_action = row_is_action,
    .action = row_action,
    .y_label = row_y_label,
    .y_action = row_y_action,
    .closed = closed,
    .save_title = lang.muxretro.save.button_mapping_title,
    .save_desc = lang.muxretro.save.button_mapping_desc,
};

static void build_rows(void) {
    for (int source = 0; source < device_row_count; source++) {
        bm_labels[source] = row_labels[source];
        bm_glyphs[source] = row_glyphs[source];
    }

    core_row_count = 0;

    for (int position = 0; position < PORT_TARGET_COUNT; position++) {
        const int target = session_settings_target_at_position(position);
        const char *label = session_settings_core_target_label(active_port, target);
        if (!label) continue;

        const int row = device_row_count + core_row_count;
        snprintf(core_labels[core_row_count], sizeof(core_labels[core_row_count]), "%s", label);
        core_row_target[core_row_count] = target;
        bm_labels[row] = core_labels[core_row_count];
        bm_glyphs[row] = "controller";
        core_row_count++;
    }

    bm_frames[0] = (list_frame) {lang.muxretro.settings_screen.core_mapping, device_row_count, core_row_count};
    bm_frames[1] = (list_frame) {lang.muxretro.settings_screen.device_mapping, 0, device_row_count};

    bm_def.row_count = device_row_count + core_row_count;
    bm_def.frame_count = core_row_count > 0 ? 2 : 0;
}

static int focused_row(void) {
    if (bm_def.frame_count > 0) return list_frame_current_row();
    return current_item_index;
}

static int turbo_available(void) {
    const int row = focused_row();
    if (is_core_row(row)) return core_first_bound(core_row_target_of(row)) >= 0;
    if (!is_device_row(row)) return 0;
    if (session_settings_source_macro(active_port)[row] >= 0) return 1;

    return session_settings_source_target(active_port)[row] >= 0;
}

static int nav_state(void) {
    if (focused_row() < 0) return -1;

    return turbo_available() ? 2 : 1;
}

static int nav_state_shown = -2;

static void apply_nav_bar(void) {
    nav_state_shown = nav_state();

    if (nav_state_shown < 0) {
        submenu_refresh_nav(&bm_self[active_port]);
        return;
    }

    if (nav_state_shown == 2) {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.muxretro.settings_screen.turbo_modes, 0},
                                      {ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.set, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    } else {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.set, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
        nav_show_lr(0);
    }

    pause_menu_fix_nav_order();
}

static const char *cell_label(const int cell) {
    switch (cell) {
        case CELL_RESET:
            return lang.generic.reset;
        case CELL_UNBOUND:
            return lang.muxretro.settings_screen.unbound;
        default:
            if (picker_core) return row_labels[cell];
            return session_settings_target_label(session_settings_target_at_position(cell));
    }
}

static int picker_last_count(void) {
    return picker_cell_count - picker_full_count;
}

static int picker_full_rows(void) {
    return picker_full_count / GRID_COLS;
}

static int cell_row(const int index) {
    return index < picker_full_count ? index / GRID_COLS : picker_full_rows();
}

static int cell_col(const int index) {
    return index < picker_full_count ? index % GRID_COLS : index - picker_full_count;
}

static void picker_build_cells(void) {
    picker_cell_count = 0;

    for (int index = 0; index < GRID_CELL_COUNT; index++) {
        const int cell = grid_cells[index];

        if (picker_core && cell >= device_row_count) continue;

        picker_cells[picker_cell_count++] = cell;
    }

    picker_full_count = 0;
    while (picker_full_count < picker_cell_count && picker_cells[picker_full_count] >= 0)
        picker_full_count++;
}

static void picker_build_map(void) {
    int slot = 0;

    for (int index = 0; index < picker_cell_count; index++) {
        grid_map[slot++] = cell_label(picker_cells[index]);
        if (index < picker_full_count && (index + 1) % GRID_COLS == 0) grid_map[slot++] = "\n";
    }

    grid_map[slot] = "";
}

static void picker_select(const int index) {
    if (index < 0 || index >= picker_cell_count) return;

    grid_index = index;

    if (!picker_grid || !lv_obj_is_valid(picker_grid)) return;

    lv_btnmatrix_set_selected_btn(picker_grid, (uint16_t) grid_index);
    lv_btnmatrix_set_btn_ctrl(picker_grid, (uint16_t) grid_index, LV_BTNMATRIX_CTRL_CHECKED);
}

static void picker_move(const int dx, const int dy) {
    const int full_rows = picker_full_rows();
    const int last_count = picker_last_count();
    const int rows = full_rows + 1;
    const int row = cell_row(grid_index);
    const int col = cell_col(grid_index);
    const int in_last = row == full_rows;
    const int width = in_last ? last_count : GRID_COLS;

    if (dx) {
        const int next = ((col + dx) % width + width) % width;
        picker_select(in_last ? picker_full_count + next : row * GRID_COLS + next);
        return;
    }

    if (!dy) return;

    const int next_row = ((row + dy) % rows + rows) % rows;

    if (next_row == full_rows) {
        picker_select(picker_full_count + col * last_count / GRID_COLS);
        return;
    }

    picker_select(next_row * GRID_COLS + (in_last ? col * GRID_COLS / last_count : col));
}

static void picker_nav_bar(void) {
    nav_show_lr(0);

    lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static int core_picker_focus(const int target) {
    const int *targets = session_settings_source_target(active_port);
    const int *macros = session_settings_source_macro(active_port);

    for (int index = 0; index < picker_cell_count; index++) {
        const int source = picker_cells[index];
        if (source >= 0 && macros[source] < 0 && targets[source] == target) return index;
    }

    return 0;
}

static int device_picker_focus(const int row) {
    const int position = session_settings_target_position(session_settings_source_target(active_port)[row]);
    const int wanted = position < 0 ? CELL_UNBOUND : position;

    for (int index = 0; index < picker_cell_count; index++) {
        if (picker_cells[index] == wanted) return index;
    }

    return 0;
}

static void picker_open(const int row) {
    picker_row = row;
    picker_core = is_core_row(row);
    picker_active = 1;

    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    picker_build_cells();
    picker_build_map();

    picker_grid = lv_btnmatrix_create(ui_pnl_content);

    lv_obj_set_width(picker_grid, lv_pct(92));
    lv_obj_set_height(picker_grid, lv_pct(96));

    lv_btnmatrix_set_one_checked(picker_grid, 1);
    lv_btnmatrix_set_map(picker_grid, grid_map);
    lv_obj_clear_flag(picker_grid, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(picker_grid, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(picker_grid, LV_ALIGN_CENTER, 0, 0);

    apply_osk_theme(picker_grid);

    lv_shadow_zone_register(
        picker_grid, lv_color_hex(theme.osk.item.shadow_colour), (lv_opa_t) theme.osk.item.shadow_alpha,
        (int8_t) theme.osk.item.shadow_x_offset, (int8_t) theme.osk.item.shadow_y_offset,
        lv_color_hex(theme.osk.item.shadow_colour_focus), (lv_opa_t) theme.osk.item.shadow_alpha_focus,
        (int8_t) theme.osk.item.shadow_x_offset_focus, (int8_t) theme.osk.item.shadow_y_offset_focus
    );

    grid_index = picker_core ? core_picker_focus(core_row_target_of(row)) : device_picker_focus(row);
    picker_select(grid_index);

    picker_prev_mask = nav_mask_standard();
    picker_nav_bar();
    pause_menu_sync_input_mask();
}

static void picker_close(void) {
    picker_active = 0;

    if (picker_grid && lv_obj_is_valid(picker_grid)) lv_shadow_zone_unregister(picker_grid);
    picker_grid = NULL;
    submenu_reopen_at(&bm_self[active_port], picker_row);
    apply_nav_bar();

    turbo_cycle_prev_mask = nav_mask_standard();
}

static void core_picker_choose(const int cell) {
    const int target = core_row_target_of(picker_row);

    if (cell >= 0) {
        const int group = core_first_bound(target);
        if (core_bound(cell, target)) return;

        session_settings_set_source_target(active_port, cell, target);
        session_settings_set_source_turbo(
            active_port, cell, group >= 0 ? session_settings_source_turbo(active_port)[group] : 0
        );
        return;
    }

    for (int source = 0; source < device_row_count; source++) {
        const int is_default = session_settings_default_source_target(source) == target;

        if (cell == CELL_RESET && is_default) {
            session_settings_reset_source(active_port, source);
        } else if (core_bound(source, target)) {
            session_settings_unbind_source(active_port, source);
        }
    }
}

static void picker_tick(void) {
    const uint64_t mask = nav_mask_standard();
    const uint64_t edge = mask & ~picker_prev_mask;
    picker_prev_mask = mask;

    if (nav_input_halted()) return;

    const uint32_t now = SDL_GetTicks();
    const int do_up = nav_repeat_step(&rpt_pick_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int do_down = nav_repeat_step(&rpt_pick_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int do_left = nav_repeat_step(&rpt_pick_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_pick_right, edge & BIT(3), mask & BIT(3), 1, now);

    if (do_up || do_down || do_left || do_right) {
        play_sound(snd_navigate);
        picker_move(do_right - do_left, do_down - do_up);
    } else if (edge & BIT(4)) {
        const int cell = picker_cells[grid_index];

        if (picker_core) {
            play_sound(snd_confirm);
            core_picker_choose(cell);
        } else if (cell == CELL_RESET) {
            play_sound(snd_option);
            session_settings_reset_source(active_port, picker_row);
        } else {
            play_sound(snd_confirm);

            const int target = cell == CELL_UNBOUND ? -1 : session_settings_target_at_position(cell);
            session_settings_set_source_target(active_port, picker_row, target);
        }

        picker_close();
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        picker_close();
    }
}

static void check_turbo_cycle(void) {
    const uint64_t nav_mask = nav_mask_standard();
    const uint64_t nav_edge = nav_mask & ~turbo_cycle_prev_mask;
    turbo_cycle_prev_mask = nav_mask;

    const uint32_t now = SDL_GetTicks();
    const int do_left = nav_repeat_step(&rpt_turbo_left, nav_edge & BIT(2), nav_mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_turbo_right, nav_edge & BIT(3), nav_mask & BIT(3), 1, now);

    const int row = focused_row();
    if (row < 0 || !(do_left || do_right) || !turbo_available()) return;

    if (is_core_row(row)) {
        const int target = core_row_target_of(row);
        const int source = core_first_bound(target);

        play_sound(snd_option);
        session_settings_cycle_source_turbo(active_port, source, do_right ? 1 : -1);
        core_sync_turbo(target, source);
        submenu_refresh_values(&bm_self[active_port]);
    } else if (session_settings_source_macro(active_port)[row] >= 0) {
        play_sound(snd_error);
        pause_menu_show_toast(lang.muxretro.settings_screen.macro_turbo_blocked);
    } else {
        play_sound(snd_option);
        session_settings_cycle_source_turbo(active_port, row, do_right ? 1 : -1);
        submenu_refresh_values(&bm_self[active_port]);
    }
}

void button_mapping_menu_init_all(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        submenu_init(&bm_self[i], &bm_def);
}

void button_mapping_menu_open(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;
    active_port = port;

    const int source = session_settings_resolve_port_source(port);
    const int sticks = source >= 0 ? mux_input_source_stick_count(source) : 0;
    device_row_count = PORT_DIGITAL_COUNT + sticks * 4;
    build_rows();

    submenu_open(&bm_self[port]);
    apply_nav_bar();
}

int button_mapping_menu_is_active(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        if (submenu_is_active(&bm_self[i])) return 1;
    return 0;
}

void button_mapping_menu_tick(void) {
    if (picker_active) {
        picker_tick();
        return;
    }

    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        if (!submenu_is_active(&bm_self[i])) continue;

        check_turbo_cycle();
        if (picker_active) return;

        submenu_tick(&bm_self[i]);

        if (!picker_active && submenu_is_active(&bm_self[i]) && nav_state() != nav_state_shown) apply_nav_bar();
        return;
    }
}
