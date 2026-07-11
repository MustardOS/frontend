#include <stdio.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../common/ui/dialogue.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "settings.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};

enum { row_fps = 0, row_header_visibility, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.show_fps, lang.muxretro.settings_screen.header_visibility
};

static const char *row_glyphs[row_count] = {"fpscounter", "header"};

static int save_dialogue_active = 0;
static mux_dialogue save_dlg;

typedef enum { save_opt_content = 0, save_opt_core, save_opt_directory, save_opt_discard } save_opt_t;

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_fps:
            snprintf(buf, buf_len, "%s", session_settings.show_fps ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_header_visibility:
            snprintf(buf, buf_len, "%s", session_settings_header_visibility_name(session_settings.header_visibility));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_fps:
            session_settings_cycle_fps(direction);
            break;
        case row_header_visibility:
            session_settings_cycle_header_visibility(direction);
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
    row_value_text(index, value_text, sizeof(value_text));
    lv_label_set_text(value, value_text);
    nav_play_shake(value, shake_dir);
}

static void build_hud_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    char value_text[32];
    row_value_text(index, value_text, sizeof(value_text));

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
        build_hud_row(i);

    ui_count_static = row_count;
    first_open = 0;
}

static void focus_row(const int index) {
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

static void hud_nav(const int force) {
    static int nav_ready = 0;
    if (!force && nav_ready) return;
    nav_ready = 1;

    nav_show_a(0, "");
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void close_hud(void) {
    active = 0;
    settings_menu_reopen_hud();
}

void hud_menu_init(void) {
    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.hud_title, lang.muxretro.save.hud_desc, save_options, 4,
        lang.generic.select, lang.generic.cancel
    );
}

void hud_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();
    focus_row(current_item_index);
    hud_nav(1);
}

int hud_menu_is_active(void) {
    return active;
}

void hud_menu_tick(void) {
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

            close_hud();
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
            close_hud();
        }
    }
}
