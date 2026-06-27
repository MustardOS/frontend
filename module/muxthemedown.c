#include "muxshare.h"
#include "../common/collection/theme.h"
#include "../common/download.h"

static int theme_extracting = 0;
static char theme_data_local_path[MAX_BUFFER_SIZE];
static theme_item *theme_items = NULL;
static size_t theme_item_count = 0;
static char *preview_zip_path = "/opt/theme_catalogue.muxzip";

static int exit_status = 0;

static int preview_display_time = 0;
static int preview_index = -1;

static int schedule_theme_update = 0;
static int pending_download_switch = 0;

static int theme_extract_done = 0;
static int theme_extract_error = 0;
static char theme_extract_zip_done[PATH_MAX];

static mux_dialogue msg_dlg;
static int msg_mode = 0;

static void show_message_dialog(void) {
    msg_mode = 1;
    dialogue_show(&msg_dlg);
}

static void hide_message_dialog(void) {
    msg_mode = 0;
    dialogue_hide(&msg_dlg);
}

static void show_help(void) {
    char text_path[MAX_BUFFER_SIZE];
    snprintf(text_path, sizeof(text_path), INFO_CAT_PATH "/theme/text/%s.txt", theme_items[current_item_index].name);

    char credits[MAX_BUFFER_SIZE];
    if (file_exist(text_path)) {
        char *raw = read_all_char_from(text_path);
        snprintf(credits, sizeof(credits), "%s", raw ? raw : "");
        free(raw);
    } else {
        snprintf(credits, sizeof(credits), "%s", lang.muxpicker.none.credit);
    }

    show_info_box(TRS(lv_label_get_text(lv_group_get_focused(ui_group))), TRS(credits), 0);
}

static int is_downloaded(const int index) {
    char theme_path[MAX_BUFFER_SIZE];
    snprintf(theme_path, sizeof(theme_path), RUN_STORAGE_PATH "theme/%s", theme_items[index].name);
    return dir_exist(theme_path);
}

static void image_refresh() {
    if (!theme_items || theme_item_count == 0 || current_item_index < 0) return;

    char base_image_path[MAX_BUFFER_SIZE];
    snprintf(base_image_path, sizeof(base_image_path), INFO_CAT_PATH "/theme/box");

    char preview_path[MAX_BUFFER_SIZE];
    if (get_theme_preview_path(
            base_image_path, theme_items[current_item_index].name, preview_path, sizeof(preview_path), preview_index
        )
        != 0) {
        preview_index = -1;
    }

    lv_img_cache_invalidate_src(lv_img_get_src(ui_img_box));

    if (strcasecmp(box_image_previous_path, preview_path) != 0) {

        if (!file_exist(preview_path)) {
            lv_img_set_src(ui_img_box, &ui_img_blank);
            snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
        } else {
            const struct image_settings image_settings = {
                preview_path,
                6,
                validate_int16((int16_t) (device.mux.width * .45), "width"),
                validate_int16(device.mux.height, "height"),
                theme.image_list.pad_left,
                theme.image_list.pad_right,
                theme.image_list.pad_top,
                theme.image_list.pad_bottom
            };
            update_image(ui_img_box, image_settings);
            snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", preview_path);
        }
    }
    preview_display_time = 0;
}

static int skip_theme_item(
    const char *name, const int grid_enabled, const int hdmi_enabled, const int language_enabled,
    const int resolution640_x480, const int resolution720_x480, const int resolution720_x720,
    const int resolution1024_x768, const int resolution1280_x720, const int resolution1920_x1080
) {
    return (config.theme.filter.resolution_640_x480 && !resolution640_x480)
           || (config.theme.filter.resolution_720_x480 && !resolution720_x480)
           || (config.theme.filter.resolution_720_x720 && !resolution720_x720)
           || (config.theme.filter.resolution_1024_x768 && !resolution1024_x768)
           || (config.theme.filter.resolution_1280_x720 && !resolution1280_x720)
           || (config.theme.filter.resolution_1920_x1080 && !resolution1920_x1080)
           || (config.theme.filter.grid && !grid_enabled) || (config.theme.filter.hdmi && !hdmi_enabled)
           || (config.theme.filter.language && !language_enabled)
           || (config.theme.filter.lookup[0] != '\0' && strcasestr(name, config.theme.filter.lookup) == NULL);
}

