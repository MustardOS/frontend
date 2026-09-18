#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <common/platform/audio.h>
#include <common/platform/input.h>
#include <common/ui/common.h>
#include <common/ui/dialogue.h>
#include "../../module/muxshare.h"
#include "colour.h"
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
static mux_dialogue actions_dlg;
static int skip_confirm;

#define NAV_X_BIT BIT(6)
#define NAV_Y_BIT BIT(7)

enum shader_action { shader_action_collect = 0, shader_action_delete, shader_action_cancel };

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
    return !browser.collection && row && row->type == preset_browser_row_item && colour_shader_is_user(row->item_index);
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
        const int collected = preset_browser_is_collected(preset_browser_shader, row->key);
        struct nav_bar items[9];
        int count = 0;
        items[count++] = (struct nav_bar) {ui_lbl_nav_a_glyph, "", 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_a, lang.generic.select, 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_b_glyph, "", 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_b, lang.generic.back, 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_x_glyph, "", 0};
        items[count++] = (struct nav_bar) {ui_lbl_nav_x, lang.muxretro.shader_screen.adjust, 0};
        if (browser.collection || selected_is_removable() || !collected) {
            const char *action = browser.collection ? lang.generic.remove
                                 : selected_is_removable()
                                     ? (collected ? lang.muxretro.catalogue_screen.delete_label : lang.generic.actions)
                                     : lang.generic.collect;
            items[count++] = (struct nav_bar) {ui_lbl_nav_y_glyph, "", 0};
            items[count++] = (struct nav_bar) {ui_lbl_nav_y, action, 0};
        }
        items[count] = (struct nav_bar) {NULL, NULL, 0};
        setup_nav(items);
    }
    pause_menu_fix_nav_order();
}

static const char *shader_cost_label(const enum colour_shader_cost cost) {
    switch (cost) {
        case colour_shader_cost_low:
            return lang.generic.low;
        case colour_shader_cost_medium:
            return lang.generic.medium;
        case colour_shader_cost_high:
            return lang.generic.high;
        default:
            return NULL;
    }
}

static const char *shader_compatibility_label(const enum colour_shader_compatibility compatibility) {
    switch (compatibility) {
        case colour_shader_compatibility_software:
            return lang.muxretro.settings_screen.game_renderer_software;
        case colour_shader_compatibility_hardware:
            return lang.muxretro.settings_screen.game_renderer_hardware;
        default:
            return NULL;
    }
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
            return "shader";
    }
}

static const char *row_glyph_module(const preset_browser_row *row) {
    return row->type == preset_browser_row_collection || (row->type == preset_browser_row_empty && browser.collection)
               ? "muxtask"
               : "muxretro";
}

static void append_metadata(char *output, const size_t output_size, const char *value) {
    if (!value || !*value || !output_size) return;
    const size_t used = strlen(output);
    if (used >= output_size - 1) return;
    snprintf(output + used, output_size - used, "%s%s", used ? " · " : "", value);
}

static void build_row(const int index) {
    const preset_browser_row *row = preset_browser_row_at(&browser, index);
    if (!row) return;
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);
    const char *text = row_text(row);
    char metadata[96] = "";

    if (row->type == preset_browser_row_item) {
        append_metadata(metadata, sizeof(metadata), row->value);
        const char *cost =
            shader_cost_label(colour_shader_cost_for_output(row->item_index, device.mux.width, device.mux.height));
        const char *compatibility = shader_compatibility_label(colour_shader_compatibility_for_index(row->item_index));
        append_metadata(metadata, sizeof(metadata), cost);
        append_metadata(metadata, sizeof(metadata), compatibility);
    }

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, text, 1);
    apply_theme_list_glyph(&theme, icon, row_glyph_module(row), row_glyph(row));
    apply_theme_list_value(&theme, value, metadata);
    apply_size_to_content(&theme, ui_pnl_content, label, icon, text);
    apply_text_long_dot(&theme, label);
    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static int refresh_browser(void) {
    return preset_browser_configure(&browser, colour_shader_count(), colour_shader_key, colour_shader_label);
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
        session_settings_set_colour_shader(0);
    else if (row->type == preset_browser_row_item)
        session_settings_set_colour_shader(row->item_index);
}

static void close_screen(void) {
    active = 0;
    display_menu_reopen_shader();
}

static void delete_selected(void) {
    const preset_browser_row *row = selected_row();
    if (!row || row->type != preset_browser_row_item) return;
    char key[PRESET_BROWSER_KEY_MAX];
    snprintf(key, sizeof(key), "%s", row->key);
    if (!colour_shader_delete(row->item_index)) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.delete_failed);
        return;
    }
    session_settings_set_colour_shader(0);
    colour_init();
    if (colour_shader_index(key) < 0) preset_browser_remove_collection(preset_browser_shader, key);
    submenu_stack_reload_colour_presets();
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
    const int result = preset_browser_toggle_collection(preset_browser_shader, key);
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

