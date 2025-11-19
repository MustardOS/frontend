#include "muxshare.h"

static void show_help(void) {
    show_info_box(lang.MUXARCHIVE.TITLE, lang.MUXARCHIVE.HELP, 0);
}

static void create_archive_items(void) {
    const char *mount_points[] = {
            device.STORAGE.ROM.MOUNT,
            device.STORAGE.SDCARD.MOUNT,
            device.STORAGE.USB.MOUNT,
            STORAGE_THEME
    };

    const char *subdirs[] = {"/backup", "/archive"};
    char archive_directories[8][MAX_BUFFER_SIZE];

    for (int i = 0, k = 0; i < 4; ++i) {
        for (int j = 0; j < 2; ++j, ++k) {
            snprintf(archive_directories[k], sizeof(archive_directories[k]), "%s%s", mount_points[i], subdirs[j]);
        }
    }

    const char *mux_archive[] = {
            ".muxupd", ".muxapp", ".muxzip", ".muxthm", ".muxcat", ".muxcfg"
    };

    const char *dirs[8];
    for (int i = 0; i < 8; ++i) dirs[i] = archive_directories[i];

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, mux_archive, &files, A_SIZE(dirs), A_SIZE(mux_archive), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        return;
    }

    if (file_count == 0) {
        free_array(files, file_count);
        return;
    }

    qsort(files, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < file_count; ++i) {
        assert(files[i] != NULL);
        char *base_filename = files[i];

        const char *prefix = NULL;
        char storage_prefix[MAX_BUFFER_SIZE];
        const char *ext_type = "UNK";

        const char *dot = strrchr(base_filename, '.');
        if (dot) {
            for (size_t e = 0; e < A_SIZE(mux_archive); ++e) {
                if (strcasecmp(dot, mux_archive[e]) == 0) {
                    ext_type = mux_archive[e] + 4;
                    break;
                }
            }
        }

        char ext_upper[8];
        strncpy(ext_upper, ext_type, sizeof(ext_upper) - 1);
        ext_upper[sizeof(ext_upper) - 1] = '\0';
        str_toupper(ext_upper);

        for (size_t j = 0; j < A_SIZE(mount_points); ++j) {
            if (strstr(base_filename, mount_points[j])) {
                snprintf(storage_prefix, sizeof(storage_prefix), "[%s-%s]",
                         j == 0 ? "SD1" :
                         j == 1 ? "SD2" :
                         j == 2 ? "USB" : "THM",
                         ext_upper);
                prefix = storage_prefix;
                break;
            }
        }

        if (!prefix) continue;

        char archive_name[MAX_BUFFER_SIZE];
        snprintf(archive_name, sizeof(archive_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char install_check[MAX_BUFFER_SIZE];
        snprintf(install_check, sizeof(install_check), OPT_PATH "update/installed/%s.done",
                 archive_name);

        int is_installed = file_exist(install_check);

        char archive_store[MAX_BUFFER_SIZE];
        snprintf(archive_store, sizeof(archive_store), "%s %s", prefix, archive_name);

        char item_glyph[MAX_BUFFER_SIZE];
        snprintf(item_glyph, sizeof(item_glyph), "%s", is_installed ? "installed" : "archive");

        ui_count++;

        add_item(&items, &item_count, base_filename, strip_ext(archive_store), item_glyph, ITEM);

        lv_obj_t *ui_pnlArchive = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlArchive);

        lv_obj_t *ui_lblArchiveItem = lv_label_create(ui_pnlArchive);
        apply_theme_list_item(&theme, ui_lblArchiveItem, strip_ext(archive_store));

        lv_obj_t *ui_lblArchiveItemGlyph = lv_img_create(ui_pnlArchive);
        apply_theme_list_glyph(&theme, ui_lblArchiveItemGlyph, mux_module, items[i].extra_data);

        lv_group_add_obj(ui_group, ui_lblArchiveItem);
        lv_group_add_obj(ui_group_glyph, ui_lblArchiveItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlArchive);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblArchiveItem, ui_lblArchiveItemGlyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblArchiveItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);

    free_array(files, file_count);
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
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (ui_count > 0) {
        play_sound(SND_CONFIRM);

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        extract_archive(items[current_item_index].name, "archive");

        load_mux("archive");

        close_input();
        mux_input_stop();
    }
}

static void handle_x(void) {
    if (msgbox_active || !ui_count) return;

    if (!hold_call) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.HOLD_CONFIRM, SHORT);
        return;
    }

    char *archive_item = items[current_item_index].name;

    if (!file_exist(archive_item)) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.REMOVE_FAIL, MEDIUM);
        return;
    }

    remove(archive_item);
    sync();

    play_sound(SND_MUOS);
    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    hold_call = 0;
    load_mux("archive");

    close_input();
    mux_input_stop();
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

    play_sound(SND_BACK);

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

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
            {ui_lblNavAGlyph, "",                   1},
            {ui_lblNavA,      lang.GENERIC.EXTRACT, 1},
            {ui_lblNavBGlyph, "",                   0},
            {ui_lblNavB,      lang.GENERIC.BACK,    0},
            {ui_lblNavXGlyph, "",                   0},
            {ui_lblNavX,      lang.GENERIC.REMOVE,  0},
            {NULL, NULL,                            0}
    });

    if (!ui_count) {
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(element_focused, get_last_subdir(strip_ext(items[current_item_index].name), '/', 4));

            adjust_wallpaper_element(ui_group, 0, ARCHIVE);
        }
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxarchive_main(void) {
    init_module("muxarchive");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXARCHIVE.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, ARCHIVE);

    init_fonts();
    create_archive_items();

    init_elements();

    int arc_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        arc_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    int nav_hidden = 0;
    if (ui_count > 0) {
        nav_hidden = 1;
        if (arc_index > -1 && arc_index <= ui_count && current_item_index < ui_count) list_nav_move(arc_index, +1);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXARCHIVE.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,      nav_hidden},
            {ui_lblNavAGlyph, nav_hidden}
    };
    set_nav_flags(nav_e, A_SIZE(nav_e));

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
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

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
