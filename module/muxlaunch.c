#include "muxshare.h"
#include "ui/ui_muxlaunch.h"

static int confirm_mode = 0;
static mux_dialogue confirm_dlg;
static int pending_power_action = 0;

static void show_confirm_dialog(const char *title) {
    confirm_mode = 1;
    confirm_dlg.selected = mux_confirm_nah;
    lv_label_set_text(confirm_dlg.title_label, title);
    dialogue_show(&confirm_dlg);
    dialogue_refresh(&confirm_dlg, &theme);
}

static void hide_confirm_dialog(void) {
    confirm_mode = 0;
    dialogue_hide(&confirm_dlg);
}

#define LAUNCH(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(LAUNCH_ELEMENTS) };
#undef LAUNCH

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define LAUNCH(NAME, UDATA) {UDATA, lang.muxlaunch.help.NAME},
        LAUNCH_ELEMENTS
#undef LAUNCH
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *item_labels[] = {lang.muxlaunch.explore, lang.muxlaunch.collection, lang.muxlaunch.history,
                           lang.muxlaunch.apps,    lang.muxlaunch.info,       lang.muxlaunch.config,
                           lang.muxlaunch.reboot,  lang.muxlaunch.shutdown};

    char *item_labels_short[] = {lang.muxlaunch.abbr.explore, lang.muxlaunch.abbr.collection,
                                 lang.muxlaunch.abbr.history, lang.muxlaunch.abbr.app,
                                 lang.muxlaunch.abbr.info,    lang.muxlaunch.abbr.config,
                                 lang.muxlaunch.abbr.reboot,  lang.muxlaunch.abbr.shutdown};

    char *glyph_names[] = {"explore", "collection", "history", "apps", "info", "config", "reboot", "shutdown"};

    reset_ui_groups();

    if (theme.grid.enabled) {
        list_nav_move(init_grid_static(ui_count_dynamic, item_labels, item_labels_short, glyph_names), +1);
    } else {
        INIT_STATIC_ITEM(-1, launch, explore, item_labels[0], glyph_names[0], 0);
        INIT_STATIC_ITEM(-1, launch, collection, item_labels[1], glyph_names[1], 0);
        INIT_STATIC_ITEM(-1, launch, history, item_labels[2], glyph_names[2], 0);
        INIT_STATIC_ITEM(-1, launch, apps, item_labels[3], glyph_names[3], 0);
        INIT_STATIC_ITEM(-1, launch, info, item_labels[4], glyph_names[4], 0);
        INIT_STATIC_ITEM(-1, launch, config, item_labels[5], glyph_names[5], 0);
        INIT_STATIC_ITEM(-1, launch, reboot, item_labels[6], glyph_names[6], 0);
        INIT_STATIC_ITEM(-1, launch, shutdown, item_labels[7], glyph_names[7], 0);

        add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);
        list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
    }
}

