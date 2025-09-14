#include "muxshare.h"
#include "../common/download.h"

static char data_local_path[MAX_BUFFER_SIZE];
static char data_type[MAX_BUFFER_SIZE];

static int exit_status = 0;
static int starter_image = 0;

static void show_help(void) {
    if (!ui_count) return;
    show_info_box(items[current_item_index].name, items[current_item_index].help, 0);
}

static bool is_downloaded(int index) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s.muxzip",
             device.STORAGE.ROM.MOUNT, MUOS_ARCH_PATH, items[index].name);
    return (file_exist(file_path));
}

static void create_content_items(void) {
    if (!file_exist(data_local_path)) {
        LOG_WARN(mux_module, "%s Data Not Found At: %s", data_type, data_local_path)
        return;
    } else {
        LOG_SUCCESS(mux_module, "Found %s Data At: %s", data_type, data_local_path)
    }

    char *json_str = read_all_char_from(data_local_path);
    if (!json_valid(json_str)) return;

    struct json fn_json = json_parse(json_str);

    size_t count = json_array_count(fn_json);

    for (int i = 0; i < count; i++) {
        struct json item = json_array_get(fn_json, i);

        char name[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(item, "name"), name, sizeof(name));

        char url[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(item, "url"), url, sizeof(url));

        char help[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(item, "help"), help, sizeof(help));

        content_item *new_item = add_item(&items, &item_count, name, name, url, ITEM);
        new_item->help = strdup(help);
    }
    sort_items(items, item_count);

    for (size_t i = 0; i < item_count; i++) {
        if (lv_obj_get_child_cnt(ui_pnlContent) >= theme.MUX.ITEM.COUNT) break;

        gen_label(mux_module, is_downloaded(i) ? "downloaded" : "download", items[i].name);
    }
}

static void update_list_item(lv_obj_t *ui_lblItem, lv_obj_t *ui_lblItemGlyph, int index) {
    lv_label_set_text(ui_lblItem, items[index].name);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (theme.LIST_DEFAULT.GLYPH_ALPHA > 0 && theme.LIST_FOCUS.GLYPH_ALPHA > 0) {
        get_glyph_path(mux_module, is_downloaded(index) ? "downloaded" : "download",
                       glyph_image_embed, MAX_BUFFER_SIZE);
        lv_img_set_src(ui_lblItemGlyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnlContent, ui_lblItem, ui_lblItemGlyph, items[index].name);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblItem);
}

static void update_list_items(int start_index) {
    for (int index = 0; index < theme.MUX.ITEM.COUNT; ++index) {
        lv_obj_t *panel_item = lv_obj_get_child(ui_pnlContent, index);
        update_list_item(lv_obj_get_child(panel_item, 0), lv_obj_get_child(panel_item, 1), start_index + index);
    }
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

        if (item_count > theme.MUX.ITEM.COUNT) {
            int items_before_selected = (theme.MUX.ITEM.COUNT - theme.MUX.ITEM.COUNT % 2) / 2;
            int items_after_selected = (theme.MUX.ITEM.COUNT - 1) / 2;

            if (direction < 0) {
                if (current_item_index == item_count - 1) {
                    update_list_items((int) item_count - theme.MUX.ITEM.COUNT);
                } else {
                    if (current_item_index >= items_before_selected &&
                        current_item_index < item_count - items_after_selected - 1) {

                        // Get the last child
                        lv_obj_t *last_item = lv_obj_get_child(ui_pnlContent, theme.MUX.ITEM.COUNT - 1);

                        lv_obj_move_to_index(last_item, 0);
                        update_list_item(lv_obj_get_child(last_item, 0), lv_obj_get_child(last_item, 1),
                                         current_item_index - items_before_selected);
                    }
                }
            } else {
                if (current_item_index == 0) {
                    update_list_items(0);
                } else {
                    if (current_item_index > items_before_selected &&
                        current_item_index < item_count - items_after_selected) {
                        lv_obj_t *first_item = lv_obj_get_child(ui_pnlContent, 0);
                        lv_obj_move_to_index(first_item, theme.MUX.ITEM.COUNT - 1);
                        update_list_item(lv_obj_get_child(first_item, 0), lv_obj_get_child(first_item, 1),
                                         current_item_index + items_after_selected);
                    }
                }
            }
        }
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    lv_label_set_text(ui_lblNavA, is_downloaded(current_item_index) ? lang.GENERIC.REMOVE : lang.GENERIC.DOWNLOAD);

    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void download_finished(int result) {
    update_list_item(lv_group_get_focused(ui_group), lv_group_get_focused(ui_group_glyph), current_item_index);
    lv_label_set_text(ui_lblNavA, is_downloaded(current_item_index) ? lang.GENERIC.REMOVE : lang.GENERIC.DOWNLOAD);

    if (result == 0) {
        char file_path[MAX_BUFFER_SIZE];
        snprintf(file_path, sizeof(file_path), "%s/%s/%s.muxzip",
                 device.STORAGE.ROM.MOUNT, MUOS_ARCH_PATH, items[current_item_index].name);
        extract_archive(file_path, "coredown");
    }
}

static void refresh_extra_data_finished(int result) {
    if (result == 0) {
        load_mux("coredown");
        close_input();
        mux_input_stop();
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.MUXDOWNLOAD.ERROR_GET_DATA, FOREVER);
    }
}

