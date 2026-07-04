#include "muxshare.h"

static char base_dir[PATH_MAX];

static int preview_display_time = 0;
static int preview_index = -1;
static int no_theme_archives = 0;

static int remove_mode = 0;
static int skip_confirm = 0;
static mux_dialogue remove_dlg;

static void show_remove_dialog(void) {
    remove_mode = 1;
    remove_dlg.selected = 0;
    dialogue_show(&remove_dlg);
    dialogue_refresh(&remove_dlg, &theme);
}

static void hide_remove_dialog(void) {
    remove_mode = 0;
    dialogue_hide(&remove_dlg);
}

#define TEMP_VERSION "version.txt"
#define TEMP_CREDITS "credits.txt"

char *theme_version_compat = "";
static char *theme_version_buf = NULL;

static void show_help(void) {
    if (items[current_item_index].content_type == FOLDER || items[current_item_index].content_type == menu) return;

    char credits_path[MAX_BUFFER_SIZE];
    snprintf(credits_path, sizeof(credits_path), "%s/%s/" TEMP_CREDITS, sys_dir, items[current_item_index].name);

    char credits[MAX_BUFFER_SIZE];
    if (file_exist(credits_path)) {
        char *raw = read_all_char_from(credits_path);
        snprintf(credits, sizeof(credits), "%s", raw ? raw : "");
        free(raw);
    } else {
        snprintf(credits, sizeof(credits), "%s", lang.muxtheme.no_credit);
    }

    show_info_box(TRS(lv_label_get_text(lv_group_get_focused(ui_group))), TRS(credits), 0);
}

static int version_check(void) {
    char theme_version_file[MAX_BUFFER_SIZE];
    snprintf(
        theme_version_file, sizeof(theme_version_file), "%s/%s/" TEMP_VERSION, sys_dir, items[current_item_index].name
    );
    if (!file_exist(theme_version_file)) return 0;

    char *theme_version = read_line_char_from(theme_version_file, 1);
    if (!theme_version || !*theme_version) return 0;

    free(theme_version_buf);
    theme_version_buf = theme_version;
    theme_version_compat = theme_version_buf;

    for (size_t i = 0; i < theme_compat; i++) {
        const char *compat = theme_back_compat[i];
        if (!compat) continue;
        if (str_startswith(compat, theme_version)) return 1;
    }

    return 0;
}

static void image_refresh(void) {
    if (items[current_item_index].content_type == FOLDER || items[current_item_index].content_type == menu) {
        lv_img_set_src(ui_img_box, &ui_img_blank);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
        return;
    }

    char base_image_path[MAX_BUFFER_SIZE];
    snprintf(base_image_path, sizeof(base_image_path), "%s/%s", sys_dir, items[current_item_index].name);

    refresh_theme_preview_image(base_image_path, "preview", &preview_index);
    preview_display_time = 0;
}

static void create_theme_items(void) {
    if (device.board.has_network && strcasecmp(base_dir, sys_dir) == 0 && !is_ksk(kiosk.custom.theme_down)) {
        add_item(&items, &item_count, lang.muxtheme.theme_down, lang.muxtheme.theme_down, "", menu);
    }

    struct dirent *tf;

    DIR *td = opendir(sys_dir);
    if (!td) {
        no_theme_archives = 1;
        goto show_dl_only;
    }

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_DIR && strcmp(tf->d_name, ".") != 0 && strcmp(tf->d_name, "..") != 0) {
            if (strcasecmp(tf->d_name, "active") == 0 || strcasecmp(tf->d_name, "override") == 0) continue;

            char version_path[FILENAME_MAX];
            snprintf(version_path, sizeof(version_path), "%s/%s/version.txt", sys_dir, tf->d_name);

            if (file_exist(version_path)) {
                add_item(&items, &item_count, tf->d_name, tf->d_name, "", ITEM);
            } else {
                add_item(&items, &item_count, tf->d_name, tf->d_name, "", FOLDER);
            }
        }
    }

    closedir(td);

