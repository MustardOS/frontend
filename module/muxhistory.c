#include "muxshare.h"
#include "ui/ui_muxhistory.h"

static lv_obj_t *ui_imgSplash;
static lv_obj_t *ui_viewport_objects[7];

static int exit_status = 0;
static int file_count = 0;
static int starter_image = 0;
static int splash_valid = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];

static char *load_content_description() {
    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_char_from(core_file, CACHE_CORE_PATH), '/', 6));

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
             INFO_CAT_PATH, read_line_char_from(pointer, CONTENT_SYSTEM),
             strip_ext(read_line_char_from(pointer, CONTENT_FULL)));

    if (file_exist(content_desc)) {
        return read_all_char_from(content_desc);
    }

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return lang.GENERIC.NO_INFO;
}

static void image_refresh(char *image_type) {
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) return;

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    char core_artwork[MAX_BUFFER_SIZE];

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_char_from(core_file, CACHE_CORE_PATH), '/', 6));

    char *h_file_name = strip_ext(read_line_char_from(pointer, CONTENT_FULL));

    char *h_core_artwork = read_line_char_from(pointer, CONTENT_SYSTEM);
    if (strlen(h_core_artwork) <= 1) {
        snprintf(image, sizeof(image), "%s/%simage/none_%s.png",
                 STORAGE_THEME, mux_dimension, image_type);
        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/image/none_%s.png",
                     STORAGE_THEME, image_type);
        }
    } else {
        load_image_catalogue(h_core_artwork, h_file_name, "default", mux_dimension, image_type,
                             image, sizeof(image));
        if (!strcasecmp(image_type, "splash") && !file_exist(image)) {
            load_splash_image_fallback(mux_dimension, image, sizeof(image));
        }
    }
    snprintf(core_artwork, sizeof(core_artwork), "%s", h_core_artwork);

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image)

    if (strcasecmp(image_type, "preview") == 0) {
        if (strcasecmp(preview_image_previous_path, image) != 0) {
            if (file_exist(image)) {
                struct ImageSettings image_settings = {
                        image, LV_ALIGN_CENTER,
                        validate_int16((int16_t) (device.MUX.WIDTH * .9) - 60, "width"),
                        validate_int16((int16_t) (device.MUX.HEIGHT * .9) - 120, "height"),
                        0, 0, 0, 0
                };
                update_image(ui_imgHelpPreviewImage, image_settings);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgHelpPreviewImage, &ui_image_Nothing);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), " ");
            }
        }
    } else if (strcasecmp(image_type, "splash") == 0) {
        if (strcasecmp(splash_image_previous_path, image) != 0) {
            if (file_exist(image)) {
                splash_valid = 1;
                snprintf(image_path, sizeof(image_path), "M:%s", image);
                lv_img_set_src(ui_imgSplash, image_path);
                snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), "%s", image);
            } else {
                splash_valid = 0;
                lv_img_set_src(ui_imgSplash, &ui_image_Nothing);
                snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), " ");
            }
        }
    } else {
        if (strcasecmp(box_image_previous_path, image) != 0) {
            char artwork_config_path[MAX_BUFFER_SIZE];
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                     INFO_CAT_PATH, core_artwork);
            if (!file_exist(artwork_config_path)) {
                snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                         INFO_CAT_PATH);
            }

            if (file_exist(artwork_config_path)) {
                viewport_refresh(ui_viewport_objects, artwork_config_path, core_artwork, h_file_name);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                if (file_exist(image)) {
                    starter_image = 1;
                    snprintf(image_path, sizeof(image_path), "M:%s", image);
                    lv_img_set_src(ui_imgBox, image_path);
                    snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
                } else {
                    lv_img_set_src(ui_imgBox, &ui_image_Nothing);
                    snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
                }
            }
        }
    }
}

static void add_file_names(const char *base_dir, char ***file_names, char ***last_dirs) {
    struct dirent *entry;
    DIR *dir = opendir(base_dir);
    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);

        char *file_path = strdup(entry->d_name);
        *file_names = realloc(*file_names, (file_count + 1) * sizeof(char *));
        (*file_names)[file_count] = file_path;

        char *path_line = read_line_char_from(full_path, 1);
        char *dir_name = NULL;

        if (path_line) {
            char *last_slash = strrchr(path_line, '/');

            if (last_slash) {
                *last_slash = '\0';
                char *second_last_slash = strrchr(path_line, '/');
                dir_name = second_last_slash ? strdup(second_last_slash + 1) : strdup(path_line);
            } else dir_name = strdup("");

            free(path_line);
        } else dir_name = strdup("");

        *last_dirs = realloc(*last_dirs, (file_count + 1) * sizeof(char *));
        (*last_dirs)[file_count] = dir_name;

        file_count++;
    }

    closedir(dir);
}

static char *get_glyph_name(size_t index) {
    char history_file[PATH_MAX];
    snprintf(history_file, sizeof(history_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[index].name));

    const char *file_name = strip_ext(items[index].name);
    const char *system_name = read_line_char_from(history_file, CACHE_CORE_DIR);

    if (search_for_config(INFO_COL_PATH, file_name, system_name)) return "collection";
    return "history";
}

