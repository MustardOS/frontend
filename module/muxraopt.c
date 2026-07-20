#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static int is_dir = 0;

static int is_app = 0;

static mux_dialogue assign_dlg;
static int assign_dialogue_active = 0;

static void show_help(void) {
    show_info_box(lang.muxraopt.title, lang.muxraopt.help, 0);
}

static void create_rac_assignment(const char *rac, const char *rom, const enum gen_type method) {
    create_marker_assignment("rac", "Assign RetroArch Config", rac, rom, rom_dir, is_app, method);

    if (file_exist(MUOS_SAR_LOAD)) remove(MUOS_SAR_LOAD);
}

static void create_rac_items(void) {
    reset_ui_groups();
    ui_count_static = 0;

    const struct {
        const char *label;
        const char *value;
    } ra_opts[] = {
        {lang.generic.enabled, "true"},
        {lang.generic.disabled, "false"},
    };

    for (size_t i = 0; i < A_SIZE(ra_opts); i++) {
        ui_count_static++;

        lv_obj_t *pnl = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(pnl);

        lv_obj_t *lbl = lv_label_create(pnl);
        apply_theme_list_item(&theme, lbl, ra_opts[i].label);

        lv_obj_set_user_data(lbl, (void *) ra_opts[i].value);

        lv_obj_t *glyph = lv_img_create(pnl);
        apply_theme_list_glyph(&theme, glyph, mux_module, ra_opts[i].value);

        lv_group_add_obj(ui_group, lbl);
        lv_group_add_obj(ui_group_glyph, glyph);
        lv_group_add_obj(ui_group_panel, pnl);

        apply_size_to_content(&theme, ui_pnl_content, lbl, glyph, ra_opts[i].label);
        apply_text_long_dot(&theme, lbl);
    }

    lv_obj_update_layout(ui_pnl_content);
}

static void close_screen(void) {
    remove(MUOS_SAR_LOAD);

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

        LOG_INFO(mux_module, "RetroArch Config Assignment Triggered (method %d)", method);
        play_sound(snd_confirm);

        const char *selected = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        create_rac_assignment(selected, is_app ? "mux_option" : rom_name, (enum gen_type) method);

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

void muxraopt_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
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
        snprintf(core_file, sizeof(core_file), INFO_CON_PATH "/%s/core.rac", get_last_subdir(rom_dir, '/', 4));
        remove_double_slashes(core_file);

        if (file_exist(core_file)) return;

        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), STORE_LOC_ASIN "/assign.json");

        char *assign_content = read_all_char_from(assign_file);
        if (json_valid(assign_content)) {
            static char assign_check[MAX_BUFFER_SIZE];
            char *last_dir_lower = str_tolower(get_last_dir(rom_dir));
            snprintf(assign_check, sizeof(assign_check), "%s", last_dir_lower);
            free(last_dir_lower);
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

                static char def_rac[MAX_BUFFER_SIZE];
                snprintf(def_rac, sizeof(def_rac), "%s", get_ini_string(global_ini, "global", "retroarch", "false"));

                static char def_sys[MAX_BUFFER_SIZE];
                snprintf(def_sys, sizeof(def_sys), "%s", get_ini_string(global_ini, "global", "default", "false"));

                if (strcmp(def_rac, "false") != 0) {
                    char default_core[MAX_BUFFER_SIZE];
                    snprintf(default_core, sizeof(default_core), STORE_LOC_ASIN "/%s/%s.ini", ass_config, def_sys);

                    static char core_retroarch[MAX_BUFFER_SIZE];
                    mini_t *local_ini = mini_load(default_core);

                    char *use_local_retroarch = get_ini_string(local_ini, def_sys, "retroarch", "false");
                    if (strcmp(use_local_retroarch, "false") != 0) {
                        snprintf(core_retroarch, sizeof(core_retroarch), "%s", use_local_retroarch);
                        LOG_INFO(mux_module, "\t(LOCAL) Core RetroArch Config: %s", core_retroarch);
                    } else {
                        snprintf(
                            core_retroarch, sizeof(core_retroarch), "%s",
                            get_ini_string(global_ini, "global", "retroarch", "false")
                        );
                        LOG_INFO(mux_module, "\t(GLOBAL) Core RetroArch Config: %s", core_retroarch);
                    }

                    mini_free(local_ini);

                    create_rac_assignment(core_retroarch, rom_name, casn_dir_nowipe);
                    LOG_SUCCESS(mux_module, "\tRetroArch Config Assignment Successful");
                } else {
                    LOG_INFO(mux_module, "\tAssigned RetroArch Config To Default: %s", "false");
                    create_rac_assignment("false", rom_name, casn_dir_nowipe);
                }

                mini_free(global_ini);

                free(assign_content);
                return;
            }
            LOG_INFO(mux_module, "\tAssigned RetroArch Config To Default: %s", "false");
            create_rac_assignment("false", rom_name, casn_dir_nowipe);

            free(assign_content);
            return;
        }

        free(assign_content);
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxraopt.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    create_rac_items();
    init_elements();

    dialogue_init_assign_scope(
        &assign_dlg, &theme, ui_screen, lang.muxoption.retroarch, is_dir, is_app, at_base(rom_dir, MAIN_ROM_DIR),
        lang.generic.select, lang.generic.cancel
    );

    gen_step_movement(0, +1, 1, 0, 1);
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
