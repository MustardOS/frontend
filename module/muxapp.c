#include "muxshare.h"

typedef struct {
    char *name;
    char *icon;
    char *grid;
    char *help;

    int16_t *(*kiosk_flag)(void);
} mux_apps;

static int16_t *flag_archive(void) {
    return &kiosk.application.archive;
}

static int16_t *flag_task(void) {
    return &kiosk.application.task;
}

static mux_apps app[] = {
    {"Archive Manager", "archive", "Archive", "", flag_archive},
    {"Task Toolkit", "task", "Toolkit", "", flag_task},
};

const char *app_paths[3];

static mux_apps *get_mux_app(const char *name) {
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
            snprintf(app_base, sizeof(app_base), "%s", app_paths[ab]);
        } else {
            snprintf(app_base, sizeof(app_base), "%s/%s", app_paths[ab], MUOS_APPS_PATH);
        }

        char launcher[MAX_BUFFER_SIZE];
        snprintf(launcher, sizeof(launcher), "%s/%s/" APP_LAUNCHER, app_base, app_name);

        if (access(launcher, F_OK) == 0) {
            snprintf(out_base, MAX_BUFFER_SIZE, "%s", app_base);
            return 0;
        }
    }

    snprintf(out_base, MAX_BUFFER_SIZE, "%s/%s", device.storage.rom.mount, MUOS_APPS_PATH);
    return -1;
}

static void show_help(void) {
    const char *item_name = get_last_dir(strdup(items[current_item_index].extra_data));
    const mux_apps *mux_app = get_mux_app(item_name);

    if (mux_app && mux_app->help) {
        char message[MAX_BUFFER_SIZE];
        snprintf(message, sizeof(message), "%s", mux_app->help);
        show_info_box(TRS(items[current_item_index].name), TRS(message), 0);
    } else {
        char app_lang_file[FILENAME_MAX];
        snprintf(app_lang_file, sizeof(app_lang_file), "%s/" APP_LANGUAGE, items[current_item_index].extra_data);

        char app_help[MAX_BUFFER_SIZE];
        if (file_exist(app_lang_file)) {
            LOG_SUCCESS(mux_module, "Loading Application Translation: %s", app_lang_file);

            mini_t *app_lang = mini_load(app_lang_file);
            snprintf(
                app_help, sizeof(app_help), "%s",
                get_ini_string(app_lang, "help", config.settings.general.language, TRS(lang.generic.no_help))
            );

            mini_free(app_lang);
        } else {
            LOG_WARN(mux_module, "No Application Translation Found: %s", app_lang_file);
            snprintf(app_help, sizeof(app_help), "%s", lang.generic.no_help);
        }

        show_info_box(TRS(items[current_item_index].name), app_help, 0);
    }
}

static int append_mux_app(char ***arr, size_t *count, size_t *cap, const char *name) {
    for (size_t i = 0; i < *count; i++) {
        if ((*arr)[i] && strcmp((*arr)[i], name) == 0) return 0;
    }

    if (*count >= *cap) {
        const size_t new_cap = *cap ? *cap * 2 : 16;
        char **tmp = realloc(*arr, new_cap * sizeof(char *));

        if (!tmp) return -1;

        *arr = tmp;
        *cap = new_cap;
    }

    (*arr)[*count] = strdup(name);
    if (!(*arr)[*count]) return -1;
    (*count)++;

    return 0;
}