static void gen_item(int file_count, char **file_names, char **last_dirs) {
    for (int i = 0; i < file_count; i++) {
        int has_custom_name = 0;
        char fn_name[MAX_BUFFER_SIZE];

        char history_file[MAX_BUFFER_SIZE];
        snprintf(history_file, sizeof(history_file), "%s/%s",
                 INFO_HIS_PATH, file_names[i]);

        char *cache_file = read_line_char_from(history_file, CACHE_CORE_PATH);
        char *stripped_name = read_line_char_from(history_file, CACHE_CORE_NAME);

        if (stripped_name && stripped_name[0] == '\0') {
            stripped_name = strip_ext(read_line_char_from(cache_file, CONTENT_FULL));
        }

        char custom_lookup[MAX_BUFFER_SIZE];
        snprintf(custom_lookup, sizeof(custom_lookup), INFO_NAM_PATH "/%s.json", last_dirs[i]);
        if (!file_exist(custom_lookup)) snprintf(custom_lookup, sizeof(custom_lookup), INFO_NAM_PATH "/global.json");

        int fn_valid = 0;
        struct json fn_json;

        if (json_valid(read_all_char_from(custom_lookup))) {
            fn_valid = 1;
            fn_json = json_parse(read_all_char_from(custom_lookup));
        }

        if (fn_valid) {
            struct json custom_lookup_json = json_object_get(fn_json, str_tolower(stripped_name));
            if (json_exists(custom_lookup_json)) {
                json_string_copy(custom_lookup_json, fn_name, sizeof(fn_name));
                has_custom_name = 1;
            }
        }

        if (!has_custom_name) {
            const char *lookup_result = read_line_int_from(cache_file, CONTENT_LOOKUP) ? lookup(stripped_name) : NULL;
            snprintf(fn_name, sizeof(fn_name), "%s", lookup_result ? lookup_result : stripped_name);
        }

        content_item *new_item = add_item(&items, &item_count, file_names[i], fn_name, "", ITEM);
        adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

        ui_count++;
    }

    sort_items_time(items, item_count);

    char *glyph_icons[item_count];
    for (size_t i = 0; i < item_count; i++) {
        glyph_icons[i] = (items[i].content_type == ITEM) ? get_glyph_name(i) : "unknown";
    }

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ITEM) {
            gen_label(mux_module, glyph_icons[i], items[i].display_name);
        }
    }
}

static void create_history_items() {
    char **file_names = NULL;
    char **last_dirs = NULL;

    lv_label_set_text(ui_lblTitle, lang.MUXHISTORY.TITLE);
    add_file_names(INFO_HIS_PATH, &file_names, &last_dirs);

    if (file_count > 0) {
        gen_item(file_count, file_names, last_dirs);
        lv_obj_update_layout(ui_pnlContent);
    }

    free(file_names);
}

static void remove_from_history() {
    char history_file[MAX_BUFFER_SIZE];
    snprintf(history_file, sizeof(history_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[current_item_index].name));

    if (file_exist(history_file)) {
        remove(history_file);
        load_mux("history");

        close_input();
        mux_input_stop();
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.MUXHISTORY.ERROR.REMOVE, 1000);
    }
}

static void add_to_collection() {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), INFO_HIS_PATH "/%s",
             items[current_item_index].name);

    char *cache_file = read_line_char_from(pointer_file, CACHE_CORE_PATH);
    char history_content[MAX_BUFFER_SIZE];
    snprintf(history_content, sizeof(history_content), "%s\n%s\n%s",
             get_last_dir(cache_file),
             cache_file,
             read_line_char_from(pointer_file, CACHE_CORE_DIR)
    );

    write_text_to_file(ADD_MODE_WORK, "w", CHAR, history_content);
    write_text_to_file(ADD_MODE_FROM, "w", CHAR, "history");

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    load_mux("collection");

    close_input();
    mux_input_stop();
}

