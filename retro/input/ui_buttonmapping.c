#include <stdio.h>
#include "../../common/input.h"
#include "../../common/ui/common.h"
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
#define GRID_FULL_ROWS  (PORT_TARGET_COUNT / GRID_COLS)
#define GRID_LAST_COUNT (GRID_CELL_COUNT - PORT_TARGET_COUNT)
#define GRID_ROWS       (GRID_FULL_ROWS + 1)
#define GRID_MAP_SIZE   (GRID_CELL_COUNT + GRID_ROWS + 1)

static const char *grid_map[GRID_MAP_SIZE];
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

static nav_repeat_t rpt_turbo_left = {0};
static nav_repeat_t rpt_turbo_right = {0};
static uint64_t turbo_cycle_prev_mask = 0;

static submenu bm_self[MUX_INPUT_PORT_COUNT];

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    session_settings_source_value(active_port, index, buf, buf_len);
}

static int row_is_action(const int index) {
    (void) index;
    return 1;
}

static void picker_open(int row);

static void row_action(const int index) {
    if (index < 0 || index >= PORT_SOURCE_COUNT) return;

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
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = PORT_SOURCE_COUNT,
    .value_text = row_value_text,
    .row_is_action = row_is_action,
    .action = row_action,
    .closed = closed,
    .save_title = lang.muxretro.save.button_mapping_title,
    .save_desc = lang.muxretro.save.button_mapping_desc,
};

static int turbo_available(void) {
    if (current_item_index < 0 || current_item_index >= bm_def.row_count) return 0;
    if (session_settings_source_macro(active_port)[current_item_index] >= 0) return 1;

    return session_settings_source_target(active_port)[current_item_index] >= 0;
}

static int nav_turbo_shown = -1;

static void apply_nav_bar(void) {
    nav_turbo_shown = turbo_available();

    if (nav_turbo_shown) {
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
            return session_settings_target_label(session_settings_target_at_position(cell));
    }
}

static int cell_row(const int index) {
    return index < PORT_TARGET_COUNT ? index / GRID_COLS : GRID_FULL_ROWS;
}

static int cell_col(const int index) {
    return index < PORT_TARGET_COUNT ? index % GRID_COLS : index - PORT_TARGET_COUNT;
}

static void picker_build_map(void) {
    int slot = 0;

    for (int index = 0; index < GRID_CELL_COUNT; index++) {
        grid_map[slot++] = cell_label(grid_cells[index]);
        if (index < PORT_TARGET_COUNT && (index + 1) % GRID_COLS == 0) grid_map[slot++] = "\n";
    }

    grid_map[slot] = "";
}

static void picker_select(const int index) {
    if (index < 0 || index >= GRID_CELL_COUNT) return;

    grid_index = index;

    if (!picker_grid || !lv_obj_is_valid(picker_grid)) return;

    lv_btnmatrix_set_selected_btn(picker_grid, (uint16_t) grid_index);
    lv_btnmatrix_set_btn_ctrl(picker_grid, (uint16_t) grid_index, LV_BTNMATRIX_CTRL_CHECKED);
}

static void picker_move(const int dx, const int dy) {
    const int row = cell_row(grid_index);
    const int col = cell_col(grid_index);
    const int in_last = row == GRID_FULL_ROWS;
    const int width = in_last ? GRID_LAST_COUNT : GRID_COLS;

    if (dx) {
        const int next = ((col + dx) % width + width) % width;
        picker_select(in_last ? PORT_TARGET_COUNT + next : row * GRID_COLS + next);
        return;
    }

    if (!dy) return;

    const int next_row = ((row + dy) % GRID_ROWS + GRID_ROWS) % GRID_ROWS;

    if (next_row == GRID_FULL_ROWS) {
        picker_select(PORT_TARGET_COUNT + col / 2);
        return;
    }

    picker_select(next_row * GRID_COLS + (in_last ? col * 2 : col));
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

static void picker_open(const int row) {
    picker_row = row;
    picker_active = 1;

    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

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

    const int position = session_settings_target_position(session_settings_source_target(active_port)[row]);

    grid_index = 0;
    for (int index = 0; index < GRID_CELL_COUNT; index++) {
        const int wanted = position < 0 ? CELL_UNBOUND : position;
        if (grid_cells[index] == wanted) {
            grid_index = index;
            break;
        }
    }

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
        const int cell = grid_cells[grid_index];

        if (cell == CELL_RESET) {
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

    if (current_item_index < 0 || current_item_index >= bm_def.row_count) return;

    if (do_left || do_right) {
        if (!turbo_available()) return;

        if (session_settings_source_macro(active_port)[current_item_index] >= 0) {
            play_sound(snd_error);
            pause_menu_show_toast(lang.muxretro.settings_screen.macro_turbo_blocked);
        } else {
            play_sound(snd_option);
            session_settings_cycle_source_turbo(active_port, current_item_index, do_right ? 1 : -1);
            submenu_refresh_values(&bm_self[active_port]);
        }
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
    bm_def.row_count = 16 + sticks * 4;

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

        if (!picker_active && submenu_is_active(&bm_self[i]) && turbo_available() != nav_turbo_shown) apply_nav_bar();
        return;
    }
}
