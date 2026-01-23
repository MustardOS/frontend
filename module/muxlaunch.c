#include "muxshare.h"
#include "ui/ui_muxlaunch.h"

#define UI_COUNT 8

static void list_nav_move(int steps, int direction);

static void show_help() {
    struct {
        char *title;
        char *content;
    } help_messages[] = {
            {lang.MUXLAUNCH.EXPLORE,    lang.MUXLAUNCH.HELP.EXPLORE},
            {lang.MUXLAUNCH.COLLECTION, lang.MUXLAUNCH.HELP.COLLECTION},
            {lang.MUXLAUNCH.HISTORY,    lang.MUXLAUNCH.HELP.HISTORY},
            {lang.MUXLAUNCH.APP,        lang.MUXLAUNCH.HELP.APP},
            {lang.MUXLAUNCH.INFO,       lang.MUXLAUNCH.HELP.INFO},
            {lang.MUXLAUNCH.CONFIG,     lang.MUXLAUNCH.HELP.CONFIG},
            {lang.MUXLAUNCH.REBOOT,     lang.MUXLAUNCH.HELP.REBOOT},
            {lang.MUXLAUNCH.SHUTDOWN,   lang.MUXLAUNCH.HELP.SHUTDOWN},
    };

    show_info_box(help_messages[current_item_index].title, help_messages[current_item_index].content, 0);
}

