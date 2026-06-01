#include "muxshare.h"

static int mount_points_init = 0;
static const char *mount_points[4];

const char *subdirs[] = {
        "/backup", "/archive"
};

const char *mount_labels[] = {
        "SD1", "SD2", "USB", "THM"
};

const char *mux_archive[] = {
        ".muxupd", ".muxapp", ".muxzip",
        ".muxthm", ".muxcat", ".muxcfg"
};

const char *del_marked[] = {
        LANG_ARCHIVE
};

static void show_help(void) {
    show_info_box(lang.MUXARCHIVE.TITLE, lang.MUXARCHIVE.HELP, 0);
}

static void init_mount_points(void) {
    if (mount_points_init) return;
    mount_points_init = 1;

    mount_points[0] = device.STORAGE.ROM.MOUNT;
    mount_points[1] = device.STORAGE.SDCARD.MOUNT;
    mount_points[2] = device.STORAGE.USB.MOUNT;
    mount_points[3] = theme_base;
}

static size_t purge_marked(char **files, size_t file_count) {
    for (size_t i = 0; i < file_count; ++i) {
        const char *file = files[i];
        if (!file) continue;

        const char *marked = strrchr(file, '/');
        marked = marked ? marked + 1 : file;

        for (size_t d = 0; d < A_SIZE(del_marked); ++d) {
            const char *mark = del_marked[d];
            if (!mark || !*mark) continue;

            if (strcmp(marked, mark) == 0) {
                LOG_INFO(mux_module, "Removing marked archive: %s", file);
                (void) remove(file);
                files[i] = NULL;
                break;
            }
        }
    }

    size_t new_count = 0;
    for (size_t r = 0; r < file_count; ++r) {
        if (files[r]) files[new_count++] = files[r];
    }

    return new_count;
}

static void create_archive_items(void) {
    init_mount_points();
    char archive_directories[A_SIZE(mount_points) * A_SIZE(subdirs)][MAX_BUFFER_SIZE];

    for (size_t i = 0, k = 0; i < A_SIZE(mount_points); ++i) {
        for (size_t j = 0; j < A_SIZE(subdirs); ++j, ++k) {
            int len = snprintf(archive_directories[k], sizeof(archive_directories[k]), "%s%s", mount_points[i], subdirs[j]);
            if (len < 0 || (size_t) len >= sizeof(archive_directories[k])) {
                LOG_WARN(mux_module, "Archive path truncated: %s%s", mount_points[i], subdirs[j]);
                archive_directories[k][0] = '\0';
            }
        }
    }

    const char *dirs[A_SIZE(mount_points) * A_SIZE(subdirs)];
    for (size_t i = 0; i < (A_SIZE(dirs)); ++i) dirs[i] = archive_directories[i];

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, mux_archive, &files, A_SIZE(dirs), A_SIZE(mux_archive), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM);
        return;
    }

    if (file_count == 0) {
        free_array(files, file_count);
        return;
    }

    file_count = purge_marked(files, file_count);

    if (file_count == 0) {
        free_array(files, 0);
        return;
    }

    qsort(files, file_count, sizeof(char *), str_compare);

    reset_ui_groups();

    for (size_t i = 0; i < file_count; ++i) {
        assert(files[i] != NULL);
        char *base_filename = files[i];

        const char *prefix = NULL;
        char storage_prefix[MAX_BUFFER_SIZE];
        const char *ext_type = "UNK";

        char *dot = strrchr(base_filename, '.');
        if (dot) {
            for (size_t e = 0; e < A_SIZE(mux_archive); ++e) {
                if (strcasecmp(dot, mux_archive[e]) == 0) {
                    ext_type = mux_archive[e] + 4;
                    break;
                }
            }
        }

        char ext_upper[16];
        snprintf(ext_upper, sizeof(ext_upper), "%s", ext_type);
        str_toupper(ext_upper);

        for (size_t j = 0; j < A_SIZE(mount_points); ++j) {
            if (!mount_points[j]) continue;
            if (strstr(base_filename, mount_points[j])) {
                snprintf(storage_prefix, sizeof(storage_prefix), "[%s-%s]", mount_labels[j], ext_upper);
                prefix = storage_prefix;
                break;
            }
        }

        if (!prefix) continue;

        char archive_name[MAX_BUFFER_SIZE];
        snprintf(archive_name, sizeof(archive_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char install_check[MAX_BUFFER_SIZE];
        int ic_len = snprintf(install_check, sizeof(install_check),
                              OPT_PATH "update/installed/%s.done", archive_name);

        int is_installed = (ic_len >= 0 && (size_t) ic_len < sizeof(install_check)) && file_exist(install_check);

        char archive_store[MAX_BUFFER_SIZE];
        snprintf(archive_store, sizeof(archive_store), "%s %s", prefix, archive_name);

        char item_glyph[MAX_BUFFER_SIZE];
        snprintf(item_glyph, sizeof(item_glyph), "%s", is_installed ? "installed" : "archive");

        ui_count++;
        char *display_name = strip_ext(archive_store);
        add_item(&items, &item_count, base_filename, display_name, item_glyph, ITEM);

        lv_obj_t *ui_pnlArchive = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlArchive);

        lv_obj_t *ui_lblArchiveItem = lv_label_create(ui_pnlArchive);
        apply_theme_list_item(&theme, ui_lblArchiveItem, display_name);

        lv_obj_t *ui_lblArchiveItemGlyph = lv_img_create(ui_pnlArchive);
        apply_theme_list_glyph(&theme, ui_lblArchiveItemGlyph, mux_module, items[i].extra_data);

        lv_group_add_obj(ui_group, ui_lblArchiveItem);
        lv_group_add_obj(ui_group_glyph, ui_lblArchiveItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlArchive);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblArchiveItem, ui_lblArchiveItemGlyph, display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblArchiveItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);

    free_array(files, file_count);
}


