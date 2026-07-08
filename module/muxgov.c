#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static int is_dir = 0;

static int is_app = 0;

static void show_help(void) {
    show_info_box(lang.muxgov.title, lang.muxgov.help, 0);
}

static void create_gov_assignment(const char *gov, const char *rom, const enum gen_type method) {
    create_marker_assignment("gov", "Assign Governor", gov, rom, rom_dir, is_app, method);

    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);
}

static void generate_available_governors(const char *default_governor) {
    int governor_count;
    char **governors = str_parse_file(device.cpu.available, &governor_count, parse_tokens);
    if (!governors) return;

    for (int i = 0; i < governor_count; ++i)
        add_item(&items, &item_count, governors[i], governors[i], "", ITEM);
    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        const char *cap_name = str_capital(items[i].display_name);
        char *raw_name = str_tolower(str_remchar(str_trim(strdup(items[i].display_name)), ' '));

        lv_obj_t *ui_pnl_gov = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_gov);

        lv_obj_t *ui_lbl_gov_item = lv_label_create(ui_pnl_gov);
        apply_theme_list_item(&theme, ui_lbl_gov_item, cap_name);

        lv_obj_t *ui_lbl_gov_item_glyph = lv_img_create(ui_pnl_gov);

        const char *glyph = strcasecmp(raw_name, default_governor) == 0 ? "default" : str_remchar(raw_name, ' ');
        apply_theme_list_glyph(&theme, ui_lbl_gov_item_glyph, mux_module, glyph);
        free(raw_name);

        lv_group_add_obj(ui_group, ui_lbl_gov_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_gov_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_gov);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_gov_item, ui_lbl_gov_item_glyph, cap_name);
        apply_text_long_dot(&theme, ui_lbl_gov_item);
    }

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        free_items(&items, &item_count);
    }
}

static void create_gov_items(const char *target) {
    if (strcmp(target, "none") == 0) generate_available_governors(target);

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN "/%s", target);

    char global_assign[FILENAME_MAX];
    snprintf(global_assign, sizeof(global_assign), "%s/global.ini", assign_dir);

    mini_t *global_config = mini_load(global_assign);

    char *target_default = get_ini_string(global_config, "global", "name", "none");
    if (strcmp(target_default, "none") == 0) return;

    char local_assign[FILENAME_MAX];
    snprintf(local_assign, sizeof(local_assign), "%s/%s.ini", assign_dir, target_default);
    mini_t *local_config = mini_load(local_assign);

    char *use_governor;
    char *local_governor = get_ini_string(local_config, target_default, "governor", "none");
    if (strcmp(local_governor, "none") != 0) {
        use_governor = local_governor;
    } else {
        use_governor = get_ini_string(global_config, "global", "governor", device.cpu.dflt);
    }

    char default_governor[FILENAME_MAX];
    snprintf(default_governor, sizeof(default_governor), "%s", use_governor);

    mini_free(global_config);
    mini_free(local_config);

    generate_available_governors(default_governor);
}