static void gen_app_label(const size_t index) {
    lv_obj_t *ui_pnl_app = lv_obj_create(ui_pnl_content);
    if (!ui_pnl_app) return;

    apply_theme_list_panel(ui_pnl_app);

    lv_obj_t *ui_lbl_app_item = lv_label_create(ui_pnl_app);
    if (ui_lbl_app_item) {
        apply_theme_list_item(&theme, ui_lbl_app_item, items[index].display_name);
        lv_group_add_obj(ui_group, ui_lbl_app_item);
    }

    lv_obj_t *ui_lbl_app_item_glyph = lv_img_create(ui_pnl_app);
    if (ui_lbl_app_item_glyph) {
        apply_theme_list_glyph(&theme, ui_lbl_app_item_glyph, mux_module, items[index].glyph_icon);
        if (config.visual.list_glyph && lv_img_get_src(ui_lbl_app_item_glyph) == NULL)
            apply_app_glyph(items[index].extra_data, items[index].glyph_icon, ui_lbl_app_item_glyph);
        lv_group_add_obj(ui_group_glyph, ui_lbl_app_item_glyph);
    }

    lv_group_add_obj(ui_group_panel, ui_pnl_app);

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_app_item, ui_lbl_app_item_glyph, items[index].display_name);
    apply_text_long_dot(&theme, ui_lbl_app_item);
}

static void create_app_items(void) {
    struct dirent *entry;

    char **dir_names = NULL;
    size_t dir_count = 0;
    size_t dir_cap = 0;

    app_paths[0] = OPT_SHARE_PATH "application";
    app_paths[1] = device.storage.sdcard.mount;
    app_paths[2] = device.storage.rom.mount;

    for (size_t ap = 0; ap < 3; ap++) {
        if (!app_paths[ap] || app_paths[ap][0] == '\0') continue;

        char apps_base[MAX_BUFFER_SIZE];
        if (ap == 0) {
            snprintf(apps_base, sizeof(apps_base), "%s", app_paths[ap]);
        } else {
            snprintf(apps_base, sizeof(apps_base), "%s/%s", app_paths[ap], MUOS_APPS_PATH);
        }

        DIR *app_dir = opendir(apps_base);
        if (!app_dir) continue;

        while ((entry = readdir(app_dir))) {
            if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char launch_script[MAX_BUFFER_SIZE];
            snprintf(launch_script, sizeof(launch_script), "%s/%s/" APP_LAUNCHER, apps_base, entry->d_name);

            if (access(launch_script, F_OK) != 0) continue;

            if (append_mux_app(&dir_names, &dir_count, &dir_cap, entry->d_name) < 0) {
                LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
                closedir(app_dir);
                goto clean_up;
            }
        }

        closedir(app_dir);
    }

    for (size_t i = 0; i < A_SIZE(app); i++) {
        if (append_mux_app(&dir_names, &dir_count, &dir_cap, app[i].name) < 0) {
            LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
            goto clean_up;
        }
    }

    if (!dir_names) return;

    qsort(dir_names, dir_count, sizeof(char *), str_compare);

    reset_ui_groups();

    for (size_t i = 0; i < dir_count; i++) {
        if (!dir_names[i]) continue;

        char resolved_base[MAX_BUFFER_SIZE];
        get_app_base(resolved_base, dir_names[i]);

        char app_folder[MAX_BUFFER_SIZE];
        snprintf(app_folder, sizeof(app_folder), "%s/%s", resolved_base, dir_names[i]);

        char app_launcher[MAX_BUFFER_SIZE];
        snprintf(app_launcher, sizeof(app_launcher), "%s/%s/" APP_LAUNCHER, resolved_base, dir_names[i]);

        mux_apps *mux_app = get_mux_app(dir_names[i]);

        char default_full_name[MAX_BUFFER_SIZE];
        char default_grid_name[MAX_BUFFER_SIZE];

        char full_app_name[MAX_BUFFER_SIZE];
        char grid_app_name[MAX_BUFFER_SIZE];

        snprintf(default_full_name, sizeof(default_full_name), "%s", TRS(dir_names[i]));

        if (mux_app && mux_app->grid) {
            snprintf(default_grid_name, sizeof(default_grid_name), "%s", TRS(mux_app->grid));
        } else {
            snprintf(default_grid_name, sizeof(default_grid_name), "%s", TRS(dir_names[i]));
        }

        snprintf(full_app_name, sizeof(full_app_name), "%s", default_full_name);
        snprintf(grid_app_name, sizeof(grid_app_name), "%s", default_grid_name);

        char app_lang_file[MAX_BUFFER_SIZE];
        snprintf(app_lang_file, sizeof(app_lang_file), "%s/%s/" APP_LANGUAGE, resolved_base, dir_names[i]);

        if (file_exist(app_lang_file)) {
            LOG_SUCCESS(mux_module, "Loading Application Translation: %s", app_lang_file);

            mini_t *app_lang = mini_load(app_lang_file);
            if (app_lang) {
                snprintf(
                    full_app_name, sizeof(full_app_name), "%s",
                    get_ini_string(app_lang, "full", config.settings.general.language, default_full_name)
                );

                snprintf(
                    grid_app_name, sizeof(grid_app_name), "%s",
                    get_ini_string(app_lang, "grid", config.settings.general.language, default_grid_name)
                );

                mini_free(app_lang);
            } else {
                LOG_WARN(mux_module, "Failed Loading Application Translation: %s", app_lang_file);
            }
        } else {
            LOG_WARN(mux_module, "No Application Translation Found: %s", app_lang_file);
        }

        if (file_exist(app_launcher)) {
            char *from_script = get_script_value(app_launcher, "GRID", "");
            if (from_script && from_script[0] != '\0')
                snprintf(grid_app_name, sizeof(grid_app_name), "%s", from_script);
        }

        const char *glyph_name = "app";
        if (mux_app && mux_app->icon) {
            glyph_name = mux_app->icon;
        } else if (file_exist(app_launcher)) {
            glyph_name = get_script_value(app_launcher, "ICON", "app");
        }

        content_item *new_item = add_item(
            &items, &item_count, full_app_name, theme.grid.enabled ? grid_app_name : full_app_name, app_folder, ITEM
        );

        if (new_item) {
            new_item->glyph_icon = strdup(glyph_name ? glyph_name : "app");
            if (!new_item->glyph_icon) LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        }

        free(dir_names[i]);
        dir_names[i] = NULL;
    }

clean_up:
    if (dir_names) {
        for (size_t i = 0; i < dir_count; i++) {
            free(dir_names[i]);
        }

        free(dir_names);
    }

    if (theme.grid.enabled && item_count > 0) {
        init_grid_dynamic(NULL, NULL);
        ui_count_static += (int) item_count;
    } else {
        ui_count_static += (int) item_count;

        const size_t limit = theme.mux.item.count;
        for (size_t i = 0; i < item_count && i < limit; i++) {
            gen_app_label(i);
        }
    }

    if (ui_count_static > 0)
        theme.grid.enabled ? lv_obj_update_layout(ui_pnl_grid) : lv_obj_update_layout(ui_pnl_content);
}