static void create_content_items(void) {
    if (!file_exist(theme_data_local_path)) {
        LOG_WARN(mux_module, "Theme Data Not Found At: %s", theme_data_local_path);
        return;
    }

    LOG_SUCCESS(mux_module, "Found Theme Data At: %s", theme_data_local_path);

    char *json_str = read_all_char_from(theme_data_local_path);
    if (!json_valid(json_str)) return;

    if (strlen(json_str) > MAX_MANIFEST_BYTES) {
        LOG_ERROR(mux_module, "Theme catalogue exceeds size limit, rejecting");
        free(json_str);
        return;
    }

    const struct json fn_json = json_parse(json_str);

    size_t theme_count = json_array_count(fn_json);
    if (theme_count > MAX_MANIFEST_ITEMS) {
        LOG_WARN(mux_module, "Theme catalogue has %zu items, capping at %d", theme_count, MAX_MANIFEST_ITEMS);
        theme_count = MAX_MANIFEST_ITEMS;
    }

    for (int i = 0; i < theme_count; i++) {
        const struct json theme_item = json_array_get(fn_json, i);

        const int grid_enabled = json_bool(json_object_get(theme_item, "grid"));
        const int hdmi = json_bool(json_object_get(theme_item, "hdmi"));
        const int language = json_bool(json_object_get(theme_item, "language"));

        const int resolution640_x480 = json_bool(json_object_get(theme_item, "resolution640x480"));
        const int resolution720_x480 = json_bool(json_object_get(theme_item, "resolution720x480"));
        const int resolution720_x720 = json_bool(json_object_get(theme_item, "resolution720x720"));
        const int resolution1024_x768 = json_bool(json_object_get(theme_item, "resolution1024x768"));
        const int resolution1280_x720 = json_bool(json_object_get(theme_item, "resolution1280x720"));
        const int resolution1920_x1080 = json_bool(json_object_get(theme_item, "resolution1920x1080"));

        char theme_name[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(theme_item, "name"), theme_name, sizeof(theme_name));

        if (strchr(theme_name, '/') || strstr(theme_name, "..")) {
            LOG_WARN(mux_module, "Skipping theme with unsafe name: %s", theme_name);
            continue;
        }

        char theme_url[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(theme_item, "url"), theme_url, sizeof(theme_url));

        if (!skip_theme_item(
                theme_name, grid_enabled, hdmi, language, resolution640_x480, resolution720_x480, resolution720_x720,
                resolution1024_x768, resolution1280_x720, resolution1920_x1080
            )) {
            add_theme_item(
                &theme_items, &theme_item_count, theme_name, theme_url, grid_enabled, hdmi, language,
                resolution640_x480, resolution720_x480, resolution720_x720, resolution1024_x768, resolution1280_x720,
                resolution1920_x1080
            );
        }
    }
    sort_theme_items(theme_items, theme_item_count);

    for (int i = 0; i < theme_item_count; i++) {
        if (lv_obj_get_child_cnt(ui_pnl_content) >= theme.mux.item.count) break;

        gen_label(mux_module, is_downloaded(i) ? "theme_down" : "theme", theme_items[i].name);
    }
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, theme_items[index].name);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (theme.list_default.glyph_alpha > 0 && theme.list_focus.glyph_alpha > 0) {
        get_glyph_path(mux_module, is_downloaded(index) ? "theme_down" : "theme", glyph_image_embed, MAX_BUFFER_SIZE);
        set_list_glyph_image(ui_lbl_item_glyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, theme_items[index].name);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void update_list_items(const int start_index) {
    for (int index = 0; index < theme.mux.item.count; ++index) {
        const lv_obj_t *panel_item = lv_obj_get_child(ui_pnl_content, index);
        update_list_item(lv_obj_get_child(panel_item, 0), lv_obj_get_child(panel_item, 1), start_index + index);
    }
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);

        if (theme_item_count > theme.mux.item.count) {
            const int items_before_selected = (theme.mux.item.count - theme.mux.item.count % 2) / 2;
            const int items_after_selected = (theme.mux.item.count - 1) / 2;

            if (direction < 0) {
                if (current_item_index == theme_item_count - 1) {
                    update_list_items((int) theme_item_count - theme.mux.item.count);
                } else {
                    if (current_item_index >= items_before_selected
                        && current_item_index < theme_item_count - items_after_selected - 1) {
                        lv_obj_t *last_item =
                            lv_obj_get_child(ui_pnl_content, theme.mux.item.count - 1); // Get the last child
                        lv_obj_move_to_index(last_item, 0);
                        update_list_item(
                            lv_obj_get_child(last_item, 0), lv_obj_get_child(last_item, 1),
                            current_item_index - items_before_selected
                        );
                    }
                }
            } else {
                if (current_item_index == 0) {
                    update_list_items(0);
                } else {
                    if (current_item_index > items_before_selected
                        && current_item_index < theme_item_count - items_after_selected) {
                        lv_obj_t *first_item = lv_obj_get_child(ui_pnl_content, 0);
                        lv_obj_move_to_index(first_item, theme.mux.item.count - 1);
                        update_list_item(
                            lv_obj_get_child(first_item, 0), lv_obj_get_child(first_item, 1),
                            current_item_index + items_after_selected
                        );
                    }
                }
            }
        }
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    lv_label_set_text(
        ui_lbl_nav_a, is_downloaded(current_item_index) ? lang.muxthemedown.remove : lang.muxthemedown.download
    );

    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void refresh_current_list_item() {
    update_list_item(lv_group_get_focused(ui_group), lv_group_get_focused(ui_group_glyph), current_item_index);
    lv_label_set_text(
        ui_lbl_nav_a, is_downloaded(current_item_index) ? lang.muxthemedown.remove : lang.muxthemedown.download
    );
}

static void theme_extraction_finished(char *theme_path) {
    if (theme_path && *theme_path) {
        LOG_INFO(mux_module, "Extraction Finished: %s", theme_path);
        snprintf(theme_extract_zip_done, sizeof(theme_extract_zip_done), "%s", theme_path);
    } else {
        LOG_ERROR(mux_module, "Extraction blocked: archive contained unsafe file paths");
        theme_extract_zip_done[0] = '\0';
        theme_extract_error = 1;
    }

    theme_extract_done = 1;
}

static void theme_download_finished(const int result) {
    if (result != 0) {
        play_sound(snd_error);
        toast_message(lang.muxthemedown.error_get_data, tst_wait_s);
        return;
    }

    char theme_path[MAX_BUFFER_SIZE];
    snprintf(theme_path, sizeof(theme_path), RUN_STORAGE_PATH "theme/%s.muxthm", theme_items[current_item_index].name);

    if (!file_exist(theme_path)) {
        LOG_WARN(mux_module, "Theme archive missing after download: %s", theme_path);
        play_sound(snd_error);
        toast_message(lang.muxthemedown.error_get_data, tst_wait_s);
        return;
    }

    char output_path[MAX_BUFFER_SIZE];
    snprintf(output_path, sizeof(output_path), RUN_STORAGE_PATH "theme/%s", theme_items[current_item_index].name);

    theme_extracting = 1;
    block_input = 1;

    extract_zip_to_dir_with_progress(theme_path, output_path, theme_extraction_finished);
}

static void refresh_theme_previews_finished(const int result) {
    if (result == 0) {
        extract_archive(preview_zip_path, "themedwn");
        pending_download_switch = 1;
    } else {
        play_sound(snd_error);
        toast_message(lang.muxthemedown.error_get_data, tst_wait_f);
    }
}

static void refresh_theme_data_finished(const int result) {
    if (result == 0) {
        if (file_exist(preview_zip_path)) remove(preview_zip_path);
        set_download_callbacks(refresh_theme_previews_finished);
        initiate_download(config.theme.download.preview, preview_zip_path, 1, lang.muxthemedown.down.preview);
    } else {
        play_sound(snd_error);
        toast_message(lang.muxthemedown.error_get_data, tst_wait_f);
    }
}

static void update_theme_data(void) {
    if (file_exist(theme_data_local_path)) remove(theme_data_local_path);
    set_download_callbacks(refresh_theme_data_finished);
    initiate_download(config.theme.download.data, theme_data_local_path, 1, lang.muxthemedown.down.data);
}

static void handle_a(void) {
    if (msg_mode || download_in_progress || msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_confirm);

    char theme_path[MAX_BUFFER_SIZE];
    snprintf(theme_path, sizeof(theme_path), RUN_STORAGE_PATH "theme/%s", theme_items[current_item_index].name);
    if (dir_exist(theme_path)) {
        if (strcasecmp(theme_path, theme_base) == 0) {
            toast_message(lang.generic.cannot_delete_active_theme, tst_wait_s);
        } else {
            remove_directory_recursive(theme_path);
            sync();
            refresh_current_list_item();
            toast_message(lang.muxthemedown.theme_removed, tst_wait_s);
        }
    } else {
        set_download_callbacks(theme_download_finished);
        snprintf(
            theme_path, sizeof(theme_path), RUN_STORAGE_PATH "theme/%s.muxthm", theme_items[current_item_index].name
        );
        initiate_download(theme_items[current_item_index].url, theme_path, 1, lang.muxthemedown.down.theme);
    }
}

static void handle_b(void) {
    if (msg_mode) {
        play_sound(snd_back);
        hide_message_dialog();
        return;
    }

    if (theme_extracting) {
        toast_message(lang.muxthemedown.theme_extracting, tst_wait_l);
        return;
    }

    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_confirm);

    if (download_in_progress) {
        cancel_download = 1;
        if (theme_items && ui_count_static > 0) {
            char theme_path[MAX_BUFFER_SIZE];
            snprintf(
                theme_path, sizeof(theme_path), RUN_STORAGE_PATH "theme/%s.muxthm", theme_items[current_item_index].name
            );
            if (file_exist(theme_path)) remove(theme_path);
        }
    } else {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "theme");
        write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, "/theme");

        load_mux("theme");

        mux_input_stop();
    }
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || !ui_count_static || hold_call) return;
    play_sound(snd_confirm);
    update_theme_data();
}

