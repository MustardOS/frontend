#include "muxshare.h"

static char base_dir[PATH_MAX];
static char picker_type[32];
static char *picker_extension;

static int remove_mode = 0;
static int skip_confirm = 0;
static mux_dialogue remove_dlg;

static void show_remove_dialog(void) {
    dialogue_open(&remove_mode, &remove_dlg, &theme);
}

static void hide_remove_dialog(void) {
    dialogue_dismiss(&remove_mode, &remove_dlg);
}

#define TEMP_VERSION "version.txt"
#define TEMP_CREDITS "credits.txt"

static void show_help(void) {
    if (items[current_item_index].content_type == FOLDER || items[current_item_index].content_type == menu) return;

    char *picker_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char picker_archive[MAX_BUFFER_SIZE];

    snprintf(picker_archive, sizeof(picker_archive), "%s/%s.%s", sys_dir, picker_name, picker_extension);

    char credits[MAX_BUFFER_SIZE];
    if (extract_file_from_zip(picker_archive, TEMP_CREDITS, "/tmp/" TEMP_CREDITS)) {
        char *raw = read_all_char_from("/tmp/" TEMP_CREDITS);
        snprintf(credits, sizeof(credits), "%s", raw ? raw : "");
        free(raw);
    } else {
        snprintf(credits, sizeof(credits), "%s", lang.muxpicker.none.credit);
    }

    show_info_box(TRS(lv_label_get_text(lv_group_get_focused(ui_group))), TRS(credits), 0);
}

static void create_picker_items(void) {
    struct dirent *tf;

    DIR *td = opendir(sys_dir);
    if (!td) return;

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_DIR && strcmp(tf->d_name, ".") != 0 && strcmp(tf->d_name, "..") != 0) {
            add_item(&items, &item_count, tf->d_name, tf->d_name, "", FOLDER);
        } else if (tf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", sys_dir, tf->d_name);

            char file_ext[FILENAME_MAX];
            snprintf(file_ext, sizeof(file_ext), ".%s", picker_extension);

            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot && strcasecmp(last_dot, file_ext) == 0) {
                *last_dot = '\0';
                add_item(&items, &item_count, tf->d_name, tf->d_name, "", ITEM);
            }
        }
    }

    closedir(td);
    sort_items(items, item_count);

    reset_ui_groups();

    ui_count_static += (int) item_count;

    const size_t limit = theme.mux.item.count;
    for (size_t i = 0; i < item_count && i < limit; i++) {
        lv_obj_t *ui_pnl_picker = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_picker);

        lv_obj_t *ui_lbl_picker_item = lv_label_create(ui_pnl_picker);
        apply_theme_list_item(&theme, ui_lbl_picker_item, items[i].display_name);

        lv_obj_t *ui_lbl_picker_item_glyph = lv_img_create(ui_pnl_picker);
        apply_theme_list_glyph(
            &theme, ui_lbl_picker_item_glyph, mux_module,
            items[i].content_type == menu     ? "download"
            : items[i].content_type == FOLDER ? "folder"
                                              : get_last_subdir(picker_type, '/', 1)
        );

        lv_group_add_obj(ui_group, ui_lbl_picker_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_picker_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_picker);

        apply_size_to_content(
            &theme, ui_pnl_content, ui_lbl_picker_item, ui_lbl_picker_item_glyph, items[i].display_name
        );
        apply_text_long_dot(&theme, ui_lbl_picker_item);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, items[index].display_name);

    apply_theme_list_glyph(
        &theme, ui_lbl_item_glyph, mux_module,
        items[index].content_type == menu     ? "download"
        : items[index].content_type == FOLDER ? "folder"
                                              : get_last_subdir(picker_type, '/', 1)
    );

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, items[index].display_name);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void list_nav_prev(const int steps) {
    list_win_nav_move(steps, -1, update_list_item);
}

static void list_nav_next(const int steps) {
    list_win_nav_move(steps, +1, update_list_item);
}

static void do_remove(void) {
    if (strcasecmp(picker_extension, "muxcat") != 0 && strcasecmp(picker_extension, "muxcfg") != 0) {
        return;
    }

    char zip_path[PATH_MAX];
    const char *label = lv_label_get_text(lv_group_get_focused(ui_group));
    snprintf(zip_path, sizeof(zip_path), "%s/%s.%s", sys_dir, label, picker_extension);

    if (!file_exist(zip_path)) {
        play_sound(snd_error);
        toast_message(lang.generic.remove_fail, tst_wait_m);
        return;
    }

    remove(zip_path);
    sync();

    play_sound(snd_muos);
    write_text_to_file(MUOS_PIN_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));

    load_mux("picker");
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    if (remove_mode) {
        const mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == mux_remove_yep) {
            do_remove();
        } else if (opt == mux_remove_skip) {
            skip_confirm = 1;
            do_remove();
        }
        return;
    }

    play_sound(snd_confirm);

    if (items[current_item_index].content_type == FOLDER) {
        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s", sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

        static char picker_script[MAX_BUFFER_SIZE];
        snprintf(
            picker_script, sizeof(picker_script), OPT_PATH "script/package/%s.sh", get_last_subdir(picker_type, '/', 1)
        );

        char *selected_item = lv_label_get_text(lv_group_get_focused(ui_group));

        char relative_zip_path[PATH_MAX];
        if (strcasecmp(base_dir, sys_dir) == 0) {
            snprintf(relative_zip_path, sizeof(relative_zip_path), "%s", selected_item);
        } else {
            char *relative_path = sys_dir + strlen(base_dir);
            if (*relative_path == '/') relative_path++;
            snprintf(relative_zip_path, sizeof(relative_zip_path), "%s/%s", relative_path, selected_item);
        }

        size_t exec_count;
        const char *args[] = {picker_script, "install", relative_zip_path, NULL};
        const char **exec = build_term_exec(args, &exec_count);

        if (exec) {
            fade_out_screen();

            run_exec(exec, exec_count, 0, 1, NULL, NULL);
        }
        free(exec);
    }

    load_mux("picker");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || remove_mode || items[current_item_index].content_type == FOLDER
        || items[current_item_index].content_type == menu) {
        return;
    }

    if (strcasecmp(picker_extension, "muxcat") != 0 && strcasecmp(picker_extension, "muxcfg") != 0) {
        return;
    }

    if (config.settings.advanced.trust_remove || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
    show_remove_dialog();
}

