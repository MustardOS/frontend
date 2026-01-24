#include "muxshare.h"
#include "ui/ui_muxcollect.h"

static lv_obj_t *ui_imgSplash;
static lv_obj_t *ui_viewport_objects[7];

static char prev_dir[MAX_BUFFER_SIZE];
static char new_dir[MAX_BUFFER_SIZE];

static int exit_status = 0;
static int add_mode = 0;
static int sys_index = -1;
static int file_count = 0;
static int dir_count = 0;
static int starter_image = 0;
static int splash_valid = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];

char *access_mode = NULL;

static char *load_content_description(void) {
    char *item_dir = strip_dir(items[current_item_index].extra_data);
    char *item_file_name = get_last_dir(strdup(items[current_item_index].extra_data));

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name, core_desc, sizeof(core_desc));

    if (strlen(core_desc) <= 1 && items[current_item_index].content_type == ITEM) return lang.GENERIC.NO_INFO;

    char *h_file_name = items[current_item_index].content_type == FOLDER
                        ? items[current_item_index].name
                        : strip_ext(item_file_name);

    char *h_core_artwork = items[current_item_index].content_type == FOLDER
                           ? "Collection"
                           : core_desc;

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
             INFO_CAT_PATH, h_core_artwork, h_file_name);

    if (file_exist(content_desc)) {
        return read_all_char_from(content_desc);
    }

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return lang.GENERIC.NO_INFO;
}

static void image_refresh(char *image_type) {
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) return;

    char *item_dir = strip_dir(items[current_item_index].extra_data);
    char *item_file_name = get_last_dir(strdup(items[current_item_index].extra_data));

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name, core_desc, sizeof(core_desc));

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];

    char *h_file_name = items[current_item_index].content_type == FOLDER
                        ? items[current_item_index].name
                        : strip_ext(item_file_name);

    char *h_core_artwork = items[current_item_index].content_type == FOLDER
                           ? "Collection"
                           : core_desc;

    if (strlen(h_core_artwork) <= 1) {
        snprintf(image, sizeof(image), "%s/%simage/none_%s.png",
                 theme_base, mux_dimension, image_type);
        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/image/none_%s.png",
                     theme_base, image_type);
        }
    } else {
        if (strcasecmp(image_type, "box") != 0 || !grid_mode_enabled || !config.VISUAL.BOX_ART_HIDE) {
            load_image_catalogue(h_core_artwork, h_file_name, "", "default",
                                 mux_dimension, image_type, image, sizeof(image));
        }

        if (strcasecmp(image_type, "splash") == 0 && !file_exist(image)) {
            load_splash_image_fallback(mux_dimension, image, sizeof(image));
        }
    }

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image);

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
                     INFO_CAT_PATH, h_core_artwork);
            if (!file_exist(artwork_config_path)) {
                snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                         INFO_CAT_PATH);
            }

            if (file_exist(artwork_config_path)) {
                viewport_refresh(ui_viewport_objects, artwork_config_path, h_core_artwork, h_file_name);
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

static void add_directory_and_file_names(const char *base_dir, char ***dir_names,
                                         char ***file_names, char ***last_dirs) {
    file_count = 0;
    dir_count = 0;
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir))) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);
        if (entry->d_type == DT_DIR && at_base(sys_dir, access_mode)) {
            if (strcmp(entry->d_name, "kiosk") != 0 &&
                strcmp(entry->d_name, ".") != 0 &&
                strcmp(entry->d_name, "..") != 0) {
                char *subdir_path = (char *) malloc(strlen(entry->d_name) + 2);
                snprintf(subdir_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                *dir_names = (char **) realloc(*dir_names, (dir_count + 1) * sizeof(char *));
                (*dir_names)[dir_count] = subdir_path;
                (dir_count)++;
            }
        } else if (entry->d_type == DT_REG) {
            if (strcmp(entry->d_name, ".nogrid") == 0) continue;
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
    }

    closedir(dir);
}

