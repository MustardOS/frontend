#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static bool is_directory = false;

static int is_app = 0;

static void show_help(void) {
    show_info_box(lang.MUXCONTROL.TITLE, lang.MUXCONTROL.HELP, 0);
}

static void write_control_file(char *path, const char *control, char *log) {
    FILE *file = fopen(path, "w");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path);
        return;
    }

    LOG_INFO(mux_module, "%s: %s", log, control);

    fprintf(file, "%s", control);
    fclose(file);
}

static void assign_control_single(char *core_dir, const char *control, char *rom) {
    char control_path[MAX_BUFFER_SIZE];
    snprintf(control_path, sizeof(control_path), "%s/%s.con", core_dir, strip_ext(rom));

    if (file_exist(control_path)) remove(control_path);

    write_control_file(control_path, control, "Assign Control (Single)");
}

static void assign_control_directory(char *core_dir, const char *control, int purge) {
    if (purge) delete_files_of_type(core_dir, ".con", NULL, 0);

    char control_path[MAX_BUFFER_SIZE];
    snprintf(control_path, sizeof(control_path), INFO_COR_PATH "/%s/core.con",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(control_path);

    write_control_file(control_path, control, "Assign Control (Directory)");
}

static void assign_control_parent(char *core_dir, const char *control) {
    delete_files_of_type(core_dir, ".con", NULL, 1);
    assign_control_directory(core_dir, control, 0);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char control_path[MAX_BUFFER_SIZE];
        snprintf(control_path, sizeof(control_path), "%s%s/core.con", core_dir, subdirs[i]);

        create_directories(strip_dir(control_path), 0);
        write_control_file(control_path, control, "Assign Control (Recursive)");
    }

    free_subdirectories(subdirs);
}

static void create_control_assignment(const char *control, char *rom, enum gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];

    if (is_app) {
        snprintf(core_dir, sizeof(core_dir), "%s/",
                 rom_dir);
    } else {
        snprintf(core_dir, sizeof(core_dir), INFO_COR_PATH "/%s/",
                 get_last_subdir(rom_dir, '/', 4));
    }

    remove_double_slashes(core_dir);
    create_directories(core_dir, 0);

    switch (method) {
        case SINGLE:
            assign_control_single(core_dir, control, rom);
            break;
        case PARENT:
            assign_control_parent(core_dir, control);
            break;
        case DIRECTORY:
            assign_control_directory(core_dir, control, 1);
            break;
        case DIRECTORY_NO_WIPE:
        default:
            assign_control_directory(core_dir, control, 0);
            break;
    }

    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);
}

static void generate_available_controls(const char *default_control) {
    DIR *cd;
    struct dirent *cf;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), INFO_GCD_PATH);

    cd = opendir(assign_dir);
    if (!cd) return;

    while ((cf = readdir(cd))) {
        if (cf->d_type == DT_REG) {
            char *last_dot = strrchr(cf->d_name, '.');
            if (last_dot && strcasecmp(last_dot, ".txt") == 0) {
                *last_dot = '\0';
                add_item(&items, &item_count, cf->d_name, cf->d_name, "", ITEM);
            }
        }
    }

    closedir(cd);
    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        char *cap_name = str_capital_all(items[i].display_name);
        char *raw_name = str_tolower(str_trim(strdup(items[i].display_name)));

        lv_obj_t *ui_pnlControl = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlControl);

        lv_obj_set_user_data(ui_pnlControl, raw_name);

        lv_obj_t *ui_lblControlItem = lv_label_create(ui_pnlControl);
        apply_theme_list_item(&theme, ui_lblControlItem, cap_name);

        lv_obj_t *ui_lblControlItemGlyph = lv_img_create(ui_pnlControl);

        const char *glyph =
                strcasecmp(raw_name, default_control) == 0 ? "system" :
                strcasecmp(raw_name, "system") == 0 ? "system" :
                strcasecmp(raw_name, "retro") == 0 ? "retro" :
                strcasecmp(raw_name, "modern") == 0 ? "modern" :
                "default";
        apply_theme_list_glyph(&theme, ui_lblControlItemGlyph, mux_module, glyph);

        lv_group_add_obj(ui_group, ui_lblControlItem);
        lv_group_add_obj(ui_group_glyph, ui_lblControlItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlControl);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblControlItem, ui_lblControlItemGlyph, cap_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblControlItem);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free_items(&items, &item_count);
    }
}

