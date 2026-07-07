#include "muxshare.h"
#include "ui/ui_muxsearch.h"

#define SEARCH(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(SEARCH_ELEMENTS) };
#undef SEARCH

#define SEARCH_RUNNER  "/run/muos/search_run"
#define SEARCH_TIMEOUT 30

static int starter_image = 0;
static int splash_valid = 0;
static int got_results = 0;

static int search_pending = 0;
static int search_use_global = 0;
static time_t search_start = 0;
static lv_timer_t *search_poll_timer = NULL;

static lv_obj_t *ui_img_splash;

static char sd1[MAX_BUFFER_SIZE];
static char sd2[MAX_BUFFER_SIZE];
static char e_usb[MAX_BUFFER_SIZE];

static char search_result[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char lookup_value[MAX_BUFFER_SIZE];

size_t all_item_count = 0;
content_item *all_items = NULL;

static lv_obj_t *ui_viewport_objects[7];

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define SEARCH(NAME, UDATA) {UDATA, lang.muxsearch.help.NAME},
        SEARCH_ELEMENTS
#undef SEARCH
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, search, lookup, lang.muxsearch.lookup, "lookup", "");
    INIT_VALUE_ITEM(-1, search, search_local, lang.muxsearch.local, "local", "");
    INIT_VALUE_ITEM(-1, search, search_global, lang.muxsearch.global, "global", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void image_refresh() {
    if (config.visual.box_art == 8) return;

    char *item_dir = get_content_path(all_items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(
        item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(all_items[current_item_index].extra_data)
    );

    char core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, core_artwork, sizeof(core_artwork));

    char item_no_ext[MAX_BUFFER_SIZE];
    snprintf(item_no_ext, sizeof(item_no_ext), "%s", item_file_name_buf);
    char *dot = strrchr(item_no_ext, '.');
    if (dot) *dot = '\0';

    render_image_refresh(
        "box", core_artwork, item_no_ext, ui_img_splash, ui_viewport_objects, &starter_image, &splash_valid
    );
}

static void video_refresh(void) {
    if (!ui_count_static || all_items[current_item_index].content_type == FOLDER) return;

    char *item_dir = get_content_path(all_items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(
        item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(all_items[current_item_index].extra_data)
    );

    char core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, core_artwork, sizeof(core_artwork));

    char item_no_ext[MAX_BUFFER_SIZE];
    snprintf(item_no_ext, sizeof(item_no_ext), "%s", item_file_name_buf);
    char *dot = strrchr(item_no_ext, '.');
    if (dot) *dot = '\0';

    render_video_refresh(core_artwork, item_no_ext);
}

static void image_refresh_transition(void) {
    image_refresh();
    transition_box_art_apply_in(config.visual.box_art_transition);
    if (config.visual.video_preview > 0) video_refresh();
}

static void gen_result(const char *item_glyph, const char *item_text, char *item_data, const char *item_value) {
    lv_obj_t *ui_pnl_result = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(ui_pnl_result);

    lv_obj_t *ui_lbl_result_item = lv_label_create(ui_pnl_result);
    apply_theme_list_item(&theme, ui_lbl_result_item, item_text);

    lv_obj_t *ui_lbl_result_item_value = lv_label_create(ui_pnl_result);
    lv_label_set_text(ui_lbl_result_item_value, item_value);
    lv_obj_set_style_text_opa(ui_lbl_result_item_value, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_width(ui_lbl_result_item_value, 0);

    lv_obj_t *ui_lbl_result_item_glyph = lv_img_create(ui_pnl_result);
    apply_theme_list_glyph(&theme, ui_lbl_result_item_glyph, mux_module, item_glyph);

    if (strcasecmp(item_data, "folder") != 0) {
        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_result_item, ui_lbl_result_item_glyph, item_text);
    }
    apply_text_long_dot(&theme, ui_lbl_result_item);

    lv_obj_set_user_data(ui_lbl_result_item, item_data);

    if (strcasecmp(item_data, "content") == 0) {
        if (file_exist(FRIENDLY_RESULT)) {
            LOG_INFO(mux_module, "Reading Friendly Name Set: %s", FRIENDLY_RESULT);

            char *fn_raw = read_all_char_from(FRIENDLY_RESULT);
            if (json_valid(fn_raw)) {
                const struct json fn_json = json_parse(fn_raw);
                char fn_name[MAX_BUFFER_SIZE];
                const struct json good_name_json = json_object_get(fn_json, strip_ext(item_text));

                if (json_exists(good_name_json)) {
                    json_string_copy(good_name_json, fn_name, sizeof(fn_name));
                    lv_label_set_text(ui_lbl_result_item, fn_name);
                }
            }

            free(fn_raw);
        }

        lv_group_add_obj(ui_group, ui_lbl_result_item);
        lv_group_add_obj(ui_group_value, ui_lbl_result_item_value);
        lv_group_add_obj(ui_group_glyph, ui_lbl_result_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_result);

        ui_count_static++;
    }

    if (strcasecmp(item_data, "folder") == 0) {
        lv_obj_set_style_border_width(ui_pnl_result, 1, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(ui_pnl_result, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnl_result, theme.list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_side(ui_pnl_result, LV_BORDER_SIDE_TOP, MU_OBJ_MAIN_DEFAULT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    for (int step = 0; step < steps; ++step) {
        if (all_item_count > 0 && all_items[current_item_index].content_type == ITEM) {
            apply_text_long_dot(&theme, lv_group_get_focused(ui_group));
        }

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    scroll_object_to_middle(ui_pnl_content, lv_group_get_focused(ui_group_panel));

    video_preview_cancel();

    if (all_item_count > 0 && all_items[current_item_index].content_type == ITEM) {
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);

        if (config.visual.box_art < 4) {
            if (config.visual.box_art_transition != TSN_DISABLED) {
                transition_box_art_nav_activity();
            } else {
                image_refresh();
                if (config.visual.video_preview > 0) video_refresh();
            }
        }
    } else {
        lv_img_set_src(ui_img_box, &ui_img_blank);
        for (int i = 1; i < 6; i++) {
            if (ui_viewport_objects[i]) lv_img_set_src(ui_viewport_objects[i], &ui_img_blank);
        }
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), "");
    }

    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void process_results(const char *json_results) {
    if (!json_valid(json_results)) {
        LOG_ERROR(mux_module, "Invalid JSON Format");
        return;
    }

    const struct json root = json_parse(json_results);

    const struct json lookup = json_object_get(root, "lookup");
    if (json_exists(lookup) && json_type(lookup) == JSON_STRING) {
        json_string_copy(lookup, lookup_value, sizeof(lookup_value));
    }

    const struct json directories = json_object_get(root, "directories");
    if (json_exists(directories) && json_type(directories) == JSON_ARRAY) {
        if (json_array_count(directories) > 0) {
            list_nav_move(json_array_count(directories) == 1 ? 1 : 2, +1);
        }
    }

    const struct json search_folders = json_object_get(root, "folders");
    if (!json_exists(search_folders) || json_type(search_folders) != JSON_OBJECT) return;

    size_t t_all_item_count = 0;
    content_item *t_all_items = NULL;

    struct json key = json_first(search_folders);
    while (json_exists(key)) {
        const struct json val = json_next(key);
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

            if (strcasecmp(storage_name_short, device.storage.rom.mount) == 0) {
                snprintf(storage_name_short, sizeof(storage_name_short), "SD1");
            } else if (strcasecmp(storage_name_short, device.storage.sdcard.mount) == 0) {
                snprintf(storage_name_short, sizeof(storage_name_short), "SD2");
            } else if (strcasecmp(storage_name_short, device.storage.usb.mount) == 0) {
                snprintf(storage_name_short, sizeof(storage_name_short), "USB");
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
                snprintf(
                    folder_name_short, sizeof(folder_name_short), " %s/%s", storage_name_short,
                    str_replace(modified_name, "/mnt//ROMS/", "")
                );
                free(modified_name);
            } else {
                snprintf(folder_name_short, sizeof(folder_name_short), "%s", folder_name);
            }
        }

        if (strcasecmp(folder_name, ".") != 0) {
            gen_result("folder", folder_name_short, "folder", "");
        }

        const struct json content = json_object_get(val, "content");
        if (!json_exists(content) || json_type(content) != JSON_ARRAY) {
            key = json_next(val);
            continue;
        }

        size_t folder_item_count = 0;
        content_item *folder_items = NULL;

        for (size_t i = 0; i < json_array_count(content); i++) {
            const struct json item = json_array_get(content, i);
            if (json_type(item) != JSON_OBJECT) continue;

            const struct json file_json = json_object_get(item, "file");
            const struct json name_json = json_object_get(item, "name");
            if (!json_exists(file_json) || !json_exists(name_json)) continue;

            char file_name[MAX_BUFFER_SIZE];
            char display_name[MAX_BUFFER_SIZE];
            json_string_copy(file_json, file_name, sizeof(file_name));
            json_string_copy(name_json, display_name, sizeof(display_name));

            char content_full_path[MAX_BUFFER_SIZE];
            if (folder_name[0] != '/') {
                snprintf(content_full_path, sizeof(content_full_path), "%s/%s/%s", rom_dir, folder_name, file_name);
            } else {
                snprintf(content_full_path, sizeof(content_full_path), "%s/%s", folder_name, file_name);
            }

            adjust_visual_label(display_name, config.visual.name, config.visual.dash);
            add_item(&folder_items, &folder_item_count, display_name, display_name, content_full_path, ITEM);
        }

        sort_items(folder_items, folder_item_count);

        skip_list skiplist;
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
                    add_item(
                        &t_all_items, &t_all_item_count, folder_items[i].name, folder_items[i].display_name,
                        folder_items[i].extra_data, ITEM
                    );

                    gen_result("content", folder_items[i].display_name, "content", folder_items[i].extra_data);
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
            add_item(
                &all_items, &all_item_count, t_all_items[i].name, t_all_items[i].display_name,
                t_all_items[i].extra_data, ITEM
            );
        }
    }

    free_items(&t_all_items, &t_all_item_count);
}

// This is more of a visual thing otherwise it'll select "Lookup" in the background so
// we just pop the search type in the pending file to highlight the correct one...
static void focus_search_button(const int use_global) {
    lv_obj_t *panel = use_global ? ui_pnl_search_global_search : ui_pnl_search_local_search;
    lv_obj_t *label = use_global ? ui_lbl_search_global_search : ui_lbl_search_local_search;
    lv_obj_t *glyph = use_global ? ui_ico_search_global_search : ui_ico_search_local_search;
    lv_obj_t *value = use_global ? ui_val_search_global_search : ui_val_search_local_search;

    lv_group_focus_obj(panel);
    lv_group_focus_obj(label);
    lv_group_focus_obj(glyph);
    lv_group_focus_obj(value);

    current_item_index = use_global ? 2 : 1;

    set_label_long_mode(&theme, label, config.visual.name_scroll);
    nav_moved = 1;
}

static void cancel_search(void) {
    if (search_poll_timer) {
        lv_timer_del(search_poll_timer);
        search_poll_timer = NULL;
    }

    search_pending = 0;
    hide_bounce_progress_bar();
}

static void search_poll_task(lv_timer_t *t) {
    if (!search_pending) {
        lv_timer_del(t);
        search_poll_timer = NULL;
        return;
    }

    const int file_ready = file_exist(search_result);
    const int elapsed = (int) (time(NULL) - search_start);
    const int timed_out = elapsed >= SEARCH_TIMEOUT;

    if (!file_ready && !timed_out) return;

    search_pending = 0;
    lv_timer_del(t);
    search_poll_timer = NULL;

    hide_bounce_progress_bar();

    if (file_ready) {
        char *json_content = read_all_char_from(search_result);
        if (json_content) {
            process_results(json_content);
            lv_label_set_text(ui_val_lookup_search, lookup_value);
            focus_search_button(search_use_global);
            free(json_content);
        }
    }
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_lookup_search)
        lv_label_set_text(ui_val_lookup_search, lv_textarea_get_text(ui_txt_entry_search));
    reset_osk(key_entry);

    lv_textarea_set_text(ui_txt_entry_search, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnl_entry_search);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_confirm(void) {
    if (file_exist(MUOS_SAA_LOAD)) remove(MUOS_SAA_LOAD);
    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_lookup_search) {
        play_sound(snd_confirm);

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;
        osk_show(ui_pnl_entry_search);

        lv_textarea_set_text(ui_txt_entry_search, lv_label_get_text(lv_group_get_focused(ui_group_value)));
        return;
    }

    if (e_focused == ui_lbl_search_local_search || e_focused == ui_lbl_search_global_search) {
        if (search_pending) return;

        char *lookup_value = lv_label_get_text(ui_val_lookup_search);

        if (strlen(lookup_value) <= 2) {
            play_sound(snd_error);

            toast_message(lang.muxsearch.error, tst_wait_s);
            return;
        }

        play_sound(snd_confirm);

        if (file_exist(search_result)) remove(search_result);
        write_text_to_file(SEARCH_RUNNER, "w", CHAR, e_focused == ui_lbl_search_global_search ? "global" : "local");

        if (e_focused == ui_lbl_search_local_search) {
            const char *args[] = {OPT_PATH "script/mux/find.sh", "--local", str_trim(lookup_value), rom_dir, NULL};
            run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
        } else {
            const char *args[6];
            int idx = 0;

            args[idx++] = OPT_PATH "script/mux/find.sh";
            args[idx++] = str_trim(lookup_value);

            int mounts = 0;
            double total_space, free_space, used_space;

            get_storage_info(device.storage.rom.mount, &total_space, &free_space, &used_space);
            if (total_space > 0) mounts |= 1;

            get_storage_info(device.storage.sdcard.mount, &total_space, &free_space, &used_space);
            if (total_space > 0) mounts |= 2;

            get_storage_info(device.storage.usb.mount, &total_space, &free_space, &used_space);
            if (total_space > 0) mounts |= 4;

            if (mounts & 1) args[idx++] = sd1;
            if (mounts & 2) args[idx++] = sd2;
            if (mounts & 4) args[idx++] = e_usb;

            args[idx] = NULL;
            run_exec(args, idx, 1, 0, NULL, NULL);
        }

        if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);

        load_mux("search");

        mux_input_stop();

        return;
    }

    if (strcasecmp(lv_obj_get_user_data(e_focused), "content") == 0) {
        play_sound(snd_confirm);

        const char *selected_raw = all_items[current_item_index].extra_data;
        char selected_path[MAX_BUFFER_SIZE];
        char resolved_path[PATH_MAX];

        snprintf(selected_path, sizeof(selected_path), "%s", selected_raw);
        remove_double_slashes(selected_path);
        char *clean_path = str_replace(selected_path, "/./", "/");
        if (clean_path) {
            snprintf(selected_path, sizeof(selected_path), "%s", clean_path);
            free(clean_path);
        }

        if (!union_resolve_to_real(selected_path, resolved_path, sizeof(resolved_path))) {
            snprintf(resolved_path, sizeof(resolved_path), "%s", selected_path);
        }

        write_text_to_file(MUOS_RES_LOAD, "w", CHAR, resolved_path);

        char base_dir[MAX_BUFFER_SIZE];
        snprintf(base_dir, sizeof(base_dir), "%s", resolved_path);

        char *last_slash = strrchr(base_dir, '/');
        if (last_slash) *last_slash = '\0';

        write_text_to_file(EXPLORE_DIR, "w", CHAR, base_dir);
        char *item_file = get_last_dir(resolved_path);
        write_text_to_file(MUOS_HST_LOAD, "w", CHAR, item_file);

        load_mux("explore");

        mux_input_stop();
    }
}

