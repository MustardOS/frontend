#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static bool is_dir = false;

static int is_app = 0;

static void show_help(void) {
    show_info_box(lang.MUXRAOPT.TITLE, lang.MUXRAOPT.HELP, 0);
}

static void write_rac_file(char *path, const char *rac, char *log) {
    FILE *file = fopen(path, "w");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path);
        return;
    }

    LOG_INFO(mux_module, "%s: %s", log, rac);

    fprintf(file, "%s", rac);
    fclose(file);
}

static void assign_rac_single(char *core_dir, const char *rac, char *rom) {
    char rac_path[MAX_BUFFER_SIZE];
    snprintf(rac_path, sizeof(rac_path), "%s/%s.rac", core_dir, strip_ext(rom));

    if (file_exist(rac_path)) remove(rac_path);

    write_rac_file(rac_path, rac, "Assign RetroArch Config (Single)");
}

static void assign_rac_directory(char *core_dir, const char *rac, int purge) {
    if (purge) delete_files_of_type(core_dir, ".rac", NULL, 0);

    char rac_path[MAX_BUFFER_SIZE];
    snprintf(rac_path, sizeof(rac_path), INFO_COR_PATH "/%s/core.rac",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(rac_path);

    write_rac_file(rac_path, rac, "Assign RetroArch Config (Directory)");
}

static void assign_rac_parent(char *core_dir, const char *rac) {
    delete_files_of_type(core_dir, ".rac", NULL, 1);
    assign_rac_directory(core_dir, rac, 0);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char rac_path[MAX_BUFFER_SIZE];
        snprintf(rac_path, sizeof(rac_path), "%s%s/core.rac", core_dir, subdirs[i]);

        create_directories(strip_dir(rac_path), 0);
        write_rac_file(rac_path, rac, "Assign RetroArch Config (Recursive)");
    }

    free_subdirectories(subdirs);
}

static void create_rac_assignment(const char *rac, char *rom, enum gen_type method) {
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
            assign_rac_single(core_dir, rac, rom);
            break;
        case PARENT:
            assign_rac_parent(core_dir, rac);
            break;
        case DIRECTORY:
            assign_rac_directory(core_dir, rac, 1);
            break;
        case DIRECTORY_NO_WIPE:
        default:
            assign_rac_directory(core_dir, rac, 0);
            break;
    }

    if (file_exist(MUOS_SAR_LOAD)) remove(MUOS_SAR_LOAD);
}

