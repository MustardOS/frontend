#include <stdio.h>
#include "../common/audio.h"
#include "../common/config.h"
#include "../common/input.h"
#include "../common/ui/common.h"
#include "../common/ui/dialogue.h"
#include "../module/muxshare.h"
#include "muxretro.h"
#include "options.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static uint32_t hold_delay_up = 0;
static uint32_t hold_tick_up = 0;
static uint32_t hold_delay_down = 0;
static uint32_t hold_tick_down = 0;
static uint32_t hold_delay_left = 0;
static uint32_t hold_tick_left = 0;
static uint32_t hold_delay_right = 0;
static uint32_t hold_tick_right = 0;

static int save_dialogue_active = 0;
static mux_dialogue save_dlg;

typedef enum { save_opt_content = 0, save_opt_core, save_opt_directory, save_opt_discard } save_opt_t;

static const char *row_value_text(const int index) {
    const struct core_option_entry *e = &options_list[index];
    return e->value_count ? e->values[e->current_index] : "";
}

static void build_option_row(const int index) {
    const struct core_option_entry *e = &options_list[index];

    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, e->label, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "option");
    apply_theme_list_value(&theme, value, row_value_text(index));
    apply_size_to_content(&theme, ui_pnl_content, label, icon, e->label);
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

    for (int i = 0; i < options_count; i++) {
        build_option_row(i);
    }

    ui_count_static = options_count;
    first_open = 0;
}

static void refresh_row(const int index, const enum nav_direction shake_dir) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    lv_label_set_text(value, row_value_text(index));
    nav_play_shake(value, shake_dir);
}

static uint64_t current_nav_mask(void) {
    const int up = mux_input_pressed(mux_input_dpad_up);
    const int down = mux_input_pressed(mux_input_dpad_down);
    const int left = mux_input_pressed(mux_input_dpad_left);
    const int right = mux_input_pressed(mux_input_dpad_right);
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);

    return (up ? BIT(0) : 0) | (down ? BIT(1) : 0) | (left ? BIT(2) : 0) | (right ? BIT(3) : 0) | (confirm ? BIT(4) : 0)
           | (back ? BIT(5) : 0);
}

static void close_options(void) {
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_options_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();
}

void options_menu_init(void) {
    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.options_title, lang.muxretro.save.options_desc, save_options,
        4, lang.generic.select, lang.generic.cancel
    );
}

void options_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

int options_menu_is_active(void) {
    return active;
}

void options_menu_tick(void) {
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
                    options_save_content();
                    break;
                case save_opt_core:
                    options_save_core();
                    break;
                case save_opt_directory:
                    options_save_directory();
                    break;
                case save_opt_discard:
                    options_discard();
                    break;
            }

            close_options();
        } else if (edge & BIT(5)) {
            dialogue_dismiss(&save_dialogue_active, &save_dlg);
        }
        return;
    }

    const uint32_t now = SDL_GetTicks();

    int do_up = 0;
    int do_down = 0;
    int do_left = 0;
    int do_right = 0;

    if (edge & BIT(0)) {
        do_up = 1;
        hold_delay_up = (uint32_t) config.settings.advanced.repeat_delay;
        hold_tick_up = now;
    } else if ((mask & BIT(0)) && now - hold_tick_up >= hold_delay_up) {
        if (current_item_index > 0) do_up = 1;
        hold_delay_up = (uint32_t) config.settings.advanced.accelerate;
        hold_tick_up = now;
    }

    if (edge & BIT(1)) {
        do_down = 1;
        hold_delay_down = (uint32_t) config.settings.advanced.repeat_delay;
        hold_tick_down = now;
    } else if ((mask & BIT(1)) && now - hold_tick_down >= hold_delay_down) {
        if (current_item_index < ui_count_static - 1) do_down = 1;
        hold_delay_down = (uint32_t) config.settings.advanced.accelerate;
        hold_tick_down = now;
    }

    if (edge & BIT(2)) {
        do_left = 1;
        hold_delay_left = (uint32_t) config.settings.advanced.repeat_delay;
        hold_tick_left = now;
    } else if ((mask & BIT(2)) && now - hold_tick_left >= hold_delay_left) {
        do_left = 1;
        hold_delay_left = (uint32_t) config.settings.advanced.accelerate;
        hold_tick_left = now;
    }

    if (edge & BIT(3)) {
        do_right = 1;
        hold_delay_right = (uint32_t) config.settings.advanced.repeat_delay;
        hold_tick_right = now;
    } else if ((mask & BIT(3)) && now - hold_tick_right >= hold_delay_right) {
        do_right = 1;
        hold_delay_right = (uint32_t) config.settings.advanced.accelerate;
        hold_tick_right = now;
    }

    if (ui_count_static < 2) {
        do_up = 0;
        do_down = 0;
    }

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
    } else if (do_left) {
        options_cycle(current_item_index, -1);
        refresh_row(current_item_index, nav_dir_left);
        play_sound(snd_option);
    } else if (do_right) {
        options_cycle(current_item_index, +1);
        refresh_row(current_item_index, nav_dir_right);
        play_sound(snd_option);
    } else if (edge & BIT(5)) {
        if (options_is_dirty()) {
            play_sound(snd_confirm);
            dialogue_open(&save_dialogue_active, &save_dlg, &theme);
        } else {
            play_sound(snd_back);
            close_options();
        }
    }
}