static void handle_random_select(void) {
    if (msgbox_active || ui_count_static <= 3 || hold_call || video_preview_active()) return;

    const uint32_t random_select = random() % ui_count_static;
    const int selected_index = (int) (random_select & INT16_MAX);

    !(selected_index & 1) ? list_nav_move(selected_index, +1) : list_nav_move(selected_index, -1);
}

static void handle_back(void) {
    video_preview_cancel();
    play_sound(snd_back);

    cancel_search();

    if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);
    load_mux("explore");

    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call || video_preview_active()) return;

    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_b(void) {
    if (key_show) {
        key_backspace(ui_txt_entry_search);
        return;
    }

    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (video_preview_active()) {
        video_preview_cancel();
        play_sound(snd_back);
        return;
    }

    handle_back();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_search);
}

static void handle_x(void) {
    if (msgbox_active || hold_call || video_preview_active()) return;

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txt_entry_search, ui_pnl_entry_search);
        return;
    }

    cancel_search();

    if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);
    if (file_exist(search_result)) remove(search_result);

    play_sound(snd_confirm);
    load_mux("search");

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || video_preview_active()) return;

    if (key_show) key_space(ui_txt_entry_search);

    // TODO: A way to directly add the item to a collection
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || key_show || hold_call || video_preview_active())
        return;

    if (!all_item_count || all_items[current_item_index].content_type != ITEM) {
        play_sound(snd_info_open);
        show_help();
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
    if (key_show) {
        key_swap_back();
        return;
    }

    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) {
        key_swap();
        return;
    }

    handle_list_nav_page_down();
}

