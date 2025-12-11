#include "muxshare.h"
#include "ui/ui_muxsearch.h"
#include "../common/skip_list.h"

#define UI_COUNT 3

static int starter_image = 0;
static int got_results = 0;

static char SD1[MAX_BUFFER_SIZE];
static char SD2[MAX_BUFFER_SIZE];
static char E_USB[MAX_BUFFER_SIZE];

static char search_result[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char lookup_value[MAX_BUFFER_SIZE];

size_t all_item_count = 0;
content_item *all_items = NULL;

static lv_obj_t *ui_viewport_objects[7];

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblLookup_search,       lang.MUXSEARCH.HELP.LOOKUP},
            {ui_lblSearchLocal_search,  lang.MUXSEARCH.HELP.LOCAL},
            {ui_lblSearchGlobal_search, lang.MUXSEARCH.HELP.GLOBAL},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, search, Lookup, lang.MUXSEARCH.LOOKUP, "lookup", "");
    INIT_VALUE_ITEM(-1, search, SearchLocal, lang.MUXSEARCH.LOCAL, "local", "");
    INIT_VALUE_ITEM(-1, search, SearchGlobal, lang.MUXSEARCH.GLOBAL, "global", "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void image_refresh() {
    if (config.VISUAL.BOX_ART == 8) return;

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    char core_artwork[MAX_BUFFER_SIZE];

    char *file_name = get_last_subdir(strip_ext(all_items[current_item_index].extra_data), '/', 4);
    char *last_dir = get_last_dir(strip_ext(all_items[current_item_index].extra_data));

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%s.cfg",
             str_replace(file_name, last_dir, ""));

    if (!file_exist(core_file)) {
        snprintf(core_file, sizeof(core_file), INFO_COR_PATH "/%score.cfg",
                 str_replace(file_name, last_dir, ""));
        snprintf(core_artwork, sizeof(core_artwork), "%s",
                 read_line_char_from(core_file, 2));
    } else {
        snprintf(core_artwork, sizeof(core_artwork), "%s",
                 read_line_char_from(core_file, 3));
    }

    LOG_INFO(mux_module, "Reading Configuration: %s", core_file)

    if (strlen(core_artwork) <= 1) {
        snprintf(image, sizeof(image), "%s/%simage/none_%s.png",
                 STORAGE_THEME, mux_dimension, "box");
        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/image/none_%s.png",
                     STORAGE_THEME, "box");
        }
    } else {
        load_image_catalogue(core_artwork, last_dir, "", "default", mux_dimension, "box",
                             image, sizeof(image));
    }

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", "box", image)

    if (strcasecmp(box_image_previous_path, image) != 0) {
        char artwork_config_path[MAX_BUFFER_SIZE];
        snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                 INFO_CAT_PATH, core_artwork);
        if (!file_exist(artwork_config_path)) {
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                     INFO_CAT_PATH);
        }

        if (file_exist(artwork_config_path)) {
            viewport_refresh(ui_viewport_objects, artwork_config_path, core_artwork, last_dir);
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

static void gen_result(char *item_glyph, char *item_text, char *item_data, char *item_value) {
    lv_obj_t *ui_pnlResult = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(ui_pnlResult);

    lv_obj_t *ui_lblResultItem = lv_label_create(ui_pnlResult);
    apply_theme_list_item(&theme, ui_lblResultItem, item_text);

    lv_obj_t *ui_lblResultItemValue = lv_label_create(ui_pnlResult);
    lv_label_set_text(ui_lblResultItemValue, item_value);
    lv_obj_set_style_text_opa(ui_lblResultItemValue, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_width(ui_lblResultItemValue, 0);

    lv_obj_t *ui_lblResultItemGlyph = lv_img_create(ui_pnlResult);
    apply_theme_list_glyph(&theme, ui_lblResultItemGlyph, mux_module, item_glyph);

    if (strcasecmp(item_data, "folder") != 0) {
        apply_size_to_content(&theme, ui_pnlContent, ui_lblResultItem, ui_lblResultItemGlyph, item_text);
    }
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblResultItem);

    lv_obj_set_user_data(ui_lblResultItem, item_data);

    if (strcasecmp(item_data, "content") == 0) {
        if (file_exist(FRIENDLY_RESULT)) {
            LOG_INFO(mux_module, "Reading Friendly Name Set: %s", FRIENDLY_RESULT)

            int fn_valid = 0;
            struct json fn_json;

            if (json_valid(read_all_char_from(FRIENDLY_RESULT))) {
                fn_valid = 1;
                fn_json = json_parse(read_all_char_from(FRIENDLY_RESULT));
            }

            if (fn_valid) {
                char fn_name[MAX_BUFFER_SIZE];
                struct json good_name_json = json_object_get(fn_json, strip_ext(item_text));

                if (json_exists(good_name_json)) {
                    json_string_copy(good_name_json, fn_name, sizeof(fn_name));
                    lv_label_set_text(ui_lblResultItem, fn_name);
                }
            }
        }

        lv_group_add_obj(ui_group, ui_lblResultItem);
        lv_group_add_obj(ui_group_value, ui_lblResultItemValue);
        lv_group_add_obj(ui_group_glyph, ui_lblResultItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlResult);

        ui_count++;
    }

    if (strcasecmp(item_data, "folder") == 0) {
        lv_obj_set_style_border_width(ui_pnlResult, 1, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(ui_pnlResult, lv_color_hex(theme.LIST_DEFAULT.TEXT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnlResult, theme.LIST_DEFAULT.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_side(ui_pnlResult, LV_BORDER_SIDE_TOP, MU_OBJ_MAIN_DEFAULT);
    }
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (all_item_count > 0 && all_items[current_item_index].content_type == ITEM) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));
        }

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    scroll_object_to_middle(ui_pnlContent, lv_group_get_focused(ui_group_panel));

    if (all_item_count > 0 && all_items[current_item_index].content_type == ITEM) {
        image_refresh();
        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    } else {
        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), "");
    }

    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void process_results(const char *json_results) {
    if (!json_valid(json_results)) {
        LOG_ERROR(mux_module, "Invalid JSON Format")
        return;
    }

    struct json root = json_parse(json_results);

    struct json lookup = json_object_get(root, "lookup");
    if (json_exists(lookup) && json_type(lookup) == JSON_STRING) {
        json_string_copy(lookup, lookup_value, sizeof(lookup_value));
    }

    struct json directories = json_object_get(root, "directories");
    if (json_exists(directories) && json_type(directories) == JSON_ARRAY) {
        if (json_array_count(directories) > 0) {
            list_nav_move(json_array_count(directories) == 1 ? 1 : 2, +1);
        }
    }

    struct json search_folders = json_object_get(root, "folders");
    if (!json_exists(search_folders) || json_type(search_folders) != JSON_OBJECT) return;

    size_t t_all_item_count = 0;
    content_item *t_all_items = NULL;

    struct json key = json_first(search_folders);
    while (json_exists(key)) {
        struct json val = json_next(key);
        if (!json_exists(val)) break;

        char folder_name[MAX_BUFFER_SIZE];
        if (json_type(key) == JSON_STRING) {
            json_string_copy(key, folder_name, sizeof(folder_name));
        } else {
            snprintf(folder_name, sizeof(folder_name), "unknown");
        }

        char storage_name_short[MAX_BUFFER_SIZE] = "";
        char *storage_name = NULL;

        if (str_extract(folder_name, "", "/ROMS/", &storage_name)) {
            snprintf(storage_name_short, sizeof(storage_name_short), "%s", storage_name);
            free(storage_name);

            if (strcasecmp(storage_name_short, device.STORAGE.ROM.MOUNT) == 0) {
                snprintf(storage_name_short, sizeof(storage_name_short), "SD1");
            } else if (strcasecmp(storage_name_short, device.STORAGE.SDCARD.MOUNT) == 0) {
                snprintf(storage_name_short, sizeof(storage_name_short), "SD2");
            } else if (strcasecmp(storage_name_short, device.STORAGE.USB.MOUNT) == 0) {
                snprintf(storage_name_short, sizeof(storage_name_short), "USB");
            } else if (strstr(folder_name, "/mnt/union/")) {
                snprintf(storage_name_short, sizeof(storage_name_short), "UNION");
            }
        }

        char folder_name_short[MAX_BUFFER_SIZE] = "";
        if (strcasecmp(storage_name_short, "UNION") == 0) {
            char *after_roms = strstr(folder_name, "/ROMS/");
            if (after_roms) {
                after_roms += strlen("/ROMS/");
                snprintf(folder_name_short, sizeof(folder_name_short), "%s", after_roms);
            } else {
                snprintf(folder_name_short, sizeof(folder_name_short), "%s", folder_name);
            }
        } else {
            char *modified_name = NULL;
            if (str_replace_segment(folder_name, "/mnt/", "/ROMS/", "", &modified_name)) {
                snprintf(folder_name_short, sizeof(folder_name_short), " %s/%s",
                         storage_name_short,
                         str_replace(modified_name, "/mnt//ROMS/", ""));
                free(modified_name);
            } else {
                snprintf(folder_name_short, sizeof(folder_name_short), "%s", folder_name);
            }
        }

        if (strcasecmp(folder_name, ".") != 0) {
            gen_result("folder", folder_name_short, "folder", "");
        }

        struct json content = json_object_get(val, "content");
        if (!json_exists(content) || json_type(content) != JSON_ARRAY) {
            key = json_next(val);
            continue;
        }

        size_t folder_item_count = 0;
        content_item *folder_items = NULL;

        for (size_t i = 0; i < json_array_count(content); i++) {
            struct json item = json_array_get(content, i);
            if (json_type(item) != JSON_OBJECT) continue;

            struct json file_json = json_object_get(item, "file");
            struct json name_json = json_object_get(item, "name");
            if (!json_exists(file_json) || !json_exists(name_json)) continue;

            char file_name[MAX_BUFFER_SIZE];
            char display_name[MAX_BUFFER_SIZE];
            json_string_copy(file_json, file_name, sizeof(file_name));
            json_string_copy(name_json, display_name, sizeof(display_name));

            char content_full_path[MAX_BUFFER_SIZE];
            if (folder_name[0] != '/') {
                snprintf(content_full_path, sizeof(content_full_path), "%s/%s/%s",
                         rom_dir, folder_name, file_name);
            } else {
                snprintf(content_full_path, sizeof(content_full_path), "%s/%s",
                         folder_name, file_name);
            }

            adjust_visual_label(display_name, config.VISUAL.NAME, config.VISUAL.DASH);
            add_item(&folder_items, &folder_item_count, display_name, display_name, content_full_path, ITEM);
        }

        sort_items(folder_items, folder_item_count);


        SkipList skiplist;
        init_skiplist(&skiplist);
        for (int i = 0; i < folder_item_count; i++) {
            char *item_dir = strip_dir(folder_items[i].extra_data);
            char *item_file = get_last_dir(strdup(folder_items[i].extra_data));
            if (ends_with(item_file, ".cue")) {
                process_cue_file(item_dir, item_file, &skiplist);
            } else if (ends_with(item_file, ".gdi")) {
                process_gdi_file(item_dir, item_file, &skiplist);
            } else if (ends_with(item_file, ".m3u")) {
                process_m3u_file(item_dir, item_file, &skiplist);
            }
        }

        for (size_t i = 0; i < folder_item_count; i++) {
            if (folder_items[i].content_type == ITEM) {
                if (!in_skiplist(&skiplist, folder_items[i].extra_data)) {
                    add_item(&t_all_items, &t_all_item_count, folder_items[i].name,
                             folder_items[i].display_name, folder_items[i].extra_data, ITEM);

                    gen_result("content", folder_items[i].display_name,
                               "content", folder_items[i].extra_data);
                }
            }
        }

        free_skiplist(&skiplist);
        free_items(&folder_items, &folder_item_count);
        key = json_next(val);
    }

    // We are done with the friendly results JSON
    if (file_exist(FRIENDLY_RESULT)) {
        remove(FRIENDLY_RESULT);
    }

    // Add the three top labels - lookup, local, and global
    add_item(&all_items, &all_item_count, "", "", "", FOLDER);
    add_item(&all_items, &all_item_count, "", "", "", FOLDER);
    add_item(&all_items, &all_item_count, "", "", "", FOLDER);

    for (size_t i = 0; i < t_all_item_count; i++) {
        if (t_all_items[i].content_type == ITEM) {
            content_item *new_item = add_item(&all_items, &all_item_count, t_all_items[i].name,
                                              t_all_items[i].display_name, t_all_items[i].extra_data, ITEM);
            char display_name[MAX_BUFFER_SIZE];
            snprintf(display_name, sizeof(display_name), "%s",
                     t_all_items[i].display_name);
            adjust_visual_label(display_name, config.VISUAL.NAME, config.VISUAL.DASH);
            new_item->display_name = strdup(display_name);
        }
    }

    free_items(&t_all_items, &t_all_item_count);
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblLookup_search) {
        lv_label_set_text(ui_lblLookupValue_search,
                          lv_textarea_get_text(ui_txtEntry_search));
    }

    reset_osk(key_entry);

    lv_textarea_set_text(ui_txtEntry_search, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry_search, LV_OBJ_FLAG_HIDDEN);
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

