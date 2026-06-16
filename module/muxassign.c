#include "muxshare.h"

static char explore_dir[PATH_MAX];

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static int is_dir = 0;

static lv_obj_t *ui_lblCoreDownloader;

static int find_assigned_system(char *out_system) {
    // File Spec CFG: line 3 = sys
    // Directory CFG: line 2 = sys

    const char *sys = get_content_line(rom_dir, rom_name, "cfg", 3);
    if (!sys || !*sys || strcasecmp(sys, "none") == 0) {
        sys = get_content_line(rom_dir, NULL, "cfg", 2);
    }

    if (!sys || !*sys || strcasecmp(sys, "none") == 0) return 0;

    char sys_dir[PATH_MAX];
    snprintf(sys_dir, sizeof(sys_dir), STORE_LOC_ASIN "/%s", sys);
    if (!dir_exist(sys_dir)) return 0;

    snprintf(out_system, 4096, "%s", sys);
    return 1;
}

static int find_system_item_index(const char *system_name) {
    content_item *tmp_items = NULL;
    size_t tmp_count = 0;

    if (device.BOARD.HASNETWORK) add_item(&tmp_items, &tmp_count, lang.MUXASSIGN.CORE_DOWN, lang.MUXASSIGN.CORE_DOWN, "", MENU);

    DIR *ad = opendir(STORE_LOC_ASIN);
    if (ad) {
        struct dirent *af;
        while ((af = readdir(ad))) {
            if (af->d_type == DT_DIR &&
                strcmp(af->d_name, ".") != 0 && strcmp(af->d_name, "..") != 0) {
                add_item(&tmp_items, &tmp_count, af->d_name, af->d_name, "", FOLDER);
            }
        }
        closedir(ad);
    }

    sort_items(tmp_items, tmp_count);

    int idx = get_item_index_by_name(tmp_items, tmp_count, system_name, FOLDER);
    if (idx < 0) idx = 0;

    free_items(&tmp_items, &tmp_count);
    return idx;
}

static int find_core_item_index(const char *system) {
    const char *file_core = get_content_line(rom_dir, rom_name, "cfg", 2);
    const char *dir_core = get_content_line(rom_dir, NULL, "cfg", 1);
    const char *target = (file_core && *file_core) ? file_core : dir_core;

    if (!target || !*target) return 0;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN "/%s", system);

    char global_assign[FILENAME_MAX];
    snprintf(global_assign, sizeof(global_assign), "%s/global.ini", assign_dir);

    mini_t *global_config = mini_load(global_assign);

    const char *default_assign = get_ini_string(global_config, "global", "default", "none");
    int has_default = strcmp(default_assign, "none") != 0;

    mini_free(global_config);

    if (!has_default) return 0;

    content_item *tmp_items = NULL;
    size_t tmp_count = 0;

    DIR *ad = opendir(assign_dir);
    if (!ad) return 0;

    struct dirent *af;
    while ((af = readdir(ad))) {
        if (af->d_type != DT_REG) continue;
        if (strcasecmp(af->d_name, "global.ini") == 0) continue;

        char *last_dot = strrchr(af->d_name, '.');
        if (!last_dot || strcasecmp(last_dot, ".ini") != 0) continue;

        char core_file[FILENAME_MAX];
        snprintf(core_file, sizeof(core_file), "%s/%s", assign_dir, af->d_name);

        *last_dot = '\0';
        mini_t *core_config = mini_load(core_file);

        char assign_name[FILENAME_MAX];
        snprintf(assign_name, sizeof(assign_name), "%s", get_ini_string(core_config, af->d_name, "name", "none"));

        char assign_core[FILENAME_MAX];
        snprintf(assign_core, sizeof(assign_core), "%s", get_ini_string(core_config, af->d_name, "core", "none"));

        mini_free(core_config);

        if (strcmp(assign_core, "none") != 0) add_item(&tmp_items, &tmp_count, assign_name, af->d_name, assign_core, ITEM);
    }
    closedir(ad);

    sort_items(tmp_items, tmp_count);

    int idx = get_item_index_by_extra_data(tmp_items, tmp_count, target);
    if (idx < 0) idx = 0;

    free_items(&tmp_items, &tmp_count);
    return idx;
}

