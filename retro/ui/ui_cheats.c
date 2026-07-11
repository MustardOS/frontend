#include <stdio.h>
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "cheats.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};

static const char *row_value_text(const int row) {
    return cheats_list[row].enabled ? lang.generic.enabled : lang.generic.disabled;
}

static void build_cheat_row(const int row) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, cheats_list[row].desc, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "cheat");
    apply_theme_list_value(&theme, value, row_value_text(row));
    apply_size_to_content(&theme, ui_pnl_content, label, icon, cheats_list[row].desc);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void refresh_row(const int row, const enum nav_direction shake_dir) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, row);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    lv_label_set_text(value, row_value_text(row));
    nav_play_shake(value, shake_dir);
}

static void rebuild_cheat_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    if (cheats_count == 0) {
        gen_label("muxretro", "cheat", lang.muxretro.cheats_screen.no_cheats);
    } else {
        for (int row = 0; row < cheats_count; row++)
            build_cheat_row(row);
    }

    ui_count_static = cheats_count;
    first_open = 0;
}

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
}

static void close_cheats(void) {
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_cheats_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();
}

void cheats_menu_init(void) {
}

void cheats_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_cheat_rows();

    nav_show_a(0, "");
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

int cheats_menu_is_active(void) {
    return active;
}

void cheats_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

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
    } else if ((do_left || do_right) && ui_count_static > 0) {
        play_sound(snd_option);
        cheats_toggle(current_item_index);
        refresh_row(current_item_index, do_left ? nav_dir_left : nav_dir_right);
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        close_cheats();
    }
}