static void handle_nav_key_released(void) {
    if (key_show) return;

    if (config.visual.box_art_transition != TSN_DISABLED) {
        transition_box_art_key_released();
    }
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_entry_search,
                                          ui_pnl_progress_brightness, ui_pnl_progress_volume, ui_pnl_message,
                                          ui_pnl_progress, NULL});
    if (config.visual.box_art == 3) lv_obj_move_foreground(ui_pnl_box);
}

static void init_elements(void) {
    lv_obj_set_align(ui_img_box, config.visual.box_art_align);
    lv_obj_set_align(ui_viewport_objects[0], config.visual.box_art_align);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, "", 0},
                                  {NULL, NULL, 0}});

    lv_label_set_text(ui_lbl_nav_a, lang.generic.select);
    lv_label_set_text(ui_lbl_nav_b, lang.generic.back);
    lv_label_set_text(ui_lbl_nav_x, lang.generic.clear);

    lv_obj_t *nav_hide[] = {ui_lbl_nav_a_glyph, ui_lbl_nav_a,       ui_lbl_nav_b_glyph,
                            ui_lbl_nav_b,       ui_lbl_nav_x_glyph, ui_lbl_nav_x};

    for (int i = 0; i < A_SIZE(nav_hide); i++) {
        lv_obj_clear_flag(nav_hide[i], MU_OBJ_FLAG_HIDE_FLOAT);
    }