static void show_help(void) {
    show_info_box(lang.MUXASSIGN.TITLE, lang.MUXASSIGN.HELP, 0);
}

static void create_system_items(void) {
    if (device.BOARD.HASNETWORK) add_item(&items, &item_count, lang.MUXASSIGN.CORE_DOWN, lang.MUXASSIGN.CORE_DOWN, "", MENU);

    DIR *ad;
    struct dirent *af;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN);

    ad = opendir(assign_dir);
    if (!ad) return;

    while ((af = readdir(ad))) {
        if (af->d_type == DT_DIR) {
            if (strcmp(af->d_name, ".") != 0 && strcmp(af->d_name, "..") != 0) {
                add_item(&items, &item_count, af->d_name, af->d_name, "", FOLDER);
            }
        }
    }

    closedir(ad);
    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlSystem = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlSystem);

        lv_obj_t *ui_lblSystemItem = lv_label_create(ui_pnlSystem);
        apply_theme_list_item(&theme, ui_lblSystemItem, items[i].name);
        lv_obj_set_user_data(ui_lblSystemItem, items[i].name);

        lv_obj_t *ui_lblSystemItemGlyph = lv_img_create(ui_pnlSystem);
        apply_theme_list_glyph(&theme, ui_lblSystemItemGlyph, mux_module,
                               items[i].content_type == MENU ? "download" : "system");

        lv_group_add_obj(ui_group, ui_lblSystemItem);
        lv_group_add_obj(ui_group_glyph, ui_lblSystemItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlSystem);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblSystemItem, ui_lblSystemItemGlyph, items[i].name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblSystemItem);

        if (items[i].content_type == MENU) ui_lblCoreDownloader = ui_lblSystemItem;
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free_items(&items, &item_count);
    }
}

static void create_core_items(const char *target) {
    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN "/%s",
             target);

    char global_assign[FILENAME_MAX];
    snprintf(global_assign, sizeof(global_assign), "%s/global.ini", assign_dir);

    char default_assign[FILENAME_MAX];
    mini_t *global_config = mini_load(global_assign);
    snprintf(default_assign, sizeof(default_assign), "%s", get_ini_string(global_config, "global", "default", "none"));
    mini_free(global_config);

    if (strcmp(default_assign, "none") == 0) return;

    DIR *ad;
    struct dirent *af;

    ad = opendir(assign_dir);
    if (!ad) return;

    while ((af = readdir(ad))) {
        if (af->d_type == DT_REG) {
            if (strcasecmp(af->d_name, "global.ini") == 0) continue;

            char core_file[FILENAME_MAX];
            snprintf(core_file, sizeof(core_file), "%s/%s", assign_dir, af->d_name);

            char *last_dot = strrchr(af->d_name, '.');
            if (last_dot && strcasecmp(last_dot, ".ini") == 0) {
                *last_dot = '\0';

                mini_t *core_config = mini_load(core_file);

                char assign_name[FILENAME_MAX];
                snprintf(assign_name, sizeof(assign_name), "%s", get_ini_string(core_config, af->d_name, "name", "none"));

                char assign_core[FILENAME_MAX];
                snprintf(assign_core, sizeof(assign_core), "%s", get_ini_string(core_config, af->d_name, "core", "none"));

                mini_free(core_config);

                if (strcmp(assign_core, "none") != 0) {
                    add_item(&items, &item_count, assign_name, af->d_name, assign_core, ITEM);
                }
            }
        }
    }

    closedir(ad);
    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        const char *directory_core = get_content_line(rom_dir, NULL, "cfg", 1);
        const char *file_core = get_content_line(rom_dir, rom_name, "cfg", 2);
        const char *core_name = format_core_name(items[i].extra_data, 1);

        char display_name[MAX_BUFFER_SIZE];
        if (strcasecmp(file_core, directory_core) != 0 && strcasecmp(file_core, items[i].extra_data) == 0) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core_name, lang.MUXASSIGN.FILE);
        } else if (strcasecmp(directory_core, items[i].extra_data) == 0) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core_name, lang.MUXASSIGN.DIR);
        } else {
            snprintf(display_name, sizeof(display_name), "%s", core_name);
        }

        lv_obj_t *ui_pnlCore = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlCore);

        lv_obj_t *ui_lblCoreItem = lv_label_create(ui_pnlCore);
        apply_theme_list_item(&theme, ui_lblCoreItem, display_name);
        lv_obj_set_user_data(ui_lblCoreItem, strdup(items[i].name));

        lv_obj_t *ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);
        char *glyph = strcasecmp(items[i].name, default_assign) == 0 ? "default" : "core";
        apply_theme_list_glyph(&theme, ui_lblCoreItemGlyph, mux_module, glyph);

        lv_group_add_obj(ui_group, ui_lblCoreItem);
        lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlCore);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblCoreItem, ui_lblCoreItemGlyph, display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblCoreItem);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free_items(&items, &item_count);
    }
}


