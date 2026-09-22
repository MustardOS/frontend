#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <common/platform/audio.h>
#include <common/platform/input.h>
#include <common/ui/common.h>
#include <common/ui/dialogue.h>
#include "../../module/muxshare.h"
#include "overlay_bridge.h"
#include "overlay_library.h"
#include "preset_browser.h"
#include "preset_catalogue.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

static int active;
static uint64_t prev_nav_mask;
static char entry_key[PRESET_BROWSER_KEY_MAX];
static preset_browser browser;
static nav_repeat_t rpt_up;
static nav_repeat_t rpt_down;
static mux_dialogue save_dlg;
static mux_dialogue delete_dlg;
static int skip_confirm;

#define NAV_X_BIT BIT(6)
#define NAV_Y_BIT BIT(7)

static uint64_t current_nav_mask(void) {
    return nav_mask_standard() | (mux_input_pressed(mux_input_x) ? NAV_X_BIT : 0)
           | (mux_input_pressed(mux_input_y) ? NAV_Y_BIT : 0);
}

static const preset_browser_row *selected_row(void) {
    return preset_browser_row_at(&browser, current_item_index);
}

static int selected_is_item(void) {
    const preset_browser_row *row = selected_row();
    return row && row->type == preset_browser_row_item;
}

static int selected_is_removable(void) {
    const preset_browser_row *row = selected_row();
    return !browser.collection && row && row->type == preset_browser_row_item
           && overlay_library_is_user(row->item_index);
}

static void refresh_nav(void) {
    const preset_browser_row *row = selected_row();
    nav_show_lr(0);
    lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    if (!row || row->type == preset_browser_row_empty) {
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}}
        );
    } else if (!selected_is_item()) {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.select, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    } else {
        struct nav_bar items[9];
        int count = 0;
        items[count++] = (struct nav_bar) {ui_lbl_nav_a_glyph, "", 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_a, lang.generic.select, 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_b_glyph, "", 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_b, lang.generic.back, 0};
        if (selected_is_removable()) {
            items[count++] = (struct nav_bar) {ui_lbl_nav_x_glyph, "", 0};
            items[count++] = (struct nav_bar) {ui_lbl_nav_x, lang.muxretro.catalogue_screen.delete_label, 0};
        }
        if (browser.collection || !preset_browser_is_collected(preset_browser_overlay, row->key)) {
            items[count++] = (struct nav_bar) {ui_lbl_nav_y_glyph, "", 0};
            items[count++] =
                (struct nav_bar) {ui_lbl_nav_y, browser.collection ? lang.generic.remove : lang.generic.collect, 0};
        }
        items[count] = (struct nav_bar) {NULL, NULL, 0};
        setup_nav(items);
    }
    pause_menu_fix_nav_order();
}

static const char *row_text(const preset_browser_row *row) {
    switch (row->type) {
        case preset_browser_row_download:
            return lang.muxretro.catalogue_screen.downloads;
        case preset_browser_row_none:
            return lang.generic.none;
        case preset_browser_row_collection:
            return lang.muxretro.catalogue_screen.collection;
        case preset_browser_row_empty:
            return lang.muxretro.catalogue_screen.collection_empty;
        default:
            return row->label;
    }
}

static const char *row_glyph(const preset_browser_row *row) {
    if (row->type == preset_browser_row_collection || (row->type == preset_browser_row_empty && browser.collection))
        return "star";

    switch (row->type) {
        case preset_browser_row_download:
            return "download";
        case preset_browser_row_directory:
        case preset_browser_row_empty:
            return "folder";
        default:
            return "overlay";
    }
}

static const char *row_glyph_module(const preset_browser_row *row) {
    return row->type == preset_browser_row_collection || (row->type == preset_browser_row_empty && browser.collection)
               ? "muxtask"
               : "muxretro";
}

static void build_row(const int index) {
    const preset_browser_row *row = preset_browser_row_at(&browser, index);
    if (!row) return;
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);
    const char *text = row_text(row);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, text, 1);
    apply_theme_list_glyph(&theme, icon, row_glyph_module(row), row_glyph(row));
    apply_theme_list_value(&theme, value, row->value);
    apply_size_to_content(&theme, ui_pnl_content, label, icon, text);
    apply_text_long_dot(&theme, label);
    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static int refresh_browser(void) {
    return preset_browser_configure(&browser, overlay_library_count(), overlay_library_key, overlay_library_label);
}

static void rebuild_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    refresh_browser();
    for (int index = 0; index < browser.row_count; index++)
        build_row(index);
    ui_count_static = browser.row_count;
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

static void preview_selected(void) {
    const preset_browser_row *row = selected_row();
    if (!row) return;
    if (row->type == preset_browser_row_none)
        session_settings_set_overlay_image(0);
    else if (row->type == preset_browser_row_item)
        session_settings_set_overlay_image(row->item_index);
}

static void close_screen(void) {
    active = 0;
    settings_menu_reopen_overlay_at(overlay_menu_row_image());
}