static void handle_b(void) {
    if (hold_call) return;

    if (remove_mode) {
        hide_remove_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    if (strcasecmp(base_dir, sys_dir) == 0) {
        remove(EXPLORE_DIR);
        load_mux("custom");
    } else {
        char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) {
            char parent_dir[MAX_BUFFER_SIZE];
            snprintf(parent_dir, sizeof(parent_dir), "%.*s", (int) (base_dir - sys_dir), sys_dir);
            write_text_to_file(EXPLORE_DIR, "w", CHAR, parent_dir);
        }

        assert(base_dir != NULL);
        base_dir++; // skip the '/' at the start

        write_text_to_file(EXPLORE_NAME, "w", CHAR, base_dir);
        load_mux("picker");
    }

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);

    write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

    static char picker_script[MAX_BUFFER_SIZE];
    snprintf(
        picker_script, sizeof(picker_script), OPT_PATH "/script/package/%s.sh", get_last_subdir(picker_type, '/', 1)
    );

    size_t exec_count;
    const char *args[] = {picker_script, "save", "-", NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        fade_out_screen();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);

    load_mux("picker");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        dialogue_handle_dpad(&remove_dlg, &theme, -1, !swap_axis);
        return;
    }
    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        dialogue_handle_dpad(&remove_dlg, &theme, +1, !swap_axis);
        return;
    }
    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (remove_mode) return;
    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (remove_mode) return;
    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    lv_obj_set_align(ui_img_box, LV_ALIGN_BOTTOM_RIGHT);

    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.select, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.save, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.remove, 0},
                                  {NULL, NULL, 0}});

    if (!ui_count_static) {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (ui_count_static > 0 && nav_moved) {
        adjust_gen_panel();

        lv_obj_invalidate(ui_pnl_box);
        nav_moved = 0;
    }
}

int muxpicker_main(char *type, char *ex_dir) {
    snprintf(picker_type, sizeof(picker_type), "%s", type);
    snprintf(sys_dir, sizeof(sys_dir), "%s", ex_dir);
    snprintf(base_dir, sizeof(base_dir), RUN_STORAGE_PATH "%s", picker_type);
    if (strcmp(sys_dir, "") == 0) snprintf(sys_dir, sizeof(sys_dir), "%s", base_dir);
    remove_double_slashes(sys_dir);
    remove_double_slashes(base_dir);

    init_module(__func__);
    init_theme(1, 1);

    const char *picker_title = NULL;
    if (strcasecmp(picker_type, "package/catalogue") == 0) {
        picker_extension = "muxcat";
        picker_title = lang.muxpicker.catalogue;
    } else if (strcasecmp(picker_type, "package/config") == 0) {
        picker_extension = "muxcfg";
        picker_title = lang.muxpicker.config;
    } else {
        picker_extension = "muxcus";
        picker_title = lang.muxpicker.custom;
    }

    init_ui_common_screen(&theme, &device, &lang, picker_title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_picker_items();
    init_elements();

    int sys_index = 0;
    if (file_exist(MUOS_PIN_LOAD)) {
        sys_index = read_line_int_from(MUOS_PIN_LOAD, 1);
        remove(MUOS_PIN_LOAD);
    }

    const char *e_name_line = file_exist(EXPLORE_NAME) ? read_line_char_from(EXPLORE_NAME, 1) : NULL;
    if (e_name_line) {
        const int index = get_item_index_by_name(items, item_count, e_name_line, FOLDER);
        if (index > -1) sys_index = index;
        remove(EXPLORE_NAME);
    }

    if (ui_count_static > 0) {
        if (sys_index > -1 && sys_index <= ui_count_static && current_item_index < ui_count_static) {
            current_item_index = sys_index;
            list_win_focus_initial(update_list_item);
        }
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

        const char *message_text = NULL;
        if (strcasecmp(picker_type, "package/catalogue") == 0) {
            message_text = lang.muxpicker.none.catalogue;
        } else if (strcasecmp(picker_type, "package/config") == 0) {
            message_text = lang.muxpicker.none.config;
        } else {
            message_text = lang.muxpicker.none.custom;
        }
        lv_label_set_text(ui_lbl_screen_message, message_text);
    }

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.cancel);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
