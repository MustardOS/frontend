#include "muxshare.h"
#include "../common/download.h"

static char data_local_path[MAX_BUFFER_SIZE];
static char data_type[MAX_BUFFER_SIZE];

static int exit_status = 0;
static int starter_image = 0;

static void sanitise_download_name(char *dest, const char *src) {
    size_t j = 0;
    while (*src && j < MAX_BUFFER_SIZE - 1) {
        if (*src == '/' || *src == '\\') {
            dest[j++] = '_';
            src++;
        } else if (src[0] == '.' && src[1] == '.') {
            dest[j++] = '.';
            src += 2;
        } else {
            dest[j++] = *src++;
        }
    }
    dest[j] = '\0';
}

static void show_help(void) {
    show_info_box(items[current_item_index].name, items[current_item_index].help, 0);
}

static int is_downloaded(const int index) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(
        file_path, sizeof(file_path), "%s/%s/%s.muxzip", device.storage.rom.mount, MUOS_ARCH_PATH, items[index].name
    );
    return file_exist(file_path);
}

static void create_content_items(void) {
    if (!file_exist(data_local_path)) {
        LOG_WARN(mux_module, "%s Data Not Found At: %s", data_type, data_local_path);
        return;
    }
    LOG_SUCCESS(mux_module, "Found %s Data At: %s", data_type, data_local_path);

    const char *json_str = read_all_char_from(data_local_path);
    if (!json_valid(json_str)) return;

    const struct json fn_json = json_parse(json_str);

    const size_t count = json_array_count(fn_json);

    for (int i = 0; i < count; i++) {
        const struct json item = json_array_get(fn_json, i);

        char raw_name[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(item, "name"), raw_name, sizeof(raw_name));

        char name[MAX_BUFFER_SIZE];
        sanitise_download_name(name, raw_name);

        char url[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(item, "url"), url, sizeof(url));

        char help[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(item, "help"), help, sizeof(help));

        content_item *new_item = add_item(&items, &item_count, name, name, url, ITEM);
        new_item->help = strdup(help);
    }
    sort_items(items, item_count);

    for (int i = 0; i < item_count; i++) {
        if (lv_obj_get_child_cnt(ui_pnl_content) >= theme.mux.item.count) break;

        gen_label(mux_module, is_downloaded(i) ? "downloaded" : "download", items[i].name);
    }
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, items[index].name);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (theme.list_default.glyph_alpha > 0 && theme.list_focus.glyph_alpha > 0) {
        get_glyph_path(
            mux_module, is_downloaded(index) ? "downloaded" : "download", glyph_image_embed, MAX_BUFFER_SIZE
        );
        set_list_glyph_image(ui_lbl_item_glyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, items[index].name);
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

        if (item_count > theme.mux.item.count) {
            const int items_before_selected = (theme.mux.item.count - theme.mux.item.count % 2) / 2;
            const int items_after_selected = (theme.mux.item.count - 1) / 2;

            if (direction < 0) {
                if (current_item_index == item_count - 1) {
                    update_list_items((int) item_count - theme.mux.item.count);
                } else {
                    if (current_item_index >= items_before_selected
                        && current_item_index < item_count - items_after_selected - 1) {

                        // Get the last child
                        lv_obj_t *last_item = lv_obj_get_child(ui_pnl_content, theme.mux.item.count - 1);

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
                        && current_item_index < item_count - items_after_selected) {
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
    lv_label_set_text(ui_lbl_nav_a, is_downloaded(current_item_index) ? lang.generic.remove : lang.generic.download);

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

static void download_finished(const int result) {
    update_list_item(lv_group_get_focused(ui_group), lv_group_get_focused(ui_group_glyph), current_item_index);
    lv_label_set_text(ui_lbl_nav_a, is_downloaded(current_item_index) ? lang.generic.remove : lang.generic.download);

    if (result == 0) {
        char file_path[MAX_BUFFER_SIZE];
        snprintf(
            file_path, sizeof(file_path), "%s/%s/%s.muxzip", device.storage.rom.mount, MUOS_ARCH_PATH,
            items[current_item_index].name
        );
        extract_archive(file_path, "coredown");
    } else {
        play_sound(snd_error);
        toast_message(lang.muxdownload.error_get_data, tst_wait_s);
    }
}

static void refresh_extra_data_finished(const int result) {
    if (result == 0) {
        load_mux("coredown");

        mux_input_stop();
    } else {
        play_sound(snd_error);
        toast_message(lang.muxdownload.error_get_data, tst_wait_f);
    }
}

static void update_extra_data(void) {
    if (file_exist(data_local_path)) remove(data_local_path);

    set_download_callbacks(refresh_extra_data_finished);
    initiate_download(config.extra.download.data, data_local_path, 1, lang.muxdownload.down.data);
}

static void handle_a(void) {
    if (download_in_progress || !ui_count_static || hold_call) return;

    play_sound(snd_confirm);

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    char file_path[MAX_BUFFER_SIZE];
    snprintf(
        file_path, sizeof(file_path), "%s/%s/%s.muxzip", device.storage.rom.mount, MUOS_ARCH_PATH,
        items[current_item_index].name
    );

    if (file_exist(file_path)) {
        remove(file_path);
        download_finished(-1);
        toast_message(lang.muxdownload.archive_removed, tst_wait_s);
    } else {
        set_download_callbacks(download_finished);
        initiate_download(items[current_item_index].extra_data, file_path, 1, lang.muxdownload.down.archive);
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_confirm);

    if (download_in_progress) {
        cancel_download = 1;

        char file_path[MAX_BUFFER_SIZE];
        snprintf(
            file_path, sizeof(file_path), "%s/%s/%s.muxzip", device.storage.rom.mount, MUOS_ARCH_PATH,
            items[current_item_index].name
        );

        if (file_exist(file_path)) remove(file_path);
    } else {
        if (file_exist(MUOS_ASS_LOAD "_temp")) {
            const char *text = read_all_char_from(MUOS_ASS_LOAD "_temp");
            write_text_to_file(MUOS_ASS_LOAD, "w", CHAR, text);
            remove(MUOS_ASS_LOAD "_temp");
        }

        load_mux("assign");

        mux_input_stop();
    }
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_confirm);
    update_extra_data();
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_progress_brightness,
                                          ui_pnl_progress_volume, ui_pnl_message, NULL});
}

static void init_elements(void) {
    lv_obj_set_align(ui_img_box, config.visual.box_art_align);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.download, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.refresh, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    download_poll();

    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, wall_general);
        adjust_panels();

        if (!lv_obj_has_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
        }

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

int muxdownload_main(char *type) {
    exit_status = 0;
    starter_image = 0;

    snprintf(
        data_local_path, sizeof(data_local_path), "%s/%s", device.storage.rom.mount, MUOS_INFO_PATH "/" EXTRA_DATA
    );
    snprintf(data_type, sizeof(data_type), "%s", type);

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(
        &theme, &device, &lang, strcmp(type, "core") == 0 ? lang.muxdownload.title.core : lang.muxdownload.title.app
    );

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lbl_datetime, get_datetime());
    init_fonts();
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    reset_ui_groups();

    create_content_items();
    ui_count_static = (int) item_count;

    init_elements();

    const int nav_vis = ui_count_static > 0 ? 1 : 0;
    const struct nav_flag nav_e[] = {{ui_lbl_nav_a, nav_vis}, {ui_lbl_nav_a_glyph, nav_vis},
                                     {ui_lbl_nav_x, nav_vis}, {ui_lbl_nav_x_glyph, nav_vis},
                                     {ui_lbl_nav_y, 0},       {ui_lbl_nav_y_glyph, 0},
                                     {ui_lbl_nav_menu, 0},    {ui_lbl_nav_menu_glyph, 0}};

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    int sys_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    if (ui_count_static > 0 && sys_index > -1 && sys_index <= ui_count_static && current_item_index < ui_count_static) {
        list_nav_move(sys_index, +1);
    }

    init_timer(ui_refresh_task, NULL);

    if (!file_exist(data_local_path)) update_extra_data();

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_dpad_left] = handle_list_nav_left,
                [mux_input_dpad_right] = handle_list_nav_right,
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
            [mux_input_dpad_left] = handle_list_nav_left_hold,
            [mux_input_dpad_right] = handle_list_nav_right_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    if (ui_count_static > 0) free_items(&items, &item_count);

    return exit_status;
}
