#include "muxshare.h"
#include "ui/ui_muxbackup.h"

#define UI_COUNT 23
#define STORAGE_COUNT (UI_COUNT - 3)

#define BACKUP(NAME, UDATA) static int NAME##_original;
BACKUP_ELEMENTS
#undef BACKUP

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblApps_backup,       lang.MUXBACKUP.HELP.APPS},
            {ui_lblBios_backup,       lang.MUXBACKUP.HELP.BIOS},
            {ui_lblCatalogue_backup,  lang.MUXBACKUP.HELP.CATALOGUE},
            {ui_lblCheats_backup,     lang.MUXBACKUP.HELP.CHEATS},
            {ui_lblCollection_backup, lang.MUXBACKUP.HELP.COLLECTION},
            {ui_lblConfig_backup,     lang.MUXBACKUP.HELP.CONFIG},
            {ui_lblHistory_backup,    lang.MUXBACKUP.HELP.HISTORY},
            {ui_lblInit_backup,       lang.MUXBACKUP.HELP.INIT},
            {ui_lblMusic_backup,      lang.MUXBACKUP.HELP.MUSIC},
            {ui_lblName_backup,       lang.MUXBACKUP.HELP.NAME},
            {ui_lblNetwork_backup,    lang.MUXBACKUP.HELP.NETWORK},
            {ui_lblOverlays_backup,   lang.MUXBACKUP.HELP.OVERLAYS},
            {ui_lblOverride_backup,   lang.MUXBACKUP.HELP.OVERRIDE},
            {ui_lblPackage_backup,    lang.MUXBACKUP.HELP.PACKAGE},
            {ui_lblSave_backup,       lang.MUXBACKUP.HELP.SAVE},
            {ui_lblScreenshot_backup, lang.MUXBACKUP.HELP.SCREENSHOT},
            {ui_lblShaders_backup,    lang.MUXBACKUP.HELP.SHADERS},
            {ui_lblSyncthing_backup,  lang.MUXBACKUP.HELP.SYNCTHING},
            {ui_lblTheme_backup,      lang.MUXBACKUP.HELP.THEME},
            {ui_lblTrack_backup,      lang.MUXBACKUP.HELP.TRACK},
            {ui_lblTarget_backup,     lang.MUXBACKUP.HELP.TARGET},
            {ui_lblMerge_backup,      lang.MUXBACKUP.HELP.MERGE},
            {ui_lblStart_backup,      lang.MUXBACKUP.HELP.START},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define BACKUP(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_backup);
    BACKUP_ELEMENTS
#undef BACKUP
}

static void set_all_options(int value) {
    lv_dropdown_set_selected(ui_droApps_backup, value);
    lv_dropdown_set_selected(ui_droBios_backup, value);
    lv_dropdown_set_selected(ui_droCatalogue_backup, value);
    lv_dropdown_set_selected(ui_droCheats_backup, value);
    lv_dropdown_set_selected(ui_droCollection_backup, value);
    lv_dropdown_set_selected(ui_droConfig_backup, value);
    lv_dropdown_set_selected(ui_droHistory_backup, value);
    lv_dropdown_set_selected(ui_droInit_backup, value);
    lv_dropdown_set_selected(ui_droMusic_backup, value);
    lv_dropdown_set_selected(ui_droName_backup, value);
    lv_dropdown_set_selected(ui_droNetwork_backup, value);
    lv_dropdown_set_selected(ui_droOverlays_backup, value);
    lv_dropdown_set_selected(ui_droOverride_backup, value);
    lv_dropdown_set_selected(ui_droPackage_backup, value);
    lv_dropdown_set_selected(ui_droSave_backup, value);
    lv_dropdown_set_selected(ui_droScreenshot_backup, value);
    lv_dropdown_set_selected(ui_droShaders_backup, value);
    lv_dropdown_set_selected(ui_droSyncthing_backup, value);
    lv_dropdown_set_selected(ui_droTheme_backup, value);
    lv_dropdown_set_selected(ui_droTrack_backup, value);
}