static void handle_y(void) {
    if (download_in_progress || msgbox_active || hold_call) return;
    play_sound(snd_confirm);

    theme_down_index = current_item_index;

    load_mux("themefilter");

    mux_input_stop();
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_lbl_counter_explore, ui_pnl_help,
                                          ui_pnl_progress_brightness, ui_pnl_progress_volume, NULL});
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.muxthemedown.download, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.refresh, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.filter, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    download_poll();
    extraction_poll();

    if (theme_extract_done) {
        theme_extract_done = 0;

        if (theme_extract_zip_done[0] && file_exist(theme_extract_zip_done)) {
            remove(theme_extract_zip_done);
        }

        block_input = 0;
        theme_extracting = 0;

        msgbox_active = 0;
        progress_onscreen = 0;

        if (msgbox_element) lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);

        if (theme_extract_error) {
            theme_extract_error = 0;
            show_message_dialog();
        } else {
            refresh_current_list_item();
            nav_moved = 1;
        }
    }

    if (pending_download_switch) {
        pending_download_switch = 0;

        mux_input_stop();

        load_mux("themedwn");
        return;
    }

    if (schedule_theme_update) {
        schedule_theme_update = 0;
        update_theme_data();
        return;
    }

    if (ui_count_static > 0 && nav_moved) {
        preview_index = -1;
        image_refresh();
        preview_display_time = 0;

        adjust_panels();

        if (!lv_obj_has_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter(ui_lbl_counter_explore, ui_count_static);
        if (overlay_image) lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }

    preview_display_time += TIMER_REFRESH;
    if (preview_display_time > THEME_PREVIEW_DELAY) {
        if (theme_items && theme_item_count > 0) {
            preview_index++;
            image_refresh();
        }
        preview_display_time = 0;
    }
}