static void handle_confirm(void) {
    if (file_exist(MUOS_SAA_LOAD)) remove(MUOS_SAA_LOAD);
    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblLookup_search) {
        play_sound(SND_CONFIRM);

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry_search, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry_search);

        lv_textarea_set_text(ui_txtEntry_search, lv_label_get_text(lv_group_get_focused(ui_group_value)));
        return;
    }

    if (element_focused == ui_lblSearchLocal_search || element_focused == ui_lblSearchGlobal_search) {
        char *lookup_value = lv_label_get_text(ui_lblLookupValue_search);

        if (strlen(lookup_value) <= 2) {
            play_sound(SND_ERROR);

            toast_message(lang.MUXSEARCH.ERROR, SHORT);
            return;
        }

        play_sound(SND_CONFIRM);

        toast_message(lang.MUXSEARCH.SEARCH, FOREVER);
        refresh_screen(ui_screen);

        if (element_focused == ui_lblSearchLocal_search) {
            const char *args[] = {(OPT_PATH "script/mux/find.sh"), "--local",
                                  str_trim(lookup_value), rom_dir, NULL};
            run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
        } else {
            const char *args[6];
            int idx = 0;

            args[idx++] = OPT_PATH "script/mux/find.sh";
            args[idx++] = str_trim(lookup_value);

            int mounts = 0;
            double total_space, free_space, used_space;

            get_storage_info(device.STORAGE.ROM.MOUNT, &total_space, &free_space, &used_space);
            if (total_space > 0) mounts |= 1;

            get_storage_info(device.STORAGE.SDCARD.MOUNT, &total_space, &free_space, &used_space);
            if (total_space > 0) mounts |= 2;

            get_storage_info(device.STORAGE.USB.MOUNT, &total_space, &free_space, &used_space);
            if (total_space > 0) mounts |= 4;

            if (mounts & 1) args[idx++] = SD1;
            if (mounts & 2) args[idx++] = SD2;
            if (mounts & 4) args[idx++] = E_USB;

            args[idx] = NULL;
            run_exec(args, idx, 0, 1, NULL, NULL);
        }

        if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);

        load_mux("search");

        close_input();
        mux_input_stop();

        return;
    }

    if (strcasecmp(lv_obj_get_user_data(element_focused), "content") == 0) {
        play_sound(SND_CONFIRM);

        const char *selected_raw = lv_label_get_text(lv_group_get_focused(ui_group_value));
        char *selected_path = str_replace(selected_raw, "/./", "/");

        char *path_union = str_replace(selected_path, device.STORAGE.ROM.MOUNT, "/mnt/union");
        path_union = str_replace(path_union, device.STORAGE.SDCARD.MOUNT, "/mnt/union");
        path_union = str_replace(path_union, device.STORAGE.USB.MOUNT, "/mnt/union");

        write_text_to_file(MUOS_RES_LOAD, "w", CHAR, path_union);

        char base_dir[MAX_BUFFER_SIZE];
        snprintf(base_dir, sizeof(base_dir), "%s", path_union);

        char *last_slash = strrchr(base_dir, '/');
        if (last_slash) *last_slash = '\0';

        write_text_to_file(EXPLORE_DIR, "w", CHAR, base_dir);

        load_mux("explore");

        close_input();
        mux_input_stop();
    }
}

