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
        snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.press_button);
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
                                  {ui_lbl_nav_x, lang.muxretro.settings_screen.unbind, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.reset, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void capture_tick(void) {
    const uint64_t mask = mux_input_source_pressed_mask(capture_source);

    if (capture_state == capture_waiting_press) {
        const uint64_t new_bits = mask & ~capture_prev_mask;
        capture_prev_mask = mask;

        for (int i = 0; i < 16; i++) {
            const mux_input_type pressed = (mux_input_type) session_settings_source_types[i];

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
        play_sound(snd_option);
        session_settings_unbind_source(active_port, current_item_index);
        submenu_refresh_values(&bm_self[active_port]);
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
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        if (!submenu_is_active(&bm_self[i])) continue;

        if (capture_state != capture_idle) {
            capture_tick();
            return;
        }

        check_row_shortcuts();
        submenu_tick(&bm_self[i]);
        return;
    }
}