static void create_rac_items(void) {
    reset_ui_groups();
    ui_count = 0;

    const struct {
        const char *label;
        const char *value;
    } ra_opts[] = {
            {lang.GENERIC.ENABLED,  "true"},
            {lang.GENERIC.DISABLED, "false"},
    };

    for (size_t i = 0; i < A_SIZE(ra_opts); i++) {
        ui_count++;

        lv_obj_t *pnl = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(pnl);

        lv_obj_t *lbl = lv_label_create(pnl);
        apply_theme_list_item(&theme, lbl, ra_opts[i].label);

        lv_obj_set_user_data(lbl, (void *) ra_opts[i].value);

        lv_obj_t *glyph = lv_img_create(pnl);
        apply_theme_list_glyph(&theme, glyph, mux_module, ra_opts[i].value);

        lv_group_add_obj(ui_group, lbl);
        lv_group_add_obj(ui_group_glyph, glyph);
        lv_group_add_obj(ui_group_panel, pnl);

        apply_size_to_content(&theme, ui_pnlContent, lbl, glyph, ra_opts[i].label);
        apply_text_long_dot(&theme, ui_pnlContent, lbl);
    }

    lv_obj_update_layout(ui_pnlContent);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call || is_dir) return;

    LOG_INFO(mux_module, "Single RetroArch Config Assignment Triggered");
    play_sound(SND_CONFIRM);

    const char *selected = lv_obj_get_user_data(lv_group_get_focused(ui_group));
    create_rac_assignment(selected, is_app ? "mux_option" : rom_name, SINGLE);

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
    remove(MUOS_SAR_LOAD);

    if (is_app) load_mux("appcon");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || is_app || hold_call) return;

    LOG_INFO(mux_module, "Directory RetroArch Config Assignment Triggered");
    play_sound(SND_CONFIRM);

    const char *selected = lv_obj_get_user_data(lv_group_get_focused(ui_group));
    create_rac_assignment(selected, rom_name, DIRECTORY);

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || is_app || at_base(rom_dir, "ROMS") || hold_call) return;

    LOG_INFO(mux_module, "Parent RetroArch Config Assignment Triggered");
    play_sound(SND_CONFIRM);

    const char *selected = lv_obj_get_user_data(lv_group_get_focused(ui_group));
    create_rac_assignment(selected, rom_name, PARENT);

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    struct nav_bar nav_items[9];
    int i = 0;

    if (!is_dir) {
        nav_items[i++] = (struct nav_bar) {ui_lblNavAGlyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lblNavA, lang.GENERIC.CONTENT, 1};
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

int muxraopt_main(int auto_assign, char *name, char *dir, char *sys, int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_dir = dir_exist(rom_dir) && !app;
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    is_app = app;

    init_module(__func__);

    if (is_app) {
        LOG_INFO(mux_module, "Assign RetroArch Config APP_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign RetroArch Config APP_DIR: \"%s\"", rom_dir);
    } else {
        LOG_INFO(mux_module, "Assign RetroArch Config ROM_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign RetroArch Config ROM_DIR: \"%s\"", rom_dir);
        LOG_INFO(mux_module, "Assign RetroArch Config ROM_SYS: \"%s\"", rom_system);
    }

    if (auto_assign && !file_exist(MUOS_SAR_LOAD) && !is_app) {
        LOG_INFO(mux_module, "Automatic Assign RetroArch Config Initiated");

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%s/core.rac",
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

                static char def_rac[MAX_BUFFER_SIZE];
                strcpy(def_rac, get_ini_string(global_ini, "global", "retroarch", "false"));

                static char def_sys[MAX_BUFFER_SIZE];
                strcpy(def_sys, get_ini_string(global_ini, "global", "default", "false"));

                if (strcmp(def_rac, "false") != 0) {
                    char default_core[MAX_BUFFER_SIZE];
                    snprintf(default_core, sizeof(default_core), STORE_LOC_ASIN "/%s/%s.ini",
                             ass_config, def_sys);

                    static char core_retroarch[MAX_BUFFER_SIZE];
                    mini_t *local_ini = mini_load(default_core);

                    char *use_local_retroarch = get_ini_string(local_ini, def_sys, "retroarch", "false");
                    if (strcmp(use_local_retroarch, "false") != 0) {
                        strcpy(core_retroarch, use_local_retroarch);
                        LOG_INFO(mux_module, "\t(LOCAL) Core RetroArch Config: %s", core_retroarch);
                    } else {
                        strcpy(core_retroarch, get_ini_string(global_ini, "global", "retroarch", "false"));
                        LOG_INFO(mux_module, "\t(GLOBAL) Core RetroArch Config: %s", core_retroarch);
                    }

                    mini_free(local_ini);

                    create_rac_assignment(core_retroarch, rom_name, DIRECTORY_NO_WIPE);
                    LOG_SUCCESS(mux_module, "\tRetroArch Config Assignment Successful");
                } else {
                    LOG_INFO(mux_module, "\tAssigned RetroArch Config To Default: %s", "false");
                    create_rac_assignment("false", rom_name, DIRECTORY_NO_WIPE);
                }

                mini_free(global_ini);

                close_input();
                return 0;
            } else {
                LOG_INFO(mux_module, "\tAssigned RetroArch Config To Default: %s", "false");
                create_rac_assignment("false", rom_name, DIRECTORY_NO_WIPE);

                close_input();
                return 0;
            }
        }
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXRAOPT.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);
    init_fonts();

    create_rac_items();
    init_elements();

    list_nav_next(0);
    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input = {
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
    init_input(&input, true);
    mux_input_task(&input);

    return 0;
}
