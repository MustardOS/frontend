#include "muxshare.h"
#include "ui/ui_muxbackup.h"

#define BACKUP(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(BACKUP_ELEMENTS), storage_count = ui_count_dynamic - 3 };
#undef BACKUP

#define BACKUP(NAME, UDATA) static int NAME##_original;
BACKUP_ELEMENTS
#undef BACKUP

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define BACKUP(NAME, UDATA) {UDATA, lang.muxbackup.help.NAME},
        BACKUP_ELEMENTS
#undef BACKUP
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define BACKUP(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_backup);
    BACKUP_ELEMENTS
#undef BACKUP
}

static void set_all_options(const int value) {
#define BACKUP(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_backup, value);
    BACKUP_ELEMENTS
#undef BACKUP
}

static void restore_backup_options(void) {
#define BACKUP(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_backup, config.backup.NAME);
    BACKUP_ELEMENTS
#undef BACKUP

    lv_dropdown_set_selected(ui_dro_target_backup, lv_dropdown_get_option_cnt(ui_dro_target_backup) - 1);
}

static void save_backup_options(void) {
    int is_modified = 0;

#define BACKUP(NAME, UDATA) CHECK_AND_SAVE_STD(backup, NAME, "backup/" UDATA, INT, 0);
    BACKUP_ELEMENTS
#undef BACKUP

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_OPTION_ITEM(-1, backup, track, lang.muxbackup.track, "track", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, apps, lang.muxbackup.apps, "application", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, music, lang.muxbackup.music, "music", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, content, lang.muxbackup.content, "content", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, collection, lang.muxbackup.collection, "collection", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, override, lang.muxbackup.override, "override", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, package, lang.muxbackup.package, "package", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, name, lang.muxbackup.name, "name", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, history, lang.muxbackup.history, "history", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, catalogue, lang.muxbackup.catalogue, "catalogue", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, network, lang.muxbackup.network, "network", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, cheats, lang.muxbackup.cheats, "cheats", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, config, lang.muxbackup.config, "config", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, overlays, lang.muxbackup.overlays, "overlays", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, shaders, lang.muxbackup.shaders, "shaders", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, save, lang.muxbackup.save, "save", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, screenshot, lang.muxbackup.screenshot, "screenshot", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, syncthing, lang.muxbackup.syncthing, "syncthing", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, bios, lang.muxbackup.bios, "bios", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, theme, lang.muxbackup.theme, "theme", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, init, lang.muxbackup.init, "init", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, target, lang.muxbackup.target, "target", NULL, 0);
    INIT_OPTION_ITEM(-1, backup, merge, lang.muxbackup.merge, "merge", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, backup, start, lang.muxbackup.start, "start", NULL, 0);

    lv_dropdown_clear_options(ui_dro_target_backup);
    lv_dropdown_add_option(ui_dro_target_backup, "SD1", LV_DROPDOWN_POS_LAST);

    if (is_partition_mounted(device.storage.sdcard.mount)) {
        lv_dropdown_add_option(ui_dro_target_backup, "SD2", LV_DROPDOWN_POS_LAST);
    }

    if (is_partition_mounted(device.storage.usb.mount)) {
        lv_dropdown_add_option(ui_dro_target_backup, "USB", LV_DROPDOWN_POS_LAST);
    }

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    int dbi_index = 0;
    if (file_exist(MUOS_DBI_LOAD)) {
        dbi_index = read_line_int_from(MUOS_DBI_LOAD, 1);
        remove(MUOS_DBI_LOAD);
    }

    if (ui_count_static > 0 && dbi_index >= 0 && dbi_index < ui_count_static && current_item_index < ui_count_static) {
        gen_step_movement(dbi_index, 1, 0, 0, 1);
    }
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(snd_confirm);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(snd_back);

    save_backup_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "backup");

    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    // Return if backup set to Excluded or if on Toggle Target Storage
    if (e_focused == ui_lbl_merge_backup || e_focused == ui_lbl_target_backup
        || (e_focused != ui_lbl_start_backup && lv_dropdown_get_selected(lv_group_get_focused(ui_group_value)) == 0))
        return;

    play_sound(snd_confirm);

    save_backup_options();

    char target_value[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_dro_target_backup, target_value, sizeof(target_value));

    char datetime[64];
    snprintf(datetime, sizeof(datetime), "%s", get_datetime());

    char *manifest_file = "/tmp/muxbackup_manifest.txt";

    FILE *fp = fopen(manifest_file, "w");
    if (!fp) {
        LOG_ERROR(mux_module, "Failed to open '%s' for writing", manifest_file);
        return;
    }

    // Write for batch backup
    char do_merge[4];
    if (e_focused == ui_lbl_start_backup) {
        snprintf(do_merge, sizeof(do_merge), "%d", lv_dropdown_get_selected(ui_dro_merge_backup));

        fprintf(fp, "%s %s\n", "BATCH", target_value);

        for (int i = 0; i < storage_count; i++) {
            const lv_obj_t *ui_pnl_item = lv_obj_get_child(ui_pnl_content, i);
            lv_obj_t *ui_lbl_item = lv_obj_get_child(ui_pnl_item, 0);
            const lv_obj_t *ui_dro_item = lv_obj_get_child(ui_pnl_item, 2);
            const int value = lv_dropdown_get_selected(ui_dro_item);
            // Skip if set to NONE
            if (value == 0) continue;
            char *runner = lv_obj_get_user_data(ui_lbl_item);
            fprintf(fp, "%s %s\n", target_value, runner);
        }
    } else { // For other backup paths, write the focused label and its path suffix
        snprintf(do_merge, sizeof(do_merge), "%d", 0);
        fprintf(fp, "%s %s\n", "INDIVIDUAL", target_value);

        char *runner = lv_obj_get_user_data(e_focused);
        fprintf(fp, "%s %s\n", target_value, runner);
    }
    fclose(fp);

    char backup_script_path[FILENAME_MAX];
    snprintf(backup_script_path, sizeof(backup_script_path), OPT_PATH "script/mux/backup.sh");

    const char *args[] = {backup_script_path, do_merge, NULL};
    size_t exec_count;
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        fade_out_screen();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);

    write_text_to_file(MUOS_DBI_LOAD, "w", INT, current_item_index);

    load_mux("backup");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);

    const int value = lv_dropdown_get_selected(ui_dro_apps_backup);
    set_all_options(value == 0 ? 1 : 0);

    nav_moved = 1;
}

static void handle_help(void) {
    if (msgbox_active || hold_call) return;

    if (progress_onscreen == -1) {
        play_sound(snd_confirm);
        show_help();
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.launch, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.toggle_all, 0},
                                  {NULL, NULL, 0}});

#define BACKUP(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_backup, UDATA);
    BACKUP_ELEMENTS
#undef BACKUP

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, wall_general);
        adjust_gen_panel();

        const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

        if (e_focused == ui_lbl_merge_backup || e_focused == ui_lbl_target_backup) {
            lv_obj_add_flag(ui_lbl_nav_a, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lbl_nav_a_glyph, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui_lbl_nav_a, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lbl_nav_a_glyph, LV_OBJ_FLAG_HIDDEN);
        }
        footer_nav_check_scroll();

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxbackup_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxbackup.title);
    init_muxbackup(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    restore_backup_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_a] = handle_a,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
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
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
