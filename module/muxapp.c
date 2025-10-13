#include "muxshare.h"

typedef struct {
    char *name;
    char *icon;
    char *grid;
    char *help;

    int16_t *(*kiosk_flag)(void);
} mux_apps;

static int16_t *flag_archive(void) { return &kiosk.APPLICATION.ARCHIVE; }

static int16_t *flag_task(void) { return &kiosk.APPLICATION.TASK; }

static mux_apps app[] = {
        {"Archive Manager", "archive", "Archive", "", flag_archive},
        {"Task Toolkit",    "task",    "Toolkit", "", flag_task},
};

const char *app_paths[3];

static inline mux_apps *get_mux_app(const char *name) {
    if (!name) return NULL;

    for (size_t i = 0; i < A_SIZE(app); i++) {
        if (strcasecmp(name, app[i].name) == 0) return &app[i];
    }

    return NULL;
}

static int get_app_base(char *out_base, const char *app_name) {
    for (size_t ab = 0; ab < 3; ab++) {
        if (!app_paths[ab] || app_paths[ab][0] == '\0') continue;

        char app_base[MAX_BUFFER_SIZE];
        if (!ab) {
            snprintf(app_base, sizeof(app_base), "%s",
                     app_paths[ab]);
        } else {
            snprintf(app_base, sizeof(app_base), "%s/%s",
                     app_paths[ab], MUOS_APPS_PATH);
        }

        char launcher[MAX_BUFFER_SIZE];
        snprintf(launcher, sizeof(launcher), "%s/%s/" APP_LAUNCHER, app_base, app_name);

        if (access(launcher, F_OK) == 0) {
            snprintf(out_base, MAX_BUFFER_SIZE, "%s", app_base);
            return 0;
        }
    }

    snprintf(out_base, MAX_BUFFER_SIZE, "%s/%s", device.STORAGE.ROM.MOUNT, MUOS_APPS_PATH);
    return -1;
}

static void show_help(void) {
    char *title = items[current_item_index].name;
    char *message = NULL;

    mux_apps *mux_app = get_mux_app(title);
    if (mux_app && mux_app->help) {
        message = mux_app->help;
    } else {
        char resolved_base[MAX_BUFFER_SIZE];
        get_app_base(resolved_base, title);

        char help_info[MAX_BUFFER_SIZE];
        snprintf(help_info, sizeof(help_info), "%s/%s.sh",
                 resolved_base, title);

        message = get_script_value(help_info, "HELP", lang.GENERIC.NO_HELP);
    }

    show_info_box(TS(title), TS(message), 0);
}

static void init_navigation_group_grid(void) {
    grid_mode_enabled = 1;

    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);

    load_font_section(FONT_PANEL_FOLDER, ui_pnlGrid);
    load_font_section(FONT_PANEL_FOLDER, ui_lblGridCurrentItem);

    for (size_t i = 0; i < item_count; i++) {
        char resolved_base[MAX_BUFFER_SIZE];
        get_app_base(resolved_base, items[i].extra_data);

        char app_launcher[MAX_BUFFER_SIZE];
        snprintf(app_launcher, sizeof(app_launcher), "%s/%s/" APP_LAUNCHER,
                resolved_base, items[i].extra_data);

        const char *glyph_name = NULL;

        mux_apps *mux_app = get_mux_app(items[i].extra_data);
        if (mux_app && mux_app->icon) {
            glyph_name = mux_app->icon;
        } else {
            char app_launcher_icon[MAX_BUFFER_SIZE];
            snprintf(app_launcher_icon, sizeof(app_launcher_icon), "%s/%s/" APP_LAUNCHER,
                    resolved_base, items[i].extra_data);
            glyph_name = get_script_value(app_launcher_icon, "ICON", "app");
        }
        items[i].glyph_icon = strdup(glyph_name);
        
        if (i < theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) {
            update_grid_image_paths(i);

            uint8_t col = i % theme.GRID.COLUMN_COUNT;
            uint8_t row = i / theme.GRID.COLUMN_COUNT;

            lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
            lv_obj_set_user_data(cell_panel, UFI(i));
            lv_obj_t *cell_image = lv_img_create(cell_panel);
            lv_obj_t *cell_label = lv_label_create(cell_panel);

            create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                            items[i].grid_image, items[i].grid_image_focused, items[i].display_name);

            lv_group_add_obj(ui_group, cell_label);
            lv_group_add_obj(ui_group_glyph, cell_image);
            lv_group_add_obj(ui_group_panel, cell_panel);
        }
    }
}

