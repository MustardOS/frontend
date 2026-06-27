#include "muxshare.h"
#include "../common/download.h"

static char language_data_local_path[MAX_BUFFER_SIZE];

static void update_font_for_language(const char *new_language) {
    char dir[MAX_BUFFER_SIZE];
    snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", new_language);

    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);

    if (n >= 0) {
        char first_name[MAX_BUFFER_SIZE] = "";
        for (int i = 0; i < n; i++) {
            const char *fname = entries[i]->d_name;
            const size_t len = strlen(fname);
            if (!first_name[0] && len > 4 && strcasecmp(fname + len - 4, ".ttf") == 0) {
                snprintf(first_name, sizeof(first_name), "%.*s", (int) (len - 4), fname);
            }
            free(entries[i]);
        }
        free(entries);

        if (first_name[0]) {
            write_text_to_file(CONF_CONFIG_PATH "settings/advanced/font", "w", INT, 0);
            write_text_to_file(CONF_CONFIG_PATH "settings/font/name", "w", CHAR, first_name);
            return;
        }
    }

    write_text_to_file(CONF_CONFIG_PATH "settings/advanced/font", "w", INT, 1);
    write_text_to_file(CONF_CONFIG_PATH "settings/font/name", "w", CHAR, "Noto Sans");
}

static void show_help(void) {
    show_info_box(lang.muxlanguage.title, lang.muxlanguage.help, 0);
}

static void populate_languages(void) {
    struct dirent *entry;
    DIR *dir = opendir(STORAGE_LANG);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return;
    }

    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            const size_t len = strlen(entry->d_name);
            if (len > 5 && strcasecmp(entry->d_name + len - 5, ".json") == 0) {
                entry->d_name[len - 5] = '\0';
                add_item(&items, &item_count, entry->d_name, TRS(entry->d_name), "", ITEM);
            }
        }
    }
    sort_items(items, item_count);
    closedir(dir);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
}

static void list_nav_prev(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void create_language_items(void) {
    reset_ui_groups();
    populate_languages();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        lv_obj_t *ui_pnl_language = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_language);

        lv_obj_t *ui_lbl_language_item = lv_label_create(ui_pnl_language);
        apply_theme_list_item(&theme, ui_lbl_language_item, items[i].display_name);

        lv_obj_t *ui_lbl_language_glyph = lv_img_create(ui_pnl_language);
        apply_theme_list_glyph(&theme, ui_lbl_language_glyph, mux_module, "language");

        lv_group_add_obj(ui_group, ui_lbl_language_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_language_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_language);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_language_item, ui_lbl_language_glyph, items[i].name);
        apply_text_long_dot(&theme, ui_lbl_language_item);
    }
}

static void refresh_language_data_finished(const int result) {
    if (result == 0) {
        extract_archive(language_data_local_path, "language");
    } else {
        play_sound(snd_error);
        toast_message(lang.muxlanguage.error_get_data, tst_wait_f);
    }
}

static void update_language_data(void) {
    snprintf(
        language_data_local_path, sizeof(language_data_local_path), "%s/%s/" LANG_ARCHIVE, device.storage.rom.mount,
        MUOS_ARCH_PATH
    );
    if (file_exist(language_data_local_path)) remove(language_data_local_path);

    set_download_callbacks(refresh_language_data_finished);
    initiate_download(config.extra.language.data, language_data_local_path, 1, lang.muxlanguage.downloading);
}

static void handle_a(void) {
    if (download_in_progress || msgbox_active || hold_call) return;

    play_sound(snd_confirm);

    toast_message(lang.generic.saving, tst_wait_f);

    write_text_to_file(CONF_CONFIG_PATH "settings/general/language", "w", CHAR, items[current_item_index].name);

    update_font_for_language(items[current_item_index].name);

    if (config.boot.factory_reset) load_mux("installer");

    refresh_config = 1;

    mux_input_stop();
}

static void handle_b(void) {
    if (download_in_progress || hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    if (config.boot.factory_reset) load_mux("installer");

    mux_input_stop();
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || hold_call || !device.board.has_network) return;

    if (is_network_connected()) {
        play_sound(snd_confirm);
        update_language_data();
    } else {
        play_sound(snd_error);
        toast_message(lang.generic.need_connect, tst_wait_m);
    }
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    download_poll();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.select, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    if (device.board.has_network) {
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_x_glyph, "", 0}, {ui_lbl_nav_x, lang.generic.refresh, 0}, {NULL, NULL, 0}}
        );
    }

    overlay_display();
}

int muxlanguage_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxlanguage.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_language_items();
    init_elements();

    if (ui_count_static > 0) {
        list_nav_next(0);
        lv_obj_update_layout(ui_pnl_content);
    } else if (!ui_count_static) {
        lv_label_set_text(ui_lbl_screen_message, lang.muxlanguage.none);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
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
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "language");
    free_items(&items, &item_count);

    return 0;
}
