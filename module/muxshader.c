#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static bool is_dir = false;

static int is_app = 0;

static char *get_selected_shader(void) {
    lv_obj_t *e_focused = lv_group_get_focused(ui_group_panel);
    if (!e_focused) return NULL;

    return (char *) lv_obj_get_user_data(e_focused);
}

static void show_help(void) {
    if (!ui_count) {
        show_info_box(lang.MUXSHADER.TITLE, lang.MUXSHADER.HELP, 0);
        return;
    }

    char *selected = get_selected_shader();
    if (!selected) {
        show_info_box(lang.MUXSHADER.TITLE, lang.MUXSHADER.HELP, 0);
        return;
    }

    char *name = read_shader_info(selected, "Name");
    char *author = read_shader_info(selected, "Author");
    char *version = read_shader_info(selected, "Version");

    if (name && *name) {
        char buffer[MAX_BUFFER_SIZE];
        buffer[0] = '\0';

        snprintf(buffer, sizeof(buffer), "%s", name);

        if (author && *author) {
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
            strncat(buffer, author, sizeof(buffer) - strlen(buffer) - 1);
        }

        if (version && *version) {
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
            strncat(buffer, version, sizeof(buffer) - strlen(buffer) - 1);
        }

        show_info_box(lang.MUXSHADER.TITLE, buffer, 0);
    } else {
        show_info_box(lang.MUXSHADER.TITLE, lang.MUXSHADER.HELP, 0);
    }

    if (name) free(name);
    if (author) free(author);
}

static void write_shader_file(char *path, char *shader, char *log) {
    if (strcasecmp(shader, "none") == 0) return;

    FILE *file = fopen(path, "w");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, path);
        return;
    }

    LOG_INFO(mux_module, "%s: %s", log, shader);

    fprintf(file, "%s", shader);
    fclose(file);
}

static void assign_shader_single(char *core_dir, char *shader, char *rom) {
    char shader_path[MAX_BUFFER_SIZE];
    snprintf(shader_path, sizeof(shader_path), "%s/%s.shd", core_dir, strip_ext(rom));

    if (file_exist(shader_path)) remove(shader_path);
    write_shader_file(shader_path, shader, "Assign Shader (Single)");
}

static void assign_shader_directory(char *core_dir, char *shader, int purge) {
    if (purge) delete_files_of_type(core_dir, ".shd", NULL, 0);

    char shader_path[MAX_BUFFER_SIZE];
    snprintf(shader_path, sizeof(shader_path), INFO_CON_PATH "/%s/core.shd",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(shader_path);

    write_shader_file(shader_path, shader, "Assign Shader (Directory)");
}

static void assign_shader_parent(char *core_dir, char *shader) {
    delete_files_of_type(core_dir, ".shd", NULL, 1);

    if (strcasecmp(shader, "none") == 0) return;

    assign_shader_directory(core_dir, shader, 0);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char shader_path[MAX_BUFFER_SIZE];
        snprintf(shader_path, sizeof(shader_path), "%s%s/core.shd", core_dir, subdirs[i]);

        create_directories(strip_dir(shader_path), 0);
        write_shader_file(shader_path, shader, "Assign Shader (Recursive)");
    }

    free_subdirectories(subdirs);
}

static void create_shader_assignment(char *shader, char *rom, enum gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), INFO_CON_PATH "/%s/",
             get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(core_dir);

    create_directories(core_dir, 0);

    switch (method) {
        case SINGLE:
            assign_shader_single(core_dir, shader, rom);
            break;
        case PARENT:
            assign_shader_parent(core_dir, shader);
            break;
        case DIRECTORY:
            assign_shader_directory(core_dir, shader, 1);
            break;
        case DIRECTORY_NO_WIPE:
            assign_shader_directory(core_dir, shader, 0);
            break;
    }
}

static void generate_available_shaders(void) {
    const char *dirs[] = {STORAGE_SHADER};
    const char *exts[] = {".frag"};

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

        char shader_name[MAX_BUFFER_SIZE];
        snprintf(shader_name, sizeof(shader_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char shader_store[MAX_BUFFER_SIZE];
        snprintf(shader_store, sizeof(shader_store), "%s",
                 strip_ext(shader_name));

        char *meta_name = read_shader_info(shader_store, "Name");

        char shader_display[MAX_BUFFER_SIZE];
        if (meta_name && *meta_name) {
            snprintf(shader_display, sizeof(shader_display), "%s", meta_name);
        } else {
            snprintf(shader_display, sizeof(shader_display), "%s",
                     str_capital_all(str_replace(shader_store, "_", " ")));
        }

        if (meta_name) free(meta_name);

        ui_count++;
        add_item(&items, &item_count, shader_store, shader_store, "", ITEM);

        lv_obj_t *ui_pnlFilter = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlFilter);

        lv_obj_set_user_data(ui_pnlFilter, strdup(shader_store));

        lv_obj_t *ui_lblFilterItem = lv_label_create(ui_pnlFilter);
        apply_theme_list_item(&theme, ui_lblFilterItem, shader_display);

        lv_obj_t *ui_lblFilterItemGlyph = lv_img_create(ui_pnlFilter);
        apply_theme_list_glyph(&theme, ui_lblFilterItemGlyph, mux_module, "shader");

        lv_group_add_obj(ui_group, ui_lblFilterItem);
        lv_group_add_obj(ui_group_glyph, ui_lblFilterItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlFilter);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblFilterItem, ui_lblFilterItemGlyph, shader_display);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblFilterItem);
    }

    if (ui_count > 0)
        lv_obj_update_layout(ui_pnlContent);

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
    if (msgbox_active || !ui_count || hold_call || is_dir) return;

    LOG_INFO(mux_module, "Single Shader Assignment Triggered");
    play_sound(SND_CONFIRM);

    char *selected = get_selected_shader();
    create_shader_assignment(selected, rom_name, SINGLE);

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

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    LOG_INFO(mux_module, "Directory Shader Assignment Triggered");
    play_sound(SND_CONFIRM);

    char *selected = get_selected_shader();
    create_shader_assignment(selected, rom_name, DIRECTORY);

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    LOG_INFO(mux_module, "Parent Shader Assignment Triggered");
    play_sound(SND_CONFIRM);

    char *selected = get_selected_shader();
    create_shader_assignment(selected, rom_name, PARENT);

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

        if (!at_base(rom_dir, MAIN_ROM_DIR)) {
            nav_items[i++] = (struct nav_bar) {ui_lblNavYGlyph, "", 1};
            nav_items[i++] = (struct nav_bar) {ui_lblNavY, lang.GENERIC.RECURSIVE, 1};
        }
    }

    nav_items[i] = (struct nav_bar) {NULL, NULL, 0};

    setup_nav(nav_items);

    overlay_display();
}

int muxshader_main(int nothing, char *name, char *dir, char *sys, int app) {
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

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSHADER.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);
    init_fonts();

    generate_available_shaders();
    init_elements();

    if (ui_count > 0) {
        LOG_SUCCESS(mux_module, "%d Shader%s Detected", ui_count, ui_count == 1 ? "" : "s");
        list_nav_next(0);
    } else {
        LOG_ERROR(mux_module, "No Shaders Detected!");
        lv_label_set_text(ui_lblScreenMessage, lang.MUXSHADER.NONE);
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
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
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

    nav_silent = 1;
    return 0;
}
