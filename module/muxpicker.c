#include "muxshare.h"

static char base_dir[PATH_MAX];
static char sys_dir[PATH_MAX];
static char picker_type[32];
static char *picker_extension;

#define TEMP_PREVIEW "preview.png"
#define TEMP_VERSION "version.txt"
#define TEMP_CREDITS "credits.txt"

static void show_help() {
    if (items[current_item_index].content_type == FOLDER) return;

    char *picker_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char picker_archive[MAX_BUFFER_SIZE];

    snprintf(picker_archive, sizeof(picker_archive), "%s/%s.%s", sys_dir, picker_name, picker_extension);

    char credits[MAX_BUFFER_SIZE];
    if (extract_file_from_zip(picker_archive, TEMP_CREDITS, "/tmp/" TEMP_CREDITS)) {
        strcpy(credits, read_all_char_from("/tmp/" TEMP_CREDITS));
    } else {
        strcpy(credits, lang.MUXPICKER.NONE.CREDIT);
    }

    show_info_box(TS(lv_label_get_text(lv_group_get_focused(ui_group))), TS(credits), 0);
}

static int version_check() {
    char picker_archive[MAX_BUFFER_SIZE];
    snprintf(picker_archive, sizeof(picker_archive), "%s/%s.%s",
             sys_dir, lv_label_get_text(lv_group_get_focused(ui_group)), picker_extension);
    if (!extract_file_from_zip(picker_archive, TEMP_VERSION, "/tmp/" TEMP_VERSION)) return 0;

    const char *theme_version = read_line_char_from("/tmp/" TEMP_VERSION, 1);
    for (int i = 0; theme_back_compat[i] != NULL; i++) {
        if (str_startswith(theme_back_compat[i], theme_version)) return 1;
    }

    return 0;
}

static int extract_preview() {
    char picker_archive[MAX_BUFFER_SIZE];
    snprintf(picker_archive, sizeof(picker_archive), "%s/%s.%s",
             sys_dir, lv_label_get_text(lv_group_get_focused(ui_group)), picker_extension);

    char device_preview[PATH_MAX];
    snprintf(device_preview, sizeof(device_preview), "%s" TEMP_PREVIEW, mux_dimension);
    return extract_file_from_zip(picker_archive, device_preview, "/tmp/" TEMP_PREVIEW);
}

static void image_refresh() {
    if (items[current_item_index].content_type == FOLDER) return;

    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgBox));

    if (!extract_preview()) {
        lv_img_set_src(ui_imgBox, &ui_image_Missing);
    } else {
        lv_img_set_src(ui_imgBox, "M:/tmp/" TEMP_PREVIEW);
    }
}