static void init_navigation_group_grid(char *item_labels[], char *item_grid_labels[], char *glyph_names[]) {
    grid_mode_enabled = 1;

    init_grid_info(UI_COUNT, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, UI_COUNT);

    load_font_section(FONT_PANEL_DIR, ui_pnlGrid);
    load_font_section(FONT_PANEL_DIR, ui_lblGridCurrentItem);

    char prev_dir[MAX_BUFFER_SIZE];
    snprintf(prev_dir, sizeof(prev_dir), "%s", (file_exist(MUOS_PDI_LOAD)) ? read_all_char_from(MUOS_PDI_LOAD) : "");

    int steps = 0;
    for (int i = 0; i < UI_COUNT; i++) {
        if (strcasecmp(glyph_names[i], prev_dir) == 0) steps = i;

        char grid_img[MAX_BUFFER_SIZE];
        load_element_image_specifics(mux_dimension, mux_module, "grid", glyph_names[i],
                                     "default", "png", grid_img, sizeof(grid_img));

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_names[i]);

        char grid_img_foc[MAX_BUFFER_SIZE];
        load_element_image_specifics(mux_dimension, mux_module, "grid", glyph_name_focused,
                                     "default_focused", "png", grid_img_foc, sizeof(grid_img_foc));

        content_item *new_item = add_item(&items, &item_count, item_labels[i], item_grid_labels[i], "", ITEM);

        new_item->glyph_icon = strdup(glyph_names[i]);
        new_item->grid_image = strdup(grid_img);
        new_item->grid_image_focused = strdup(grid_img_foc);
    }

    if (is_carousel_grid_mode()) {
        create_carousel_grid();
    } else {
        for (int i = 0; i < item_count; i++) {
            if (i < theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) gen_grid_item(i);
        }
    }

    list_nav_move(steps, +1);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *item_labels[] = {lang.MUXLAUNCH.EXPLORE,
                           lang.MUXLAUNCH.COLLECTION,
                           lang.MUXLAUNCH.HISTORY,
                           lang.MUXLAUNCH.APP,
                           lang.MUXLAUNCH.INFO,
                           lang.MUXLAUNCH.CONFIG,
                           lang.MUXLAUNCH.REBOOT,
                           lang.MUXLAUNCH.SHUTDOWN};

    char *item_labels_short[] = {lang.MUXLAUNCH.SHORT.EXPLORE,
                                 lang.MUXLAUNCH.SHORT.COLLECTION,
                                 lang.MUXLAUNCH.SHORT.HISTORY,
                                 lang.MUXLAUNCH.SHORT.APP,
                                 lang.MUXLAUNCH.SHORT.INFO,
                                 lang.MUXLAUNCH.SHORT.CONFIG,
                                 lang.MUXLAUNCH.SHORT.REBOOT,
                                 lang.MUXLAUNCH.SHORT.SHUTDOWN};

    char *glyph_names[] = {"explore",
                           "collection",
                           "history",
                           "apps",
                           "info",
                           "config",
                           "reboot",
                           "shutdown"};

    reset_ui_groups();

    if (theme.GRID.ENABLED) {
        init_navigation_group_grid(item_labels, item_labels_short, glyph_names);
    } else {
        INIT_STATIC_ITEM(-1, launch, Explore, item_labels[0], glyph_names[0], 0);
        INIT_STATIC_ITEM(-1, launch, Collection, item_labels[1], glyph_names[1], 0);
        INIT_STATIC_ITEM(-1, launch, History, item_labels[2], glyph_names[2], 0);
        INIT_STATIC_ITEM(-1, launch, Apps, item_labels[3], glyph_names[3], 0);
        INIT_STATIC_ITEM(-1, launch, Info, item_labels[4], glyph_names[4], 0);
        INIT_STATIC_ITEM(-1, launch, Config, item_labels[5], glyph_names[5], 0);
        INIT_STATIC_ITEM(-1, launch, Reboot, item_labels[6], glyph_names[6], 0);
        INIT_STATIC_ITEM(-1, launch, Shutdown, item_labels[7], glyph_names[7], 0);

        add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, false);
        list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? UI_COUNT - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == UI_COUNT - 1) ? 0 : current_item_index + 1;
        }

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        if (grid_mode_enabled) update_grid(direction);
    }

    if (!grid_mode_enabled) {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    } else {
        lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].name);
    }

    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    static int16_t KIOSK_PASS = 0;

    typedef enum {
        MENU_GENERAL = 0,
        MENU_REBOOT,
        MENU_SHUTDOWN,
    } menu_action;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action menu_action;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {"explore",    &kiosk.LAUNCH.EXPLORE,       MENU_GENERAL},
            {"collection", &kiosk.LAUNCH.COLLECTION,    MENU_GENERAL},
            {"history",    &kiosk.LAUNCH.HISTORY,       MENU_GENERAL},
            {"app",        &kiosk.LAUNCH.APPLICATION,   MENU_GENERAL},
            {"info",       &kiosk.LAUNCH.INFORMATION,   MENU_GENERAL},
            {"config",     &kiosk.LAUNCH.CONFIGURATION, MENU_GENERAL},
            {"reboot",     &KIOSK_PASS,                 MENU_REBOOT},
            {"shutdown",   &KIOSK_PASS,                 MENU_SHUTDOWN},
    };

    if ((unsigned) current_item_index >= UI_COUNT) return;
    const menu_entry *entry = &entries[current_item_index];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    switch (entry->menu_action) {
        case MENU_GENERAL:
            play_sound(SND_CONFIRM);
            break;
        case MENU_REBOOT:
            toast_message(lang.GENERIC.REBOOTING, FOREVER);
            break;
        case MENU_SHUTDOWN:
            toast_message(lang.GENERIC.SHUTTING_DOWN, FOREVER);
            break;
        default:
            return;
    }

    load_mux(entry->mux_name);

    close_input();
    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    load_mux("launcher");
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void handle_up(void) {
    if (msgbox_active) return;

    // Grid mode.  Wrap on Row.
    if (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 && !get_grid_column_index(current_item_index)) {
        list_nav_next(get_grid_row_index(current_item_index) == grid_info.last_row_index ?
                      grid_info.last_row_item_count - 1 : grid_info.column_count - 1);
        // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    } else if (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 &&
               (!current_item_index || current_item_index == 4)) {
        list_nav_next(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && !current_item_index) {
        list_nav_next(2);
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 3) {
        list_nav_next(4);
        // Regular Navigation
    } else {
        list_nav_prev(1);
    }
}

static void handle_down(void) {
    if (msgbox_active) return;

    // Grid Navigation.  Wrap on Row.
    if (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 &&
        get_grid_column_index(current_item_index) == get_grid_row_item_count(current_item_index) - 1) {
        list_nav_prev(get_grid_row_item_count(current_item_index) - 1);
        // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    } else if (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 &&
               (current_item_index == 3 || current_item_index == 7)) {
        list_nav_prev(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 2) {
        list_nav_prev(2);
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 7) {
        list_nav_prev(4);
        // Regular Navigation
    } else {
        list_nav_next(1);
    }
}

static void handle_up_hold(void) {//prev
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if ((theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 && get_grid_column_index(current_item_index) > 0) ||
        (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE < 4 && current_item_index > 0) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index > 0 && current_item_index <= 3) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index > 4) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index > 0 && current_item_index <= 2) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index > 3) ||
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index > 0) ||
        is_carousel_grid_mode()) {
        handle_up();
    }
}