static void gen_item(int file_count, char **file_names, char **last_dirs) {
    char init_meta_dir[MAX_BUFFER_SIZE];
    for (int i = 0; i < file_count; i++) {
        int has_custom_name = 0;
        char fn_name[MAX_BUFFER_SIZE];

        char collection_file[MAX_BUFFER_SIZE];
        snprintf(collection_file, sizeof(collection_file), "%s/%s",
                 sys_dir, file_names[i]);

        char *file_path = read_line_char_from(collection_file, CACHE_CORE_PATH);
        char *file_name = get_last_dir(strdup(file_path));
        char *stripped_name = read_line_char_from(collection_file, CACHE_CORE_NAME);
        char *sub_path = read_line_char_from(collection_file, CACHE_CORE_DIR);

        if (stripped_name && stripped_name[0] == '\0') stripped_name = strip_ext(file_name);

        snprintf(init_meta_dir, sizeof(init_meta_dir), INFO_COR_PATH "/%s/", sub_path);
        create_directories(init_meta_dir, 0);

        char custom_lookup[MAX_BUFFER_SIZE];
        snprintf(custom_lookup, sizeof(custom_lookup), INFO_NAM_PATH "/%s.json", last_dirs[i]);
        if (!file_exist(custom_lookup)) snprintf(custom_lookup, sizeof(custom_lookup), INFO_NAM_PATH "/global.json");

        int fn_valid = 0;
        struct json fn_json;

        if (file_exist(custom_lookup)) {
            char *lookup_content = read_all_char_from(custom_lookup);

            if (lookup_content && json_valid(lookup_content)) {
                fn_valid = 1;
                fn_json = json_parse(read_all_char_from(custom_lookup));
                LOG_SUCCESS(mux_module, "Using Friendly Name: %s", custom_lookup);
            } else {
                LOG_WARN(mux_module, "Invalid Friendly Name: %s", custom_lookup);
            }

            free(lookup_content);
        } else {
            LOG_WARN(mux_module, "Friendly Name does not exist: %s", custom_lookup);
        }

        if (fn_valid) {
            struct json custom_lookup_json = json_object_get(fn_json, str_tolower(stripped_name));
            if (json_exists(custom_lookup_json)) {
                json_string_copy(custom_lookup_json, fn_name, sizeof(fn_name));
                has_custom_name = 1;
            }
        }

        int lookup_line = CONTENT_LOOKUP;
        char name_lookup[MAX_BUFFER_SIZE];
        snprintf(name_lookup, sizeof(name_lookup), "%s/%s.cfg", str_rem_last_char(init_meta_dir, 1), stripped_name);

        if (!file_exist(name_lookup)) {
            snprintf(name_lookup, sizeof(name_lookup), "%score.cfg", init_meta_dir);
            lookup_line = GLOBAL_LOOKUP;
        }

        if (!has_custom_name) {
            const char *lookup_result = read_line_int_from(name_lookup, lookup_line) ? lookup(stripped_name) : NULL;
            snprintf(fn_name, sizeof(fn_name), "%s", lookup_result ? lookup_result : stripped_name);
        }

        content_item *new_item = add_item(&items, &item_count, file_names[i], fn_name, file_path, ITEM);
        adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

        ui_count++;
    }

    sort_items(items, item_count);

    if (grid_mode_enabled) return;

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ITEM) {
            gen_label(mux_module, "collection", items[i].display_name);
        }
    }
}

static void init_navigation_group_grid(void) {
    grid_mode_enabled = 1;

    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);

    load_font_section(FONT_PANEL_DIR, ui_pnlGrid);
    load_font_section(FONT_PANEL_DIR, ui_lblGridCurrentItem);

    if (is_carousel_grid_mode()) {
        create_carousel_grid();
        int prev_dir_index = get_folder_item_index_by_name(items, item_count, prev_dir);
        if (prev_dir_index > -1) sys_index = prev_dir_index;
    } else {
        for (int i = 0; i < item_count; i++) {
            if (strcasecmp(items[i].name, prev_dir) == 0) sys_index = (int) i;
            if (i < theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) gen_grid_item(i);
        }
    }
}

