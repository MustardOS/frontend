#include "muxshare.h"
#include "../common/collection_theme.h"
#include "../common/download.h"

static char theme_data_local_path[MAX_BUFFER_SIZE];
static theme_item *theme_items = NULL;
static size_t theme_item_count = 0;
static char *preview_zip_path = "/tmp/theme_catalogue.muxzip";

static int exit_status = 0;
static int starter_image = 0;

static void show_help(void) {
    char text_path[MAX_BUFFER_SIZE];
    snprintf(text_path, sizeof(text_path), "%s/theme/text/%s.txt", INFO_CAT_PATH, theme_items[current_item_index].name);

    char credits[MAX_BUFFER_SIZE];
    if (file_exist(text_path)) {
        strcpy(credits, read_all_char_from(text_path));
    } else {
        strcpy(credits, lang.MUXPICKER.NONE.CREDIT);
    }

    show_info_box(TS(lv_label_get_text(lv_group_get_focused(ui_group))), TS(credits), 0);
}

static bool is_downloaded(int index) {
    char theme_path[MAX_BUFFER_SIZE];
    snprintf(theme_path, sizeof(theme_path), "%stheme/%s.muxthm",
             RUN_STORAGE_PATH, theme_items[index].name);
    return (file_exist(theme_path));
}

static void image_refresh(char *image_type) {
    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgBox));

    char image[MAX_BUFFER_SIZE];
    char *core_artwork = "theme";

    load_image_catalogue(core_artwork, theme_items[current_item_index].name, "", "default", mux_dimension,
                         image_type,
                         image, sizeof(image));
    if (!file_exist(image)) {
        load_image_catalogue(core_artwork, theme_items[current_item_index].name, "", "default", "640x480/",
                             image_type,
                             image, sizeof(image));
    }

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
    } else {
        if (strcasecmp(box_image_previous_path, image) != 0) {
            if (file_exist(image)) {
                starter_image = 1;
                struct ImageSettings image_settings = {
                        image, 6,
                        validate_int16((int16_t) (device.MUX.WIDTH * .45), "width"),
                        validate_int16((int16_t) (device.MUX.HEIGHT), "height"),
                        theme.IMAGE_LIST.PAD_LEFT, theme.IMAGE_LIST.PAD_RIGHT,
                        theme.IMAGE_LIST.PAD_TOP, theme.IMAGE_LIST.PAD_BOTTOM
                };
                update_image(ui_imgBox, image_settings);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
            }
        }
    }
}

static bool skip_theme_item(const char *name, bool grid_enabled, bool hdmi_enabled, bool language_enabled,
                            bool resolution640x480, bool resolution720x480, bool resolution720x720,
                            bool resolution1024x768, bool resolution1280x720) {
    return (
            (config.THEME.FILTER.RESOLUTION_640x480 && !resolution640x480) ||
            (config.THEME.FILTER.RESOLUTION_720x480 && !resolution720x480) ||
            (config.THEME.FILTER.RESOLUTION_720x720 && !resolution720x720) ||
            (config.THEME.FILTER.RESOLUTION_1024x768 && !resolution1024x768) ||
            (config.THEME.FILTER.RESOLUTION_1280x720 && !resolution1280x720) ||
            (config.THEME.FILTER.GRID && !grid_enabled) ||
            (config.THEME.FILTER.HDMI && !hdmi_enabled) ||
            (config.THEME.FILTER.LANGUAGE && !language_enabled) ||
            (config.THEME.FILTER.LOOKUP[0] != '\0' && strcasestr(name, config.THEME.FILTER.LOOKUP) == NULL)
    );
}

