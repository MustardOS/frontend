#include "muxshare.h"
#include "ui/ui_muxinstall.h"

#define UI_COUNT 4

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblRtc_install,      lang.MUXINSTALL.HELP.DATETIME},
            {ui_lblLanguage_install, lang.MUXINSTALL.HELP.LANGUAGE},
            {ui_lblShutdown_install, lang.MUXINSTALL.HELP.SHUTDOWN},
            {ui_lblInstall_install,  lang.MUXINSTALL.HELP.INSTALL},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_navigation_group_grid(lv_obj_t *ui_objects[], char *item_labels[], char *glyph_names[]) {
    init_grid_info(UI_COUNT, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, UI_COUNT);

    load_font_section(FONT_PANEL_FOLDER, ui_pnlGrid);
    load_font_section(FONT_PANEL_FOLDER, ui_lblGridCurrentItem);

    for (int i = 0; i < UI_COUNT; i++) {
        uint8_t col = i % theme.GRID.COLUMN_COUNT;
        uint8_t row = i / theme.GRID.COLUMN_COUNT;

        lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
        lv_obj_set_user_data(cell_panel, strdup(item_labels[i]));

        lv_obj_t *cell_image = lv_img_create(cell_panel);

        lv_obj_t *cell_label = lv_label_create(cell_panel);
        lv_obj_set_user_data(cell_label, glyph_names[i]);

        ui_objects[i] = cell_label;

        char grid_img[MAX_BUFFER_SIZE];
        load_element_image_specifics(STORAGE_THEME, mux_dimension, mux_module, "grid", glyph_names[i],
                                     "default", "png", grid_img, sizeof(grid_img));

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_names[i]);

        char grid_img_foc[MAX_BUFFER_SIZE];
        load_element_image_specifics(STORAGE_THEME, mux_dimension, mux_module, "grid", glyph_name_focused,
                                     "default_focused", "png", grid_img_foc, sizeof(grid_img_foc));

        create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row, grid_img, grid_img_foc, item_labels[i]);

        lv_group_add_obj(ui_group, cell_label);
        lv_group_add_obj(ui_group_glyph, cell_image);
        lv_group_add_obj(ui_group_panel, cell_panel);
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *item_labels[] = {lang.MUXINSTALL.DATETIME,
                           lang.MUXINSTALL.LANGUAGE,
                           lang.MUXINSTALL.SHUTDOWN,
                           lang.MUXINSTALL.INSTALL};

    char *item_labels_short[] = {lang.MUXINSTALL.SHORT.DATETIME,
                                 lang.MUXINSTALL.SHORT.LANGUAGE,
                                 lang.MUXINSTALL.SHORT.SHUTDOWN,
                                 lang.MUXINSTALL.SHORT.INSTALL};

    char *glyph_names[] = {"clock",
                           "language",
                           "shutdown",
                           "install"};

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    if (theme.GRID.ENABLED) {
        init_navigation_group_grid(ui_objects, item_labels_short, glyph_names);
    } else {
        INIT_STATIC_ITEM(-1, install, Rtc, item_labels[0], glyph_names[0], 0);
        INIT_STATIC_ITEM(-1, install, Language, item_labels[1], glyph_names[1], 0);
        INIT_STATIC_ITEM(-1, install, Shutdown, item_labels[2], glyph_names[2], 0);
        INIT_STATIC_ITEM(-1, install, Install, item_labels[3], glyph_names[3], 0);

        for (unsigned int i = 0; i < ui_count; i++) {
            lv_group_add_obj(ui_group, ui_objects[i]);
            lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
            lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
        }
    }

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? UI_COUNT - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == UI_COUNT - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    if (theme.GRID.ENABLED) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    lv_label_set_text(ui_lblGridCurrentItem, lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));

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

    struct {
        const char *glyph_name;
        const char *mux_name;
    } elements[] = {
            {"clock",    "rtc"},
            {"language", "language"},
            {"shutdown", "shutdown"},
            {"install",  "install"}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (!strcasecmp(u_data, elements[i].glyph_name)) {
            if (strcmp(elements[i].mux_name, "fti-shutdown") != 0) {
                play_sound(SND_CONFIRM);
            } else {
                toast_message(lang.GENERIC.SHUTTING_DOWN, FOREVER);
                refresh_screen(ui_screen);
            }

            load_mux(elements[i].mux_name);

            break;
        }
    }

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
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
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index > 0)) {
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
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index < UI_COUNT - 1)) {
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

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {NULL, NULL,                           0}
    });

#define INSTALL(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_install, UDATA);
    INSTALL_ELEMENTS
#undef INSTALL

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxinstall_main(void) {
    init_module("muxinstall");
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXINSTALL.TITLE);
    init_muxinstall(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();
    init_navigation_group();

    adjust_wallpaper_element(ui_group, 0, GENERAL);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.GRID.ENABLED && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5) ||
                         (!theme.GRID.ENABLED && theme.MISC.NAVIGATION_TYPE >= 1 && theme.MISC.NAVIGATION_TYPE <= 5),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
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
                    [MUX_INPUT_L2] = hold_call_set,
            }
    };
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
