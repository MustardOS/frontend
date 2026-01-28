#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static bool is_directory = false;

static int is_app = 0;

static char *get_selected_filter(void) {
    lv_obj_t *focused = lv_group_get_focused(ui_group);
    if (!focused) return NULL;

    const char *text = lv_label_get_text(focused);
    if (!text) return NULL;

    return str_tolower(str_trim(str_replace(text, " ", "_")));
}

static char *read_filter_info(const char *filter_store) {
    if (!filter_store || !*filter_store) return NULL;

    // We aren't using the INI parsing here since it gets cranky
    // the matrix is not valid "values" or something like that
    char ini_path[PATH_MAX];
    snprintf(ini_path, sizeof(ini_path), "%s/%s.ini", STORAGE_FILTER, filter_store);
    remove_double_slashes(ini_path);

    FILE *f = fopen(ini_path, "r");
    if (!f) return NULL;

    char line[MAX_BUFFER_SIZE];
    int in_profile = 0;

    while (fgets(line, sizeof(line), f)) {
        char *s = str_trim(line);
        if (!s || !*s) continue;
        if (*s == '#' || *s == ';') continue;

        if (*s == '[') {
            in_profile = (strncasecmp(s, "[profile]", 9) == 0);
            continue;
        }

        if (!in_profile) continue;

        if (strncasecmp(s, "info", 4) == 0) {
            char *eq = strchr(s, '=');
            if (!eq) continue;

            char *val = str_trim(eq + 1);
            if (!val || !*val) break;

            char *out = strdup(val);
            fclose(f);
            return out;
        }
    }

    fclose(f);
    return NULL;
}

static void show_help(void) {
    if (!ui_count) {
        show_info_box(lang.MUXCOLFILTER.TITLE, lang.MUXCOLFILTER.HELP, 0);
        return;
    }

    char *selected = get_selected_filter();
    char *info = read_filter_info(selected);

    if (info && *info) {
        show_info_box(lang.MUXCOLFILTER.TITLE, info, 0);
        free(info);
        return;
    }

    if (info) free(info);
    show_info_box(lang.MUXCOLFILTER.TITLE, lang.MUXCOLFILTER.HELP, 0);
}

static void write_filter_file(char *path, char *filter, char *log) {
    if (strcasecmp(filter, "none") == 0) return;

    FILE *file = fopen(path, "w");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path);
        return;
    }

    LOG_INFO(mux_module, "%s: %s", log, filter);

    fprintf(file, "%s", filter);
    fclose(file);
}

static void assign_filter_single(char *core_dir, char *filter, char *rom) {
    char filter_path[MAX_BUFFER_SIZE];
    snprintf(filter_path, sizeof(filter_path), "%s/%s.flt", core_dir, strip_ext(rom));

    if (file_exist(filter_path)) remove(filter_path);
    write_filter_file(filter_path, filter, "Assign Colour Filter (Single)");
}

static void assign_filter_directory(char *core_dir, char *filter, int purge) {
    if (purge) delete_files_of_type(core_dir, ".flt", NULL, 0);

    char filter_path[MAX_BUFFER_SIZE];
    snprintf(filter_path, sizeof(filter_path), INFO_COR_PATH "/%s/core.flt",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(filter_path);

    write_filter_file(filter_path, filter, "Assign Colour Filter (Directory)");
}

static void assign_filter_parent(char *core_dir, char *filter) {
    delete_files_of_type(core_dir, ".flt", NULL, 1);

    if (strcasecmp(filter, "none") == 0) return;

    assign_filter_directory(core_dir, filter, 0);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char filter_path[MAX_BUFFER_SIZE];
        snprintf(filter_path, sizeof(filter_path), "%s%s/core.flt", core_dir, subdirs[i]);

        create_directories(strip_dir(filter_path), 0);
        write_filter_file(filter_path, filter, "Assign Colour Filter (Recursive)");
    }

    free_subdirectories(subdirs);
}

