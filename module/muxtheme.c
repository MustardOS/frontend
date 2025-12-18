#include "muxshare.h"

static char base_dir[PATH_MAX];

#define TEMP_PREVIEW "preview.png"
#define TEMP_VERSION "version.txt"
#define TEMP_CREDITS "credits.txt"

static void show_help(void) {
    if (items[current_item_index].content_type == FOLDER || items[current_item_index].content_type == MENU) return;

    char credits_path[MAX_BUFFER_SIZE];
    snprintf(credits_path, sizeof(credits_path), "%s/%s/" TEMP_CREDITS, sys_dir, items[current_item_index].name);

    char credits[MAX_BUFFER_SIZE];
    if (file_exist(credits_path)) {
        strcpy(credits, read_all_char_from(credits_path));
    } else {
        strcpy(credits, lang.MUXPICKER.NONE.CREDIT);
    }

    show_info_box(TS(lv_label_get_text(lv_group_get_focused(ui_group))), TS(credits), 0);
}

static int version_check(void) {
    char version_path[MAX_BUFFER_SIZE];
    snprintf(version_path, sizeof(version_path), "%s/%s/" TEMP_VERSION, sys_dir, items[current_item_index].name);

    const char *theme_version = read_line_char_from(version_path, 1);
    for (int i = 0; theme_back_compat[i] != NULL; i++) {
        if (str_startswith(theme_back_compat[i], theme_version)) return 1;
    }

    return 0;
}

static void image_refresh(void) {
    if (items[current_item_index].content_type == FOLDER ||
        items[current_item_index].content_type == MENU ) {

        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
        return;
    }

    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgBox));

    char preview_path[PATH_MAX];
    snprintf(preview_path, sizeof(preview_path), "%s/%s/%s" TEMP_PREVIEW, sys_dir, items[current_item_index].name, mux_dimension);

    char fallback_path[PATH_MAX];
    snprintf(fallback_path, sizeof(fallback_path), "%s/%s/640x480/" TEMP_PREVIEW, sys_dir, items[current_item_index].name);

    if (!file_exist(preview_path) && !file_exist(fallback_path)) {
        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
    } else {
        struct ImageSettings image_settings = {
                file_exist(preview_path) ? preview_path : fallback_path, 6,
                validate_int16((int16_t) (device.MUX.WIDTH * .45), "width"),
                validate_int16((int16_t) (device.MUX.HEIGHT), "height"),
                theme.IMAGE_LIST.PAD_LEFT, theme.IMAGE_LIST.PAD_RIGHT,
                theme.IMAGE_LIST.PAD_TOP, theme.IMAGE_LIST.PAD_BOTTOM
        };
        update_image(ui_imgBox, image_settings);
    }
}

