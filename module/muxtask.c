#include "muxshare.h"

static char base_dir[PATH_MAX];

static void show_help(void) {
    char *title = items[current_item_index].name;

    char help_info[MAX_BUFFER_SIZE];
    snprintf(help_info, sizeof(help_info), OPT_SHARE_PATH "task/%s.sh",
             title);

    char *message = get_script_value(help_info, "HELP", lang.GENERIC.NO_HELP);
    show_info_box(TS(title), TS(message), 0);
}

static void create_task_items(void) {
    DIR *td;
    struct dirent *tf;

    td = opendir(sys_dir);
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

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlTask = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlTask);

        lv_obj_t *ui_lblTaskItem = lv_label_create(ui_pnlTask);
        apply_theme_list_item(&theme, ui_lblTaskItem, items[i].display_name);

        lv_obj_t *ui_lblTaskItemGlyph = lv_img_create(ui_pnlTask);
        apply_theme_list_glyph(&theme, ui_lblTaskItemGlyph, mux_module,
                               items[i].content_type == FOLDER ? "folder" :
                               get_script_value(items[i].extra_data, "ICON", "task"));

        lv_group_add_obj(ui_group, ui_lblTaskItem);
        lv_group_add_obj(ui_group_glyph, ui_lblTaskItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlTask);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblTaskItem, ui_lblTaskItemGlyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblTaskItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    if (items[current_item_index].content_type == FOLDER) {
        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                 sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        write_text_to_file(MUOS_TIN_LOAD, "w", INT, current_item_index);

        static char task_script[MAX_BUFFER_SIZE];
        snprintf(task_script, sizeof(task_script), "%s/%s.sh",
                 sys_dir, items[current_item_index].name);

        size_t exec_count;
        const char *args[] = {task_script, NULL};
        const char **exec = build_term_exec(args, &exec_count);

        if (exec) {
            config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
            run_exec(exec, exec_count, 0, 1, NULL, NULL);
        }
        free(exec);
    }

    load_mux("task");

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
    if (strcasecmp(base_dir, sys_dir) == 0) {
        remove(EXPLORE_DIR);
        load_mux("app");
    } else {
        char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) write_text_to_file(EXPLORE_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));
        write_text_to_file(EXPLORE_NAME, "w", CHAR, get_last_subdir(sys_dir, '/', 5));
        load_mux("task");
    }

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.LAUNCH, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (ui_count > 0 && nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(element_focused, items[current_item_index].name);

            lv_label_set_text(ui_lblNavA, items[current_item_index].content_type == FOLDER
                                          ? lang.GENERIC.OPEN
                                          : lang.GENERIC.LAUNCH);

            adjust_wallpaper_element(ui_group, 0, TASK);
        }
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxtask_main(char *ex_dir) {
    snprintf(sys_dir, sizeof(sys_dir), "%s", ex_dir);
    snprintf(base_dir, sizeof(base_dir), OPT_SHARE_PATH "task/");
    if (strcmp(sys_dir, "") == 0) snprintf(sys_dir, sizeof(sys_dir), "%s", base_dir);

    init_module("muxtask");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTASK.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, TASK);

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
            if (strcasecmp(items[i].name, e_name_line) == 0) {
                tin_index = (int) i;
                remove(EXPLORE_NAME);
                break;
            }
        }
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);

    if (ui_count > 0) {
        if (tin_index > -1 && tin_index <= ui_count && current_item_index < ui_count) list_nav_move(tin_index, +1);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_label_set_text(ui_lblScreenMessage, lang.MUXTASK.NONE);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
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