static void create_collection_items(void) {
    char **dir_names = NULL;
    char **file_names = NULL;
    char **last_dirs = NULL;

    add_directory_and_file_names(sys_dir, &dir_names, &file_names, &last_dirs);

    int fn_valid = 0;
    struct json fn_json = {0};

    turbo_time(1, 1);

    if (config.VISUAL.FRIENDLYFOLDER) {
        char folder_name_file[MAX_BUFFER_SIZE];
        snprintf(folder_name_file, sizeof(folder_name_file), INFO_NAM_PATH "/folder.json");

        if (file_exist(folder_name_file)) {
            char *file_content = read_all_char_from(folder_name_file);

            if (file_content && json_valid(file_content)) {
                fn_valid = 1;
                fn_json = json_parse(strdup(file_content));
                LOG_SUCCESS(mux_module, "Using Friendly Folder: %s", folder_name_file);
            } else {
                LOG_WARN(mux_module, "Invalid Friendly Folder: %s", folder_name_file);
            }

            free(file_content);
        } else {
            LOG_WARN(mux_module, "Friendly Folder does not exist: %s", folder_name_file);
        }
    }

    const char *col_path = (is_ksk(kiosk.COLLECT.ACCESS) && directory_exist(INFO_CKS_PATH))
                           ? INFO_CKS_PATH : INFO_COL_PATH;
    update_title(sys_dir, fn_valid, fn_json, lang.MUXCOLLECT.TITLE, col_path);

    if (dir_count > 0 || file_count > 0) {
        if (at_base(sys_dir, access_mode)) {
            for (int i = 0; i < dir_count; i++) {
                char *friendly_folder_name = get_friendly_folder_name(dir_names[i], fn_valid, fn_json);

                content_item *new_item = add_item(&items, &item_count, dir_names[i], friendly_folder_name, "", FOLDER);
                adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

                if (config.VISUAL.FOLDERITEMCOUNT) {
                    char display_name[MAX_BUFFER_SIZE];
                    snprintf(display_name, sizeof(display_name), "%s (%d)",
                             new_item->display_name, get_directory_item_count(sys_dir, new_item->name, 0));
                    new_item->display_name = strdup(display_name);
                }

                free(dir_names[i]);
                free(friendly_folder_name);
            }
        }

        sort_items(items, item_count);

        grid_mode_enabled = !disable_grid_file_exists(sys_dir) && theme.GRID.ENABLED &&
                            (
                                    (file_count > 0 && config.VISUAL.GRID_MODE_CONTENT) ||
                                    (dir_count > 0 && file_count == 0)
                            );
        if (!grid_mode_enabled) {
            for (int i = 0; i < dir_count; i++) {
                gen_label(mux_module, "folder", items[i].display_name);
                if (strcasecmp(items[i].name, prev_dir) == 0) sys_index = i;
            }
        }

        gen_item(file_count, file_names, last_dirs);

        if (grid_mode_enabled) {
            init_navigation_group_grid();
        }

        if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);

        free(file_names);
        free(dir_names);
    }

    turbo_time(0, 1);
}