static void create_content_items(void) {
    if (!file_exist(theme_data_local_path)) {
        LOG_WARN(mux_module, "Theme Data Not Found At: %s", theme_data_local_path)
        return;
    } else {
        LOG_SUCCESS(mux_module, "Found Theme Data At: %s", theme_data_local_path)
    }

    char *json_str = read_all_char_from(theme_data_local_path);
    if (!json_valid(json_str)) return;

    struct json fn_json = json_parse(json_str);

    size_t theme_count = json_array_count(fn_json);

    for (int i = 0; i < theme_count; i++) {
        struct json theme_item = json_array_get(fn_json, i);

        bool grid_enabled = json_bool(json_object_get(theme_item, "grid"));
        bool hdmi = json_bool(json_object_get(theme_item, "hdmi"));
        bool language = json_bool(json_object_get(theme_item, "language"));
        bool resolution640x480 = json_bool(json_object_get(theme_item, "resolution640x480"));
        bool resolution720x480 = json_bool(json_object_get(theme_item, "resolution720x480"));
        bool resolution720x720 = json_bool(json_object_get(theme_item, "resolution720x720"));
        bool resolution1024x768 = json_bool(json_object_get(theme_item, "resolution1024x768"));
        bool resolution1280x720 = json_bool(json_object_get(theme_item, "resolution1280x720"));
        char theme_name[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(theme_item, "name"), theme_name, sizeof(theme_name));
        char theme_url[MAX_BUFFER_SIZE];
        json_string_copy(json_object_get(theme_item, "url"), theme_url, sizeof(theme_url));

        if (!skip_theme_item(theme_name, grid_enabled, hdmi, language,
                             resolution640x480, resolution720x480, resolution720x720, resolution1024x768,
                             resolution1280x720)) {
            add_theme_item(&theme_items, &theme_item_count, theme_name, theme_url, grid_enabled, hdmi, language,
                           resolution640x480, resolution720x480, resolution720x720, resolution1024x768,
                           resolution1280x720);
        }
    }
    sort_theme_items(theme_items, theme_item_count);

    for (size_t i = 0; i < theme_item_count; i++) {
        if (lv_obj_get_child_cnt(ui_pnlContent) >= theme.MUX.ITEM.COUNT) break;

        gen_label(mux_module, is_downloaded(i) ? "theme_down" : "theme", theme_items[i].name);
    }
}

