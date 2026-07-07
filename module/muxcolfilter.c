#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static int is_dir = 0;
static int is_app = 0;

static char *get_selected_filter(void) {
    if (!ui_count_static) return NULL;

    return items[current_item_index].name;
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    const char *filter_store = items[index].name;

    char filter_display[MAX_BUFFER_SIZE];
    snprintf(filter_display, sizeof(filter_display), "%s", str_capital_all(str_replace(filter_store, "_", " ")));

    lv_label_set_text(ui_lbl_item, filter_display);
    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, "filter");

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, filter_display);
    apply_text_long_dot(&theme, ui_lbl_item);
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
            in_profile = strncasecmp(s, "[profile]", 9) == 0;
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
    if (!ui_count_static) {
        show_info_box(lang.muxcolfilter.title, lang.muxcolfilter.help, 0);
        return;
    }

    const char *selected = get_selected_filter();
    char *info = read_filter_info(selected);

    if (info && *info) {
        show_info_box(lang.muxcolfilter.title, info, 0);
        free(info);
        return;
    }

    if (info) free(info);
    show_info_box(lang.muxcolfilter.title, lang.muxcolfilter.help, 0);
}

static void create_filter_assignment(const char *filter, const char *rom, const enum gen_type method) {
    create_marker_assignment("flt", "Assign Colour Filter", filter, rom, rom_dir, 0, method);
}

static void generate_available_filters(void) {
    const char *dirs[] = {STORAGE_FILTER};
    const char *exts[] = {".ini"};

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, exts, &files, A_SIZE(dirs), A_SIZE(exts), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
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
        const char *base_filename = files[i];

        char filter_name[MAX_BUFFER_SIZE];
        snprintf(
            filter_name, sizeof(filter_name), "%s",
            str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/')
        );

        char filter_store[MAX_BUFFER_SIZE];
        snprintf(filter_store, sizeof(filter_store), "%s", strip_ext(filter_name));

        add_item(&items, &item_count, filter_store, filter_store, "", ITEM);
    }

    ui_count_static += (int) item_count;

    const size_t limit = theme.mux.item.count;
    for (size_t i = 0; i < item_count && i < limit; i++) {
        lv_obj_t *ui_pnl_filter = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_filter);

        lv_obj_t *ui_lbl_filter_item = lv_label_create(ui_pnl_filter);
        apply_theme_list_item(&theme, ui_lbl_filter_item, items[i].display_name);

        lv_obj_t *ui_lbl_filter_item_glyph = lv_img_create(ui_pnl_filter);
        apply_theme_list_glyph(&theme, ui_lbl_filter_item_glyph, mux_module, "filter");

        lv_group_add_obj(ui_group, ui_lbl_filter_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_filter_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_filter);

        update_list_item(ui_lbl_filter_item, ui_lbl_filter_item_glyph, (int) i);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
    free_array(files, file_count);
}

static void list_nav_prev(const int steps) {
    list_win_nav_move(steps, -1, update_list_item);
}

static void list_nav_next(const int steps) {
    list_win_nav_move(steps, +1, update_list_item);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count_static || hold_call || is_dir) return;

    LOG_INFO(mux_module, "Single Colour Filter Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = get_selected_filter();
    create_filter_assignment(selected, rom_name, SINGLE);

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

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    LOG_INFO(mux_module, "Directory Colour Filter Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = get_selected_filter();
    create_filter_assignment(selected, rom_name, DIRECTORY);

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    LOG_INFO(mux_module, "Parent Colour Filter Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = get_selected_filter();
    create_filter_assignment(selected, rom_name, PARENT);

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

void muxcolfilter_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
    (void) auto_assign;

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

    init_ui_common_screen(&theme, &device, &lang, lang.muxcolfilter.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    generate_available_filters();
    init_elements();

    if (ui_count_static > 0) {
        LOG_SUCCESS(mux_module, "%d Colour Filter%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        list_win_focus_initial(update_list_item);
    } else {
        LOG_ERROR(mux_module, "No Colour Filters Detected!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxcolfilter.none);
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

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);
}