static void create_control_items(const char *target) {
    if (strcmp(target, "none") == 0) generate_available_controls(target);

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN "/%s",
             target);

    char global_assign[FILENAME_MAX];
    snprintf(global_assign, sizeof(global_assign), "%s/global.ini", assign_dir);

    mini_t *global_config = mini_load(global_assign);

    char *target_default = get_ini_string(global_config, "global", "name", "none");
    if (strcmp(target_default, "none") == 0) return;

    char local_assign[FILENAME_MAX];
    snprintf(local_assign, sizeof(local_assign), "%s/%s.ini", assign_dir, target_default);
    mini_t *local_config = mini_load(local_assign);

    char *use_control;
    char *local_control = get_ini_string(local_config, target_default, "control", "none");
    if (strcmp(local_control, "none") != 0) {
        use_control = local_control;
    } else {
        use_control = get_ini_string(global_config, "global", "control", "system");
    }

    char default_control[FILENAME_MAX];
    strncpy(default_control, use_control, sizeof(default_control));
    default_control[sizeof(default_control) - 1] = '\0';

    mini_free(global_config);
    mini_free(local_config);

    generate_available_controls(default_control);
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call || is_directory) return;

    LOG_INFO(mux_module, "Single Control Assignment Triggered");
    play_sound(SND_CONFIRM);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_control_assignment(selected, is_app ? "mux_option" : rom_name, SINGLE);

    if (is_app) load_mux("appcon");

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

    play_sound(SND_BACK);
    remove(MUOS_SAG_LOAD);

    if (is_app) load_mux("appcon");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || is_app || hold_call) return;

    LOG_INFO(mux_module, "Directory Control Assignment Triggered");
    play_sound(SND_CONFIRM);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_control_assignment(selected, rom_name, DIRECTORY);

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || is_app || at_base(rom_dir, "ROMS") || hold_call) return;

    LOG_INFO(mux_module, "Parent Control Assignment Triggered");
    play_sound(SND_CONFIRM);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_control_assignment(selected, rom_name, PARENT);

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
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

    struct nav_bar nav_items[9];
    int i = 0;

    if (!is_directory) {
        nav_items[i++] = (struct nav_bar) {ui_lblNavAGlyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lblNavA, lang.GENERIC.INDIVIDUAL, 1};
    }
    nav_items[i++] = (struct nav_bar) {ui_lblNavBGlyph, "", 0};
    nav_items[i++] = (struct nav_bar) {ui_lblNavB, lang.GENERIC.BACK, 0};

    if (!is_app) {
        nav_items[i++] = (struct nav_bar) {ui_lblNavXGlyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lblNavX, lang.GENERIC.DIRECTORY, 1};

        if (!at_base(rom_dir, "ROMS")) {
            nav_items[i++] = (struct nav_bar) {ui_lblNavYGlyph, "", 1};
            nav_items[i++] = (struct nav_bar) {ui_lblNavY, lang.GENERIC.RECURSIVE, 1};
        }
    }

    nav_items[i] = (struct nav_bar) {NULL, NULL, 0};  // Null-terminate

    setup_nav(nav_items);

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