show_dl_only:
    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        lv_obj_t *ui_pnl_theme = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_theme);

        lv_obj_t *ui_lbl_theme_item = lv_label_create(ui_pnl_theme);
        apply_theme_list_item(&theme, ui_lbl_theme_item, items[i].display_name);

        lv_obj_t *ui_lbl_theme_item_glyph = lv_img_create(ui_pnl_theme);
        apply_theme_list_glyph(
            &theme, ui_lbl_theme_item_glyph, mux_module,
            items[i].content_type == menu     ? "download"
            : items[i].content_type == FOLDER ? "folder"
                                              : "theme"
        );

        lv_group_add_obj(ui_group, ui_lbl_theme_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_theme_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_theme);

        apply_size_to_content(
            &theme, ui_pnl_content, ui_lbl_theme_item, ui_lbl_theme_item_glyph, items[i].display_name
        );
        apply_text_long_dot(&theme, ui_lbl_theme_item);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void check_focus() {
    if (current_item_index) {
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void save_active_theme(char *path) {
    write_text_to_file_atomic(CONF_CONFIG_PATH "theme/active", CHAR, path);
    if (strcmp(config.theme.active, path) != 0) run_tweak_script(lang.generic.loading);
}

static void do_remove(void) {
    if (strcasecmp(items[current_item_index].name, "MustardOS") == 0) {
        play_sound(snd_error);
        toast_message(lang.muxtheme.protected_theme, tst_wait_m);
        return;
    }

    char active_path[PATH_MAX];
    snprintf(active_path, sizeof(active_path), "%s/%s", sys_dir, items[current_item_index].name);
    if (strcasecmp(active_path, theme_base) == 0) {
        play_sound(snd_error);
        toast_message(lang.generic.cannot_delete_active_theme, tst_wait_m);
        return;
    }

    if (!dir_exist(active_path)) {
        play_sound(snd_error);
        toast_message(lang.generic.remove_fail, tst_wait_m);
        return;
    }

    remove_directory_recursive(active_path);
    sync();

    play_sound(snd_muos);
    write_text_to_file(MUOS_PIN_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));

    load_mux("theme");
    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }
    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
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

    if (items[current_item_index].content_type == menu) {
        if (is_network_connected()) {
            load_mux("themedwn");

            mux_input_stop();
        } else {
            play_sound(snd_error);
            toast_message(lang.generic.need_connect, tst_wait_m);
        }
        return;
    }
    if (items[current_item_index].content_type == FOLDER) {
        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s", sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

        if (!version_check()) {
            char invalid_ver[MAX_BUFFER_SIZE];
            snprintf(invalid_ver, sizeof(invalid_ver), "%s (%s)", lang.muxtheme.invalid_ver, theme_version_compat);

            play_sound(snd_error);
            toast_message(invalid_ver, tst_wait_s);
            return;
        }

        char theme_path[MAX_BUFFER_SIZE];
        snprintf(theme_path, sizeof(theme_path), "%s/%s", sys_dir, items[current_item_index].name);
        if (!resolution_check(theme_path)) {
            play_sound(snd_error);
            toast_message(lang.muxtheme.invalid_res, tst_wait_s);
            return;
        }

        refresh_config = 1;
        refresh_resolution = 1;

        if (strcasecmp(base_dir, sys_dir) == 0) {
            save_active_theme(items[current_item_index].name);
        } else {
            char *relative_path = sys_dir + strlen(base_dir);
            if (*relative_path == '/') relative_path++;
            char active_path[PATH_MAX];
            snprintf(active_path, sizeof(active_path), "%s/%s", relative_path, items[current_item_index].name);
            save_active_theme(active_path);
        }

        write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
        check_theme_change();
    }

    load_mux("theme");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || remove_mode || items[current_item_index].content_type == FOLDER
        || items[current_item_index].content_type == menu) {
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
        if (base_dir) write_text_to_file(EXPLORE_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));

        assert(base_dir != NULL);
        base_dir++; // skip the '/' at the start

        write_text_to_file(EXPLORE_NAME, "w", CHAR, base_dir);
        load_mux("theme");
    }

    mux_input_stop();
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
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.remove, 0},
                                  {NULL, NULL, 0}});

    if (!ui_count_static || no_theme_archives) {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (ui_count_static > 0 && nav_moved) {
        preview_index = -1;
        image_refresh();
        preview_display_time = 0;

        adjust_gen_panel();

        lv_obj_invalidate(ui_pnl_box);
        nav_moved = 0;
    }

    preview_display_time += TIMER_REFRESH;
    if (preview_display_time > THEME_PREVIEW_DELAY) {
        preview_index++;
        image_refresh();
        preview_display_time = 0;
    }
}

int muxtheme_main(char *ex_dir) {
    snprintf(sys_dir, sizeof(sys_dir), "%s", ex_dir);
    snprintf(base_dir, sizeof(base_dir), RUN_STORAGE_PATH "/theme");
    if (strcmp(sys_dir, "") == 0) snprintf(sys_dir, sizeof(sys_dir), "%s", base_dir);
    remove_double_slashes(sys_dir);
    remove_double_slashes(base_dir);

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxtheme.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_theme_items();
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

    if (ui_count_static > 0 && sys_index > -1 && sys_index <= ui_count_static && current_item_index < ui_count_static)
        list_nav_move(sys_index, +1);

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.back);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
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