static void open_actions(void) {
    const preset_browser_row *row = selected_row();
    if (!row || row->type != preset_browser_row_item) return;
    dialogue_open_at(&actions_dlg, &theme, 0);
}

void shader_menu_init(void) {
    preset_catalogue_init();
    preset_browser_init(&browser, preset_browser_shader);
    shader_adjust_menu_init();
    static const char *save_options[] = {
        lang.muxretro.save.content_save, lang.muxretro.save.core_save, lang.muxretro.save.directory_save,
        lang.muxretro.save.session_save, lang.generic.discard
    };
    dialogue_init(
        &save_dlg, &theme, ui_screen, lang.muxretro.save.display_title, lang.muxretro.save.display_desc, save_options,
        5, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_remove(
        &delete_dlg, &theme, ui_screen, lang.muxretro.catalogue_screen.delete_confirm, lang.generic.select,
        lang.generic.cancel
    );
    static const char *action_options[] = {"", "", ""};
    action_options[shader_action_collect] = lang.generic.collect;
    action_options[shader_action_delete] = lang.muxretro.catalogue_screen.delete_label;
    action_options[shader_action_cancel] = lang.generic.cancel;
    dialogue_init(
        &actions_dlg, &theme, ui_screen, lang.generic.actions, NULL, action_options, 3, lang.generic.select,
        lang.generic.cancel
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

void shader_menu_reopen(void) {
    reopen_key(colour_shader_key(session_settings.colour_shader));
}

void shader_menu_reopen_download(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();
    refresh_browser();
    preset_browser_open_root(&browser);
    rebuild_rows();
    focus_item(preset_browser_find_row(&browser, preset_browser_row_download, NULL));
    refresh_nav();
}

void shader_menu_open(void) {
    snprintf(entry_key, sizeof(entry_key), "%s", colour_shader_key(session_settings.colour_shader));
    shader_menu_reopen();
}

int shader_menu_is_active(void) {
    return active;
}

void shader_menu_tick(void) {
    if (preset_catalogue_tick()) return;
    if (shader_adjust_menu_is_active()) {
        shader_adjust_menu_tick();
        return;
    }

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;
    if (nav_input_halted()) return;

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

    if (dialogue_active(&actions_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&actions_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int option = actions_dlg.selected;
            dialogue_dismiss(&actions_dlg);
            if (option == shader_action_collect) {
                const preset_browser_row *row = selected_row();
                if (row && !preset_browser_is_collected(preset_browser_shader, row->key)) toggle_collection();
            } else if (option == shader_action_delete) {
                if (!selected_is_removable()) {
                    pause_menu_show_toast(lang.muxretro.catalogue_screen.delete_builtin);
                } else if (skip_confirm) {
                    delete_selected();
                } else {
                    dialogue_open(&delete_dlg, &theme);
                }
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&actions_dlg);
            dialogue_dismiss(&actions_dlg);
        }
        return;
    }

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
        const int collected = preset_browser_is_collected(preset_browser_shader, row->key);
        if (browser.collection) {
            play_sound(snd_confirm);
            toggle_collection();
        } else if (selected_is_removable() && collected) {
            play_sound(snd_confirm);
            if (skip_confirm)
                delete_selected();
            else
                dialogue_open(&delete_dlg, &theme);
        } else if (selected_is_removable()) {
            play_sound(snd_confirm);
            open_actions();
        } else if (!collected) {
            play_sound(snd_confirm);
            toggle_collection();
        }
    } else if (edge & NAV_X_BIT) {
        const preset_browser_row *row = selected_row();
        if (!row || row->type != preset_browser_row_item) return;
        session_settings_set_colour_shader(row->item_index);
        if (colour_shader_param_count() > 0) {
            play_sound(snd_confirm);
            shader_adjust_menu_open();
        } else {
            play_sound(snd_error);
            pause_menu_show_toast(lang.muxretro.shader_screen.no_adjust);
        }
    } else if (edge & BIT(4)) {
        const preset_browser_row *row = selected_row();
        if (!row || row->type == preset_browser_row_empty) return;
        play_sound(snd_confirm);
        if (row->type == preset_browser_row_download) {
            preset_catalogue_open(preset_catalogue_shader);
            return;
        }
        if (row->type == preset_browser_row_directory || row->type == preset_browser_row_collection) {
            char selected_key[PRESET_BROWSER_KEY_MAX] = "";
            if (row->type == preset_browser_row_collection)
                snprintf(selected_key, sizeof(selected_key), "%s", colour_shader_key(session_settings.colour_shader));
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
        const int index = colour_shader_index(entry_key);
        session_settings_set_colour_shader(index >= 0 ? index : 0);
        close_screen();
    }
}
