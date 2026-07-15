#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];
static int is_dir = 0;

static int is_app = 0;

static mux_dialogue assign_dlg;
static int assign_dialogue_active = 0;

static char *get_selected_shader(void) {
    if (!ui_count_static) return NULL;

    return items[current_item_index].name;
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    const char *shader_store = items[index].name;

    char *meta_name = read_shader_info(shader_store, "Name");

    char shader_display[MAX_BUFFER_SIZE];
    if (meta_name && *meta_name) {
        snprintf(shader_display, sizeof(shader_display), "%s", meta_name);
    } else {
        char *shd_spaced = str_replace(shader_store, "_", " ");
        snprintf(shader_display, sizeof(shader_display), "%s", str_capital_all(shd_spaced));
        free(shd_spaced);
    }
    if (meta_name) free(meta_name);

    lv_label_set_text(ui_lbl_item, shader_display);
    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, "shader");

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, shader_display);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void show_help(void) {
    if (!ui_count_static) {
        show_info_box(lang.muxshader.title, lang.muxshader.help, 0);
        return;
    }

    char *selected = get_selected_shader();
    if (!selected) {
        show_info_box(lang.muxshader.title, lang.muxshader.help, 0);
        return;
    }

    char *name = read_shader_info(selected, "Name");
    char *author = read_shader_info(selected, "Author");
    const char *version = read_shader_info(selected, "Version");

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

        show_info_box(lang.muxshader.title, buffer, 0);
    } else {
        show_info_box(lang.muxshader.title, lang.muxshader.help, 0);
    }

    if (name) free(name);
    if (author) free(author);
}

static void create_shader_assignment(const char *shader, const char *rom, const enum gen_type method) {
    create_marker_assignment("shd", "Assign Shader", shader, rom, rom_dir, 0, method);
}

static void generate_available_shaders(void) {
    const char *dirs[] = {STORAGE_SHADER};
    const char *exts[] = {".frag"};

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

        char shader_name[MAX_BUFFER_SIZE];

        char *bf_dir = strip_dir(base_filename);
        char *bf_rel = str_replace(base_filename, bf_dir, "");

        free(bf_dir);
        str_remchar(bf_rel, '/');
        snprintf(shader_name, sizeof(shader_name), "%s", bf_rel);
        free(bf_rel);

        char shader_store[MAX_BUFFER_SIZE];
        char *shd_no_ext = strip_ext(shader_name);
        snprintf(shader_store, sizeof(shader_store), "%s", shd_no_ext);
        free(shd_no_ext);

        add_item(&items, &item_count, shader_store, shader_store, "", ITEM);
    }

    ui_count_static += (int) item_count;

    const size_t limit = theme.mux.item.count;
    for (size_t i = 0; i < item_count && i < limit; i++) {
        lv_obj_t *ui_pnl_filter = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_filter);

        lv_obj_t *ui_lbl_filter_item = lv_label_create(ui_pnl_filter);
        apply_theme_list_item(&theme, ui_lbl_filter_item, items[i].display_name);

        lv_obj_t *ui_lbl_filter_item_glyph = lv_img_create(ui_pnl_filter);
        apply_theme_list_glyph(&theme, ui_lbl_filter_item_glyph, mux_module, "shader");

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

static void close_screen(void) {
    remove(MUOS_SAG_LOAD);

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

        LOG_INFO(mux_module, "Shader Assignment Triggered (method %d)", method);
        play_sound(snd_confirm);

        const char *selected = get_selected_shader();
        create_shader_assignment(selected, rom_name, (enum gen_type) method);

        mux_input_stop();
        return;
    }

    if (msgbox_active || !ui_count_static || hold_call) return;

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

void muxshader_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
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

    init_ui_common_screen(&theme, &device, &lang, lang.muxshader.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    generate_available_shaders();
    init_elements();

    dialogue_init_assign_scope(
        &assign_dlg, &theme, ui_screen, lang.muxoption.shader, is_dir, is_app, at_base(rom_dir, MAIN_ROM_DIR),
        lang.generic.select, lang.generic.cancel
    );

    if (ui_count_static > 0) {
        LOG_SUCCESS(mux_module, "%d Shader%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        list_win_focus_initial(update_list_item);
    } else {
        LOG_ERROR(mux_module, "No Shaders Detected!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxshader.none);
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

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);
}