static void delete_selected(void) {
    const preset_browser_row *row = selected_row();
    if (!row || row->type != preset_browser_row_item) return;
    char key[PRESET_BROWSER_KEY_MAX];
    snprintf(key, sizeof(key), "%s", row->key);
    if (!overlay_library_delete(row->item_index)) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.delete_failed);
        return;
    }
    if (overlay_library_index(key) < 0) preset_browser_remove_collection(preset_browser_overlay, key);
    session_settings_set_overlay_image(0);
    overlay_bridge_apply();
    const int focus = current_item_index;
    rebuild_rows();
    focus_item(focus < ui_count_static ? focus : ui_count_static - 1);
    preview_selected();
    refresh_nav();
    pause_menu_show_toast(lang.muxretro.catalogue_screen.delete_done);
}

static void toggle_collection(void) {
    const preset_browser_row *row = selected_row();
    if (!row || row->type != preset_browser_row_item) return;
    char key[PRESET_BROWSER_KEY_MAX];
    snprintf(key, sizeof(key), "%s", row->key);
    const int result = preset_browser_toggle_collection(preset_browser_overlay, key);
    if (result < 0) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.collection_failed);
        return;
    }
    const int focus = current_item_index;
    rebuild_rows();
    focus_item(focus < ui_count_static ? focus : ui_count_static - 1);
    refresh_nav();
    pause_menu_show_toast(
        result ? lang.muxretro.catalogue_screen.collection_added : lang.muxretro.catalogue_screen.collection_removed
    );
}

void overlay_image_menu_init(void) {
    preset_catalogue_init();
    preset_browser_init(&browser, preset_browser_overlay);
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

static void reopen_key(const char *key) {
    active = 1;
    prev_nav_mask = current_nav_mask();
    refresh_browser();
    preset_browser_focus_key(&browser, key);
    rebuild_rows();
    focus_item(
        key && strcasecmp(key, "none") != 0 ? preset_browser_find_row(&browser, preset_browser_row_item, key)
                                            : preset_browser_find_row(&browser, preset_browser_row_none, NULL)
    );
    refresh_nav();
}

void overlay_image_menu_reopen(void) {
    reopen_key(overlay_library_key(session_settings.overlay_image));
}

void overlay_image_menu_reopen_download(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();
    refresh_browser();
    preset_browser_open_root(&browser);
    rebuild_rows();
    focus_item(preset_browser_find_row(&browser, preset_browser_row_download, NULL));
    refresh_nav();
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

    const int menu_tap = pause_menu_take_menu_tap();
    if (pause_menu_help_input(edge & BIT(0), edge & BIT(1), menu_tap || edge & (BIT(4) | BIT(5)))) return;

    if (menu_tap && !(dialogue_active(&save_dlg) || dialogue_active(&delete_dlg))) {
        play_sound(snd_info_open);
        show_info_box(lang.muxretro.overlay_screen.image, lang.muxretro.help.screen.overlay_image, 0);
        return;
    }

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
            const int option = save_dlg.selected;
            dialogue_dismiss(&save_dlg);
            if (option != 3) session_settings_apply_save_choice(option);
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
        preview_selected();
        refresh_nav();
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
        preview_selected();
        refresh_nav();
    } else if (nav_page_tick(edge, mask, 2)) {
        preview_selected();
        refresh_nav();
    } else if (edge & NAV_Y_BIT) {
        if (!selected_is_item()) return;
        const preset_browser_row *row = selected_row();
        if (!browser.collection && preset_browser_is_collected(preset_browser_overlay, row->key)) return;
        play_sound(snd_confirm);
        toggle_collection();
    } else if (edge & NAV_X_BIT) {
        if (!selected_is_removable()) return;
        play_sound(snd_confirm);
        if (skip_confirm)
            delete_selected();
        else
            dialogue_open(&delete_dlg, &theme);
    } else if (edge & BIT(4)) {
        const preset_browser_row *row = selected_row();
        if (!row || row->type == preset_browser_row_empty) return;
        play_sound(snd_confirm);
        if (row->type == preset_browser_row_download) {
            preset_catalogue_open(preset_catalogue_overlay);
            return;
        }
        if (row->type == preset_browser_row_directory || row->type == preset_browser_row_collection) {
            char selected_key[PRESET_BROWSER_KEY_MAX] = "";
            if (row->type == preset_browser_row_collection)
                snprintf(selected_key, sizeof(selected_key), "%s", overlay_library_key(session_settings.overlay_image));
            preset_browser_enter(&browser, current_item_index);
            rebuild_rows();
            focus_item(selected_key[0] ? preset_browser_find_row(&browser, preset_browser_row_item, selected_key) : 0);
            preview_selected();
            refresh_nav();
            return;
        }
        preview_selected();
        if (session_settings_is_dirty())
            dialogue_open(&save_dlg, &theme);
        else
            close_screen();
    } else if (edge & BIT(5)) {
        char return_key[PRESET_BROWSER_KEY_MAX];
        play_sound(snd_back);
        if (preset_browser_back(&browser, return_key, sizeof(return_key))) {
            rebuild_rows();
            const enum preset_browser_row_type type =
                strcmp(return_key, "collection") == 0 ? preset_browser_row_collection : preset_browser_row_directory;
            focus_item(preset_browser_find_row(&browser, type, return_key));
            refresh_nav();
            return;
        }
        const int index = overlay_library_index(entry_key);
        session_settings_set_overlay_image(index >= 0 ? index : 0);
        close_screen();
    }
}