static void load_return_module() {
    if (file_exist(MUOS_ASS_FROM)) {
        remove(OPTION_SKIP);
        load_mux(read_all_char_from(MUOS_ASS_FROM));
        remove(MUOS_ASS_FROM);
        remove(MUOS_SYS_LOAD);
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(SND_BACK);
    if (strcasecmp(rom_system, "none") == 0) {
        FILE *file = fopen(MUOS_SYS_LOAD, "w");
        fprintf(file, "%s", "");
        fclose(file);
        load_return_module();
    } else {
        write_text_to_file(MUOS_ASS_SYSP, "w", CHAR, rom_system);
        load_assign(MUOS_ASS_LOAD, rom_name, explore_dir, "none", 0, 0);
    }

    remove(MUOS_SAA_LOAD);

    mux_input_stop();
}

static void handle_core_assignment(const char *log_msg, int assignment_mode) {
    LOG_INFO(mux_module, "%s", log_msg);
    play_sound(SND_CONFIRM);

    char *item_data = lv_obj_get_user_data(lv_group_get_focused(ui_group));
    char *selected_item = str_tolower(item_data);
    LOG_INFO(mux_module, "Selected Core: %s (%s)", selected_item, item_data);

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), STORE_LOC_ASIN "/%s", rom_system);

    char global_core[FILENAME_MAX];
    snprintf(global_core, sizeof(global_core), "%s/global.ini", assign_dir);
    mini_t *global_ini = mini_load(global_core);
    LOG_INFO(mux_module, "Global Core Path: %s", global_core);

    char local_core[FILENAME_MAX];
    snprintf(local_core, sizeof(local_core), "%s/%s.ini", assign_dir, selected_item);
    mini_t *local_ini = mini_load(local_core);
    LOG_INFO(mux_module, "Local Core Path: %s", local_core);

    static char core_catalogue[MAX_BUFFER_SIZE];
    char *use_local_catalogue = get_ini_string(local_ini, selected_item, "catalogue", "none");
    if (strcmp(use_local_catalogue, "none") != 0) {
        snprintf(core_catalogue, sizeof(core_catalogue), "%s", use_local_catalogue);
    } else {
        snprintf(core_catalogue, sizeof(core_catalogue), "%s", get_ini_string(global_ini, "global", "catalogue", "none"));
    }
    LOG_INFO(mux_module, "Content Core Catalogue: %s", core_catalogue);

    static char core_governor[MAX_BUFFER_SIZE];
    char *use_local_governor = get_ini_string(local_ini, selected_item, "governor", "none");
    if (strcmp(use_local_governor, "none") != 0) {
        snprintf(core_governor, sizeof(core_governor), "%s", use_local_governor);
    } else {
        snprintf(core_governor, sizeof(core_governor), "%s", get_ini_string(global_ini, "global", "governor", device.CPU.DEFAULT));
    }
    LOG_INFO(mux_module, "Content Core Governor: %s", core_governor);

    static char core_control[MAX_BUFFER_SIZE];
    char *use_local_control = get_ini_string(local_ini, selected_item, "control", "none");
    if (strcmp(use_local_control, "none") != 0) {
        snprintf(core_control, sizeof(core_control), "%s", use_local_control);
    } else {
        snprintf(core_control, sizeof(core_control), "%s", get_ini_string(global_ini, "global", "control", "system"));
    }
    LOG_INFO(mux_module, "Content Core Control: %s", core_control);

    static char core_retroarch[MAX_BUFFER_SIZE];
    char *use_local_retroarch = get_ini_string(local_ini, selected_item, "retroarch", "false");
    if (strcmp(use_local_retroarch, "false") != 0) {
        snprintf(core_retroarch, sizeof(core_retroarch), "%s", use_local_retroarch);
    } else {
        snprintf(core_retroarch, sizeof(core_retroarch), "%s", get_ini_string(global_ini, "global", "retroarch", "false"));
    }
    LOG_INFO(mux_module, "Content Core RetroArch Config: %s", core_retroarch);

    static int core_lookup;
    int use_local_lookup = get_ini_int(local_ini, selected_item, "lookup", 0);
    core_lookup = use_local_lookup ? use_local_lookup : get_ini_int(global_ini, "global", "lookup", 0);
    LOG_INFO(mux_module, "Content Core Lookup: %d", core_lookup);

    static char core_launch[MAX_BUFFER_SIZE];
    snprintf(core_launch, sizeof(core_launch), "%s", get_ini_string(local_ini, selected_item, "core", "none"));
    LOG_INFO(mux_module, "Content Core Launcher: %s", core_launch);

    create_core_assignment(selected_item, rom_dir, core_launch, rom_system, core_catalogue, rom_name,
                           core_governor, core_control, core_retroarch, core_lookup, assignment_mode);

    mini_free(global_ini);
    mini_free(local_ini);

    load_return_module();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (lv_group_get_focused(ui_group) == ui_lblCoreDownloader) {
        if (is_network_connected()) {
            play_sound(SND_CONFIRM);
            load_assign(MUOS_ASS_LOAD "_temp", rom_name, explore_dir, "none", 0, 0);
            load_mux("coredown");
        } else {
            play_sound(SND_ERROR);
            toast_message(lang.GENERIC.NEED_CONNECT, MEDIUM);
            return;
        }
    } else {
        if (strcasecmp(rom_system, "none") == 0) {
            play_sound(SND_CONFIRM);
            load_assign(MUOS_ASS_LOAD, rom_name, explore_dir, lv_label_get_text(lv_group_get_focused(ui_group)), 0, 0);
        } else {
            if (is_dir) return;
            handle_core_assignment("Single Core Assignment Triggered", SINGLE);
        }
    }

    remove(MUOS_SYS_LOAD);
    remove(OPTION_SKIP);

    write_text_to_file(MUOS_AIX_LOAD, "w", INT, current_item_index);

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || strcasecmp(rom_system, "none") == 0 || hold_call) return;

    handle_core_assignment("Directory Core Assignment Triggered", DIRECTORY);

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || strcasecmp(rom_system, "none") == 0 || at_base(rom_dir, MAIN_ROM_DIR) || hold_call) return;

    handle_core_assignment("Parent Core Assignment Triggered", PARENT);

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    struct nav_bar nav_items[7];
    int i = 0;

    if (!is_dir) {
        nav_items[i++] = (struct nav_bar) {ui_lblNavAGlyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lblNavA, lang.GENERIC.SELECT, 1};
    }

    nav_items[i++] = (struct nav_bar) {ui_lblNavBGlyph, "", 0};
    nav_items[i++] = (struct nav_bar) {ui_lblNavB, lang.GENERIC.BACK, 0};
    nav_items[i] = (struct nav_bar) {NULL, NULL, 0};

    setup_nav(nav_items);

    if (strcasecmp(rom_system, "none") != 0) {
        i = 0;

        if (!is_dir) {
            nav_items[i++] = (struct nav_bar) {ui_lblNavAGlyph, "", 1};
            nav_items[i++] = (struct nav_bar) {ui_lblNavA, lang.GENERIC.CONTENT, 1};
        }
        nav_items[i++] = (struct nav_bar) {ui_lblNavXGlyph, "", 1};
        nav_items[i++] = (struct nav_bar) {ui_lblNavX, lang.GENERIC.DIRECTORY, 1};

        if (!at_base(rom_dir, MAIN_ROM_DIR)) {
            nav_items[i++] = (struct nav_bar) {ui_lblNavYGlyph, "", 1};
            nav_items[i++] = (struct nav_bar) {ui_lblNavY, lang.GENERIC.RECURSIVE, 1};
        }

        nav_items[i] = (struct nav_bar) {NULL, NULL, 0};

        setup_nav(nav_items);
    }

    overlay_display();
}

