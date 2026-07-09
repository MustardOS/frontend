#include <stdio.h>
#include <string.h>
#include "../common/audio.h"
#include "../common/input.h"
#include "../common/ui/common.h"
#include "../common/ui/dialogue.h"
#include "../module/muxshare.h"
#include "muxretro.h"
#include "nav_repeat.h"
#include "options.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};

static int save_dialogue_active = 0;
static mux_dialogue save_dlg;

typedef enum { save_opt_content = 0, save_opt_core, save_opt_directory, save_opt_discard } save_opt_t;

typedef enum { screen_categories, screen_options } screen_state_t;
static screen_state_t screen_state = screen_options;

static int visible_indices[OPTIONS_MAX];
static int visible_count = 0;

static int has_uncategorized_options = 0;
static int category_item_count = 0;
static int category_cursor = 0;

static const char *row_value_text(const int row) {
    const struct core_option_entry *e = &options_list[visible_indices[row]];
    return e->value_count ? e->values[e->current_index] : "";
}

static void build_option_row(const int row) {
    const struct core_option_entry *e = &options_list[visible_indices[row]];

    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, e->label, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "option");
    apply_theme_list_value(&theme, value, row_value_text(row));
    apply_size_to_content(&theme, ui_pnl_content, label, icon, e->label);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void build_visible_indices(const int category_index) {
    visible_count = 0;

    for (int i = 0; i < options_count && visible_count < OPTIONS_MAX; i++) {
        int matches;
        if (category_index < 0) {
            matches = 1;
        } else if (category_index == options_category_count) {
            matches = options_list[i].category_key[0] == '\0';
        } else {
            matches = strcmp(options_list[i].category_key, options_categories[category_index].key) == 0;
        }

        if (matches) visible_indices[visible_count++] = i;
    }
}

static void rebuild_option_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int row = 0; row < visible_count; row++)
        build_option_row(row);

    ui_count_static = visible_count;
    first_open = 0;
}

static void refresh_row(const int row, const enum nav_direction shake_dir) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, row);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    lv_label_set_text(value, row_value_text(row));
    nav_play_shake(value, shake_dir);
}

static int category_index_for_row(const int row) {
    if (has_uncategorized_options) {
        if (row == 0) return options_category_count;
        return row - 1;
    }
    return row;
}

static void rebuild_category_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    if (has_uncategorized_options) gen_label("muxretro", "folder", lang.muxretro.options_screen.miscellaneous);

    for (int i = 0; i < options_category_count; i++)
        gen_label("muxretro", "folder", options_categories[i].label);

    ui_count_static = category_item_count;
    first_open = 0;
}

static void focus_category_row(const int index) {
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

static void set_options_nav(void) {
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void set_category_nav(void) {
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void open_category(const int category_index) {
    screen_state = screen_options;
    build_visible_indices(category_index);
    rebuild_option_rows();
    set_options_nav();
}

static void open_category_picker(void) {
    screen_state = screen_categories;
    rebuild_category_rows();
    focus_category_row(category_cursor);
    set_category_nav();
}

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
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

    has_uncategorized_options = 0;
    if (options_category_count > 0) {
        for (int i = 0; i < options_count; i++) {
            if (options_list[i].category_key[0] == '\0') {
                has_uncategorized_options = 1;
                break;
            }
        }
    }
    category_item_count = options_category_count + (has_uncategorized_options ? 1 : 0);
    category_cursor = 0;

    if (options_category_count > 0) {
        open_category_picker();
    } else {
        screen_state = screen_options;
        build_visible_indices(-1);
        rebuild_option_rows();
        set_options_nav();
    }
}

int options_menu_is_active(void) {
    return active;
}

static void tick_category_picker(const uint64_t edge, const uint64_t mask) {
    const uint32_t now = SDL_GetTicks();

    int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);

    if (ui_count_static < 2) {
        do_up = 0;
        do_down = 0;
    }

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 1, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 1, 0, 1);
    } else if (edge & BIT(4)) {
        play_sound(snd_confirm);
        category_cursor = current_item_index;
        open_category(category_index_for_row(current_item_index));
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

static void tick_options(const uint64_t edge, const uint64_t mask) {
    const uint32_t now = SDL_GetTicks();

    int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);
    const int do_left = nav_repeat_step(&rpt_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_right, edge & BIT(3), mask & BIT(3), 1, now);

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
        options_cycle(visible_indices[current_item_index], -1);
        refresh_row(current_item_index, nav_dir_left);
        play_sound(snd_option);
    } else if (do_right) {
        options_cycle(visible_indices[current_item_index], +1);
        refresh_row(current_item_index, nav_dir_right);
        play_sound(snd_option);
    } else if (edge & BIT(5)) {
        if (options_category_count > 0) {
            play_sound(snd_back);
            open_category_picker();
        } else if (options_is_dirty()) {
            play_sound(snd_confirm);
            dialogue_open(&save_dialogue_active, &save_dlg, &theme);
        } else {
            play_sound(snd_back);
            close_options();
        }
    }
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

    if (screen_state == screen_categories) {
        tick_category_picker(edge, mask);
    } else {
        tick_options(edge, mask);
    }
}
