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

#define PICKER_ROW_COUNT (1 + PORT_TARGET_COUNT)

static const char *picker_labels[PICKER_ROW_COUNT];

static int picker_active = 0;
static int picker_row = -1;
static uint64_t picker_prev_mask = 0;

static nav_repeat_t rpt_pick_up = {0};
static nav_repeat_t rpt_pick_down = {0};

static int active_port = 0;

static int prev_x = 0;
static int prev_y = 0;

typedef enum { capture_idle, capture_waiting_press, capture_waiting_release } capture_state_t;

static capture_state_t capture_state = capture_idle;
static int capture_row = -1;
static int capture_source = -1;
static uint64_t capture_prev_mask = 0;
static mux_input_type capture_pressed_type = mux_input_count;
static int capture_prev_b = 0;

static nav_repeat_t rpt_turbo_left = {0};
static nav_repeat_t rpt_turbo_right = {0};
static uint64_t turbo_cycle_prev_mask = 0;

static submenu bm_self[MUX_INPUT_PORT_COUNT];

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    if (capture_state == capture_waiting_press && index == capture_row) {
        snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.assign_control);
        return;
    }

    session_settings_source_value(active_port, index, buf, buf_len);
}

static int row_is_action(const int index) {
    (void) index;
    return 1;
}

static void row_action(const int index) {
    if (index < 0 || index >= PORT_SOURCE_COUNT) return;

    capture_source = session_settings_resolve_port_source(active_port);
    if (capture_source < 0) {
        pause_menu_show_toast(lang.generic.not_connected);
        return;
    }

    capture_row = index;
    capture_prev_mask = mux_input_source_pressed_mask(capture_source);
    capture_prev_b = mux_input_pressed(mux_input_b);
    capture_state = capture_waiting_press;
    submenu_refresh_values(&bm_self[active_port]);
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

static void apply_nav_bar(void) {
    nav_show_lr(0);
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.muxretro.settings_screen.turbo_modes, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.set, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.muxretro.settings_screen.targets, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.reset, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void picker_build_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, picker_labels[index], 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "controller");
    apply_theme_list_value(&theme, value, "");
    apply_size_to_content(&theme, ui_pnl_content, label, icon, picker_labels[index]);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void picker_focus(const int index) {
    if (index < 0 || index >= ui_count_static) return;
    current_item_index = index;

    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *label = lv_obj_get_child(panel, 0);
    lv_obj_t *glyph = lv_obj_get_child(panel, 1);
    lv_obj_t *value = lv_obj_get_child(panel, 2);

    nav_suppress_next_shake();

    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    if (value) lv_group_focus_obj(value);
    lv_group_focus_obj(panel);

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
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

    for (int i = 0; i < PICKER_ROW_COUNT; i++)
        picker_build_row(i);

    ui_count_static = PICKER_ROW_COUNT;
    first_open = 0;

    // Opening on whatever the row already uses, with Unbound sitting at the top
    const int position = session_settings_target_position(session_settings.port_source_target[active_port][row]);
    picker_focus(position < 0 ? 0 : position + 1);

    picker_prev_mask = nav_mask_standard();
    picker_nav_bar();
    pause_menu_sync_input_mask();
}

static void picker_close(void) {
    picker_active = 0;
    submenu_reopen_at(&bm_self[active_port], picker_row);
    apply_nav_bar();

    prev_x = mux_input_pressed(mux_input_x);
    prev_y = mux_input_pressed(mux_input_y);

    turbo_cycle_prev_mask = nav_mask_standard();
}

static void picker_tick(void) {
    const uint64_t mask = nav_mask_standard();
    const uint64_t edge = mask & ~picker_prev_mask;
    picker_prev_mask = mask;

    if (nav_input_halted()) return;

    const uint32_t now = SDL_GetTicks();
    const int do_up = nav_repeat_step(&rpt_pick_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int do_down =
        nav_repeat_step(&rpt_pick_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
    } else if (nav_page_tick(edge, mask, 2)) {
        // do nothing!
    } else if (edge & BIT(4)) {
        play_sound(snd_confirm);

        const int target = current_item_index == 0 ? -1 : session_settings_target_at_position(current_item_index - 1);

        session_settings_set_source_target(active_port, picker_row, target);
        picker_close();
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        picker_close();
    }
}

static void capture_tick(void) {
    const uint64_t mask = mux_input_source_pressed_mask(capture_source);

    if (capture_state == capture_waiting_press) {
        const uint64_t new_bits = mask & ~capture_prev_mask;
        capture_prev_mask = mask;

        for (int i = 0; i < PORT_SOURCE_COUNT; i++) {
            const mux_input_type pressed = (mux_input_type) session_settings_source_types[i];
            if (pressed == mux_input_b) continue;

            if (new_bits & BIT(pressed)) {
                session_settings_set_source_by_button(active_port, capture_row, pressed);

                capture_pressed_type = pressed;
                capture_state = capture_waiting_release;
                submenu_refresh_values(&bm_self[active_port]);
                play_sound(snd_confirm);
                return;
            }
        }

        const int b_now = mux_input_pressed(mux_input_b);
        const int b_edge = b_now && !capture_prev_b;
        capture_prev_b = b_now;

        if (b_edge) {
            bm_self[active_port].prev_nav_mask = nav_mask_standard();
            capture_state = capture_idle;
            submenu_refresh_values(&bm_self[active_port]);
            play_sound(snd_back);
        }

        return;
    }

    if (capture_state == capture_waiting_release) {
        if (!(mask & BIT(capture_pressed_type))) {
            bm_self[active_port].prev_nav_mask = nav_mask_standard();
            capture_state = capture_idle;
        }
    }
}

static void check_row_shortcuts(void) {
    const int x_now = mux_input_pressed(mux_input_x);
    const int x_edge = x_now && !prev_x;
    prev_x = x_now;

    const int y_now = mux_input_pressed(mux_input_y);
    const int y_edge = y_now && !prev_y;
    prev_y = y_now;

    const uint64_t nav_mask = nav_mask_standard();
    const uint64_t nav_edge = nav_mask & ~turbo_cycle_prev_mask;
    turbo_cycle_prev_mask = nav_mask;

    const uint32_t now = SDL_GetTicks();
    const int do_left = nav_repeat_step(&rpt_turbo_left, nav_edge & BIT(2), nav_mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_turbo_right, nav_edge & BIT(3), nav_mask & BIT(3), 1, now);

    if (current_item_index < 0 || current_item_index >= bm_def.row_count) return;

    if (x_edge) {
        play_sound(snd_confirm);
        picker_open(current_item_index);
    } else if (y_edge) {
        play_sound(snd_option);
        session_settings_reset_source(active_port, current_item_index);
        submenu_refresh_values(&bm_self[active_port]);
    } else if (do_left || do_right) {
        if (session_settings.port_source_macro[active_port][current_item_index] >= 0) {
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
    picker_labels[0] = lang.muxretro.settings_screen.unbound;
    for (int position = 0; position < PORT_TARGET_COUNT; position++)
        picker_labels[position + 1] = session_settings_target_label(session_settings_target_at_position(position));

    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        submenu_init(&bm_self[i], &bm_def);
}

void button_mapping_menu_open(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;
    active_port = port;
    capture_state = capture_idle;

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

        if (capture_state != capture_idle) {
            capture_tick();
            return;
        }

        check_row_shortcuts();
        if (picker_active) return;

        submenu_tick(&bm_self[i]);
        return;
    }
}