int muxassign_main(int auto_assign, char *name, char *dir, char *sys, int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);

    is_dir = dir_exist(rom_dir);
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(explore_dir, sizeof(explore_dir), "%s", dir);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    init_module(__func__);

    LOG_INFO(mux_module, "Assign Core explore_dir: \"%s\"", explore_dir);
    LOG_INFO(mux_module, "Assign Core ROM_NAME: \"%s\"", rom_name);
    LOG_INFO(mux_module, "Assign Core ROM_DIR: \"%s\"", rom_dir);
    LOG_INFO(mux_module, "Assign Core ROM_SYS: \"%s\"", rom_system);

    if (auto_assign && !file_exist(MUOS_SAA_LOAD)) {
        if (automatic_assign_core(rom_dir) || strcmp(rom_system, "none") == 0) {
            return 0;
        }
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXASSIGN.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);
    init_fonts();

    int ass_index = 0;

    if (strcasecmp(rom_system, "none") == 0) {
        char force_sys_name[PATH_MAX] = "";
        int force_sys_picker = file_exist(MUOS_ASS_SYSP);
        if (force_sys_picker) {
            snprintf(force_sys_name, sizeof(force_sys_name), "%s", read_line_char_from(MUOS_ASS_SYSP, 1));
            remove(MUOS_ASS_SYSP);
        }

        char detected_system[PATH_MAX];
        if (!force_sys_picker && find_assigned_system(detected_system)) {
            LOG_INFO(mux_module, "Detected assigned system: '%s'... skipping system picker!", detected_system);
            snprintf(rom_system, sizeof(rom_system), "%s", detected_system);
            create_core_items(rom_system);
            ass_index = find_core_item_index(rom_system);
        } else {
            create_system_items();

            if (*force_sys_name) {
                ass_index = find_system_item_index(force_sys_name);
            } else if (file_exist(MUOS_AIX_LOAD)) {
                ass_index = read_line_int_from(MUOS_AIX_LOAD, 1);
                remove(MUOS_AIX_LOAD);
            }
        }
    } else {
        create_core_items(rom_system);
        ass_index = find_core_item_index(rom_system);
    }

    init_elements();

    if (ui_count > 0) {
        if (strcasecmp(rom_system, "none") == 0) {
            LOG_SUCCESS(mux_module, "%d System%s Detected", ui_count, ui_count == 1 ? "" : "s");
        } else {
            LOG_SUCCESS(mux_module, "%d Core%s Detected", ui_count, ui_count == 1 ? "" : "s");
        }

        if (ui_count > 0 && ass_index > -1 && ass_index <= ui_count && current_item_index < ui_count) {
            gen_step_movement(ass_index, +1, 1, 0);
        }
    } else {
        LOG_ERROR(mux_module, "No Cores Detected - Check Directory!");
        lv_label_set_text(ui_lblScreenMessage, lang.MUXASSIGN.NONE);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    nav_silent = 1;
    return 0;
}
