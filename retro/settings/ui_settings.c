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

enum {
    row_hotkey_controls = 0,
    row_video,
    row_display,
    row_sound,
    row_input,
    row_performance,
    row_hud,
    row_storage,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.hotkeys,
    lang.muxretro.settings_screen.category_video,
    lang.muxretro.display,
    lang.muxretro.settings_screen.category_sound,
    lang.muxretro.settings_screen.category_input,
    lang.muxretro.settings_screen.category_performance,
    lang.muxretro.settings_screen.category_hud,
    lang.muxretro.settings_screen.category_storage,
};

static const char *row_glyphs[row_count] = {"hotkeys",       "videosettings", "display",    "soundsettings",
                                            "inputsettings", "performance",   "screeninfo", "storagesettings"};

static int save_dialogue_active = 0;
static mux_dialogue save_dlg;

typedef enum { save_opt_content = 0, save_opt_core, save_opt_directory, save_opt_discard } save_opt_t;

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
}

static void build_settings_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, row_labels[index], 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", row_glyphs[index]);
    apply_size_to_content(&theme, ui_pnl_content, label, icon, row_labels[index]);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
}

static void rebuild_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int i = 0; i < row_count; i++)
        build_settings_row(i);

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

    nav_suppress_next_shake();

    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    lv_group_focus_obj(panel);

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
}

static void settings_nav(void) {
    nav_show_lr(0);
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void close_settings(void) {
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_settings_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();
}

static void reopen_at_row(const int index) {
    rebuild_rows();
    focus_row(index);
    settings_nav();
    prev_nav_mask = current_nav_mask();
    pause_menu_sync_input_mask();
}

void settings_menu_reopen_hotkeys(void) {
    reopen_at_row(row_hotkey_controls);
}

void settings_menu_reopen_display(void) {
    reopen_at_row(row_display);
}

void settings_menu_reopen_video(void) {
    reopen_at_row(row_video);
}

void settings_menu_reopen_sound(void) {
    reopen_at_row(row_sound);
}

void settings_menu_reopen_input(void) {
    reopen_at_row(row_input);
}

void settings_menu_reopen_performance(void) {
    reopen_at_row(row_performance);
}

void settings_menu_reopen_hud(void) {
    reopen_at_row(row_hud);
}

void settings_menu_reopen_storage(void) {
    reopen_at_row(row_storage);
}

void settings_menu_init(void) {
    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.settings_title, lang.muxretro.save.settings_desc, save_options,
        4, lang.generic.select, lang.generic.cancel
    );

    video_menu_init();
    display_menu_init();
    sound_menu_init();
    input_menu_init();
    performance_menu_init();
    hud_menu_init();
    storage_menu_init();
}

void settings_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();
    settings_nav();
}

int settings_menu_is_active(void) {
    return active;
}

void settings_menu_tick(void) {
    if (hotkeys_menu_is_active()) {
        hotkeys_menu_tick();
        return;
    }

    if (video_menu_is_active()) {
        video_menu_tick();
        return;
    }

    if (display_menu_is_active()) {
        display_menu_tick();
        return;
    }

    if (sound_menu_is_active()) {
        sound_menu_tick();
        return;
    }

    if (input_menu_is_active()) {
        input_menu_tick();
        return;
    }

    if (performance_menu_is_active()) {
        performance_menu_tick();
        return;
    }

    if (hud_menu_is_active()) {
        hud_menu_tick();
        return;
    }

    if (storage_menu_is_active()) {
        storage_menu_tick();
        return;
    }

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

            close_settings();
        } else if (edge & BIT(5)) {
            dialogue_dismiss(&save_dialogue_active, &save_dlg);
        }
        return;
    }

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
    } else if (edge & BIT(4)) {
        play_sound(snd_confirm);

        if (current_item_index == row_hotkey_controls) {
            hotkeys_menu_open();
        } else if (current_item_index == row_video) {
            video_menu_open();
        } else if (current_item_index == row_display) {
            display_menu_open();
        } else if (current_item_index == row_sound) {
            sound_menu_open();
        } else if (current_item_index == row_input) {
            input_menu_open();
        } else if (current_item_index == row_performance) {
            performance_menu_open();
        } else if (current_item_index == row_hud) {
            hud_menu_open();
        } else if (current_item_index == row_storage) {
            storage_menu_open();
        }
    } else if (edge & BIT(5)) {
        if (session_settings_is_dirty()) {
            play_sound(snd_confirm);
            dialogue_open(&save_dialogue_active, &save_dlg, &theme);
        } else {
            play_sound(snd_back);
            close_settings();
        }
    }
}