static void update_footer_glyph(void) {
    if (!add_mode) return;
    lv_label_set_text(ui_lblNavA,
                      items[current_item_index].content_type == FOLDER ? lang.GENERIC.OPEN : lang.GENERIC.ADD);
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        if (grid_mode_enabled) update_grid(direction);
    }

    if (!grid_mode_enabled) {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                               current_item_index, ui_pnlContent);
    }

    if (!grid_mode_enabled) set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

    image_refresh("box");
    update_footer_glyph();
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;

    snprintf(new_dir, sizeof(new_dir), "%s/%s",
             sys_dir, lv_textarea_get_text(ui_txtEntry_collect));
    create_directories(new_dir, 0);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_textarea_get_text(ui_txtEntry_collect));
    load_mux("collection");

    close_input();
    mux_input_stop();
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else if (strcmp(is_key, OSK_UPPER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, OSK_CHAR) == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, OSK_LOWER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void add_collection_item(void) {
    char *base_file_name = read_line_char_from(ADD_MODE_WORK, 1);
    char *cache_file = read_line_char_from(ADD_MODE_WORK, 2);
    char *system_sub = read_line_char_from(ADD_MODE_WORK, 3);
    char full_file_path[MAX_BUFFER_SIZE];
    snprintf(full_file_path, sizeof(full_file_path), "%s%s/%s",
             read_line_char_from(cache_file, CONTENT_MOUNT),
             read_line_char_from(cache_file, CONTENT_DIR),
             base_file_name);

    char collection_content[MAX_BUFFER_SIZE];
    snprintf(collection_content, sizeof(collection_content), "%s\n%s\n%s",
             full_file_path, system_sub,
             strip_ext(base_file_name));

    char collection_file[MAX_BUFFER_SIZE];
    snprintf(collection_file, sizeof(collection_file), "%s/%s-%08X.cfg",
             sys_dir, strip_ext(base_file_name), fnv1a_hash_str(full_file_path));

    write_text_to_file(collection_file, "w", CHAR, collection_content);

    if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
    write_text_to_file(ADD_MODE_DONE, "w", CHAR, "DONE");

    if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
}

static void show_splash() {
    if (config.VISUAL.LAUNCHSPLASH) {
        image_refresh("splash");
        if (splash_valid) {
            lv_obj_center(ui_imgSplash);
            lv_obj_move_foreground(ui_imgSplash);
            lv_obj_move_foreground(overlay_image);

            for (unsigned int i = 0; i <= 255; i += 15) {
                lv_obj_set_style_img_opa(ui_imgSplash, i, MU_OBJ_MAIN_DEFAULT);
                lv_task_handler();
                usleep(128);
            }

            sleep(1);
        }
    }
}

static void process_load(int from_start) {
    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (hold_call || (!add_mode && !ui_count)) return;

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

    int load_message = 0;

    if (add_mode && !ui_count) {
        add_collection_item();
        goto load_end;
    }

    if (items[current_item_index].content_type == FOLDER) {
        load_message = 1;

        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                 sys_dir, items[current_item_index].name);

        write_text_to_file(COLLECTION_DIR, "w", CHAR, n_dir);
    } else {
        if (add_mode) {
            add_collection_item();
            goto load_end;
        } else {
            write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

            char *item_dir = strip_dir(items[current_item_index].extra_data);
            char *item_file_name = get_last_dir(strdup(items[current_item_index].extra_data));

            if (load_content(0, item_dir, item_file_name)) {
                if (config.SETTINGS.ADVANCED.PASSCODE) {
                    int result = 0;

                    while (result != 1) {
                        result = muxpass_main(PCT_LAUNCH);

                        switch (result) {
                            case 1:
                                show_splash();
                                config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
                                exit_status = 1;
                                break;
                            case 2:
                            default:
                                if (file_exist(MUOS_ROM_LOAD)) remove(MUOS_ROM_LOAD);
                                if (file_exist(MUOS_CON_LOAD)) remove(MUOS_CON_LOAD);
                                if (file_exist(MUOS_GOV_LOAD)) remove(MUOS_GOV_LOAD);

                                write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

                                goto load_end;
                        }
                    }
                } else {
                    show_splash();
                    config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
                    exit_status = 1;
                }
            } else {
                write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
                write_text_to_file(MUOS_ASS_FROM, "w", CHAR, "collection");
                write_text_to_file(OPTION_SKIP, "w", CHAR, "");
                load_mux("assign");
            }
        }
    }

    if (from_start) write_text_to_file(MANUAL_RA_LOAD, "w", INT, 1);
    if (load_message) toast_message(lang.GENERIC.LOADING, FOREVER);

    load_end:
    if (file_exist(ADD_MODE_DONE)) {
        load_mux(read_all_char_from(ADD_MODE_FROM));
    } else {
        load_mux("collection");
    }

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (hold_call) return;
    process_load(launch_flag(config.VISUAL.LAUNCH_SWAP, 0));
}

static void handle_a_hold(void) {
    if (msgbox_active || hold_call) return;
    process_load(launch_flag(config.VISUAL.LAUNCH_SWAP, 1));
}