static void create_picker_items() {
    DIR *td;
    struct dirent *tf;

    td = opendir(sys_dir);
    if (!td) return;

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_DIR) {
            if (strcasecmp(tf->d_name, "active") == 0 ||
                strcasecmp(tf->d_name, "override") == 0)
                continue;
            add_item(&items, &item_count, tf->d_name, tf->d_name, "", FOLDER);
        } else if (tf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", sys_dir, tf->d_name);

            char file_ext[FILENAME_MAX];
            snprintf(file_ext, sizeof(file_ext), ".%s", picker_extension);

            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot && !strcasecmp(last_dot, file_ext)) {
                *last_dot = '\0';
                add_item(&items, &item_count, tf->d_name, tf->d_name, "", ITEM);
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

        lv_obj_t *ui_pnlPicker = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlPicker);

        lv_obj_t *ui_lblPickerItem = lv_label_create(ui_pnlPicker);
        apply_theme_list_item(&theme, ui_lblPickerItem, items[i].display_name);

        lv_obj_t *ui_lblPickerItemGlyph = lv_img_create(ui_pnlPicker);
        apply_theme_list_glyph(&theme, ui_lblPickerItemGlyph, mux_module,
                               items[i].content_type == FOLDER ? "folder" : get_last_subdir(picker_type, '/', 1));

        lv_group_add_obj(ui_group, ui_lblPickerItem);
        lv_group_add_obj(ui_group_glyph, ui_lblPickerItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlPicker);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblPickerItem, ui_lblPickerItemGlyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblPickerItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
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
    image_refresh();
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_confirm() {
    if (msgbox_active || ui_count <= 0) return;

    play_sound(SND_CONFIRM);

    write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

    if (items[current_item_index].content_type == FOLDER) {
        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                 sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        if (!strcasecmp(picker_type, "/theme") && !version_check()) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXPICKER.INVALID_VER, 1000);
            return;
        }

        char picker_archive[MAX_BUFFER_SIZE];
        snprintf(picker_archive, sizeof(picker_archive), "%s/%s.%s",
                 sys_dir, lv_label_get_text(lv_group_get_focused(ui_group)), picker_extension);
        if (!strcasecmp(picker_type, "/theme") && !resolution_check(picker_archive)) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXPICKER.INVALID_RES, 1000);
            return;
        }

        static char picker_script[MAX_BUFFER_SIZE];
        snprintf(picker_script, sizeof(picker_script),
                 "%sscript/package/%s.sh", INTERNAL_PATH, get_last_subdir(picker_type, '/', 1));

        char *selected_item = lv_label_get_text(lv_group_get_focused(ui_group));

        char relative_zip_path[PATH_MAX];
        if (strcasecmp(base_dir, sys_dir) == 0) {
            snprintf(relative_zip_path, sizeof(relative_zip_path), "%s",
                     selected_item);
        } else {
            char *relative_path = sys_dir + strlen(base_dir);
            if (*relative_path == '/') relative_path++;
            snprintf(relative_zip_path, sizeof(relative_zip_path), "%s/%s",
                     relative_path, selected_item);
        }

        if (!strcasecmp(picker_type, "/theme")) delete_files_of_type(INTERNAL_THEME, ".ttf", NULL, 1);

        size_t exec_count;
        const char *args[] = {picker_script, "install", relative_zip_path, NULL};
        const char **exec = build_term_exec(args, &exec_count);

        if (exec) {
            config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
            if (config.SETTINGS.GENERAL.BGM == 2 && !strcasecmp(picker_type, "/theme")) play_silence_bgm();

            run_exec(exec, exec_count, 0);
        }
        free(exec);
    }

    load_mux("picker");

    close_input();
    mux_input_stop();
}

static void handle_confirm_force() {
    if (msgbox_active || ui_count <= 0 ||
        strcasecmp(picker_type, "/theme") != 0 ||
        items[current_item_index].content_type == FOLDER) {
        return;
    }

    play_sound(SND_CONFIRM);

    write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

    static char picker_script[MAX_BUFFER_SIZE];
    snprintf(picker_script, sizeof(picker_script),
             "%sscript/package/%s.sh", INTERNAL_PATH, get_last_subdir(picker_type, '/', 1));

    char *selected_item = lv_label_get_text(lv_group_get_focused(ui_group));

    char relative_zip_path[PATH_MAX];
    if (strcasecmp(base_dir, sys_dir) == 0) {
        snprintf(relative_zip_path, sizeof(relative_zip_path), "%s",
                 selected_item);
    } else {
        char *relative_path = sys_dir + strlen(base_dir);
        if (*relative_path == '/') relative_path++;
        snprintf(relative_zip_path, sizeof(relative_zip_path), "%s/%s",
                 relative_path, selected_item);
    }

    size_t exec_count;
    const char *args[] = {picker_script, "install", relative_zip_path, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        if (config.SETTINGS.GENERAL.BGM == 2 && !strcasecmp(picker_type, "/theme")) {
            play_silence_bgm();
        }
        run_exec(exec, exec_count, 0);
    }
    free(exec);

    load_mux("picker");

    close_input();
    mux_input_stop();
}

static void handle_back() {
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
        write_text_to_file(EXPLORE_NAME, "w", CHAR, get_last_subdir(sys_dir, '/', 5));
        load_mux("picker");
    }

    close_input();
    mux_input_stop();
}

