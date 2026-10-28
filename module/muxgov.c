#include <common/content/core/coredb.h>
#include "muxshare.h"
#include <common/platform/cpu_profile.h>
#include <common/ui/orientation.h>

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static int is_dir = 0;

static int is_app = 0;

static mux_dialogue assign_dlg;

static int default_profile = cpu_profile_automatic;

static void show_help(void) {
    show_info_box(lang.muxgov.title, lang.muxgov.help, 0);
}

static const char *profile_description(const int id) {
    switch (id) {
        case cpu_profile_powersave:
            return lang.muxgov.desc.powersave;
        case cpu_profile_efficient:
            return lang.muxgov.desc.efficient;
        case cpu_profile_balanced:
            return lang.muxgov.desc.balanced;
        case cpu_profile_responsive:
            return lang.muxgov.desc.responsive;
        case cpu_profile_turbo:
            return lang.muxgov.desc.turbo;
        case cpu_profile_extreme:
            return lang.muxgov.desc.extreme;
        default:
            return lang.muxgov.desc.automatic;
    }
}

static void show_profile_help(const int id) {
    char range[MAX_BUFFER_SIZE] = "";
    unsigned min = 0;
    unsigned max = 0;

    if (cpu_profile_range(id, &min, &max)) {
        if (min == max) {
            snprintf(range, sizeof(range), lang.muxgov.fixed, max / 1000);
        } else {
            snprintf(range, sizeof(range), lang.muxgov.range, min / 1000, max / 1000);
        }
    }

    char text[MAX_BUFFER_SIZE * 2];
    const int written = snprintf(text, sizeof(text), "%s%s%s", profile_description(id), range[0] ? "\n\n" : "", range);
    if (written < 0) return;

    show_info_box(cpu_profile_label(id), text, 0);
}

static void create_gov_assignment(const char *gov, const char *rom, const enum gen_type method) {
    create_marker_assignment("gov", "Assign Performance", gov, rom, rom_dir, is_app, method);

    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);
}

static void generate_profiles(void) {
    reset_ui_groups();

    for (int i = 0; i < cpu_profile_count; i++) {
        ui_count_static++;

        const char *label = cpu_profile_label(i);

        lv_obj_t *ui_pnl_gov = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_gov);

        lv_obj_t *ui_lbl_gov_item = lv_label_create(ui_pnl_gov);
        apply_theme_list_item(&theme, ui_lbl_gov_item, label);

        lv_obj_t *ui_lbl_gov_item_glyph = lv_img_create(ui_pnl_gov);
        apply_theme_list_glyph(
            &theme, ui_lbl_gov_item_glyph, mux_module, i == default_profile ? "default" : cpu_profile_name(i)
        );

        lv_group_add_obj(ui_group, ui_lbl_gov_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_gov_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_gov);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_gov_item, ui_lbl_gov_item_glyph, label);
        apply_text_long_dot(&theme, ui_lbl_gov_item);
    }

    lv_obj_update_layout(ui_pnl_content);
}

static const char *system_default_value(const char *target) {
    char governor[COREDB_NAME_MAX] = "";
    if (!coredb_system_governor(target, governor, sizeof(governor))) governor[0] = '\0';

    struct coredb_core core;
    if (coredb_system_default_core(target, &core)) {
        const char *board_governor = coredb_board_governor(&core);
        if (board_governor) {
            snprintf(governor, sizeof(governor), "%s", board_governor);
        } else if (core.governor[0]) {
            snprintf(governor, sizeof(governor), "%s", core.governor);
        }
    }

    return cpu_profile_assign_value(governor);
}

static int profile_from_value(const char *value) {
    if (!value || !*value || strcasecmp(value, "default") == 0) return cpu_profile_setting("gov_default");
    return cpu_profile_from_name(value);
}

static int current_profile(void) {
    char value[MAX_BUFFER_SIZE] = "";

    if (is_app) {
        snprintf(value, sizeof(value), "%s", get_application_line(rom_dir, "gov", 1));
    } else {
        snprintf(value, sizeof(value), "%s", get_content_line(rom_dir, is_dir ? NULL : rom_name, "gov", 1));
        if (!value[0] && !is_dir) snprintf(value, sizeof(value), "%s", get_content_line(rom_dir, NULL, "gov", 1));
    }

    return value[0] ? profile_from_value(value) : default_profile;
}

static void close_screen(void) {
    remove(MUOS_SAG_LOAD);

    if (is_app) load_mux("appcon");

    mux_input_stop();
}