static void create_theme_items(void) {
    if (device.BOARD.HAS_NETWORK && strcasecmp(base_dir, sys_dir) == 0 && !is_ksk(kiosk.CUSTOM.THEME_DOWN)) {
        add_item(&items, &item_count, lang.MUXPICKER.THEME_DOWN,
                 lang.MUXPICKER.THEME_DOWN, "", MENU);
    }

    DIR *td;
    struct dirent *tf;

    td = opendir(sys_dir);
    if (!td) return;

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_DIR && strcmp(tf->d_name, ".") != 0 && strcmp(tf->d_name, "..") != 0) {
            if (strcasecmp(tf->d_name, "active") == 0 ||
                strcasecmp(tf->d_name, "override") == 0)
                continue;

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
    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlTheme = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlTheme);

        lv_obj_t *ui_lblThemeItem = lv_label_create(ui_pnlTheme);
        apply_theme_list_item(&theme, ui_lblThemeItem, items[i].display_name);

        lv_obj_t *ui_lblThemeItemGlyph = lv_img_create(ui_pnlTheme);
        apply_theme_list_glyph(&theme, ui_lblThemeItemGlyph, mux_module,
                               items[i].content_type == MENU ? "download" :
                               items[i].content_type == FOLDER ? "folder" :
                               "theme");

        lv_group_add_obj(ui_group, ui_lblThemeItem);
        lv_group_add_obj(ui_group_glyph, ui_lblThemeItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlTheme);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblThemeItem, ui_lblThemeItemGlyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblThemeItem);
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

    if (items[current_item_index].content_type == MENU) {
        if (is_network_connected()) {
            load_mux("themedwn");
            close_input();
            mux_input_stop();
        } else {
            play_sound(SND_ERROR);
            toast_message(lang.GENERIC.NEED_CONNECT, MEDIUM);
        }
        return;
    } else if (items[current_item_index].content_type == FOLDER) {
        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                 sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

        printf("base_dir: %s\n", base_dir);
        printf("sys_dir: %s\n", sys_dir);
        printf("items[current_item_index].name: %s\n", items[current_item_index].name);

        if (!version_check()) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXPICKER.INVALID_VER, SHORT);
            return;
        }

        char theme_path[MAX_BUFFER_SIZE];
        snprintf(theme_path, sizeof(theme_path), "%s/%s",
                 sys_dir, lv_label_get_text(lv_group_get_focused(ui_group)));
        if (!resolution_check(theme_path)) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXPICKER.INVALID_RES, SHORT);
            return;
        }

        refresh_config = 1;

        if (strcasecmp(base_dir, sys_dir) == 0) {
            write_text_to_file(CONF_CONFIG_PATH "theme/active", "w", CHAR, items[current_item_index].name);
        } else {
            char *relative_path = sys_dir + strlen(base_dir);
            if (*relative_path == '/') relative_path++;
            char active_path[PATH_MAX];
            snprintf(active_path, sizeof(active_path), "%s/%s",
                     relative_path, items[current_item_index].name);
            write_text_to_file(CONF_CONFIG_PATH "theme/active", "w", CHAR, active_path);
        }
        write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
    }

    load_mux("theme");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count ||
        items[current_item_index].content_type == FOLDER ||
        items[current_item_index].content_type == MENU) {
        return;
    }

    if (!hold_call) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.HOLD_CONFIRM, SHORT);
        return;
    }

    if (strcasecmp(items[current_item_index].name, "MustardOS") == 0) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXPICKER.PROTECTED, MEDIUM);
        return;
    }

    char active_path[PATH_MAX];
    snprintf(active_path, sizeof(active_path), "%s/%s",
                sys_dir, items[current_item_index].name);    
    if (strcasecmp(active_path, config.THEME.STORAGE_THEME) == 0) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.CANNOT_DELETE_ACTIVE_THEME, MEDIUM);
        return;
    }

    if (!directory_exist(active_path)) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.REMOVE_FAIL, MEDIUM);
        return;
    }

    remove_directory_recursive(active_path);
    sync();

    play_sound(SND_MUOS);
    write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

    hold_call = 0;
    load_mux("theme");

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
        load_mux("custom");
    } else {
        char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) write_text_to_file(EXPLORE_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));

        assert(base_dir != NULL);
        base_dir++; // skip the '/' at the start

        write_text_to_file(EXPLORE_NAME, "w", CHAR, base_dir);
        load_mux("theme");
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
    lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);

    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.SELECT, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  0},
            {ui_lblNavX,      lang.GENERIC.REMOVE, 0},
            {NULL, NULL,                           0}
    });

    if (!ui_count) {
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    overlay_display();
}

static void ui_refresh_task() {
    if (ui_count > 0 && nav_moved) {
        image_refresh();

        adjust_panels();

        lv_obj_invalidate(ui_pnlBox);
        nav_moved = 0;
    }
}

int muxtheme_main(char *ex_dir) {
    snprintf(sys_dir, sizeof(sys_dir), "%s", ex_dir);
    snprintf(base_dir, sizeof(base_dir), RUN_STORAGE_PATH "/theme");
    if (strcmp(sys_dir, "") == 0) snprintf(sys_dir, sizeof(sys_dir), "%s", base_dir);
    remove_double_slashes(sys_dir);
    remove_double_slashes(base_dir);

    const char *m = "muxtheme";
    set_process_name(m);
    init_module(m);

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPICKER.THEME);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_theme_items();
    init_elements();

    int sys_index = 0;
    if (file_exist(MUOS_PIN_LOAD)) {
        sys_index = read_line_int_from(MUOS_PIN_LOAD, 1);
        remove(MUOS_PIN_LOAD);
    }

    char *e_name_line = file_exist(EXPLORE_NAME) ? read_line_char_from(EXPLORE_NAME, 1) : NULL;
    if (e_name_line) {
        int index = get_folder_item_index_by_name(items, item_count, e_name_line);
        if (index > -1) sys_index = index;
        remove(EXPLORE_NAME);
    }

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) list_nav_move(sys_index, +1);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);

        lv_label_set_text(ui_lblScreenMessage, lang.MUXPICKER.NONE.THEME);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
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
