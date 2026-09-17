#include <stdio.h>
#include <common/platform/audio.h>
#include <common/platform/input.h>
#include <common/ui/common.h>
#include <common/ui/dialogue.h>
#include "../../module/muxshare.h"
#include "overlay_bridge.h"
#include "overlay_library.h"
#include "preset_catalogue.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;
static char entry_key[64];

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static mux_dialogue save_dlg;
static mux_dialogue delete_dlg;
static int skip_confirm = 0;

#define NAV_X_BIT BIT(6)

static uint64_t current_nav_mask(void) {
    return nav_mask_standard() | (mux_input_pressed(mux_input_x) ? NAV_X_BIT : 0);
}

static int selected_is_removable(void) {
    return current_item_index > 0 && overlay_library_is_user(current_item_index - 1);
}

static void refresh_nav(void) {
    nav_show_lr(0);

    if (!selected_is_removable()) {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.select, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
        pause_menu_fix_nav_order();
        return;
    }

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.muxretro.catalogue_screen.delete_label, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void rebuild_rows(void);

static void focus_item(int index);

static void delete_selected(void) {
    const int index = current_item_index - 1;
    const int was_selected = session_settings.overlay_image == index;

    if (!overlay_library_delete(index)) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.delete_failed);
        return;
    }

    if (session_settings.overlay_image >= overlay_library_count())
        session_settings_set_overlay_image(overlay_library_count() - 1);
    if (was_selected) session_settings_set_overlay_image(0);

    overlay_bridge_apply();
    rebuild_rows();
    focus_item(index < ui_count_static - 1 ? index + 1 : ui_count_static - 1);
    refresh_nav();

    pause_menu_show_toast(lang.muxretro.catalogue_screen.delete_done);
}

static void build_row(const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);

    const char *text = index == 0 ? lang.generic.download : overlay_library_label(index - 1);
    apply_theme_option_item_label(&theme, label, text, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", index == 0 ? "download" : "overlay");
    apply_theme_list_value(&theme, value, "");
    apply_size_to_content(&theme, ui_pnl_content, label, icon, text);
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

    const int count = overlay_library_count() + 1;
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
    settings_menu_reopen_overlay_at(overlay_menu_row_image());
}

void overlay_image_menu_init(void) {
    preset_catalogue_init();

    dialogue_init_remove(
        &delete_dlg, &theme, ui_screen, lang.muxretro.catalogue_screen.delete_confirm, lang.generic.select,
        lang.generic.cancel
    );

    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.muxretro.save.session_save, lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.display_title, lang.muxretro.save.display_desc, save_options,
        5, lang.generic.select, lang.generic.cancel
    );
}

static void reopen_at(const int index) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();
    focus_item(index);
    refresh_nav();
}

void overlay_image_menu_reopen(void) {
    reopen_at(session_settings.overlay_image + 1);
}

void overlay_image_menu_reopen_download(void) {
    reopen_at(0);
}

void overlay_image_menu_open(void) {
    snprintf(entry_key, sizeof(entry_key), "%s", overlay_library_key(session_settings.overlay_image));
    overlay_image_menu_reopen();
}

int overlay_image_menu_is_active(void) {
    return active;
}

void overlay_image_menu_tick(void) {
    if (preset_catalogue_tick()) return;

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

    if (dialogue_active(&delete_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&delete_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_remove_opt option = (mux_remove_opt) delete_dlg.selected;
            dialogue_dismiss(&delete_dlg);

            if (option == mux_remove_skip) skip_confirm = 1;
            if (option != mux_remove_nah) delete_selected();
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&delete_dlg);
            dialogue_dismiss(&delete_dlg);
        }
        return;
    }

    if (dialogue_active(&save_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&save_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = save_dlg.selected;
            dialogue_dismiss(&save_dlg);

            if (opt != 3) session_settings_apply_save_choice(opt);
            submenu_stack_resync();

            close_screen();
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&save_dlg);
            dialogue_dismiss(&save_dlg);
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
        if (current_item_index > 0) session_settings_set_overlay_image(current_item_index - 1);
        refresh_nav();
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
        if (current_item_index > 0) session_settings_set_overlay_image(current_item_index - 1);
        refresh_nav();
    } else if (nav_page_tick(edge, mask, 2)) {
        if (current_item_index > 0) session_settings_set_overlay_image(current_item_index - 1);
        refresh_nav();
    } else if (edge & NAV_X_BIT) {
        if (!selected_is_removable()) return;

        play_sound(snd_confirm);
        if (skip_confirm)
            delete_selected();
        else
            dialogue_open(&delete_dlg, &theme);
    } else if (edge & BIT(4)) {
        play_sound(snd_confirm);
        if (current_item_index == 0) {
            preset_catalogue_open(preset_catalogue_overlay);
            return;
        }
        session_settings_set_overlay_image(current_item_index - 1);

        if (session_settings_is_dirty()) {
            dialogue_open(&save_dlg, &theme);
        } else {
            close_screen();
        }
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        const int index = overlay_library_index(entry_key);
        session_settings_set_overlay_image(index >= 0 ? index : 0);
        close_screen();
    }
}