static void handle_random_select(void) {
    if (msgbox_active || ui_count <= 3 || hold_call) return;

    uint32_t random_select = random() % ui_count;
    int selected_index = (int) (random_select & INT16_MAX);

    !(selected_index & 1) ? list_nav_move(selected_index, +1) : list_nav_move(selected_index, -1);
}

static void handle_back(void) {
    play_sound(SND_BACK);

    if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);
    if (is_ksk(kiosk.CONTENT.OPTION)) load_mux("explore");

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    key_show ? handle_keyboard_press() : handle_confirm();
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

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txtEntry_search, ui_pnlEntry_search);
        return;
    }

    handle_back();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        key_backspace(ui_txtEntry_search);
        return;
    }

    if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);
    if (file_exist(search_result)) remove(search_result);

    play_sound(SND_CONFIRM);
    load_mux("search");

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_swap();

    // TODO: A way to directly add the item to a collection
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call) return;

    if (all_items[current_item_index].content_type != ITEM) {
        play_sound(SND_INFO_OPEN);
        show_help(lv_group_get_focused(ui_group));
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
    if (key_show) key_left();
}

static void handle_right(void) {
    if (key_show) key_right();
}

static void handle_left_hold(void) {
    if (key_show) key_left();
}

static void handle_right_hold(void) {
    if (key_show) key_right();
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
            ui_pnlHelp,
            ui_pnlEntry_search,
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

    setup_nav((struct nav_bar[]) {
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {ui_lblNavXGlyph, "",                0},
            {ui_lblNavX,      "",                0},
            {NULL, NULL,                         0}
    });

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.GENERIC.CLEAR);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB,
            ui_lblNavXGlyph,
            ui_lblNavX
    };

    for (int i = 0; i < A_SIZE(nav_hide); i++) {
        lv_obj_clear_flag(nav_hide[i], MU_OBJ_FLAG_HIDE_FLOAT);
    }

