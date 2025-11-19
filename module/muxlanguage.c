#include "muxshare.h"
#include "../common/download.h"

static char language_data_local_path[MAX_BUFFER_SIZE];

static void show_help(void) {
    show_info_box(lang.MUXLANGUAGE.TITLE, lang.MUXLANGUAGE.HELP, 0);
}

static void populate_languages(void) {
    struct dirent *entry;
    DIR *dir = opendir(STORAGE_LANG);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN)
        return;
    }

    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            size_t len = strlen(entry->d_name);
            if (len > 5 && strcasecmp(entry->d_name + len - 5, ".json") == 0) {
                entry->d_name[len - 5] = '\0';
                add_item(&items, &item_count, entry->d_name, TS(entry->d_name), "", ITEM);
            }
        }
    }
    sort_items(items, item_count);
    closedir(dir);
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
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void create_language_items(void) {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    populate_languages();
    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlLanguage = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlLanguage);

        lv_obj_t *ui_lblLanguageItem = lv_label_create(ui_pnlLanguage);
        apply_theme_list_item(&theme, ui_lblLanguageItem, items[i].display_name);

        lv_obj_t *ui_lblLanguageGlyph = lv_img_create(ui_pnlLanguage);
        apply_theme_list_glyph(&theme, ui_lblLanguageGlyph, mux_module, "language");

        lv_group_add_obj(ui_group, ui_lblLanguageItem);
        lv_group_add_obj(ui_group_glyph, ui_lblLanguageGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlLanguage);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblLanguageItem, ui_lblLanguageGlyph, items[i].name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblLanguageItem);
    }
}

static void refresh_language_data_finished(int result) {
    if (result == 0) {
        extract_archive(language_data_local_path, "language");
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.MUXLANGUAGE.ERROR_GET_DATA, FOREVER);
    }
}

static void update_language_data(void) {
    snprintf(language_data_local_path, sizeof(language_data_local_path), "%s/%s/lang.muxzip",
             device.STORAGE.ROM.MOUNT, MUOS_ARCH_PATH);

    if (file_exist(language_data_local_path)) remove(language_data_local_path);
    set_download_callbacks(refresh_language_data_finished);
    initiate_download(config.EXTRA.LANGUAGE.DATA, language_data_local_path, true,
                      lang.MUXLANGUAGE.DOWNLOADING);
}

static void handle_a(void) {
    if (download_in_progress || msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);

    toast_message(lang.GENERIC.SAVING, FOREVER);
    refresh_screen(ui_screen);

    write_text_to_file(CONF_CONFIG_PATH "settings/general/language", "w", CHAR,
                       items[current_item_index].name);

    if (config.BOOT.FACTORY_RESET) load_mux("installer");

    refresh_config = 1;

    close_input();
    mux_input_stop();
}

static void handle_b(void) {
    if (download_in_progress || hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    if (config.BOOT.FACTORY_RESET) load_mux("installer");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || hold_call || !device.BOARD.HAS_NETWORK) return;

    if (is_network_connected()) {
        play_sound(SND_CONFIRM);
        update_language_data();
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.NEED_CONNECT, MEDIUM);
    }
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

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
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.SELECT, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

    if (device.BOARD.HAS_NETWORK) {
        setup_nav((struct nav_bar[]) {
                {ui_lblNavXGlyph, "",                       0},
                {ui_lblNavX,      lang.MUXLANGUAGE.REFRESH, 0},
                {NULL, NULL,                                0}
        });
    }

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxlanguage_main(void) {
    init_module("muxlanguage");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXLANGUAGE.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_language_items();
    init_elements();

    if (ui_count > 0) {
        list_nav_next(0);
        lv_obj_update_layout(ui_pnlContent);
    } else if (!ui_count) {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXLANGUAGE.NONE);
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

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "language");
    free_items(&items, &item_count);

    return 0;
}