int muxcontrol_main(int auto_assign, char *name, char *dir, char *sys, int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_directory = directory_exist(rom_dir) && !app;
    if (!is_directory) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    is_app = app;

    init_module(__func__);

    if (is_app) {
        LOG_INFO(mux_module, "Assign Control APP_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Control APP_DIR: \"%s\"", rom_dir);
    } else {
        LOG_INFO(mux_module, "Assign Control ROM_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Control ROM_DIR: \"%s\"", rom_dir);
        LOG_INFO(mux_module, "Assign Control ROM_SYS: \"%s\"", rom_system);
    }

    if (auto_assign && !file_exist(MUOS_SAG_LOAD) && !is_app) {
        LOG_INFO(mux_module, "Automatic Assign Control Initiated");

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%s/core.con",
                 get_last_subdir(rom_dir, '/', 4));
        remove_double_slashes(core_file);

        if (file_exist(core_file)) {
            close_input();
            return 0;
        }

        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        if (json_valid(read_all_char_from(assign_file))) {
            static char assign_check[MAX_BUFFER_SIZE];
            snprintf(assign_check, sizeof(assign_check), "%s",
                     str_tolower(get_last_dir(rom_dir)));
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(
                    json_parse(read_all_char_from(assign_file)),
                    assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "\tCore Assigned: %s", ass_config);

                char assigned_global[MAX_BUFFER_SIZE];
                snprintf(assigned_global, sizeof(assigned_global), STORE_LOC_ASIN "/%s/global.ini",
                         ass_config);

                LOG_INFO(mux_module, "\tObtaining Core INI: %s", assigned_global);

                mini_t *global_ini = mini_load(assigned_global);

                static char def_control[MAX_BUFFER_SIZE];
                strcpy(def_control, get_ini_string(global_ini, "global", "control", "none"));

                static char def_sys[MAX_BUFFER_SIZE];
                strcpy(def_sys, get_ini_string(global_ini, "global", "default", "none"));

                if (strcmp(def_control, "none") != 0) {
                    char default_core[MAX_BUFFER_SIZE];
                    snprintf(default_core, sizeof(default_core), STORE_LOC_ASIN "/%s/%s.ini",
                             ass_config, def_sys);

                    static char core_control[MAX_BUFFER_SIZE];
                    mini_t *local_ini = mini_load(default_core);

                    char *use_local_control = get_ini_string(local_ini, def_sys, "control", "none");
                    if (strcmp(use_local_control, "none") != 0) {
                        strcpy(core_control, use_local_control);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Control: %s", core_control);
                    } else {
                        strcpy(core_control, get_ini_string(global_ini, "global", "control", "system"));
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Control: %s", core_control);
                    }

                    mini_free(local_ini);

                    create_control_assignment(core_control, rom_name, DIRECTORY_NO_WIPE);
                    LOG_SUCCESS(mux_module, "\tControl Assignment Successful");
                } else {
                    LOG_INFO(mux_module, "\tAssigned Control To Default: %s", "system");
                    create_control_assignment("system", rom_name, DIRECTORY_NO_WIPE);
                }

                mini_free(global_ini);

                close_input();
                return 0;
            } else {
                LOG_INFO(mux_module, "\tAssigned Control To Default: %s", "system");
                create_control_assignment("system", rom_name, DIRECTORY_NO_WIPE);

                close_input();
                return 0;
            }
        }
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    init_fonts();

    if (strcasecmp(rom_system, "none") == 0 && !is_app) {
        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        if (json_valid(read_all_char_from(assign_file))) {
            static char assign_check[MAX_BUFFER_SIZE];
            snprintf(assign_check, sizeof(assign_check), "%s",
                     str_tolower(get_last_dir(rom_dir)));
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(
                    json_parse(read_all_char_from(assign_file)),
                    assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "<Obtaining System> Core Assigned: %s", ass_config);
                snprintf(rom_system, sizeof(rom_system), "%s", strip_ext(ass_config));
            }
        }
    }

    char title[MAX_BUFFER_SIZE];
    snprintf(title, sizeof(title), "%s - %s", lang.MUXCONTROL.TITLE, get_last_dir(dir));
    lv_label_set_text(ui_lblTitle, title);

    create_control_items(rom_system);
    init_elements();

    if (ui_count > 0) {
        LOG_SUCCESS(mux_module, "%d Control%s Detected", ui_count, ui_count == 1 ? "" : "s");
        list_nav_next(0);
    } else {
        LOG_ERROR(mux_module, "No Controls Detected!");
        lv_label_set_text(ui_lblScreenMessage, lang.MUXCONTROL.NONE);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