static void check_focus(void) {
    const char *item_name = get_last_dir(strdup(items[current_item_index].extra_data));
    const mux_apps *mux_app = get_mux_app(item_name);

    if (mux_app) {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, items[index].display_name);

    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, items[index].glyph_icon);
    if (config.visual.list_glyph && lv_img_get_src(ui_lbl_item_glyph) == NULL)
        apply_app_glyph(items[index].extra_data, items[index].glyph_icon, ui_lbl_item_glyph);

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, items[index].display_name);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void update_list_items(const int start_index) {
    list_win_update_items(start_index, update_list_item);
}

static void focus_initial(void) {
    list_win_focus_initial(update_list_item);

    lv_label_set_text(ui_lbl_grid_current_item, TRS(items[current_item_index].name));

    check_focus();
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    const int visible_count = theme.mux.item.count;
    const int static_list = !grid_mode_enabled && (int) item_count <= visible_count;
    const int multi_list = !grid_mode_enabled && (int) item_count > visible_count;

    if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

    if (static_list) {
        for (int step = 0; step < steps; ++step) {
            list_win_move_index(direction);
        }
        list_win_focus_group(current_item_index);
    } else {
        for (int step = 0; step < steps; ++step) {
            list_win_move_index(direction);

            if (!is_carousel_grid_mode()) {
                nav_move(ui_group, direction);
                nav_move(ui_group_glyph, direction);
                nav_move(ui_group_panel, direction);
            }

            if (multi_list) {
                update_windowed_list(
                    ui_pnl_content, direction, current_item_index, (int) item_count, visible_count, update_list_item,
                    update_list_items
                );
            } else if (grid_mode_enabled) {
                update_grid(direction);
            }

            if (multi_list) list_win_focus_group(list_win_focus_index());
        }
    }

    if (!grid_mode_enabled) set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    lv_label_set_text(ui_lbl_grid_current_item, TRS(items[current_item_index].name));

    nav_moved = 1;
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (ui_count_static > 0) {
        int skip_toast = 0;

        char *extra_data_dup = strdup(items[current_item_index].extra_data);
        char *item_name = get_last_dir(extra_data_dup);
        for (size_t i = 0; i < A_SIZE(app); i++) {
            if (strcasecmp(item_name, app[i].name) == 0) {
                const int16_t *kf = app[i].kiosk_flag ? app[i].kiosk_flag() : NULL;

                if (kf && is_ksk(*kf)) {
                    kiosk_denied();
                    free(extra_data_dup);
                    return;
                }

                skip_toast = 1;
                break;
            }
        }

        play_sound(snd_confirm);

        if (!skip_toast) {
            toast_message(lang.muxapp.load_app, tst_wait_f);

            char *assigned_gov = specify_asset(
                load_content_governor(items[current_item_index].extra_data, NULL, 0, 1, 1), device.cpu.dflt, "Governor"
            );

            char *assigned_con = specify_asset(
                load_content_control_scheme(items[current_item_index].extra_data, NULL, 0, 1, 1), "system",
                "Control Scheme"
            );

            write_text_to_file(MUOS_GOV_LOAD, "w", CHAR, assigned_gov);
            write_text_to_file(MUOS_CON_LOAD, "w", CHAR, assigned_con);

            free(assigned_gov);
            free(assigned_con);

            fade_out_screen();
        }

        write_text_to_file(MUOS_APP_LOAD, "w", CHAR, skip_toast ? item_name : items[current_item_index].extra_data);
        write_text_to_file(MUOS_AIN_LOAD, "w", INT, current_item_index);

        free(extra_data_dup);

        mux_input_stop();
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "apps");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    const char *item_name = get_last_dir(strdup(items[current_item_index].extra_data));

    for (size_t i = 0; i < A_SIZE(app); i++) {
        if (strcasecmp(item_name, app[i].name) == 0) return;
    }

    load_assign(MUOS_APL_LOAD, item_name, items[current_item_index].extra_data, "none", 0, 1);

    play_sound(snd_confirm);
    write_text_to_file(MUOS_AIN_LOAD, "w", INT, current_item_index);

    load_mux("appcon");

    mux_input_stop();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.launch, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.option, 0},
                                  {NULL, NULL, 0}});

    check_focus();
    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
            char *item_name = get_last_dir(strdup(items[current_item_index].extra_data));
            lv_obj_set_user_data(e_focused, item_name);

            adjust_wallpaper_element(ui_group, 0, wall_application);
        }
        adjust_gen_panel();

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxapp_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxapp.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();
    create_app_items();

    init_elements();

    int ain_index = 0;
    if (file_exist(MUOS_AIN_LOAD)) {
        ain_index = read_line_int_from(MUOS_AIN_LOAD, 1);
        remove(MUOS_AIN_LOAD);
    }

    char *item_name = get_last_dir(strdup(items[current_item_index].extra_data));
    lv_obj_set_user_data(lv_group_get_focused(ui_group), item_name);
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_application);

    if (ui_count_static > 0) {
        if (ain_index > -1 && ain_index <= ui_count_static && current_item_index < ui_count_static) {
            if (grid_mode_enabled) {
                list_nav_move(ain_index, +1);
            } else {
                current_item_index = ain_index;
                focus_initial();
            }
        }

        first_open = 0;
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxapp.no_app);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1
                     || (grid_mode_enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_dpad_left] = handle_list_nav_left,
                [mux_input_dpad_right] = handle_list_nav_right,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_dpad_left] = handle_list_nav_left_hold,
            [mux_input_dpad_right] = handle_list_nav_right_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