static int append_mux_app(char ***arr, size_t *count, const char *name) {
    for (size_t i = 0; i < *count; i++) {
        if ((*arr)[i] && strcmp((*arr)[i], name) == 0) return 0;
    }

    char **tmp = realloc(*arr, (*count + 1) * sizeof(char *));
    if (!tmp) return -1;
    *arr = tmp;

    (*arr)[*count] = strdup(name);
    if (!(*arr)[*count]) return -1;
    (*count)++;

    return 0;
}

static void create_app_items(void) {
    struct dirent *entry;

    char **dir_names = NULL;
    size_t dir_count = 0;

    app_paths[0] = RUN_SHARE_PATH "application";
    app_paths[1] = device.STORAGE.SDCARD.MOUNT;
    app_paths[2] = device.STORAGE.ROM.MOUNT;

    for (size_t ap = 0; ap < 3; ap++) {
        if (!app_paths[ap] || app_paths[ap][0] == '\0') continue;

        char apps_base[MAX_BUFFER_SIZE];
        if (!ap) {
            snprintf(apps_base, sizeof(apps_base), "%s",
                     app_paths[ap]);
        } else {
            snprintf(apps_base, sizeof(apps_base), "%s/%s",
                     app_paths[ap], MUOS_APPS_PATH);
        }

        DIR *app_dir = opendir(apps_base);
        if (!app_dir) continue;

        while ((entry = readdir(app_dir))) {
            if (entry->d_type != DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char launch_script[MAX_BUFFER_SIZE];
            snprintf(launch_script, sizeof(launch_script), "%s/%s/" APP_LAUNCHER,
                     apps_base, entry->d_name);

            if (access(launch_script, F_OK) == 0) {
                if (append_mux_app(&dir_names, &dir_count, entry->d_name) < 0) {
                    LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
                    closedir(app_dir);
                    goto clean_up;
                }
            }
        }
        closedir(app_dir);
    }

    for (size_t i = 0; i < A_SIZE(app); i++) {
        if (append_mux_app(&dir_names, &dir_count, app[i].name) < 0) {
            LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        }
    }

    if (!dir_names) return;
    qsort(dir_names, dir_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < dir_count; i++) {
        if (!dir_names[i]) continue;

        char resolved_base[MAX_BUFFER_SIZE];
        get_app_base(resolved_base, dir_names[i]);

        char app_launcher[MAX_BUFFER_SIZE];
        snprintf(app_launcher, sizeof(app_launcher), "%s/%s/" APP_LAUNCHER,
                 resolved_base, dir_names[i]);

        char *grid_label = dir_names[i];
        if (theme.GRID.ENABLED) {
            mux_apps *mux_app = get_mux_app(dir_names[i]);
            if (mux_app && mux_app->grid) {
                grid_label = mux_app->grid;
            } else {
                char *from_script = get_script_value(app_launcher, "GRID", dir_names[i]);
                grid_label = from_script ? from_script : dir_names[i];
            }
        }

        char app_store[MAX_BUFFER_SIZE];
        snprintf(app_store, sizeof(app_store), "%s", grid_label);

        add_item(&items, &item_count, dir_names[i], TS(app_store), dir_names[i], ITEM);

        free(dir_names[i]);
    }

    clean_up:
    free(dir_names);

    if (theme.GRID.ENABLED && item_count > 0) {
        init_navigation_group_grid();
        ui_count += (int) item_count;
    } else {
        for (size_t i = 0; i < item_count; i++) {
            lv_obj_t *ui_pnlApp = lv_obj_create(ui_pnlContent);
            if (ui_pnlApp) {
                apply_theme_list_panel(ui_pnlApp);

                lv_obj_t *ui_lblAppItem = lv_label_create(ui_pnlApp);
                if (ui_lblAppItem) apply_theme_list_item(&theme, ui_lblAppItem, TS(items[i].name));

                lv_obj_t *ui_lblAppItemGlyph = lv_img_create(ui_pnlApp);
                if (ui_lblAppItemGlyph) {
                    char resolved_base[MAX_BUFFER_SIZE];
                    get_app_base(resolved_base, items[i].name);

                    char app_icon[MAX_BUFFER_SIZE];
                    snprintf(app_icon, sizeof(app_icon), "%s/%s/" APP_LAUNCHER,
                             resolved_base, items[i].name);

                    char *glyph_name = NULL;

                    mux_apps *mux_app = get_mux_app(items[i].name);
                    if (mux_app && mux_app->icon) {
                        glyph_name = mux_app->icon;
                    } else {
                        glyph_name = get_script_value(app_icon, "ICON", "app");
                    }

                    apply_theme_list_glyph(&theme, ui_lblAppItemGlyph, mux_module, glyph_name);
                    if (lv_img_get_src(ui_lblAppItemGlyph) == NULL) {
                        apply_app_glyph(items[i].name, glyph_name, ui_lblAppItemGlyph);
                    }
                }

                lv_group_add_obj(ui_group, ui_lblAppItem);
                lv_group_add_obj(ui_group_glyph, ui_lblAppItemGlyph);
                lv_group_add_obj(ui_group_panel, ui_pnlApp);

                apply_size_to_content(&theme, ui_pnlContent, ui_lblAppItem, ui_lblAppItemGlyph, TS(items[i].name));
                apply_text_long_dot(&theme, ui_pnlContent, ui_lblAppItem);

                ui_count++;
            }
        }
    }

    if (ui_count > 0) {
        theme.GRID.ENABLED ? lv_obj_update_layout(ui_pnlGrid) : lv_obj_update_layout(ui_pnlContent);
    }
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);

        if (grid_mode_enabled) update_grid(direction);
    }

    if (!grid_mode_enabled) {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL,
                               ui_count, current_item_index, ui_pnlContent);
    }

    if (!grid_mode_enabled) set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    lv_label_set_text(ui_lblGridCurrentItem, TS(items[current_item_index].name));

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

    if (ui_count > 0) {
        int skip_toast = 0;

        for (size_t i = 0; i < A_SIZE(app); i++) {
            if (strcasecmp(items[current_item_index].name, app[i].name) == 0) {
                int16_t *kf = app[i].kiosk_flag ? app[i].kiosk_flag() : NULL;

                if (kf && is_ksk(*kf)) {
                    kiosk_denied();
                    return;
                }

                skip_toast = 1;
                break;
            }
        }

        play_sound(SND_CONFIRM);

        char app_dir[MAX_BUFFER_SIZE];
        char resolved_base[MAX_BUFFER_SIZE];
        get_app_base(resolved_base, items[current_item_index].name);

        if (!skip_toast) {
            snprintf(app_dir, sizeof(app_dir), "%s/%s",
                     resolved_base, items[current_item_index].name);

            toast_message(lang.MUXAPP.LOAD_APP, FOREVER);
            refresh_screen(ui_screen);

            char *assigned_gov = specify_asset(load_content_governor(app_dir, NULL, 0, 1, 1),
                                               device.CPU.DEFAULT, "Governor");

            char *assigned_con = specify_asset(load_content_control_scheme(app_dir, NULL, 0, 1, 1),
                                               "system", "Control Scheme");

            write_text_to_file(MUOS_GOV_LOAD, "w", CHAR, assigned_gov);
            write_text_to_file(MUOS_CON_LOAD, "w", CHAR, assigned_con);
        } else {
            snprintf(app_dir, sizeof(app_dir), "%s",
                     items[current_item_index].extra_data);
        }

        write_text_to_file(MUOS_APP_LOAD, "w", CHAR, app_dir);
        write_text_to_file(MUOS_AIN_LOAD, "w", INT, current_item_index);

        close_input();
        mux_input_stop();
    }
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

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "apps");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void handle_select(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    for (size_t i = 0; i < A_SIZE(app); i++) {
        if (strcasecmp(items[current_item_index].name, app[i].name) == 0) return;
    }

    char resolved_base[MAX_BUFFER_SIZE];
    get_app_base(resolved_base, items[current_item_index].name);

    char app_dir[MAX_BUFFER_SIZE];
    snprintf(app_dir, sizeof(app_dir), "%s/%s",
             resolved_base, items[current_item_index].name);

    load_assign(MUOS_APL_LOAD, items[current_item_index].name, app_dir, "none", 0, 1);

    play_sound(SND_CONFIRM);
    write_text_to_file(MUOS_AIN_LOAD, "w", INT, current_item_index);

    load_mux("appcon");

    close_input();
    mux_input_stop();
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
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.LAUNCH, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL,            NULL,                0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(element_focused, items[current_item_index].name);

            adjust_wallpaper_element(ui_group, 0, APPLICATION);
        }
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxapp_main(void) {
    init_module("muxapp");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXAPP.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();
    create_app_items();

    init_elements();

    int ain_index = 0;
    if (file_exist(MUOS_AIN_LOAD)) {
        ain_index = read_line_int_from(MUOS_AIN_LOAD, 1);
        remove(MUOS_AIN_LOAD);
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, APPLICATION);

    if (ui_count > 0) {
        if (ain_index > -1 && ain_index <= ui_count && current_item_index < ui_count) list_nav_move(ain_index, +1);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXAPP.NO_APP);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_SELECT] = handle_select,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