static void restore_backup_options(void) {
    lv_dropdown_set_selected(ui_droApps_backup, config.BACKUP.APPS);
    lv_dropdown_set_selected(ui_droBios_backup, config.BACKUP.BIOS);
    lv_dropdown_set_selected(ui_droCatalogue_backup, config.BACKUP.CATALOGUE);
    lv_dropdown_set_selected(ui_droCheats_backup, config.BACKUP.CHEATS);
    lv_dropdown_set_selected(ui_droCollection_backup, config.BACKUP.COLLECTION);
    lv_dropdown_set_selected(ui_droConfig_backup, config.BACKUP.CONFIG);
    lv_dropdown_set_selected(ui_droHistory_backup, config.BACKUP.HISTORY);
    lv_dropdown_set_selected(ui_droInit_backup, config.BACKUP.INIT);
    lv_dropdown_set_selected(ui_droMerge_backup, config.BACKUP.MERGE);
    lv_dropdown_set_selected(ui_droMusic_backup, config.BACKUP.MUSIC);
    lv_dropdown_set_selected(ui_droName_backup, config.BACKUP.NAME);
    lv_dropdown_set_selected(ui_droNetwork_backup, config.BACKUP.NETWORK);
    lv_dropdown_set_selected(ui_droOverlays_backup, config.BACKUP.OVERLAYS);
    lv_dropdown_set_selected(ui_droOverride_backup, config.BACKUP.OVERRIDE);
    lv_dropdown_set_selected(ui_droPackage_backup, config.BACKUP.PACKAGE);
    lv_dropdown_set_selected(ui_droSave_backup, config.BACKUP.SAVE);
    lv_dropdown_set_selected(ui_droScreenshot_backup, config.BACKUP.SCREENSHOT);
    lv_dropdown_set_selected(ui_droShaders_backup, config.BACKUP.SHADERS);
    lv_dropdown_set_selected(ui_droSyncthing_backup, config.BACKUP.SYNCTHING);
    lv_dropdown_set_selected(ui_droTheme_backup, config.BACKUP.THEME);
    lv_dropdown_set_selected(ui_droTrack_backup, config.BACKUP.TRACK);

    lv_dropdown_set_selected(ui_droTarget_backup, lv_dropdown_get_option_cnt(ui_droTarget_backup) - 1);
}