static int remove_mode = 0;
static int skip_confirm = 0;
static mux_dialogue remove_dlg;

static void show_remove_dialog(void) {
    remove_mode = 1;
    remove_dlg.selected = 0;
    dialogue_show(&remove_dlg);
    dialogue_refresh(&remove_dlg, &theme);
}

static void hide_remove_dialog(void) {
    remove_mode = 0;
    dialogue_hide(&remove_dlg);
}

static void do_remove(void) {
    char *archive_item = items[current_item_index].name;

    if (!file_exist(archive_item)) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.REMOVE_FAIL, MEDIUM);
        return;
    }

    remove(archive_item);
    sync();

    play_sound(SND_MUOS);
    write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count - 1));

    load_mux("archive");
    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (remove_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (remove_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (remove_mode) {
        mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == MUX_REMOVE_YEP) {
            do_remove();
        } else if (opt == MUX_REMOVE_SKIP) {
            skip_confirm = 1;
            do_remove();
        }
        return;
    }

    if (msgbox_active || hold_call) return;

    if (ui_count > 0) {
        play_sound(SND_CONFIRM);

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        extract_archive(items[current_item_index].name, "archive");

        load_mux("archive");

        mux_input_stop();
    }
}

static void handle_x(void) {
    if (msgbox_active || !ui_count || remove_mode) return;

    if (config.SETTINGS.ADVANCED.TRUSTREMOVE || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(SND_CONFIRM);
    show_remove_dialog();
}

static void handle_b(void) {
    if (hold_call) return;

    if (remove_mode) {
        hide_remove_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(SND_BACK);

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                   1},
            {ui_lblNavA,      lang.GENERIC.EXTRACT, 1},
            {ui_lblNavBGlyph, "",                   0},
            {ui_lblNavB,      lang.GENERIC.BACK,    0},
            {ui_lblNavXGlyph, "",                   1},
            {ui_lblNavX,      lang.GENERIC.REMOVE,  1},
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
            struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(e_focused, get_last_subdir(strip_ext(items[current_item_index].name), '/', 4));

            adjust_wallpaper_element(ui_group, 0, WALL_ARCHIVE);
        }
        adjust_gen_panel();

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxarchive_main(void) {
    skip_confirm = 0;
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXARCHIVE.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_ARCHIVE);

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
        if (arc_index > -1 && arc_index <= ui_count && current_item_index < ui_count) gen_step_movement(arc_index, +1, 1, 0);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXARCHIVE.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,      nav_hidden},
            {ui_lblNavAGlyph, nav_hidden}
    };
    set_nav_flags(nav_e, A_SIZE(nav_e));

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