static void create_filter_assignment(char *filter, char *rom, enum gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), INFO_COR_PATH "/%s/",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(core_dir);

    create_directories(core_dir, 0);

    switch (method) {
        case SINGLE:
            assign_filter_single(core_dir, filter, rom);
            break;
        case PARENT:
            assign_filter_parent(core_dir, filter);
            break;
        case DIRECTORY:
            assign_filter_directory(core_dir, filter, 1);
            break;
        case DIRECTORY_NO_WIPE:
            assign_filter_directory(core_dir, filter, 0);
            break;
    }
}

static void generate_available_filters(void) {
    const char *dirs[] = {STORAGE_FILTER};
    const char *exts[] = {".ini"};

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, exts, &files, A_SIZE(dirs), A_SIZE(exts), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM);
        return;
    }

    if (file_count == 0) {
        free_array(files, file_count);
        return;
    }

    qsort(files, file_count, sizeof(char *), str_compare);
    reset_ui_groups();

    for (size_t i = 0; i < file_count; i++) {
        assert(files[i] != NULL);
        char *base_filename = files[i];

        char filter_name[MAX_BUFFER_SIZE];
        snprintf(filter_name, sizeof(filter_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char filter_store[MAX_BUFFER_SIZE];
        snprintf(filter_store, sizeof(filter_store), "%s",
                 strip_ext(filter_name));

        char filter_display[MAX_BUFFER_SIZE];
        snprintf(filter_display, sizeof(filter_display), "%s",
                 str_capital_all(str_replace(filter_store, "_", " ")));

        ui_count++;
        add_item(&items, &item_count, filter_store, filter_store, "", ITEM);

        lv_obj_t *ui_pnlFilter = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlFilter);
        lv_obj_set_user_data(ui_pnlFilter, strdup(filter_store));

        lv_obj_t *ui_lblFilterItem = lv_label_create(ui_pnlFilter);
        apply_theme_list_item(&theme, ui_lblFilterItem, filter_display);

        lv_obj_t *ui_lblFilterItemGlyph = lv_img_create(ui_pnlFilter);
        apply_theme_list_glyph(&theme, ui_lblFilterItemGlyph, mux_module, "filter");

        lv_group_add_obj(ui_group, ui_lblFilterItem);
        lv_group_add_obj(ui_group_glyph, ui_lblFilterItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlFilter);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblFilterItem, ui_lblFilterItemGlyph, filter_display);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblFilterItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free_array(files, file_count);
}

static void list_nav_move(int steps, int dir) {
    gen_step_movement(steps, dir, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || hold_call || is_directory) return;

    LOG_INFO(mux_module, "Single Colour Filter Assignment Triggered");
    play_sound(SND_CONFIRM);

    char *selected = get_selected_filter();
    create_filter_assignment(selected, rom_name, SINGLE);

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

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    LOG_INFO(mux_module, "Directory Colour Filter Assignment Triggered");
    play_sound(SND_CONFIRM);

    char *selected = get_selected_filter();
    create_filter_assignment(selected, rom_name, DIRECTORY);

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    LOG_INFO(mux_module, "Parent Colour Filter Assignment Triggered");
    play_sound(SND_CONFIRM);

    char *selected = get_selected_filter();
    create_filter_assignment(selected, rom_name, PARENT);

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

int muxcolfilter_main(int nothing, char *name, char *dir, char *sys, int app) {
    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);
    is_directory = directory_exist(rom_dir) && !app;
    if (!is_directory) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_name, sizeof(rom_name), "%s", name);
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

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCOLFILTER.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);
    init_fonts();

    generate_available_filters();
    init_elements();

    if (ui_count > 0) {
        LOG_SUCCESS(mux_module, "%d Colour Filter%s Detected", ui_count, ui_count == 1 ? "" : "s");
        list_nav_next(0);
    } else {
        LOG_ERROR(mux_module, "No Colour Filters Detected!");
        lv_label_set_text(ui_lblScreenMessage, lang.MUXCOLFILTER.NONE);
    }

    init_timer(ui_gen_refresh_task, NULL);

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

    free_items(&items, &item_count);

    return 0;
}