static void save_backup_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(backup, Apps, "backup/application", INT, 0);
    CHECK_AND_SAVE_STD(backup, Bios, "backup/bios", INT, 0);
    CHECK_AND_SAVE_STD(backup, Catalogue, "backup/catalogue", INT, 0);
    CHECK_AND_SAVE_STD(backup, Cheats, "backup/cheats", INT, 0);
    CHECK_AND_SAVE_STD(backup, Collection, "backup/collection", INT, 0);
    CHECK_AND_SAVE_STD(backup, Config, "backup/config", INT, 0);
    CHECK_AND_SAVE_STD(backup, History, "backup/history", INT, 0);
    CHECK_AND_SAVE_STD(backup, Init, "backup/init", INT, 0);
    CHECK_AND_SAVE_STD(backup, Merge, "backup/merge", INT, 0);
    CHECK_AND_SAVE_STD(backup, Music, "backup/music", INT, 0);
    CHECK_AND_SAVE_STD(backup, Name, "backup/name", INT, 0);
    CHECK_AND_SAVE_STD(backup, Network, "backup/network", INT, 0);
    CHECK_AND_SAVE_STD(backup, Overlays, "backup/overlays", INT, 0);
    CHECK_AND_SAVE_STD(backup, Override, "backup/override", INT, 0);
    CHECK_AND_SAVE_STD(backup, Package, "backup/package", INT, 0);
    CHECK_AND_SAVE_STD(backup, Save, "backup/save", INT, 0);
    CHECK_AND_SAVE_STD(backup, Screenshot, "backup/screenshot", INT, 0);
    CHECK_AND_SAVE_STD(backup, Shaders, "backup/shaders", INT, 0);
    CHECK_AND_SAVE_STD(backup, Syncthing, "backup/syncthing", INT, 0);
    CHECK_AND_SAVE_STD(backup, Theme, "backup/theme", INT, 0);
    CHECK_AND_SAVE_STD(backup, Track, "backup/track", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, backup, Track, lang.MUXBACKUP.TRACK, "track", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Apps, lang.MUXBACKUP.APPS, "application", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Music, lang.MUXBACKUP.MUSIC, "music", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Collection, lang.MUXBACKUP.COLLECTION, "collection", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Override, lang.MUXBACKUP.OVERRIDE, "override", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Package, lang.MUXBACKUP.PACKAGE, "package", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Name, lang.MUXBACKUP.NAME, "name", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, History, lang.MUXBACKUP.HISTORY, "history", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Catalogue, lang.MUXBACKUP.CATALOGUE, "catalogue", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Network, lang.MUXBACKUP.NETWORK, "network", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Cheats, lang.MUXBACKUP.CHEATS, "cheats", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Config, lang.MUXBACKUP.CONFIG, "config", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Overlays, lang.MUXBACKUP.OVERLAYS, "overlays", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Shaders, lang.MUXBACKUP.SHADERS, "shaders", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Save, lang.MUXBACKUP.SAVE, "save", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Screenshot, lang.MUXBACKUP.SCREENSHOT, "screenshot", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Syncthing, lang.MUXBACKUP.SYNCTHING, "syncthing", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Bios, lang.MUXBACKUP.BIOS, "bios", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Theme, lang.MUXBACKUP.THEME, "theme", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Init, lang.MUXBACKUP.INIT, "init", excluded_included, 2);
    INIT_OPTION_ITEM(-1, backup, Target, lang.MUXBACKUP.TARGET, "target", NULL, 0);
    INIT_OPTION_ITEM(-1, backup, Merge, lang.MUXBACKUP.MERGE, "merge", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, backup, Start, lang.MUXBACKUP.START, "start", NULL, 0);

    lv_dropdown_clear_options(ui_droTarget_backup);
    lv_dropdown_add_option(ui_droTarget_backup, "SD1", LV_DROPDOWN_POS_LAST);

    if (is_partition_mounted(device.STORAGE.SDCARD.MOUNT)) {
        lv_dropdown_add_option(ui_droTarget_backup, "SD2", LV_DROPDOWN_POS_LAST);
    }

    if (is_partition_mounted(device.STORAGE.USB.MOUNT)) {
        lv_dropdown_add_option(ui_droTarget_backup, "USB", LV_DROPDOWN_POS_LAST);
    }

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    int dbi_index = 0;
    if (file_exist(MUOS_DBI_LOAD)) {
        dbi_index = read_line_int_from(MUOS_DBI_LOAD, 1);
        remove(MUOS_DBI_LOAD);
    }

    if (ui_count > 0 && dbi_index >= 0 && dbi_index < ui_count && current_item_index < ui_count) {
        list_nav_move(dbi_index, 1);
    }
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
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
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    save_backup_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "backup");

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    // Return if backup set to Excluded or if on Toggle Target Storage
    if (element_focused == ui_lblMerge_backup ||
        element_focused == ui_lblTarget_backup ||
        (element_focused != ui_lblStart_backup && lv_dropdown_get_selected(lv_group_get_focused(ui_group_value)) == 0))
        return;

    play_sound(SND_CONFIRM);

    save_backup_options();

    char target_value[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_droTarget_backup, target_value, sizeof(target_value));
    char datetime[64];

    strncpy(datetime, get_datetime(), sizeof(datetime) - 1);
    datetime[sizeof(datetime) - 1] = '\0';

    FILE *fp;
    fp = fopen("/tmp/muxbackup_manifest.txt", "w");
    if (!fp) {
        fprintf(stderr, "Failed to open /tmp/muxbackup_manifest.txt for writing\n");
        return;
    }

    // Write for batch backup
    char do_merge[4];
    if (element_focused == ui_lblStart_backup) {
        sprintf(do_merge, "%d", lv_dropdown_get_selected(ui_droMerge_backup));

        fprintf(fp, "%s %s\n", "BATCH", target_value);

        for (int i = 0; i < STORAGE_COUNT; i++) {
            lv_obj_t *ui_pnlItem = lv_obj_get_child(ui_pnlContent, i);
            lv_obj_t *ui_lblItem = lv_obj_get_child(ui_pnlItem, 0);
            lv_obj_t *ui_droItem = lv_obj_get_child(ui_pnlItem, 2);
            int value = lv_dropdown_get_selected(ui_droItem);
            // Skip if set to NONE
            if (value == 0) continue;
            char *runner = lv_obj_get_user_data(ui_lblItem);
            fprintf(fp, "%s %s\n", target_value, runner);
        }
    } else { // For other backup paths, write the focused label and its path suffix
        sprintf(do_merge, "%d", 0);
        fprintf(fp, "%s %s\n", "INDIVIDUAL", target_value);
        char *runner = lv_obj_get_user_data(element_focused);
        fprintf(fp, "%s %s\n", target_value, runner);
    }
    fclose(fp);

    char backup_script_path[FILENAME_MAX];
    snprintf(backup_script_path, sizeof(backup_script_path), OPT_PATH "script/mux/backup.sh");

    const char *args[] = {backup_script_path, do_merge, NULL};
    size_t exec_count;
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);

    write_text_to_file(MUOS_DBI_LOAD, "w", INT, current_item_index);

    load_mux("backup");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);

    int value = lv_dropdown_get_selected(ui_droApps_backup);
    set_all_options(value == 0 ? 1 : 0);

    nav_moved = 1;
}

static void handle_help(void) {
    if (msgbox_active || hold_call) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                      0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE,     0},
            {ui_lblNavAGlyph,  "",                      0},
            {ui_lblNavA,       lang.GENERIC.LAUNCH,     0},
            {ui_lblNavBGlyph,  "",                      0},
            {ui_lblNavB,       lang.GENERIC.BACK,       0},
            {ui_lblNavXGlyph,  "",                      0},
            {ui_lblNavX,       lang.GENERIC.TOGGLE_ALL, 0},
            {NULL, NULL,                                0}
    });

#define BACKUP(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_backup, UDATA);
    BACKUP_ELEMENTS
#undef BACKUP

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_gen_panel();

        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

        if (element_focused == ui_lblMerge_backup || element_focused == ui_lblTarget_backup) {
            lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxbackup_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXBACKUP.TITLE);
    init_muxbackup(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    restore_backup_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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

    return 0;
}
