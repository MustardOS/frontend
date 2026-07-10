#include <stdio.h>
#include "../common/input.h"
#include "../common/ui/common.h"
#include "../common/ui/dialogue.h"
#include "../module/muxshare.h"
#include "muxretro.h"
#include "nav_repeat.h"
#include "settings.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};

enum {
    row_ff_enabled = 0,
    row_ff_speed,
    row_ff_glyph_enabled,
    row_slowmo_enabled,
    row_slowmo_speed,
    row_slowmo_glyph_enabled,
    row_quicksave_enabled,
    row_quickload_enabled,
    row_toggle_fps_enabled,
    row_header_toggle_enabled,
    row_quit_enabled,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.hotkeys_screen.fast_forward,
    lang.muxretro.hotkeys_screen.ff_speed,
    lang.muxretro.hotkeys_screen.ff_glyph,
    lang.muxretro.hotkeys_screen.slow_motion,
    lang.muxretro.hotkeys_screen.slowmo_speed,
    lang.muxretro.hotkeys_screen.slowmo_glyph,
    lang.muxretro.hotkeys_screen.quick_save,
    lang.muxretro.hotkeys_screen.quick_load,
    lang.muxretro.hotkeys_screen.toggle_fps,
    lang.muxretro.hotkeys_screen.toggle_header,
    lang.muxretro.quit
};

static const char *row_glyphs[row_count] = {"fastforward", "ffspeed",      "ffglyph",   "slowmotion",
                                            "slowmospeed", "slowmoglyph",  "quicksave", "quickload",
                                            "togglefps",   "toggleheader", "quit"};

static int save_dialogue_active = 0;
static mux_dialogue save_dlg;

typedef enum { save_opt_content = 0, save_opt_core, save_opt_directory, save_opt_discard } save_opt_t;

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
}

static void enabled_text(char *buf, const int enabled, const char *combo) {
    if (enabled) {
        snprintf(buf, 32, "%s (%s)", lang.generic.enabled, combo);
    } else {
        snprintf(buf, 32, "%s", lang.generic.disabled);
    }
}

static void row_value_text(const int index, char *buf) {
    switch (index) {
        case row_ff_enabled:
            enabled_text(buf, session_settings.hotkey_ff_enabled, "M+R1");
            break;
        case row_ff_speed:
            snprintf(buf, 32, "%s", session_settings_ff_speed_name(session_settings.ff_speed));
            break;
        case row_ff_glyph_enabled:
            snprintf(
                buf, 32, "%s", session_settings.hotkey_ff_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_slowmo_enabled:
            enabled_text(buf, session_settings.hotkey_slowmo_enabled, "M+L1");
            break;
        case row_slowmo_speed:
            snprintf(buf, 32, "%s", session_settings_slowmo_speed_name(session_settings.slowmo_speed));
            break;
        case row_slowmo_glyph_enabled:
            snprintf(
                buf, 32, "%s",
                session_settings.hotkey_slowmo_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_quicksave_enabled:
            enabled_text(buf, session_settings.hotkey_quicksave_enabled, "M+R2");
            break;
        case row_quickload_enabled:
            enabled_text(buf, session_settings.hotkey_quickload_enabled, "M+L2");
            break;
        case row_toggle_fps_enabled:
            enabled_text(buf, session_settings.hotkey_toggle_fps_enabled, "M+Y");
            break;
        case row_header_toggle_enabled:
            enabled_text(buf, session_settings.hotkey_header_toggle_enabled, "M+X");
            break;
        case row_quit_enabled:
            enabled_text(buf, session_settings.hotkey_quit_enabled, "M+START");
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_ff_enabled:
            session_settings_cycle_hotkey_ff_enabled(direction);
            break;
        case row_ff_speed:
            session_settings_cycle_ff_speed(direction);
            break;
        case row_ff_glyph_enabled:
            session_settings_cycle_hotkey_ff_glyph_enabled(direction);
            break;
        case row_slowmo_enabled:
            session_settings_cycle_hotkey_slowmo_enabled(direction);
            break;
        case row_slowmo_speed:
            session_settings_cycle_slowmo_speed(direction);
            break;
        case row_slowmo_glyph_enabled:
            session_settings_cycle_hotkey_slowmo_glyph_enabled(direction);
            break;
        case row_quicksave_enabled:
            session_settings_cycle_hotkey_quicksave_enabled(direction);
            break;
        case row_quickload_enabled:
            session_settings_cycle_hotkey_quickload_enabled(direction);
            break;
        case row_toggle_fps_enabled:
            session_settings_cycle_hotkey_toggle_fps_enabled(direction);
            break;
        case row_header_toggle_enabled:
            session_settings_cycle_hotkey_header_toggle_enabled(direction);
            break;
        case row_quit_enabled:
            session_settings_cycle_hotkey_quit_enabled(direction);
            break;
        default:
            break;
    }
}

static void refresh_row(const int index, const enum nav_direction shake_dir) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    char value_text[32];
    row_value_text(index, value_text);
    lv_label_set_text(value, value_text);
    nav_play_shake(value, shake_dir);
}

static void build_hotkeys_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    char value_text[32];
    row_value_text(index, value_text);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, row_labels[index], 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", row_glyphs[index]);
    apply_theme_list_value(&theme, value, value_text);
    apply_size_to_content(&theme, ui_pnl_content, label, icon, row_labels[index]);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void rebuild_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int i = 0; i < row_count; i++)
        build_hotkeys_row(i);

    ui_count_static = row_count;
    first_open = 0;
}

static void close_hotkeys(void) {
    active = 0;
    settings_menu_reopen_hotkeys();
}

void hotkeys_menu_init(void) {
    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.hotkeys_title, lang.muxretro.save.hotkeys_desc, save_options,
        4, lang.generic.select, lang.generic.cancel
    );
}

void hotkeys_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();

    nav_show_a(0, "");
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

int hotkeys_menu_is_active(void) {
    return active;
}

void hotkeys_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (save_dialogue_active) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&save_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const save_opt_t opt = (save_opt_t) save_dlg.selected;
            dialogue_dismiss(&save_dialogue_active, &save_dlg);
            play_sound(snd_confirm);

            switch (opt) {
                case save_opt_content:
                    session_settings_save_content();
                    break;
                case save_opt_core:
                    session_settings_save_core();
                    break;
                case save_opt_directory:
                    session_settings_save_directory();
                    break;
                case save_opt_discard:
                    session_settings_discard();
                    break;
            }

            close_hotkeys();
        } else if (edge & BIT(5)) {
            dialogue_dismiss(&save_dialogue_active, &save_dlg);
        }
        return;
    }

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);
    const int do_left = nav_repeat_step(&rpt_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_right, edge & BIT(3), mask & BIT(3), 1, now);

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
    } else if (do_left) {
        cycle_row(current_item_index, -1);
        refresh_row(current_item_index, nav_dir_left);
        play_sound(snd_option);
    } else if (do_right) {
        cycle_row(current_item_index, +1);
        refresh_row(current_item_index, nav_dir_right);
        play_sound(snd_option);
    } else if (edge & BIT(5)) {
        if (session_settings_is_dirty()) {
            play_sound(snd_confirm);
            dialogue_open(&save_dialogue_active, &save_dlg, &theme);
        } else {
            play_sound(snd_back);
            close_hotkeys();
        }
    }
}