static void list_nav_move(const int steps, const int direction) {
    if (first_open) {
        first_open = 0;
    } else {
        nav_unsuppress_shake();
        play_sound(snd_navigate);
    }

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_dynamic - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_dynamic - 1 ? 0 : current_item_index + 1;
        }

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        if (grid_mode_enabled) update_grid(direction);
    }

    if (!grid_mode_enabled) {
        update_scroll_position(
            theme.mux.item.count, theme.mux.item.panel, ui_count_dynamic, current_item_index, ui_pnl_content
        );
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    } else {
        lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].name);
    }

    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (confirm_mode) {
        const mux_confirm_opt opt = (mux_confirm_opt) confirm_dlg.selected;
        hide_confirm_dialog();

        if (opt == mux_confirm_yep) {
            if (pending_power_action == 1) {
                toast_message(lang.generic.rebooting, tst_wait_f);
                load_mux("reboot");
            } else {
                toast_message(lang.generic.shutting_down, tst_wait_f);
                load_mux("shutdown");
            }

            mux_input_stop();
        }

        return;
    }

    static int16_t kiosk_pass = 0;

    typedef enum {
        menu_general,
        menu_content,
        menu_reboot,
        menu_shutdown,
    } menu_action;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action menu_action;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"explore", &kiosk.launch.explore, menu_content},
        {"collection", &kiosk.launch.collection, menu_content},
        {"history", &kiosk.launch.history, menu_content},
        {"app", &kiosk.launch.application, menu_general},
        {"info", &kiosk.launch.information, menu_general},
        {"config", &kiosk.launch.configuration, menu_general},
        {"reboot", &kiosk_pass, menu_reboot},
        {"shutdown", &kiosk_pass, menu_shutdown},
    };

    if ((unsigned) current_item_index >= ui_count_dynamic) return;
    const menu_entry *entry = &entries[current_item_index];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    switch (entry->menu_action) {
        case menu_general:
            play_sound(snd_confirm);
            break;
        case menu_content:
            invalidate_catalogue_cache();
            play_sound(snd_confirm);
            break;
        case menu_reboot:
            if (!config.settings.advanced.trust_power) {
                pending_power_action = 1;
                show_confirm_dialog(lang.muxlaunch.confirm_reboot);
                return;
            }
            toast_message(lang.generic.rebooting, tst_wait_f);
            break;
        case menu_shutdown:
            if (!config.settings.advanced.trust_power) {
                pending_power_action = 2;
                show_confirm_dialog(lang.muxlaunch.confirm_shutdown);
                return;
            }
            toast_message(lang.generic.shutting_down, tst_wait_f);
            break;
        default:
            return;
    }

    load_mux(entry->mux_name);

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (confirm_mode) {
        hide_confirm_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    load_mux("launcher");
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (current_item_index == 5) {
        play_sound(snd_confirm);

        load_mux("kiosk");
        mux_input_stop();
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call || confirm_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    if (msgbox_active) return;

    if (confirm_mode) {
        if (!swap_axis) {
            dialogue_navigate(&confirm_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    set_nav_input_dir(nav_dir_up);

    // Grid mode.  Wrap on Row.
    if (theme.grid.enabled && theme.grid.navigation_type == 4 && !get_grid_column_index(current_item_index)) {
        list_nav_next(
            get_grid_row_index(current_item_index) == grid_info.last_row_index ? grid_info.last_row_item_count - 1
                                                                               : grid_info.column_count - 1
        );
        // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    } else if (!theme.grid.enabled && theme.misc.navigation_type == 4
               && (!current_item_index || current_item_index == 4)) {
        list_nav_next(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.misc.navigation_type == 5 && !current_item_index) {
        list_nav_next(2);
    } else if (theme.misc.navigation_type == 5 && current_item_index == 3) {
        list_nav_next(4);
        // Regular Navigation
    } else {
        list_nav_prev(1);
    }
}

static void handle_down(void) {
    if (msgbox_active) return;

    if (confirm_mode) {
        if (!swap_axis) {
            dialogue_navigate(&confirm_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    set_nav_input_dir(nav_dir_down);

    // Grid Navigation.  Wrap on Row.
    if (theme.grid.enabled && theme.grid.navigation_type == 4
        && get_grid_column_index(current_item_index) == get_grid_row_item_count(current_item_index) - 1) {
        list_nav_prev(get_grid_row_item_count(current_item_index) - 1);
        // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    } else if (!theme.grid.enabled && theme.misc.navigation_type == 4
               && (current_item_index == 3 || current_item_index == 7)) {
        list_nav_prev(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.misc.navigation_type == 5 && current_item_index == 2) {
        list_nav_prev(2);
    } else if (theme.misc.navigation_type == 5 && current_item_index == 7) {
        list_nav_prev(4);
        // Regular Navigation
    } else {
        list_nav_next(1);
    }
}

static void handle_up_hold(void) { // prev
    if (msgbox_active || confirm_mode) return;

    // Don't wrap around when scrolling on hold.
    if ((theme.grid.enabled && theme.grid.navigation_type == 4 && get_grid_column_index(current_item_index) > 0)
        || (theme.grid.enabled && theme.grid.navigation_type < 4 && current_item_index > 0)
        || (!theme.grid.enabled && theme.misc.navigation_type == 4 && current_item_index > 0 && current_item_index <= 3)
        || (!theme.grid.enabled && theme.misc.navigation_type == 4 && current_item_index > 4)
        || (theme.misc.navigation_type == 5 && current_item_index > 0 && current_item_index <= 2)
        || (theme.misc.navigation_type == 5 && current_item_index > 3)
        || (theme.misc.navigation_type < 4 && current_item_index > 0) || is_carousel_grid_mode()) {
        handle_up();
    }
}

static void handle_down_hold(void) { // next
    if (msgbox_active || confirm_mode) return;

    // Don't wrap around when scrolling on hold.
    if ((theme.grid.enabled && theme.grid.navigation_type == 4
         && get_grid_column_index(current_item_index) < get_grid_row_item_count(current_item_index) - 1)
        || (theme.grid.enabled && theme.grid.navigation_type < 4 && current_item_index < ui_count_dynamic - 1)
        || (!theme.grid.enabled && theme.misc.navigation_type == 4 && current_item_index < ui_count_dynamic - 1
            && current_item_index > 3)
        || (!theme.grid.enabled && theme.misc.navigation_type == 4 && current_item_index < 3)
        || (theme.misc.navigation_type == 5 && current_item_index < ui_count_dynamic - 1 && current_item_index > 2)
        || (theme.misc.navigation_type == 5 && current_item_index < 2)
        || (theme.misc.navigation_type < 4 && current_item_index < ui_count_dynamic - 1) || is_carousel_grid_mode()) {
        handle_down();
    }
}

static void handle_left(void) {
    if (msgbox_active) return;

    if (confirm_mode) {
        if (swap_axis) {
            dialogue_navigate(&confirm_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    set_nav_input_dir(nav_dir_left);
    if (is_carousel_grid_mode() && theme.grid.row_count == 1) {
        list_nav_prev(1);
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.grid.enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)) {
        const int column_index = current_item_index % grid_info.column_count;

        if (current_item_index < grid_info.column_count) {
            list_nav_prev(LV_MAX(column_index + 1, grid_info.last_row_item_count));
        } else {
            list_nav_prev(grid_info.column_count);
        }
    } else if (!theme.grid.enabled && (theme.misc.navigation_type == 2 || theme.misc.navigation_type == 4)) {
        list_nav_prev(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.misc.navigation_type == 3 || theme.misc.navigation_type == 5) {
        switch (current_item_index) {
            case 3:
            case 4:
                list_nav_prev(3);
                break;
            case 5:
                list_nav_prev(4);
                break;
            case 6:
            case 7:
                list_nav_prev(5);
                break;
            default:
                break;
        }
    }
}

static void handle_right(void) {
    if (msgbox_active) return;

    if (confirm_mode) {
        if (swap_axis) {
            dialogue_navigate(&confirm_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    set_nav_input_dir(nav_dir_right);
    if (is_carousel_grid_mode() && theme.grid.row_count == 1) {
        list_nav_next(1);
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.grid.enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)) {
        const uint8_t row_index = current_item_index / grid_info.column_count;

        // when on 2nd to last row do not go past last item
        if (row_index == grid_info.last_row_index - 1) {
            const int new_item_index = LV_MIN(current_item_index + grid_info.column_count, ui_count_dynamic - 1);
            list_nav_next(new_item_index - current_item_index);
            // when on the last row only move based on amount of items in that row
        } else if (row_index == grid_info.last_row_index) {
            list_nav_next(grid_info.last_row_item_count);
        } else {
            list_nav_next(grid_info.column_count);
        }
    } else if (!theme.grid.enabled && (theme.misc.navigation_type == 2 || theme.misc.navigation_type == 4)) {
        list_nav_next(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.misc.navigation_type == 3 || theme.misc.navigation_type == 5) {
        switch (current_item_index) {
            case 0:
                list_nav_next(3);
                break;
            case 1:
                list_nav_next(4);
                break;
            case 2:
                list_nav_next(5);
                break;
            default:
                break;
        }
    }
}

static void handle_left_hold(void) {
    if (msgbox_active || confirm_mode) return;

    // Don't wrap around when scrolling on hold.
    if (grid_mode_enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)
        && (get_grid_row_index(current_item_index) > 0 || is_carousel_grid_mode())) {
        handle_left();
    }
}

static void handle_right_hold(void) {
    if (msgbox_active || confirm_mode) return;

    // Don't wrap around when scrolling on hold.
    if (grid_mode_enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)
        && (get_grid_row_index(current_item_index) < grid_info.last_row_index || is_carousel_grid_mode())) {
        handle_right();
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav(
        (struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0}, {ui_lbl_nav_a, lang.generic.select, 0}, {NULL, NULL, 0}}
    );

#define LAUNCH(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_launch, UDATA);
    LAUNCH_ELEMENTS
#undef LAUNCH

    overlay_display();
}

int muxlaunch_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxlaunch.title);
    init_muxlaunch(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();
    init_navigation_group();

    dialogue_init_confirm(
        &confirm_dlg, &theme, ui_screen, lang.muxlaunch.confirm_reboot, NULL, lang.generic.confirm, lang.generic.cancel,
        lang.generic.select, lang.generic.back
    );

    adjust_wallpaper_element(ui_group, 0, wall_general);

    init_timer(ui_gen_refresh_task, NULL);

    if (file_exist(MUOS_SYS_LOAD)) remove(MUOS_SYS_LOAD);

    mux_input_options input_opts = {
        .swap_axis = (theme.grid.enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5)
                     || (!theme.grid.enabled && theme.misc.navigation_type >= 1 && theme.misc.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_x] = handle_x,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
        }
    };

    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
