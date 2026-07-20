#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static int is_dir = 0;

static int is_app = 0;

static mux_dialogue assign_dlg;
static int assign_dialogue_active = 0;

static void show_help(void) {
    show_info_box(lang.muxcontrol.title, lang.muxcontrol.help, 0);
}

static void create_control_assignment(const char *control, const char *rom, const enum gen_type method) {
    create_marker_assignment("con", "Assign Control", control, rom, rom_dir, is_app, method);

    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);
}

static void generate_available_controls(const char *default_control) {
    struct dirent *cf;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), INFO_GCD_PATH);

    DIR *cd = opendir(assign_dir);
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

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        const char *cap_name = str_capital_all(items[i].display_name);
        char *raw_name = str_tolower(str_trim(strdup(items[i].display_name)));

        lv_obj_t *ui_pnl_control = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_control);

        lv_obj_t *ui_lbl_control_item = lv_label_create(ui_pnl_control);
        apply_theme_list_item(&theme, ui_lbl_control_item, cap_name);

        lv_obj_t *ui_lbl_control_item_glyph = lv_img_create(ui_pnl_control);

        const char *glyph = strcasecmp(raw_name, default_control) == 0 ? "system"
                            : strcasecmp(raw_name, "system") == 0      ? "system"
                            : strcasecmp(raw_name, "retro") == 0       ? "retro"
                            : strcasecmp(raw_name, "modern") == 0      ? "modern"
                                                                       : "default";
        apply_theme_list_glyph(&theme, ui_lbl_control_item_glyph, mux_module, glyph);
        free(raw_name);

        lv_group_add_obj(ui_group, ui_lbl_control_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_control_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_control);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_control_item, ui_lbl_control_item_glyph, cap_name);
        apply_text_long_dot(&theme, ui_lbl_control_item);
    }

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        free_items(&items, &item_count);
    }
}

static void create_control_items(const char *target) {
    if (strcmp(target, "none") == 0) generate_available_controls(target);

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

    char *use_control;
    char *local_control = get_ini_string(local_config, target_default, "control", "none");
    if (strcmp(local_control, "none") != 0) {
        use_control = local_control;
    } else {
        use_control = get_ini_string(global_config, "global", "control", "system");
    }

    char default_control[FILENAME_MAX];
    snprintf(default_control, sizeof(default_control), "%s", use_control);

    mini_free(global_config);
    mini_free(local_config);

    generate_available_controls(default_control);
}

static void close_screen(void) {
    remove(MUOS_SAG_LOAD);

    if (is_app) load_mux("appcon");

    mux_input_stop();
}