static void handle_down_hold(void) {//next
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if ((theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE == 4 &&
         get_grid_column_index(current_item_index) < get_grid_row_item_count(current_item_index) - 1) ||
        (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE < 4 && current_item_index < UI_COUNT - 1) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index < UI_COUNT - 1 &&
         current_item_index > 3) ||
        (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE == 4 && current_item_index < 3) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index < UI_COUNT - 1 && current_item_index > 2) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index < 2) ||
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index < UI_COUNT - 1) ||
        is_carousel_grid_mode()) {
        handle_down();
    }
}

static void handle_left(void) {
    if (msgbox_active) return;

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.GRID.ENABLED && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4)) {
        int column_index = current_item_index % grid_info.column_count;

        if (current_item_index < grid_info.column_count) {
            list_nav_prev(LV_MAX(column_index + 1, grid_info.last_row_item_count));
        } else {
            list_nav_prev(grid_info.column_count);
        }
    } else if (!theme.GRID.ENABLED && (theme.MISC.NAVIGATION_TYPE == 2 || theme.MISC.NAVIGATION_TYPE == 4)) {
        list_nav_prev(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.MISC.NAVIGATION_TYPE == 3 || theme.MISC.NAVIGATION_TYPE == 5) {
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

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.GRID.ENABLED && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4)) {
        uint8_t row_index = current_item_index / grid_info.column_count;

        //when on 2nd to last row do not go past last item
        if (row_index == grid_info.last_row_index - 1) {
            int new_item_index = LV_MIN(current_item_index + grid_info.column_count, UI_COUNT - 1);
            list_nav_next(new_item_index - current_item_index);
            //when on the last row only move based on amount of items in that row
        } else if (row_index == grid_info.last_row_index) {
            list_nav_next(grid_info.last_row_item_count);
        } else {
            list_nav_next(grid_info.column_count);
        }
    } else if (!theme.GRID.ENABLED && (theme.MISC.NAVIGATION_TYPE == 2 || theme.MISC.NAVIGATION_TYPE == 4)) {
        list_nav_next(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.MISC.NAVIGATION_TYPE == 3 || theme.MISC.NAVIGATION_TYPE == 5) {
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
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if (grid_mode_enabled && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4) &&
        (get_grid_row_index(current_item_index) > 0 ||
         is_carousel_grid_mode())) {
        handle_left();
    }
}

static void handle_right_hold(void) {
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if (grid_mode_enabled && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4) &&
        (get_grid_row_index(current_item_index) < grid_info.last_row_index ||
         is_carousel_grid_mode())) {
        handle_right();
    }
}

static void launch_kiosk(void) {
    if (msgbox_active || hold_call) return;

    if (current_item_index == 5) { /* config */
        load_mux("kiosk");

        close_input();
        mux_input_stop();
    }
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {NULL, NULL,                           0}
    });

#define LAUNCH(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_launch, UDATA);
    LAUNCH_ELEMENTS
#undef LAUNCH

    overlay_display();
}

int muxlaunch_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXLAUNCH.TITLE);
    init_muxlaunch(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();
    init_navigation_group();

    adjust_wallpaper_element(ui_group, 0, GENERAL);

    init_timer(ui_gen_refresh_task, NULL);

    if (file_exist(MUOS_SYS_LOAD)) remove(MUOS_SYS_LOAD);

    mux_input_options input_opts = {
            .swap_axis = (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5) ||
                         (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE >= 1 && theme.MISC.NAVIGATION_TYPE <= 5),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L2] = hold_call_set,
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_L1) | BIT(MUX_INPUT_R2) | BIT(MUX_INPUT_Y),
                            .press_handler = launch_kiosk,
                            .hold_handler = launch_kiosk,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright_up,
                            .hold_handler = ui_common_handle_bright_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright_down,
                            .hold_handler = ui_common_handle_bright_down,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright_up,
                            .hold_handler = ui_common_handle_bright_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright_down,
                            .hold_handler = ui_common_handle_bright_down,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_volume_up,
                            .hold_handler = ui_common_handle_volume_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_volume_down,
                            .hold_handler = ui_common_handle_volume_down,
                    },
            }
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