static void handle_save() {
    if (msgbox_active) return;

    play_sound(SND_CONFIRM);

    write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

    static char picker_script[MAX_BUFFER_SIZE];
    snprintf(picker_script, sizeof(picker_script),
             "%s/script/package/%s.sh", INTERNAL_PATH, get_last_subdir(picker_type, '/', 1));

    size_t exec_count;
    const char *args[] = {picker_script, "save", "-", NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0);
    }
    free(exec);

    load_mux("picker");

    close_input();
    mux_input_stop();
}

static void handle_help() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements() {
    lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);

    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.SELECT, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavYGlyph, "",                  0},
            {ui_lblNavY,      lang.GENERIC.SAVE,   0},
            {NULL, NULL,                           0}
    });

    if (!strcasecmp(picker_type, "package/bootlogo")) {
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
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

int muxpicker_main(char *type, char *ex_dir) {
    snprintf(picker_type, sizeof(picker_type), "%s", type);
    snprintf(sys_dir, sizeof(sys_dir), "%s", ex_dir);
    snprintf(base_dir, sizeof(base_dir), (RUN_STORAGE_PATH "%s"), picker_type);
    if (strcmp(sys_dir, "") == 0) snprintf(sys_dir, sizeof(sys_dir), "%s", base_dir);

    init_module("muxpicker");

    init_theme(1, 1);

    config.VISUAL.BOX_ART = 1;  //Force correct panel size for displaying preview in bottom right

    const char *picker_title = NULL;
    if (!strcasecmp(picker_type, "/theme")) {
        picker_extension = "muxthm";
        picker_title = lang.MUXPICKER.THEME;
    } else if (!strcasecmp(picker_type, "package/bootlogo")) {
        picker_extension = "muxblg";
        picker_title = lang.MUXPICKER.BOOTLOGO;
    } else if (!strcasecmp(picker_type, "package/catalogue")) {
        picker_extension = "muxcat";
        picker_title = lang.MUXPICKER.CATALOGUE;
    } else if (!strcasecmp(picker_type, "package/config")) {
        picker_extension = "muxcfg";
        picker_title = lang.MUXPICKER.CONFIG;
    } else {
        picker_extension = "muxcus";
        picker_title = lang.MUXPICKER.CUSTOM;
    }
    init_ui_common_screen(&theme, &device, &lang, picker_title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_picker_items();
    init_elements();

    int sys_index = 0;
    if (file_exist(MUOS_PIN_LOAD)) {
        sys_index = read_line_int_from(MUOS_PIN_LOAD, 1);
        remove(MUOS_PIN_LOAD);
    }

    char *e_name_line = file_exist(EXPLORE_NAME) ? read_line_char_from(EXPLORE_NAME, 1) : NULL;
    if (e_name_line) {
        for (size_t i = 0; i < item_count; i++) {
            if (!strcasecmp(items[i].name, e_name_line)) {
                sys_index = (int) i;
                remove(EXPLORE_NAME);
                break;
            }
        }
    }

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) list_nav_move(sys_index, +1);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);

        const char *message_text = NULL;
        if (!strcasecmp(picker_type, "/theme")) {
            message_text = lang.MUXPICKER.NONE.THEME;
        } else if (!strcasecmp(picker_type, "package/bootlogo")) {
            message_text = lang.MUXPICKER.NONE.BOOTLOGO;
        } else if (!strcasecmp(picker_type, "package/catalogue")) {
            message_text = lang.MUXPICKER.NONE.CATALOGUE;
        } else if (!strcasecmp(picker_type, "package/config")) {
            message_text = lang.MUXPICKER.NONE.CONFIG;
        } else {
            message_text = lang.MUXPICKER.NONE.CUSTOM;
        }
        lv_label_set_text(ui_lblScreenMessage, message_text);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_confirm_force,
                    [MUX_INPUT_Y] = handle_save,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