static void handle_b(void) {
    if (key_show) {
        close_osk(key_entry, ui_group, ui_txtEntry_collect, ui_pnlEntry_collect);
        return;
    }

    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    if (at_base(sys_dir, access_mode)) {
        if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
        if (add_mode) write_text_to_file(ADD_MODE_DONE, "w", CHAR, "CANCEL");
        if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
    } else {
        char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) write_text_to_file(COLLECTION_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));
    }

    if (file_exist(COLLECTION_DIR)) {
        load_mux("collection");
    } else {
        if (file_exist(ADD_MODE_DONE)) {
            load_mux(read_all_char_from(ADD_MODE_FROM));
            remove(ADD_MODE_FROM);
        } else {
            load_mux("launcher");
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "collection");
        }
    }

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (key_show) {
        key_backspace(ui_txtEntry_collect);
        return;
    }

    if (msgbox_active || !ui_count || add_mode || hold_call) return;

    if (items[current_item_index].content_type == FOLDER) {
        if (get_directory_item_count(sys_dir, items[current_item_index].name, 0) > 0) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXCOLLECT.ERROR.REMOVE_DIR, SHORT);
            return;
        } else {
            char empty_dir[MAX_BUFFER_SIZE];
            snprintf(empty_dir, sizeof(empty_dir), "%s/%s",
                     sys_dir, items[current_item_index].name);

            remove(empty_dir);
        }
    } else {
        char collection_file[MAX_BUFFER_SIZE];
        snprintf(collection_file, sizeof(collection_file), "%s/%s.cfg",
                 sys_dir, strip_ext(items[current_item_index].name));

        remove(collection_file);
    }

    load_mux("collection");

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (key_show) {
        key_swap();
        return;
    }

    if (msgbox_active || hold_call) return;

    if (!is_ksk(kiosk.COLLECT.NEW_DIR) && at_base(sys_dir, access_mode)) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry_collect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry_collect);

        lv_textarea_set_text(ui_txtEntry_collect, "");
    }
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    image_refresh("preview");

    show_info_box(items[current_item_index].display_name, load_content_description(), 1);
}

static void handle_random_select(void) {
    if (msgbox_active || ui_count < 2 || hold_call || !config.VISUAL.SHUFFLE) return;

    int dir, target;
    shuffle_index(current_item_index, &dir, &target);

    if (grid_mode_enabled) {
        current_item_index = target;
        update_grid_items(1);
        list_nav_next(0);
    } else {
        list_nav_move(target, dir);
    }
}

static void handle_up(void) {
    key_show ? key_up() : handle_list_nav_up();
}

static void handle_up_hold(void) {
    key_show ? key_up() : handle_list_nav_up_hold();
}

static void handle_down(void) {
    key_show ? key_down() : handle_list_nav_down();
}

static void handle_down_hold(void) {
    key_show ? key_down() : handle_list_nav_down_hold();
}

static void handle_left(void) {
    key_show ? key_left() : handle_list_nav_left();
}

static void handle_right(void) {
    key_show ? key_right() : handle_list_nav_right();
}

static void handle_left_hold(void) {
    key_show ? key_left() : handle_list_nav_left_hold();
}

static void handle_right_hold(void) {
    key_show ? key_right() : handle_list_nav_right_hold();
}

static void handle_l1(void) {
    if (!key_show) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (!key_show) handle_list_nav_page_down();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_lblCounter_collect,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_elements(void) {
    lv_obj_set_align(ui_imgBox, config.VISUAL.BOX_ART_ALIGN);
    lv_obj_set_align(ui_viewport_objects[0], config.VISUAL.BOX_ART_ALIGN);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();
    lv_label_set_text(ui_lblPreviewHeader, lang.GENERIC.SWITCH_IMAGE);
    lv_obj_clear_flag(ui_lblPreviewHeaderGlyph, LV_OBJ_FLAG_HIDDEN);

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph,    "",                  0},
            {ui_lblNavA,         lang.GENERIC.OPEN,   0},
            {ui_lblNavBGlyph,    "",                  0},
            {ui_lblNavB,         lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,    "",                  0},
            {ui_lblNavX,         lang.GENERIC.REMOVE, 0},
            {ui_lblNavYGlyph,    "",                  0},
            {ui_lblNavY,         lang.GENERIC.NEW,    0},
            {ui_lblNavMenuGlyph, "",                  0},
            {ui_lblNavMenu,      lang.GENERIC.INFO,   0},
            {NULL, NULL,                              0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, WALL_GENERAL);
        adjust_panels();

        const char *content_label = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter(ui_lblCounter_collect, file_count);
        lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_OK_press();
    }

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txtEntry_collect);
    }
}

