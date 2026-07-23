#include <stdio.h>
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_count = 16 };

static const int row_target_id[row_count] = {8, 0, 9, 1, 10, 11, 12, 13, 14, 15, 2, 3, 4, 5, 6, 7};

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.target_a,         lang.muxretro.settings_screen.target_b,
    lang.muxretro.settings_screen.target_x,         lang.muxretro.settings_screen.target_y,
    lang.muxretro.settings_screen.target_l1,        lang.muxretro.settings_screen.target_r1,
    lang.muxretro.settings_screen.target_l2,        lang.muxretro.settings_screen.target_r2,
    lang.muxretro.settings_screen.target_l3,        lang.muxretro.settings_screen.target_r3,
    lang.muxretro.settings_screen.target_select,    lang.muxretro.settings_screen.target_start,
    lang.muxretro.settings_screen.target_dpad_up,   lang.muxretro.settings_screen.target_dpad_down,
    lang.muxretro.settings_screen.target_dpad_left, lang.muxretro.settings_screen.target_dpad_right,
};

static const char *row_glyphs[row_count] = {
    "controller", "controller", "controller", "controller", "controller", "controller", "controller", "controller",
    "controller", "controller", "controller", "controller", "controller", "controller", "controller", "controller",
};

static const mux_input_type capturable_types[] = {
    mux_input_b,         mux_input_y,         mux_input_select,     mux_input_start,    mux_input_dpad_up,
    mux_input_dpad_down, mux_input_dpad_left, mux_input_dpad_right, mux_input_a,        mux_input_x,
    mux_input_l1,        mux_input_r1,        mux_input_l2,         mux_input_r2,       mux_input_l3,
    mux_input_r3,        mux_input_ls_up,     mux_input_ls_down,    mux_input_ls_left,  mux_input_ls_right,
    mux_input_rs_up,     mux_input_rs_down,   mux_input_rs_left,    mux_input_rs_right,
};

#define CAPTURABLE_COUNT (int) (sizeof(capturable_types) / sizeof(capturable_types[0]))

static int active_port = 0;

typedef enum { capture_idle, capture_waiting_press, capture_waiting_release } capture_state_t;

static capture_state_t capture_state = capture_idle;
static int capture_target = -1;
static int capture_source = -1;

static uint64_t capture_prev_mask = 0;
static mux_input_type capture_bound_type = mux_input_count;
static int capture_prev_b = 0;

static int prev_x = 0;
static int prev_y = 0;

static nav_repeat_t rpt_turbo_left = {0};
static nav_repeat_t rpt_turbo_right = {0};
static uint64_t turbo_cycle_prev_mask = 0;

static submenu bm_self[MUX_INPUT_PORT_COUNT];

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    if (index < 0 || index >= row_count) {
        buf[0] = '\0';
        return;
    }

    const int target_id = row_target_id[index];

    if (capture_state == capture_waiting_press && target_id == capture_target) {
        snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.press_button);
        return;
    }

    session_settings_button_map_value(active_port, target_id, buf, buf_len);
}

static int row_is_action(const int index) {
    (void) index;
    return 1;
}

static void row_action(const int index) {
    if (index < 0 || index >= row_count) return;

    capture_source = session_settings_resolve_port_source(active_port);
    if (capture_source < 0) {
        pause_menu_show_toast(lang.generic.not_connected);
        return;
    }

    capture_target = row_target_id[index];
    capture_prev_mask = mux_input_source_pressed_mask(capture_source);
    capture_prev_b = mux_input_pressed(mux_input_b);
    capture_state = capture_waiting_press;
    submenu_refresh_values(&bm_self[active_port]);
}

static void closed(void) {
    input_port_menu_reopen_button_mapping(active_port);
}

static const submenu_def bm_def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .row_is_action = row_is_action,
    .action = row_action,
    .closed = closed,
    .save_title = lang.muxretro.save.button_mapping_title,
    .save_desc = lang.muxretro.save.button_mapping_desc,
};

static void capture_tick(void) {
    const uint64_t mask = mux_input_source_pressed_mask(capture_source);

    if (capture_state == capture_waiting_press) {
        const uint64_t new_bits = mask & ~capture_prev_mask;
        capture_prev_mask = mask;

        for (int i = 0; i < CAPTURABLE_COUNT; i++) {
            if (new_bits & BIT(capturable_types[i])) {
                session_settings_capture_button(active_port, capture_target, capturable_types[i]);

                capture_bound_type = capturable_types[i];
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
        if (!(mask & BIT(capture_bound_type))) {
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

    if (current_item_index < 0 || current_item_index >= row_count) return;
    const int target_id = row_target_id[current_item_index];

    if (x_edge) {
        play_sound(snd_option);
        session_settings_clear_button(active_port, target_id);
        submenu_refresh_values(&bm_self[active_port]);
    } else if (y_edge) {
        play_sound(snd_option);
        session_settings_reset_button(active_port, target_id);
        submenu_refresh_values(&bm_self[active_port]);
    } else if (do_left) {
        play_sound(snd_option);
        session_settings_cycle_turbo_rate(active_port, target_id, -1);
        submenu_refresh_values(&bm_self[active_port]);
    } else if (do_right) {
        play_sound(snd_option);
        session_settings_cycle_turbo_rate(active_port, target_id, 1);
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
    capture_state = capture_idle;
    submenu_open(&bm_self[port]);

    nav_show_lr(0);
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    pause_menu_fix_nav_order();
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