static void handle_a(void) {
    if (dialogue_active(&assign_dlg)) {
        const int method = assign_dlg.option_data[assign_dlg.selected];
        dialogue_dismiss(&assign_dlg);

        if (method < 0) {
            close_screen();
            return;
        }

        LOG_INFO(mux_module, "Governor Assignment Triggered (method %d)", method);

        create_gov_assignment(
            cpu_profile_name(current_item_index), is_app ? "mux_option" : rom_name, (enum gen_type) method
        );

        if (is_app) load_mux("appcon");

        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    dialogue_open(&assign_dlg, &theme);
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&assign_dlg)) {
        dialogue_cancel(&assign_dlg);
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
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad(&assign_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad(&assign_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad_hold(&assign_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad_hold(&assign_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_page_up(void) {
    if (dialogue_active(&assign_dlg)) return;

    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (dialogue_active(&assign_dlg)) return;

    handle_list_nav_page_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || dialogue_active(&assign_dlg))
        return;

    play_sound(snd_info_open);
    show_profile_help(current_item_index);
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

void muxgov_main(const int auto_assign, const char *name, const char *dir, const char *sys, const int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_dir = dir_exist(rom_dir) && !app && !folder_is_content(dir, name);
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    is_app = app;

    init_module(__func__);

    if (is_app) {
        LOG_INFO(mux_module, "Assign Performance APP_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Performance APP_DIR: \"%s\"", rom_dir);
    } else {
        LOG_INFO(mux_module, "Assign Performance ROM_NAME: \"%s\"", rom_name);
        LOG_INFO(mux_module, "Assign Performance ROM_DIR: \"%s\"", rom_dir);
        LOG_INFO(mux_module, "Assign Performance ROM_SYS: \"%s\"", rom_system);
    }

    if (auto_assign && !file_exist(MUOS_SAG_LOAD) && !is_app) {
        LOG_INFO(mux_module, "Automatic Assign Performance Initiated");

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), INFO_CON_PATH "/%s/core.gov", get_last_subdir(rom_dir, '/', 4));
        remove_double_slashes(core_file);

        if (file_exist(core_file)) return;

        const char *assign_value = "default";

        char *assign_content = read_all_char_from(CORE_ASSIGN_INDEX);
        if (json_valid(assign_content)) {
            static char assign_check[MAX_BUFFER_SIZE];
            char *last_dir_lower = str_tolower(get_last_dir(rom_dir));
            snprintf(assign_check, sizeof(assign_check), "%s", last_dir_lower);
            free(last_dir_lower);
            str_remchars(assign_check, " -_+");

            const struct json auto_assign_config = json_object_get(json_parse(assign_content), assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_module, "\tCore Assigned: %s", ass_config);
                assign_value = system_default_value(ass_config);
            }
        }

        free(assign_content);

        LOG_INFO(mux_module, "\tAssigned Performance: %s", assign_value);
        create_gov_assignment(assign_value, rom_name, casn_dir_nowipe);
        return;
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxgov.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    if (strcasecmp(rom_system, "none") == 0 && !is_app) {
        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), CORE_ASSIGN_INDEX);

        char *assign_content = read_all_char_from(assign_file);
        if (json_valid(assign_content)) {
            static char assign_check[MAX_BUFFER_SIZE];
            char *last_dir_lower2 = str_tolower(get_last_dir(rom_dir));
            snprintf(assign_check, sizeof(assign_check), "%s", last_dir_lower2);
            free(last_dir_lower2);
            str_remchars(assign_check, " -_+");

            const struct json auto_assign_config = json_object_get(json_parse(assign_content), assign_check);

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

    if (strcasecmp(rom_system, "none") != 0)
        default_profile = profile_from_value(system_default_value(rom_system));
    else
        default_profile = cpu_profile_setting("gov_default");

    generate_profiles();
    init_elements();

    dialogue_init_assign_scope(
        &assign_dlg, &theme, ui_screen, lang.muxoption.governor, is_dir, is_app, at_base(rom_dir, MAIN_ROM_DIR),
        lang.generic.select, lang.generic.cancel
    );

    if (ui_count_static > 0) {
        gen_step_movement(current_profile(), +1, 1, 0, 1);
    } else {
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

    if (orientation_should_show(mux_module)) {
        orientation_mark_shown(mux_module);
        show_help();
    }

    mux_input_task(&input_opts);
}