int muxcollect_main(int add, char *dir, int last_index) {
    exit_status = 0;
    add_mode = add;
    sys_index = last_index;
    file_count = 0;
    dir_count = 0;
    starter_image = 0;
    splash_valid = 0;

    const char *collection_path = (is_ksk(kiosk.COLLECT.ACCESS) && directory_exist(INFO_CKS_PATH))
                                  ? INFO_CKS_PATH
                                  : INFO_COL_PATH;

    snprintf(sys_dir, sizeof(sys_dir), "%s", (strcmp(dir, "") == 0) ? collection_path : dir);
    access_mode = strcasecmp(collection_path, INFO_CKS_PATH) ? "collection" : "kiosk";

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxcollect(ui_screen, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    ui_imgSplash = lv_img_create(ui_screen);
    lv_obj_set_style_img_opa(ui_imgSplash, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    reset_ui_groups();

    snprintf(prev_dir, sizeof(prev_dir), "%s", (file_exist(MUOS_PDI_LOAD)) ? read_all_char_from(MUOS_PDI_LOAD) : "");

    create_collection_items();
    init_elements();

    ui_count = (int) item_count;

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_dir(sys_dir));
    if (strcasecmp(read_all_char_from(MUOS_PDI_LOAD), "ROMS") == 0) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_subdir(sys_dir, '/', 4));
    }

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count &&
            current_item_index < ui_count) {
            list_nav_move(sys_index, +1);
        } else {
            image_refresh("box");
        }
        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXCOLLECT.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,         1},
            {ui_lblNavAGlyph,    1},
            {ui_lblNavX,         1},
            {ui_lblNavXGlyph,    1},
            {ui_lblNavY,         1},
            {ui_lblNavYGlyph,    1},
            {ui_lblNavMenu,      1},
            {ui_lblNavMenuGlyph, 1}
    };

    /* this should make it somewhat easier to reference hidden navs... */
    enum {
        NavA, NavAGlyph,
        NavX, NavXGlyph,
        NavY, NavYGlyph,
        NavMenu, NavMenuGlyph
    };

    struct {
        int add_mode;
        int at_collect;
        int ui_count;
        const int *hidden;
        size_t count;
    } nav_rules[] = {
            {1, 1, 0, (int[]) {NavX, NavXGlyph, NavMenu, NavMenuGlyph},                                   4},
            {1, 1, 1, (int[]) {NavX, NavXGlyph},                                                          2},
            {1, 0, 0, (int[]) {NavX, NavXGlyph, NavY, NavYGlyph, NavMenu, NavMenuGlyph},                  6},
            {1, 0, 1, (int[]) {NavX, NavXGlyph, NavY, NavYGlyph},                                         4},
            {0, 1, 0, (int[]) {NavA, NavAGlyph, NavX, NavXGlyph, NavMenu, NavMenuGlyph},                  6},
            {0, 0, 0, (int[]) {NavA, NavAGlyph, NavX, NavXGlyph, NavY, NavYGlyph, NavMenu, NavMenuGlyph}, 8},
            {0, 0, 1, (int[]) {NavY, NavYGlyph},                                                          2},
    };

    const int *hidden = NULL;
    size_t hidden_count;

    for (size_t i = 0; i < A_SIZE(nav_rules); ++i) {
        if (nav_rules[i].add_mode == add_mode &&
            nav_rules[i].at_collect == at_base(sys_dir, access_mode) &&
            nav_rules[i].ui_count == !!ui_count) {
            hidden = nav_rules[i].hidden;
            hidden_count = nav_rules[i].count;
            break;
        }
    }

    if (hidden != NULL) {
        for (int i = 0; i < hidden_count; ++i) {
            int k = hidden[i];
            if (k >= 0 && k < (int) A_SIZE(nav_e)) nav_e[k].visible = 0;
        }
    }

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (is_ksk(kiosk.COLLECT.REMOVE)) {
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (is_ksk(kiosk.COLLECT.NEW_DIR)) {
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    update_file_counter(ui_lblCounter_collect, file_count);
    init_osk(ui_pnlEntry_collect, ui_txtEntry_collect, false);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
                    [MUX_INPUT_R2] = handle_random_select,
            },
            .release_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_A] = handle_a_hold,
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_r1,
                    [MUX_INPUT_R2] = handle_random_select,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return exit_status;
}