static void handle_a(void) {
    if (assign_dialogue_active) {
        const int method = assign_dlg.option_data[assign_dlg.selected];
        dialogue_dismiss(&assign_dialogue_active, &assign_dlg);

        if (method < 0) {
            play_sound(snd_back);
            close_screen();
            return;
        }

        LOG_INFO(mux_module, "Control Assignment Triggered (method %d)", method);
        play_sound(snd_confirm);

        const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
        create_control_assignment(selected, is_app ? "mux_option" : rom_name, (enum gen_type) method);

        if (is_app) load_mux("appcon");

        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    dialogue_open(&assign_dialogue_active, &assign_dlg, &theme);
}

static void handle_b(void) {
    if (hold_call) return;

    if (assign_dialogue_active) {
        play_sound(snd_back);
        dialogue_dismiss(&assign_dialogue_active, &assign_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    close_screen();
}

static void handle_dpad_up(void) {
    if (assign_dialogue_active) {
        dialogue_handle_dpad(&assign_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (assign_dialogue_active) {
        dialogue_handle_dpad(&assign_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_down_hold();
}

static void handle_page_up(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (assign_dialogue_active) return;

    handle_list_nav_page_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || assign_dialogue_active) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

void muxcontrol_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_dir = dir_exist(rom_dir) && !app;
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);

    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
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
        snprintf(core_file, sizeof(core_file), INFO_CON_PATH "/%s/core.con", get_last_subdir(rom_dir, '/', 4));
        remove_double_slashes(core_file);

        if (file_exist(core_file)) return;

        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        char *assign_content = read_all_char_from(assign_file);
        if (json_valid(assign_content)) {
            static char assign_check[MAX_BUFFER_SIZE];
            snprintf(assign_check, sizeof(assign_check), "%s", str_tolower(get_last_dir(rom_dir)));
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(json_parse(assign_content), assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "\tCore Assigned: %s", ass_config);

                char assigned_global[MAX_BUFFER_SIZE];
                snprintf(assigned_global, sizeof(assigned_global), STORE_LOC_ASIN "/%s/global.ini", ass_config);

                LOG_INFO(mux_module, "\tObtaining Core INI: %s", assigned_global);

                mini_t *global_ini = mini_load(assigned_global);

                static char def_control[MAX_BUFFER_SIZE];
                snprintf(
                    def_control, sizeof(def_control), "%s", get_ini_string(global_ini, "global", "control", "none")
                );

                static char def_sys[MAX_BUFFER_SIZE];
                snprintf(def_sys, sizeof(def_sys), "%s", get_ini_string(global_ini, "global", "default", "none"));

                if (strcmp(def_control, "none") != 0) {
                    char default_core[MAX_BUFFER_SIZE];
                    snprintf(default_core, sizeof(default_core), STORE_LOC_ASIN "/%s/%s.ini", ass_config, def_sys);

                    static char core_control[MAX_BUFFER_SIZE];
                    mini_t *local_ini = mini_load(default_core);

                    char *use_local_control = get_ini_string(local_ini, def_sys, "control", "none");
                    if (strcmp(use_local_control, "none") != 0) {
                        snprintf(core_control, sizeof(core_control), "%s", use_local_control);
                        LOG_INFO(mux_module, "\t(LOCAL) Core Control: %s", core_control);
                    } else {
                        snprintf(
                            core_control, sizeof(core_control), "%s",
                            get_ini_string(global_ini, "global", "control", "system")
                        );
                        LOG_INFO(mux_module, "\t(GLOBAL) Core Control: %s", core_control);
                    }

                    mini_free(local_ini);

                    create_control_assignment(core_control, rom_name, casn_dir_nowipe);
                    LOG_SUCCESS(mux_module, "\tControl Assignment Successful");
                } else {
                    LOG_INFO(mux_module, "\tAssigned Control To Default: %s", "system");
                    create_control_assignment("system", rom_name, casn_dir_nowipe);
                }

                mini_free(global_ini);

                free(assign_content);
                return;
            }
            LOG_INFO(mux_module, "\tAssigned Control To Default: %s", "system");
            create_control_assignment("system", rom_name, casn_dir_nowipe);

            free(assign_content);
            return;
        }

        free(assign_content);
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxcontrol.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    if (strcasecmp(rom_system, "none") == 0 && !is_app) {
        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        char *assign_content = read_all_char_from(assign_file);
        if (json_valid(assign_content)) {
            static char assign_check[MAX_BUFFER_SIZE];
            snprintf(assign_check, sizeof(assign_check), "%s", str_tolower(get_last_dir(rom_dir)));
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(json_parse(assign_content), assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "<Obtaining System> Core Assigned: %s", ass_config);
                char *sys_no_ext = strip_ext(ass_config);
                snprintf(rom_system, sizeof(rom_system), "%s", sys_no_ext);
                free(sys_no_ext);
            }
        }

        free(assign_content);
    }

    create_control_items(rom_system);
    init_elements();

    dialogue_init_assign_scope(
        &assign_dlg, &theme, ui_screen, lang.muxoption.control, is_dir, is_app, at_base(rom_dir, MAIN_ROM_DIR),
        lang.generic.select, lang.generic.cancel
    );

    if (ui_count_static > 0) {
        LOG_SUCCESS(mux_module, "%d Control%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        gen_step_movement(0, +1, 1, 0, 1);
    } else {
        LOG_ERROR(mux_module, "No Controls Detected!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxcontrol.none);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_page_up,
            [mux_input_r1] = handle_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);
}
