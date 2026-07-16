#include <stdio.h>
#include <string.h>
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../state/patch.h"

static int active = 0;
static int changed = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};

static const char *row_value_text(const int row) {
    return patch_manual_list[row].enabled ? lang.generic.enabled : lang.generic.disabled;
}

static void row_display_name(const int row, char *out) {
    snprintf(out, PATCH_NAME_MAX, "%s", patch_manual_list[row].filename);
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

static void build_patch_row(const int row) {
    char display_name[PATCH_NAME_MAX];
    row_display_name(row, display_name);

    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, display_name, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "patch");
    apply_theme_list_value(&theme, value, row_value_text(row));
    apply_size_to_content(&theme, ui_pnl_content, label, icon, display_name);
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

static void rebuild_patch_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int row = 0; row < patch_manual_count; row++)
        build_patch_row(row);

    ui_count_static = patch_manual_count;
    first_open = 0;
}

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
}

static void close_patches(void) {
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_patches_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();

    if (changed) {
        changed = 0;
        pause_menu_show_toast(lang.muxretro.patches_screen.restart_required);
    }
}

void patch_menu_init(void) {
}

void patch_menu_open(void) {
    active = 1;
    changed = 0;
    prev_nav_mask = current_nav_mask();

    rebuild_patch_rows();

    nav_show_a(0, "");
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

int patch_menu_is_active(void) {
    return active;
}

void patch_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

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
        patch_manual_toggle(current_item_index);
        refresh_row(current_item_index, do_left ? nav_dir_left : nav_dir_right);
        changed = 1;
    } else if (nav_page_tick(edge, mask, 2)) {
        // do nothing!
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        close_patches();
    }
}
