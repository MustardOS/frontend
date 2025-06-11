#include "muxshare.h"

static void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXARCHIVE.TITLE, lang.MUXARCHIVE.HELP);
}

static void create_archive_items() {
    const char *mount_points[] = {
            device.STORAGE.ROM.MOUNT,
            device.STORAGE.SDCARD.MOUNT,
            device.STORAGE.USB.MOUNT
    };

    const char *subdirs[] = {"/muos/update", "/backup", "/archive"};
    char archive_directories[9][MAX_BUFFER_SIZE];

    for (int i = 0, k = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j, ++k) {
            snprintf(archive_directories[k], sizeof(archive_directories[k]), "%s%s", mount_points[i], subdirs[j]);
        }
    }

    char **file_names = NULL;
    size_t file_count = 0;

    const char *ext_map[][2] = {
            {".muxupd", "UPD"},
            {".muxapp", "APP"},
            {".muxzip", "ZIP"},
            {".muxthm", "THM"},
            {".muxcat", "CAT"},
            {".muxcfg", "CFG"}
    };

    for (size_t dir_index = 0; dir_index < sizeof(archive_directories) / sizeof(archive_directories[0]); ++dir_index) {
        DIR *ad = opendir(archive_directories[dir_index]);
        if (!ad) continue;

        struct dirent *af;
        while ((af = readdir(ad))) {
            if (af->d_type == DT_REG) {
                const char *last_dot = strrchr(af->d_name, '.');
                if (!last_dot) continue;

                const char *prefix = NULL;
                for (size_t i = 0; i < sizeof(ext_map) / sizeof(ext_map[0]); ++i) {
                    if (strcasecmp(last_dot, ext_map[i][0]) == 0) {
                        prefix = ext_map[i][1];
                        break;
                    }
                }

                if (!prefix) continue;

                char base_name[MAX_BUFFER_SIZE];
                strncpy(base_name, af->d_name, last_dot - af->d_name);
                base_name[last_dot - af->d_name] = '\0';

                char full_app_name[MAX_BUFFER_SIZE];
                snprintf(full_app_name, sizeof(full_app_name), "%s/%s", archive_directories[dir_index], af->d_name);

                char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                if (!temp) {
                    perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                    free(file_names);
                    closedir(ad);
                    return;
                }

                file_names = temp;
                file_names[file_count] = strdup(full_app_name);
                if (!file_names[file_count++]) {
                    perror(lang.SYSTEM.FAIL_DUP_STRING);
                    free(file_names);
                    closedir(ad);
                    return;
                }
            }
        }
        closedir(ad);
    }

    if (!file_names) return;
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        char *base_filename = file_names[i];
        if (!file_names[i]) continue;

        const char *prefix = NULL;
        for (size_t j = 0; j < sizeof(mount_points) / sizeof(mount_points[0]); ++j) {
            if (strstr(base_filename, mount_points[j])) {
                static char storage_prefix[MAX_BUFFER_SIZE];

                const char *file_extension = strrchr(base_filename, '.');
                const char *ext_type = NULL;
                if (file_extension) {
                    for (size_t i = 0; i < sizeof(ext_map) / sizeof(ext_map[0]); ++i) {
                        if (strcasecmp(file_extension, ext_map[i][0]) == 0) {
                            ext_type = ext_map[i][1];
                            break;
                        }
                    }
                }
                if (!ext_type) ext_type = "UNK";

                snprintf(storage_prefix, sizeof(storage_prefix), "[%s-%s]",
                         j == 0 ? "SD1" : (j == 1 ? "SD2" : "USB"), ext_type);
                prefix = storage_prefix;
                break;
            }
        }

        if (!prefix) continue;

        static char archive_name[MAX_BUFFER_SIZE];
        snprintf(archive_name, sizeof(archive_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char install_check[MAX_BUFFER_SIZE];
        snprintf(install_check, sizeof(install_check), "%s/muos/update/installed/%s.done",
                 mount_points[0], archive_name);

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
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblArchiveItem, items[i].display_name);

        free(base_filename);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free(file_names);
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);

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
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a() {
    if (msgbox_active) return;

    if (ui_count > 0) {
        play_sound(SND_CONFIRM);

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        extract_archive(items[current_item_index].name);

        load_mux("archive");

        close_input();
        mux_input_stop();
    }
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    close_input();
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help();
    }
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements() {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                   1},
            {ui_lblNavA,      lang.GENERIC.EXTRACT, 1},
            {ui_lblNavBGlyph, "",                   0},
            {ui_lblNavB,      lang.GENERIC.BACK,    0},
            {NULL, NULL,                            0}
    });

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

int muxarchive_main() {
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
    set_nav_flags(nav_e, sizeof(nav_e) / sizeof(nav_e[0]));

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