int muxthemedown_main(void) {
    exit_status = 0;
    snprintf(
        theme_data_local_path, sizeof(theme_data_local_path), "%s/%s", device.storage.rom.mount,
        MUOS_INFO_PATH "/" THEME_DATA
    );
    create_directories(theme_data_local_path, 1);

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxthemedown.title);
    init_ui_item_counter(&theme);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    reset_ui_groups();

    create_content_items();
    ui_count_static = (int) theme_item_count;
    init_elements();

    const int nav_vis = ui_count_static > 0 ? 1 : 0;
    const struct nav_flag nav_e[] = {{ui_lbl_nav_a, nav_vis}, {ui_lbl_nav_a_glyph, nav_vis},
                                     {ui_lbl_nav_x, nav_vis}, {ui_lbl_nav_x_glyph, nav_vis},
                                     {ui_lbl_nav_y, nav_vis}, {ui_lbl_nav_y_glyph, nav_vis},
                                     {ui_lbl_nav_menu, 0},    {ui_lbl_nav_menu_glyph, 0}};

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    dialogue_init_message(
        &msg_dlg, &theme, ui_screen, lang.generic.warning, NULL, lang.generic.unsafe_archive, lang.generic.back
    );

    if (ui_count_static > 0) list_nav_move(theme_down_index, 1);
    theme_down_index = 0;

    update_file_counter(ui_lbl_counter_explore, ui_count_static);

    init_timer(ui_refresh_task, NULL);

    if (!file_exist(theme_data_local_path)) {
        nav_moved = 1;
        schedule_theme_update = 1;
    }

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_dpad_left] = handle_list_nav_left,
                [mux_input_dpad_right] = handle_list_nav_right,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_l2] = hold_call_release,
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_dpad_left] = handle_list_nav_left_hold,
            [mux_input_dpad_right] = handle_list_nav_right_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_l2] = hold_call_set,
            [mux_input_r1] = handle_list_nav_page_down,
        },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    if (ui_count_static > 0) free_theme_items(&theme_items, &theme_item_count);

    return exit_status;
}