static void update_extra_data(void) {
    if (file_exist(data_local_path)) remove(data_local_path);

    set_download_callbacks(refresh_extra_data_finished);
    initiate_download(config.EXTRA.DOWNLOAD.DATA, data_local_path, true, lang.MUXDOWNLOAD.DOWN.DATA);
}

static void handle_a(void) {
    if (download_in_progress || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s.muxzip",
             device.STORAGE.ROM.MOUNT, MUOS_ARCH_PATH, items[current_item_index].name);

    if (file_exist(file_path)) {
        remove(file_path);
        download_finished(-1);
        toast_message(lang.MUXDOWNLOAD.ARCHIVE_REMOVED, SHORT);
    } else {
        set_download_callbacks(download_finished);
        initiate_download(items[current_item_index].extra_data, file_path, true, lang.MUXDOWNLOAD.DOWN.ARCHIVE);
    }
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

    play_sound(SND_CONFIRM);

    if (download_in_progress) {
        cancel_download = true;

        char file_path[MAX_BUFFER_SIZE];
        snprintf(file_path, sizeof(file_path), "%s/%s/%s.muxzip",
                 device.STORAGE.ROM.MOUNT, MUOS_ARCH_PATH, items[current_item_index].name);

        if (file_exist(file_path)) remove(file_path);
    } else {
        if (file_exist(MUOS_ASS_LOAD "_temp")) {
            const char *text = read_all_char_from(MUOS_ASS_LOAD "_temp");
            write_text_to_file(MUOS_ASS_LOAD, "w", CHAR, text);
            remove(MUOS_ASS_LOAD "_temp");
        }

        load_mux("assign");

        close_input();
        mux_input_stop();
    }
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);
    update_extra_data();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_elements(void) {
    lv_obj_set_align(ui_imgBox, config.VISUAL.BOX_ART_ALIGN);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                       1},
            {ui_lblNavA,      lang.GENERIC.DOWNLOAD,    1},
            {ui_lblNavBGlyph, "",                       0},
            {ui_lblNavB,      lang.GENERIC.BACK,        0},
            {ui_lblNavXGlyph, "",                       0},
            {ui_lblNavX,      lang.MUXDOWNLOAD.REFRESH, 0},
            {NULL, NULL,                                0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panels();

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

int muxdownload_main(char *type) {
    exit_status = 0;
    starter_image = 0;

    snprintf(data_local_path, sizeof(data_local_path), "%s/%s",
             device.STORAGE.ROM.MOUNT, MUOS_INFO_PATH "/" EXTRA_DATA);
    snprintf(data_type, sizeof(data_type), "%s", type);

    init_module("muxdownload");
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang,
                          !strcmp(type, "core") ? lang.MUXDOWNLOAD.TITLE.CORE : lang.MUXDOWNLOAD.TITLE.APP);
    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());
    init_fonts();
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    create_content_items();
    ui_count = (int) item_count;

    init_elements();

    int nav_vis = ui_count > 0 ? 1 : 0;
    struct nav_flag nav_e[] = {
            {ui_lblNavA,         nav_vis},
            {ui_lblNavAGlyph,    nav_vis},
            {ui_lblNavX,         nav_vis},
            {ui_lblNavXGlyph,    nav_vis},
            {ui_lblNavY,         0},
            {ui_lblNavYGlyph,    0},
            {ui_lblNavMenu,      0},
            {ui_lblNavMenuGlyph, 0}
    };

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    int sys_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    if (ui_count > 0 && sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) {
        list_nav_move(sys_index, +1);
    }

    init_timer(ui_refresh_task, NULL);

    if (!file_exist(data_local_path)) update_extra_data();

    mux_input_options input_opts = {
            .swap_axis = theme.MISC.NAVIGATION_TYPE == 1,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_MENU_SHORT] = show_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    if (ui_count > 0) free_items(&items, &item_count);

    return exit_status;
}
