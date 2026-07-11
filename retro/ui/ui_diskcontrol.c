#include <stdio.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"

#define DISKCONTROL_MAX_DISCS 32
#define DISKCONTROL_LABEL_MAX 128

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static char disc_labels[DISKCONTROL_MAX_DISCS][DISKCONTROL_LABEL_MAX];
static int disc_count = 0;

static uint64_t current_nav_mask(void) {
    const int up = mux_input_pressed(mux_input_dpad_up);
    const int down = mux_input_pressed(mux_input_dpad_down);
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);

    return (up ? BIT(0) : 0) | (down ? BIT(1) : 0) | (confirm ? BIT(2) : 0) | (back ? BIT(3) : 0);
}

static void refresh_disc_labels(void) {
    disc_count = mux_retro_disk_get_num_images();
    if (disc_count > DISKCONTROL_MAX_DISCS) disc_count = DISKCONTROL_MAX_DISCS;

    for (int i = 0; i < disc_count; i++) {
        if (!mux_retro_disk_get_image_label((unsigned) i, disc_labels[i], sizeof(disc_labels[i]))
            || !disc_labels[i][0]) {
            snprintf(disc_labels[i], sizeof(disc_labels[i]), lang.muxretro.diskcontrol.disc, i + 1);
        }
    }
}

static void refresh_current_marker(void) {
    const unsigned current_index = mux_retro_disk_get_image_index();

    for (int i = 0; i < ui_count_static; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, i);
        if (!panel) continue;

        lv_obj_t *value = lv_obj_get_child(panel, 2);
        if (!value) continue;

        lv_label_set_text(value, (unsigned) i == current_index ? lang.muxretro.diskcontrol.inserted : "");
    }
}

static void build_disc_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, disc_labels[index], 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "disc");
    apply_theme_list_value(&theme, value, "");
    apply_size_to_content(&theme, ui_pnl_content, label, icon, disc_labels[index]);
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

    refresh_disc_labels();

    for (int i = 0; i < disc_count; i++) {
        build_disc_row(i);
    }

    ui_count_static = disc_count;
    first_open = 0;

    refresh_current_marker();
}

static void close_diskcontrol(void) {
    active = 0;
    nav_show_a(0, NULL);

    pause_menu_rebuild();
    pause_menu_focus_diskcontrol_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();
}

static void swap_to_disc(const int index) {
    mux_retro_disk_set_eject_state(true);
    mux_retro_disk_set_image_index((unsigned) index);
    mux_retro_disk_set_eject_state(false);

    refresh_current_marker();
}

void diskcontrol_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});
    nav_show_a(1, lang.muxretro.diskcontrol.swap);
    pause_menu_fix_nav_order();
}

int diskcontrol_menu_is_active(void) {
    return active;
}

void diskcontrol_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

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
        gen_step_movement(1, -1, 2, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
    } else if (edge & BIT(3)) {
        play_sound(snd_back);
        close_diskcontrol();
    } else if (edge & BIT(2)) {
        if (disc_count > 0) {
            play_sound(snd_confirm);
            swap_to_disc(current_item_index);
        }
    }
}
