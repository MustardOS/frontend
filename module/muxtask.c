#include "muxshare.h"

static char base_dir[PATH_MAX];

static void show_help(void) {
    char *title = items[current_item_index].name;

    char help_info[MAX_BUFFER_SIZE];
    snprintf(help_info, sizeof(help_info), OPT_SHARE_PATH "task/%s.sh", title);

    char *message = get_script_value(help_info, "HELP", lang.generic.no_help);
    show_info_box(TRS(title), TRS(message), 0);
}

static void create_task_items(void) {
    struct dirent *tf;

    DIR *td = opendir(sys_dir);
    if (!td) return;

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_DIR && strcmp(tf->d_name, ".") != 0 && strcmp(tf->d_name, "..") != 0) {
            add_item(&items, &item_count, tf->d_name, tf->d_name, "", FOLDER);
        } else if (tf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", sys_dir, tf->d_name);

            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot && strcasecmp(last_dot, ".sh") == 0) {
                *last_dot = '\0';
                add_item(&items, &item_count, tf->d_name, tf->d_name, filename, ITEM);
            }
        }
    }

    closedir(td);
    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        lv_obj_t *ui_pnl_task = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_task);

        lv_obj_t *ui_lbl_task_item = lv_label_create(ui_pnl_task);
        apply_theme_list_item(&theme, ui_lbl_task_item, items[i].display_name);

        lv_obj_t *ui_lbl_task_item_glyph = lv_img_create(ui_pnl_task);
        apply_theme_list_glyph(
            &theme, ui_lbl_task_item_glyph, mux_module,
            items[i].content_type == FOLDER ? "folder" : get_script_value(items[i].extra_data, "ICON", "task")
        );

        lv_group_add_obj(ui_group, ui_lbl_task_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_task_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_task);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_task_item, ui_lbl_task_item_glyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_lbl_task_item);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_confirm);

    if (items[current_item_index].content_type == FOLDER) {
        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s", sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        write_text_to_file(MUOS_TIN_LOAD, "w", INT, current_item_index);

        static char task_script[MAX_BUFFER_SIZE];
        snprintf(task_script, sizeof(task_script), "%s/%s.sh", sys_dir, items[current_item_index].name);

        size_t exec_count;
        const char *args[] = {task_script, NULL};
        const char **exec = build_term_exec(args, &exec_count);

        if (exec) {
            fade_out_screen();
            run_exec(exec, exec_count, 0, 1, NULL, NULL);
        }
        free(exec);
    }

    load_mux("task");

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    if (strcasecmp(base_dir, sys_dir) == 0) {
        remove(EXPLORE_DIR);
        load_mux("app");
    } else {
        const char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) {
            char parent_dir[MAX_BUFFER_SIZE];
            snprintf(parent_dir, sizeof(parent_dir), "%.*s", (int) (base_dir - sys_dir), sys_dir);
            write_text_to_file(EXPLORE_DIR, "w", CHAR, parent_dir);
        }
        write_text_to_file(EXPLORE_NAME, "w", CHAR, get_last_subdir(sys_dir, '/', 5));
        load_mux("task");
    }

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.launch, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (ui_count_static > 0 && nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(e_focused, items[current_item_index].name);
            lv_label_set_text(
                ui_lbl_nav_a, items[current_item_index].content_type == FOLDER ? lang.generic.open : lang.generic.launch
            );
            adjust_wallpaper_element(ui_group, 0, wall_task);
        }
        adjust_gen_panel();

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxtask_main(char *ex_dir) {
    snprintf(sys_dir, sizeof(sys_dir), "%s", ex_dir);
    snprintf(base_dir, sizeof(base_dir), OPT_SHARE_PATH "task/");
    if (strcmp(sys_dir, "") == 0) snprintf(sys_dir, sizeof(sys_dir), "%s", base_dir);

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxtask.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_task);

    init_fonts();
    create_task_items();
    init_elements();

    int tin_index = 0;
    if (file_exist(MUOS_TIN_LOAD)) {
        tin_index = read_line_int_from(MUOS_TIN_LOAD, 1);
        remove(MUOS_TIN_LOAD);
    }

    char *e_name_line = file_exist(EXPLORE_NAME) ? read_line_char_from(EXPLORE_NAME, 1) : NULL;
    if (e_name_line) {
        for (size_t i = 0; i < item_count; i++) {
            if (strcasecmp(items[i].name, str_rem_first_char(e_name_line, 1)) == 0) {
                tin_index = (int) i;
                remove(EXPLORE_NAME);
                break;
            }
        }
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);

    if (ui_count_static > 0) {
        if (tin_index > -1 && tin_index <= ui_count_static && current_item_index < ui_count_static)
            gen_step_movement(tin_index, +1, 1, 0, 1);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_label_set_text(ui_lbl_screen_message, lang.muxtask.none);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
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

    free_items(&items, &item_count);

    return 0;
}
