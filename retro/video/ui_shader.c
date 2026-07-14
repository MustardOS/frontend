#include <stdio.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../common/ui/dialogue.h"
#include "../../module/muxshare.h"
#include "colour.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;
static int entry_value = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static int save_dialogue_active = 0;
static mux_dialogue save_dlg;

static uint64_t current_nav_mask(void) {
    return nav_mask_standard();
}

static void build_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, colour_shader_label(index), 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "shader");
    apply_theme_list_value(&theme, value, "");
    apply_size_to_content(&theme, ui_pnl_content, label, icon, colour_shader_label(index));
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

    const int count = colour_shader_count();
    for (int i = 0; i < count; i++)
        build_row(i);

    ui_count_static = count;
    first_open = 0;
}

static void focus_item(const int index) {
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

static void close_screen(void) {
    active = 0;
    display_menu_reopen_shader();
}

void shader_menu_init(void) {
    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.display_title, lang.muxretro.save.display_desc, save_options,
        4, lang.generic.select, lang.generic.cancel
    );
}

void shader_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();
    entry_value = session_settings.colour_shader;

    rebuild_rows();
    focus_item(session_settings.colour_shader);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

int shader_menu_is_active(void) {
    return active;
}

void shader_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

    if (save_dialogue_active) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&save_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = save_dlg.selected;
            dialogue_dismiss(&save_dialogue_active, &save_dlg);
            play_sound(snd_confirm);

            session_settings_apply_save_choice(opt);

            close_screen();
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
        session_settings_set_colour_shader(current_item_index);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
        session_settings_set_colour_shader(current_item_index);
    } else if (nav_page_tick(edge, mask, 2)) {
        session_settings_set_colour_shader(current_item_index);
    } else if (edge & BIT(4)) {
        play_sound(snd_confirm);
        session_settings_set_colour_shader(current_item_index);

        if (session_settings_is_dirty()) {
            dialogue_open(&save_dialogue_active, &save_dlg, &theme);
        } else {
            close_screen();
        }
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        session_settings_set_colour_shader(entry_value);
        close_screen();
    }
}