#define SEARCH(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_search, UDATA);
    SEARCH_ELEMENTS
#undef SEARCH

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panels();
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
        process_key_event(&ev, ui_txtEntry_search);
    }
}

int muxsearch_main(char *dir) {
    if (!strlen(dir)) dir = "/mnt/union/ROMS";
    snprintf(rom_dir, sizeof(rom_dir), "%s", dir);

    starter_image = 0;
    got_results = 0;

    const char *m = "muxsearch";
    set_process_name(m);
    init_module(m);

    snprintf(search_result, sizeof(search_result), "%s/%s/search.json",
             device.STORAGE.ROM.MOUNT, MUOS_INFO_PATH);

    char *json_content;

    if (file_exist(search_result)) {
        json_content = read_all_char_from(search_result);
        if (json_content) {
            got_results = 1;
        } else {
            LOG_ERROR(mux_module, "Error reading search results")
        }
    }

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSEARCH.TITLE);
    init_muxsearch(ui_screen, ui_pnlContent, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    snprintf(SD1, sizeof(SD1), "%s/ROMS", device.STORAGE.ROM.MOUNT);
    snprintf(SD2, sizeof(SD2), "%s/ROMS", device.STORAGE.SDCARD.MOUNT);
    snprintf(E_USB, sizeof(E_USB), "%s/ROMS", device.STORAGE.USB.MOUNT);

    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    if (got_results) {
        process_results(json_content);
        lv_label_set_text(ui_lblLookupValue_search, lookup_value);
        free(json_content);
    }

    init_osk(ui_pnlEntry_search, ui_txtEntry_search, false);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
                    [MUX_INPUT_R2] = handle_random_select,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
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

    free_items(&all_items, &all_item_count);

    return 0;
}