static void update_list_item(lv_obj_t *ui_lblItem, lv_obj_t *ui_lblItemGlyph, int index) {
    lv_label_set_text(ui_lblItem, theme_items[index].name);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (theme.LIST_DEFAULT.GLYPH_ALPHA > 0 && theme.LIST_FOCUS.GLYPH_ALPHA > 0) {
        get_glyph_path(mux_module, is_downloaded(index) ? "theme_down" : "theme", glyph_image_embed,
                       MAX_BUFFER_SIZE);
        lv_img_set_src(ui_lblItemGlyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnlContent, ui_lblItem, ui_lblItemGlyph, theme_items[index].name);
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

        if (theme_item_count > theme.MUX.ITEM.COUNT) {
            int items_before_selected = (theme.MUX.ITEM.COUNT - theme.MUX.ITEM.COUNT % 2) / 2;
            int items_after_selected = (theme.MUX.ITEM.COUNT - 1) / 2;

            if (direction < 0) {
                if (current_item_index == theme_item_count - 1) {
                    update_list_items((int) theme_item_count - theme.MUX.ITEM.COUNT);
                } else {
                    if (current_item_index >= items_before_selected &&
                        current_item_index < theme_item_count - items_after_selected - 1) {
                        lv_obj_t *last_item = lv_obj_get_child(ui_pnlContent,
                                                               theme.MUX.ITEM.COUNT - 1); // Get the last child
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
                        current_item_index < theme_item_count - items_after_selected) {
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

    lv_label_set_text(ui_lblNavA, is_downloaded(current_item_index) ? lang.MUXTHEMEDOWN.REMOVE
                                                                    : lang.MUXTHEMEDOWN.DOWNLOAD);

    image_refresh("box");
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

static void theme_download_finished(int result) {
    update_list_item(lv_group_get_focused(ui_group), lv_group_get_focused(ui_group_glyph), current_item_index);
    lv_label_set_text(ui_lblNavA, is_downloaded(current_item_index) ? lang.MUXTHEMEDOWN.REMOVE
                                                                    : lang.MUXTHEMEDOWN.DOWNLOAD);
}

static void refresh_theme_previews_finished(int result) {
    if (result == 0) {
        extract_archive(preview_zip_path, "themedwn");
        load_mux("themedwn");
        close_input();
        mux_input_stop();
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.MUXTHEMEDOWN.ERROR_GET_DATA, FOREVER);
    }
}

static void refresh_theme_data_finished(int result) {
    if (result == 0) {
        if (file_exist(preview_zip_path)) remove(preview_zip_path);
        set_download_callbacks(refresh_theme_previews_finished);
        initiate_download(config.THEME.DOWNLOAD.PREVIEW, preview_zip_path, true,
                          lang.MUXTHEMEDOWN.DOWN.PREVIEW);
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.MUXTHEMEDOWN.ERROR_GET_DATA, FOREVER);
    }
}

static void update_theme_data(void) {
    if (file_exist(theme_data_local_path)) remove(theme_data_local_path);
    set_download_callbacks(refresh_theme_data_finished);
    initiate_download(config.THEME.DOWNLOAD.DATA, theme_data_local_path, true,
                      lang.MUXTHEMEDOWN.DOWN.DATA);
}

static void handle_a(void) {
    if (download_in_progress || msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    char theme_path[MAX_BUFFER_SIZE];
    snprintf(theme_path, sizeof(theme_path), "%stheme/%s.muxthm",
             RUN_STORAGE_PATH, theme_items[current_item_index].name);
    if (file_exist(theme_path)) {
        remove(theme_path);
        theme_download_finished(0);
        toast_message(lang.MUXTHEMEDOWN.THEME_REMOVED, SHORT);
    } else {
        set_download_callbacks(theme_download_finished);
        initiate_download(theme_items[current_item_index].url, theme_path, true,
                          lang.MUXTHEMEDOWN.DOWN.THEME);
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
        char theme_path[MAX_BUFFER_SIZE];
        snprintf(theme_path, sizeof(theme_path), "%stheme/%s.muxthm",
                 RUN_STORAGE_PATH, theme_items[current_item_index].name);
        if (file_exist(theme_path)) {
            remove(theme_path);
        }
    } else {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "theme");
        write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, "/theme");

        load_mux("picker");

        close_input();
        mux_input_stop();
    }
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || !ui_count || hold_call) return;
    play_sound(SND_CONFIRM);
    update_theme_data();
}

static void handle_y(void) {
    if (download_in_progress || msgbox_active || hold_call) return;
    play_sound(SND_CONFIRM);

    theme_down_index = current_item_index;

    load_mux("themefilter");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_lblCounter_explore,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                         1},
            {ui_lblNavA,      lang.MUXTHEMEDOWN.DOWNLOAD, 1},
            {ui_lblNavBGlyph, "",                         0},
            {ui_lblNavB,      lang.GENERIC.BACK,          0},
            {ui_lblNavXGlyph, "",                         0},
            {ui_lblNavX,      lang.MUXTHEMEDOWN.REFRESH,  0},
            {ui_lblNavYGlyph, "",                         0},
            {ui_lblNavY,      lang.GENERIC.FILTER,        0},
            {NULL,            NULL,                       0}
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

        update_file_counter(ui_lblCounter_explore, ui_count);
        lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

int muxthemedown_main(void) {
    exit_status = 0;
    starter_image = 0;
    snprintf(theme_data_local_path, sizeof(theme_data_local_path), "%s/%s",
             device.STORAGE.ROM.MOUNT, MUOS_INFO_PATH "/" THEME_DATA);

    init_module("muxthemedown");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTHEMEDOWN.TITLE);
    init_ui_item_counter(&theme);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    create_content_items();
    ui_count = (int) theme_item_count;
    init_elements();

    int nav_vis = ui_count > 0 ? 1 : 0;
    struct nav_flag nav_e[] = {
            {ui_lblNavA,         nav_vis},
            {ui_lblNavAGlyph,    nav_vis},
            {ui_lblNavX,         nav_vis},
            {ui_lblNavXGlyph,    nav_vis},
            {ui_lblNavY,         nav_vis},
            {ui_lblNavYGlyph,    nav_vis},
            {ui_lblNavMenu,      0},
            {ui_lblNavMenuGlyph, 0}
    };

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (ui_count > 0) list_nav_move(theme_down_index, 1);
    theme_down_index = 0;

    update_file_counter(ui_lblCounter_explore, ui_count);

    init_timer(ui_refresh_task, NULL);

    if (!file_exist(theme_data_local_path)) update_theme_data();

    mux_input_options input_opts = {
            .swap_axis = theme.MISC.NAVIGATION_TYPE == 1,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
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
            },
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    if (ui_count > 0) free_theme_items(&theme_items, &theme_item_count);

    return exit_status;
}