static void handle_a(void) {
    if (msgbox_active || hold_call || is_dir) return;

    LOG_INFO(mux_module, "Single Governor Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_gov_assignment(selected, is_app ? "mux_option" : rom_name, casn_single);

    if (is_app) load_mux("appcon");

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    remove(MUOS_SAG_LOAD);

    if (is_app) load_mux("appcon");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || is_app || hold_call) return;

    LOG_INFO(mux_module, "Directory Governor Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_gov_assignment(selected, rom_name, casn_dir);

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || is_app || at_base(rom_dir, MAIN_ROM_DIR) || hold_call) return;

    LOG_INFO(mux_module, "Parent Governor Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_gov_assignment(selected, rom_name, casn_parent);

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    struct nav_bar nav_items[9];
    int i = 0;

    if (!is_dir) {
        nav_items[i++] = (struct nav_bar) {ui_lbl_nav_a_glyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lbl_nav_a, lang.generic.content, 1};
    }
    nav_items[i++] = (struct nav_bar) {ui_lbl_nav_b_glyph, "", 0};
    nav_items[i++] = (struct nav_bar) {ui_lbl_nav_b, lang.generic.back, 0};

    if (!is_app) {
        nav_items[i++] = (struct nav_bar) {ui_lbl_nav_x_glyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lbl_nav_x, lang.generic.directory, 1};

        if (!at_base(rom_dir, MAIN_ROM_DIR)) {
            nav_items[i++] = (struct nav_bar) {ui_lbl_nav_y_glyph, "", 1};
            nav_items[i++] = (struct nav_bar) {ui_lbl_nav_y, lang.generic.recursive, 1};
        }
    }

    nav_items[i] = (struct nav_bar) {NULL, NULL, 0}; // Null-terminate

    setup_nav(nav_items);

    overlay_display();
}

void muxgov_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_dir = dir_exist(rom_dir) && !app;
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    is_app = app;

    init_module(__func__);

    if (is_app) {
        LOG_INFO(mux_module, "Assign Governor APP_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Governor APP_DIR: \"%s\"", rom_dir);
    } else {
        LOG_INFO(mux_module, "Assign Governor ROM_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Governor ROM_DIR: \"%s\"", rom_dir);
        LOG_INFO(mux_module, "Assign Governor ROM_SYS: \"%s\"", rom_system);
    }

    if (auto_assign && !file_exist(MUOS_SAG_LOAD) && !is_app) {
        LOG_INFO(mux_module, "Automatic Assign Governor Initiated");

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), INFO_CON_PATH "/%s/core.gov", get_last_subdir(rom_dir, '/', 4));
        remove_double_slashes(core_file);

        if (file_exist(core_file)) return;

        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        if (json_valid(read_all_char_from(assign_file))) {
            static char assign_check[MAX_BUFFER_SIZE];
            char *last_dir_lower = str_tolower(get_last_dir(rom_dir));
            snprintf(assign_check, sizeof(assign_check), "%s", last_dir_lower);
            free(last_dir_lower);
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(json_parse(read_all_char_from(assign_file)), assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "\tCore Assigned: %s", ass_config);

                char assigned_global[MAX_BUFFER_SIZE];
                snprintf(assigned_global, sizeof(assigned_global), STORE_LOC_ASIN "/%s/global.ini", ass_config);

                LOG_INFO(mux_module, "\tObtaining Core INI: %s", assigned_global);

                mini_t *global_ini = mini_load(assigned_global);

                static char def_gov[MAX_BUFFER_SIZE];
                snprintf(def_gov, sizeof(def_gov), "%s", get_ini_string(global_ini, "global", "governor", "none"));

                static char def_sys[MAX_BUFFER_SIZE];
                snprintf(def_sys, sizeof(def_sys), "%s", get_ini_string(global_ini, "global", "default", "none"));

                if (strcmp(def_gov, "none") != 0) {
                    char default_core[MAX_BUFFER_SIZE];
                    snprintf(default_core, sizeof(default_core), STORE_LOC_ASIN "/%s/%s.ini", ass_config, def_sys);

                    static char core_governor[MAX_BUFFER_SIZE];
                    mini_t *local_ini = mini_load(default_core);

                    char *use_local_governor = get_ini_string(local_ini, def_sys, "governor", "none");
                    if (strcmp(use_local_governor, "none") != 0) {
                        snprintf(core_governor, sizeof(core_governor), "%s", use_local_governor);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Governor: %s", core_governor);
                    } else {
                        snprintf(
                            core_governor, sizeof(core_governor), "%s",
                            get_ini_string(global_ini, "global", "governor", device.cpu.dflt)
                        );
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Governor: %s", core_governor);
                    }

                    mini_free(local_ini);

                    create_gov_assignment(core_governor, rom_name, casn_dir_nowipe);
                    LOG_SUCCESS(mux_module, "\tGovernor Assignment Successful");
                } else {
                    LOG_INFO(mux_module, "\tAssigned Governor To Default: %s", device.cpu.dflt);
                    create_gov_assignment(device.cpu.dflt, rom_name, casn_dir_nowipe);
                }

                mini_free(global_ini);

                return;
            }
            LOG_INFO(mux_module, "\tAssigned Governor To Default: %s", device.cpu.dflt);
            create_gov_assignment(device.cpu.dflt, rom_name, casn_dir_nowipe);

            return;
        }
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxgov.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    if (strcasecmp(rom_system, "none") == 0 && !is_app) {
        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        if (json_valid(read_all_char_from(assign_file))) {
            static char assign_check[MAX_BUFFER_SIZE];
            char *last_dir_lower2 = str_tolower(get_last_dir(rom_dir));
            snprintf(assign_check, sizeof(assign_check), "%s", last_dir_lower2);
            free(last_dir_lower2);
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(json_parse(read_all_char_from(assign_file)), assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "<Obtaining System> Core Assigned: %s", ass_config);
                char *sys_no_ext = strip_ext(ass_config);
                snprintf(rom_system, sizeof(rom_system), "%s", sys_no_ext);
                free(sys_no_ext);
            }
        }
    }

    create_gov_items(rom_system);
    init_elements();

    if (ui_count_static > 0) {
        LOG_SUCCESS(mux_module, "%d Governor%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        gen_step_movement(0, +1, 1, 0, 1);
    } else {
        LOG_ERROR(mux_module, "No Governors Detected!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxgov.none);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
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
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);
}
