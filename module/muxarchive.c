#include "muxshare.h"

static int mount_points_init = 0;
static const char *mount_points[4];

const char *subdirs[] = {"/backup", "/archive"};

const char *mount_labels[] = {"SD1", "SD2", "USB", "THM"};

const char *mux_archive[] = {".muxupd", ".muxapp", ".muxzip", ".muxthm", ".muxcat", ".muxcfg"};

const char *del_marked[] = {LANG_ARCHIVE};

static void show_help(void) {
    show_info_box(lang.muxarchive.title, lang.muxarchive.help, 0);
}

static void init_mount_points(void) {
    if (mount_points_init) return;
    mount_points_init = 1;

    mount_points[0] = device.storage.rom.mount;
    mount_points[1] = device.storage.sdcard.mount;
    mount_points[2] = device.storage.usb.mount;
    mount_points[3] = theme_base;
}

static size_t purge_marked(char **files, const size_t file_count) {
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
            const int len =
                snprintf(archive_directories[k], sizeof(archive_directories[k]), "%s%s", mount_points[i], subdirs[j]);
            if (len < 0 || (size_t) len >= sizeof(archive_directories[k])) {
                LOG_WARN(mux_module, "Archive path truncated: %s%s", mount_points[i], subdirs[j]);
                archive_directories[k][0] = '\0';
            }
        }
    }

    const char *dirs[A_SIZE(mount_points) * A_SIZE(subdirs)];
    for (size_t i = 0; i < A_SIZE(dirs); ++i)
        dirs[i] = archive_directories[i];

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, mux_archive, &files, A_SIZE(dirs), A_SIZE(mux_archive), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
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
        const char *base_filename = files[i];

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
        snprintf(
            archive_name, sizeof(archive_name), "%s",
            str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/')
        );

        char install_check[MAX_BUFFER_SIZE];
        const int ic_len =
            snprintf(install_check, sizeof(install_check), OPT_PATH "update/installed/%s.done", archive_name);

        const int is_installed = ic_len >= 0 && (size_t) ic_len < sizeof(install_check) && file_exist(install_check);

        char archive_store[MAX_BUFFER_SIZE];
        snprintf(archive_store, sizeof(archive_store), "%s %s", prefix, archive_name);

        char item_glyph[MAX_BUFFER_SIZE];
        snprintf(item_glyph, sizeof(item_glyph), "%s", is_installed ? "installed" : "archive");

        ui_count_static++;
        const char *display_name = strip_ext(archive_store);
        add_item(&items, &item_count, base_filename, display_name, item_glyph, ITEM);

        lv_obj_t *ui_pnl_archive = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_archive);

        lv_obj_t *ui_lbl_archive_item = lv_label_create(ui_pnl_archive);
        apply_theme_list_item(&theme, ui_lbl_archive_item, display_name);

        lv_obj_t *ui_lbl_archive_item_glyph = lv_img_create(ui_pnl_archive);
        apply_theme_list_glyph(&theme, ui_lbl_archive_item_glyph, mux_module, items[i].extra_data);

        lv_group_add_obj(ui_group, ui_lbl_archive_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_archive_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_archive);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_archive_item, ui_lbl_archive_item_glyph, display_name);
        apply_text_long_dot(&theme, ui_lbl_archive_item);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);

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
    const char *archive_item = items[current_item_index].name;

    if (!file_exist(archive_item)) {
        play_sound(snd_error);
        toast_message(lang.generic.remove_fail, tst_wait_m);
        return;
    }

    remove(archive_item);
    sync();

    play_sound(snd_muos);
    write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));

    load_mux("archive");
    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
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
        const mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == mux_remove_yep) {
            do_remove();
        } else if (opt == mux_remove_skip) {
            skip_confirm = 1;
            do_remove();
        }
        return;
    }

    if (msgbox_active || hold_call) return;

    if (ui_count_static > 0) {
        play_sound(snd_confirm);

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        extract_archive(items[current_item_index].name, "archive");

        load_mux("archive");

        skip_confirm = 0;
        mux_input_stop();
    }
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || remove_mode) return;

    if (config.settings.advanced.trust_remove || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
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

    play_sound(snd_back);

    skip_confirm = 0;
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.extract, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 1},
                                  {ui_lbl_nav_x, lang.generic.remove, 1},
                                  {NULL, NULL, 0}});

    if (!ui_count_static) {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(e_focused, get_last_subdir(strip_ext(items[current_item_index].name), '/', 4));

            adjust_wallpaper_element(ui_group, 0, wall_archive);
        }
        adjust_gen_panel();

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxarchive_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxarchive.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_archive);

    init_fonts();
    create_archive_items();

    init_elements();

    int arc_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        arc_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    int nav_hidden = 0;
    if (ui_count_static > 0) {
        nav_hidden = 1;
        if (arc_index > -1 && arc_index <= ui_count_static && current_item_index < ui_count_static)
            gen_step_movement(arc_index, +1, 1, 0, 1);
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxarchive.none);
    }

    const struct nav_flag nav_e[] = {{ui_lbl_nav_a, nav_hidden}, {ui_lbl_nav_a_glyph, nav_hidden}};
    set_nav_flags(nav_e, A_SIZE(nav_e));

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.back);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