static int load_content(const char *content_name) {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), INFO_HIS_PATH "/%s",
             content_name);

    char cache_file[MAX_BUFFER_SIZE];
    snprintf(cache_file, sizeof(cache_file), "%s",
             read_line_char_from(pointer_file, CACHE_CORE_PATH));

    char *assigned_gov = NULL;
    assigned_gov = load_content_governor(NULL, cache_file, 0, 0);
    if (!assigned_gov) assigned_gov = device.CPU.DEFAULT;

    if (file_exist(cache_file)) {
        char *assigned_core = read_line_char_from(cache_file, CONTENT_CORE);
        LOG_INFO(mux_module, "Assigned Core: %s", assigned_core)
        LOG_INFO(mux_module, "Assigned Governor: %s", assigned_gov)
        LOG_INFO(mux_module, "Using Configuration: %s", cache_file)

        char add_to_history[MAX_BUFFER_SIZE];
        snprintf(add_to_history, sizeof(add_to_history), "%s/%s",
                 INFO_HIS_PATH, content_name);

        write_text_to_file(add_to_history, "w", CHAR, read_all_char_from(pointer_file));
        write_text_to_file(LAST_PLAY_FILE, "w", CHAR, read_line_char_from(pointer_file, CONTENT_NAME));
        write_text_to_file(MUOS_GOV_LOAD, "w", CHAR, assigned_gov);
        write_text_to_file(MUOS_ROM_LOAD, "w", CHAR, read_all_char_from(cache_file));
        return 1;
    }

    toast_message(lang.MUXHISTORY.ERROR.LOAD, 1000);
    LOG_ERROR(mux_module, "Cache Pointer Not Found: %s", cache_file)

    return 0;
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
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));

    image_refresh("box");
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a() {
    if (!ui_count) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    play_sound(SND_CONFIRM);

    char f_content[MAX_BUFFER_SIZE];
    snprintf(f_content, sizeof(f_content), "%s.cfg",
             strip_ext(items[current_item_index].name));

    if (load_content(f_content)) {
        if (config.VISUAL.LAUNCHSPLASH) {
            image_refresh("splash");
            if (splash_valid) {
                lv_obj_center(ui_imgSplash);
                lv_obj_move_foreground(ui_imgSplash);
                lv_obj_move_foreground(overlay_image);

                for (unsigned int i = 0; i <= 255; i += 15) {
                    lv_obj_set_style_img_opa(ui_imgSplash, i, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_task_handler();
                    usleep(128);
                }

                sleep(1);
            }
        }

        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        exit_status = 1;
    } else {
        return;
    }

    load_mux("history");
    close_input();
    mux_input_stop();
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "history");
    close_input();
    mux_input_stop();
}

static void handle_x() {
    if (msgbox_active || !ui_count || kiosk.CONTENT.HISTORY) return;

    play_sound(SND_CONFIRM);
    remove_from_history();
}

static void handle_y() {
    if (msgbox_active || !ui_count || kiosk.COLLECT.ADD_CON) return;

    play_sound(SND_CONFIRM);
    add_to_collection();
}

static void handle_menu() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    image_refresh("preview");

    show_info_box(items[current_item_index].display_name, load_content_description(), 1);
}

static void handle_random_select() {
    if (msgbox_active || ui_count < 2) return;

    uint32_t random_select = random() % ui_count;
    int selected_index = (int) (random_select & INT16_MAX);

    !(selected_index & 1) ? list_nav_move(selected_index, +1) : list_nav_move(selected_index, -1);
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_lblCounter_history,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_elements() {
    lv_obj_set_align(ui_imgBox, config.VISUAL.BOX_ART_ALIGN);
    lv_obj_set_align(ui_viewport_objects[0], config.VISUAL.BOX_ART_ALIGN);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph,    "",                   1},
            {ui_lblNavA,         lang.GENERIC.OPEN,    1},
            {ui_lblNavBGlyph,    "",                   0},
            {ui_lblNavB,         lang.GENERIC.BACK,    0},
            {ui_lblNavXGlyph,    "",                   1},
            {ui_lblNavX,         lang.GENERIC.REMOVE,  1},
            {ui_lblNavYGlyph,    "",                   1},
            {ui_lblNavY,         lang.GENERIC.COLLECT, 1},
            {ui_lblNavMenuGlyph, "",                   1},
            {ui_lblNavMenu,      lang.GENERIC.INFO,    1},
            {NULL, NULL,                               0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panels();

        const char *content_label = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter(ui_lblCounter_history, file_count);
        lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

int muxhistory_main(int his_index) {
    exit_status = 0;
    file_count = 0;
    starter_image = 0;
    splash_valid = 0;

    init_module("muxhistory");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxhistory(ui_screen, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    ui_imgSplash = lv_img_create(ui_screen);
    lv_obj_set_style_img_opa(ui_imgSplash, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    create_history_items();
    init_elements();

    int nav_vis = 0;
    if (ui_count > 0) {
        nav_vis = 1;
        if (his_index > -1 && his_index <= ui_count && current_item_index < ui_count) {
            list_nav_move(his_index, +1);
        } else {
            image_refresh("box");
        }
        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXHISTORY.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,         nav_vis},
            {ui_lblNavAGlyph,    nav_vis},
            {ui_lblNavX,         nav_vis},
            {ui_lblNavXGlyph,    nav_vis},
            {ui_lblNavY,         nav_vis},
            {ui_lblNavYGlyph,    nav_vis},
            {ui_lblNavMenu,      nav_vis},
            {ui_lblNavMenuGlyph, nav_vis}
    };

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (kiosk.CONTENT.HISTORY) {
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (kiosk.COLLECT.ADD_CON) {
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    update_file_counter(ui_lblCounter_history, file_count);

    if (file_exist(ADD_MODE_DONE)) {
        if (!strcasecmp(read_all_char_from(ADD_MODE_DONE), "DONE")) {
            toast_message(lang.GENERIC.ADD_COLLECT, 1000);
            refresh_screen(ui_screen);
        }
        remove(ADD_MODE_DONE);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_random_select,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_random_select,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return exit_status;
}