#define SEARCH(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_search, UDATA);
    SEARCH_ELEMENTS
#undef SEARCH

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, wall_general);
        adjust_panels();
        if (overlay_image) lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_ok_press();
    }

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txt_entry_search);
    }
}

int muxsearch_main(char *dir) {
    if (!strlen(dir)) {
        union_get_roms_root(rom_dir, sizeof(rom_dir));
    } else {
        if (!union_resolve_to_real(dir, rom_dir, sizeof(rom_dir))) {
            snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
        }
    }

    starter_image = 0;
    got_results = 0;

    init_module(__func__);

    snprintf(search_result, sizeof(search_result), "%s/%s/search.json", device.storage.rom.mount, MUOS_INFO_PATH);

    char *json_content = 0;

    if (file_exist(search_result)) {
        json_content = read_all_char_from(search_result);
        if (json_content) {
            got_results = 1;
        } else {
            LOG_ERROR(mux_module, "Error reading search results");
        }
    }

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxsearch.title);
    init_muxsearch(ui_screen, ui_pnl_content, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnl_box);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    snprintf(sd1, sizeof(sd1), "%s/ROMS", device.storage.rom.mount);
    snprintf(sd2, sizeof(sd2), "%s/ROMS", device.storage.sdcard.mount);
    snprintf(e_usb, sizeof(e_usb), "%s/ROMS", device.storage.usb.mount);

    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    transition_box_art_init(image_refresh_transition);

    if (got_results) {
        process_results(json_content);
        lv_label_set_text(ui_val_lookup_search, lookup_value);
        free(json_content);
    } else if (file_exist(SEARCH_RUNNER)) {
        const char *pending_mode = read_line_char_from(SEARCH_RUNNER, 1);
        search_use_global = pending_mode && strcasecmp(pending_mode, "global") == 0;
        remove(SEARCH_RUNNER);

        search_start = time(NULL);
        search_pending = 1;

        focus_search_button(search_use_global);

        show_bounce_progress_bar(lang.muxsearch.search, SEARCH_TIMEOUT);
        search_poll_timer = lv_timer_create(search_poll_task, 500, NULL);
    }

    init_osk(ui_pnl_entry_search, ui_txt_entry_search, 0, 0, OSK_MAX);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
                [mux_input_r2] = handle_random_select,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
                [mux_input_dpad_up] = handle_nav_key_released,
                [mux_input_dpad_down] = handle_nav_key_released,
                [mux_input_dpad_left] = handle_nav_key_released,
                [mux_input_dpad_right] = handle_nav_key_released,
                [mux_input_l1] = handle_nav_key_released,
                [mux_input_r1] = handle_nav_key_released,
            },
        .hold_handler = {
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
            [mux_input_r2] = handle_random_select,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    transition_box_art_destroy();
    video_preview_destroy();

    free_items(&all_items, &all_item_count);

    return 0;
}
