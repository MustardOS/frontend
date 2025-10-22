#include "muxshare.h"
#include "ui/ui_muxbackup.h"

#define UI_COUNT 23
#define STORAGE_COUNT (UI_COUNT - 3)
#define START_INDEX (UI_COUNT - 1)
#define MERGE_INDEX (UI_COUNT - 2)
#define TARGET_INDEX (UI_COUNT - 3)

int merge_archives = 1;

struct backup {
    lv_obj_t *ui_label;
    const char *archive_runner;
};

struct backup backup_path[STORAGE_COUNT];

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAppsValue_backup,  lang.MUXBACKUP.HELP.APPS},
            {ui_lblBiosValue_backup,  lang.MUXBACKUP.HELP.BIOS},
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

static inline void add_backup(int *bp, lv_obj_t *label, const char *runner) {
    backup_path[*bp].ui_label = label;
    backup_path[*bp].archive_runner = runner;
    (*bp)++;
}

static void update_backup_info(void) {
    int bp = 0;

    add_backup(&bp, ui_lblAppsValue_backup, "application");
    add_backup(&bp, ui_lblBiosValue_backup, "bios");
    add_backup(&bp, ui_lblCatalogueValue_backup, "catalogue");
    add_backup(&bp, ui_lblCheatsValue_backup, "cheats");
    add_backup(&bp, ui_lblCollectionValue_backup, "collection");
    add_backup(&bp, ui_lblConfigValue_backup, "config");
    add_backup(&bp, ui_lblHistoryValue_backup, "history");
    add_backup(&bp, ui_lblInitValue_backup, "init");
    add_backup(&bp, ui_lblMusicValue_backup, "music");
    add_backup(&bp, ui_lblNameValue_backup, "name");
    add_backup(&bp, ui_lblNetworkValue_backup, "network");
    add_backup(&bp, ui_lblOverlaysValue_backup, "overlays");
    add_backup(&bp, ui_lblOverrideValue_backup, "override");
    add_backup(&bp, ui_lblPackageValue_backup, "package");
    add_backup(&bp, ui_lblSaveValue_backup, "save");
    add_backup(&bp, ui_lblScreenshotValue_backup, "screenshot");
    add_backup(&bp, ui_lblShadersValue_backup, "shaders");
    add_backup(&bp, ui_lblSyncthingValue_backup, "syncthing");
    add_backup(&bp, ui_lblThemeValue_backup, "theme");
    add_backup(&bp, ui_lblTrackValue_backup, "track");

    const char *sd = is_partition_mounted(device.STORAGE.SDCARD.MOUNT) ? "SD2" : "SD1";
    for (int i = 0; i < bp; i++) lv_label_set_text(backup_path[i].ui_label, sd);

    // Set target to SD1 by default
    lv_label_set_text(ui_lblTargetValue_backup, "SD1");
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, backup, Apps, lang.MUXBACKUP.APPS, "application", "");
    INIT_VALUE_ITEM(-1, backup, Bios, lang.MUXBACKUP.BIOS, "bios", "");
    INIT_VALUE_ITEM(-1, backup, Catalogue, lang.MUXBACKUP.CATALOGUE, "catalogue", "");
    INIT_VALUE_ITEM(-1, backup, Cheats, lang.MUXBACKUP.CHEATS, "cheats", "");
    INIT_VALUE_ITEM(-1, backup, Collection, lang.MUXBACKUP.COLLECTION, "collection", "");
    INIT_VALUE_ITEM(-1, backup, Config, lang.MUXBACKUP.CONFIG, "config", "");
    INIT_VALUE_ITEM(-1, backup, History, lang.MUXBACKUP.HISTORY, "history", "");
    INIT_VALUE_ITEM(-1, backup, Init, lang.MUXBACKUP.INIT, "init", "");
    INIT_VALUE_ITEM(-1, backup, Music, lang.MUXBACKUP.MUSIC, "music", "");
    INIT_VALUE_ITEM(-1, backup, Name, lang.MUXBACKUP.NAME, "name", "");
    INIT_VALUE_ITEM(-1, backup, Network, lang.MUXBACKUP.NETWORK, "network", "");
    INIT_VALUE_ITEM(-1, backup, Overlays, lang.MUXBACKUP.OVERLAYS, "overlays", "");
    INIT_VALUE_ITEM(-1, backup, Override, lang.MUXBACKUP.OVERRIDE, "override", "");
    INIT_VALUE_ITEM(-1, backup, Package, lang.MUXBACKUP.PACKAGE, "package", "");
    INIT_VALUE_ITEM(-1, backup, Save, lang.MUXBACKUP.SAVE, "save", "");
    INIT_VALUE_ITEM(-1, backup, Screenshot, lang.MUXBACKUP.SCREENSHOT, "screenshot", "");
    INIT_VALUE_ITEM(-1, backup, Shaders, lang.MUXBACKUP.SHADERS, "shaders", "");
    INIT_VALUE_ITEM(-1, backup, Syncthing, lang.MUXBACKUP.SYNCTHING, "syncthing", "");
    INIT_VALUE_ITEM(-1, backup, Theme, lang.MUXBACKUP.THEME, "theme", "");
    INIT_VALUE_ITEM(-1, backup, Track, lang.MUXBACKUP.TRACK, "track", "");
    INIT_VALUE_ITEM(-1, backup, Target, lang.MUXBACKUP.TARGET, "target", "");
    INIT_VALUE_ITEM(-1, backup, Merge, lang.MUXBACKUP.MERGE, "merge", lang.GENERIC.ENABLED);
    INIT_VALUE_ITEM(-1, backup, Start, lang.MUXBACKUP.START, "start", "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }

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
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);

    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
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

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "backup");

    close_input();
    mux_input_stop();
}

// Fuck you I'm a ukulele
static int get_focused_element_index(struct _lv_obj_t *element_focused) {
    if (ui_lblStartValue_backup == element_focused) {
        return START_INDEX;
    } else if (ui_lblTargetValue_backup == element_focused) {
        return TARGET_INDEX;
    } else if (ui_lblMergeValue_backup == element_focused) {
        return MERGE_INDEX;
    } else
        for (int i = 0; i < STORAGE_COUNT; i++) {
            if (backup_path[i].ui_label == element_focused) return i;
        }

    return -1; // Not found
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
    int focused_index = get_focused_element_index(element_focused);
    const char *label_value = lv_label_get_text(element_focused);

    // Return if backup set to NONE or if on Toggle Target Storage
    if (strcasecmp(label_value, "NONE") == 0 ||
        focused_index == MERGE_INDEX ||
        focused_index == TARGET_INDEX)
        return;

    play_sound(SND_CONFIRM);

    const char *target_value = lv_label_get_text(ui_lblTargetValue_backup);
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
    if (focused_index == START_INDEX) {
        fprintf(fp, "%s %s\n", "BATCH", target_value);

        for (int i = 0; i < STORAGE_COUNT; i++) {
            label_value = lv_label_get_text(backup_path[i].ui_label);

            // Skip if set to NONE
            if (strcasecmp(label_value, "NONE") == 0) continue;
            fprintf(fp, "%s %s\n", label_value, backup_path[i].archive_runner);
        }
    } else { // For other backup paths, write the focused label and its path suffix
        fprintf(fp, "%s %s\n", "INDIVIDUAL", target_value);

        const char *label_value = lv_label_get_text(backup_path[focused_index].ui_label);
        if (strcasecmp(label_value, "NONE") != 0) {
            fprintf(fp, "%s %s\n", label_value, backup_path[focused_index].archive_runner);
        }
    }
    fclose(fp);

    char backup_script_path[FILENAME_MAX];
    snprintf(backup_script_path, sizeof(backup_script_path), OPT_PATH "script/mux/backup.sh");

    char do_merge[4];
    sprintf(do_merge, "%d", merge_archives);

    const char *args[] = {backup_script_path, do_merge, NULL};
    size_t exec_count;
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0, 1, NULL);
    }
    free(exec);

    write_text_to_file(MUOS_DBI_LOAD, "w", INT, current_item_index);

    load_mux("backup");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
    const char *label_text = lv_label_get_text(element_focused);
    int focused_index = get_focused_element_index(element_focused);

    if (focused_index == TARGET_INDEX) {
        play_sound(SND_CONFIRM);

        if (strcasecmp(label_text, "SD2") == 0 && is_partition_mounted(device.STORAGE.USB.MOUNT)) {
            lv_label_set_text(element_focused, "USB");
        } else if (strcasecmp(label_text, "SD1") == 0 && is_partition_mounted(device.STORAGE.SDCARD.MOUNT)) {
            lv_label_set_text(element_focused, "SD2");
        } else {
            lv_label_set_text(element_focused, "SD1");
        }

        return;
    } else if (focused_index == MERGE_INDEX) {
        play_sound(SND_CONFIRM);

        if (strcasecmp(label_text, lang.GENERIC.ENABLED) == 0) {
            merge_archives = 0;
            lv_label_set_text(element_focused, lang.GENERIC.DISABLED);
        } else {
            merge_archives = 1;
            lv_label_set_text(element_focused, lang.GENERIC.ENABLED);
        }

        return;
    } else if (focused_index == START_INDEX) {
        // If focused on Start Backup, just return
        return;
    } else {
        play_sound(SND_CONFIRM);

        // Toggle between SD1 and SD2 (or from NONE to SD1)
        if (strcasecmp(label_text, "SD2") == 0) {
            lv_label_set_text(element_focused, "SD1");
        } else if (strcasecmp(label_text, "SD1") == 0
                   && is_partition_mounted(device.STORAGE.SDCARD.MOUNT)) {
            lv_label_set_text(element_focused, "SD2");
        } else if (strcasecmp(label_text, "NONE") == 0) {
            lv_label_set_text(element_focused, "SD1");
        }

        nav_moved = 1;
    }
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);

    int focused_index = get_focused_element_index(element_focused);
    if (focused_index == TARGET_INDEX || focused_index == START_INDEX) return;

    play_sound(SND_CONFIRM);

    // If SD1 or SD2, set to NONE
    const char *label_text = lv_label_get_text(element_focused);
    if (strcasecmp(label_text, "SD1") == 0
        || strcasecmp(label_text, "SD2") == 0
        || strcasecmp(label_text, "CUSTOM") == 0) {
        lv_label_set_text(element_focused, "NONE");
    }

    nav_moved = 1;
}

static void handle_help(void) {
    if (msgbox_active || hold_call) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
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
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  0},
            {ui_lblNavX,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavYGlyph, "",                  0},
            {ui_lblNavY,      lang.GENERIC.CLEAR,  0},
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.LAUNCH, 0},
            {NULL,            NULL,                0}
    });

#define BACKUP(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_backup, UDATA);
    BACKUP_ELEMENTS
#undef BACKUP

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
        const char *label_text = lv_label_get_text(element_focused);
        int focused_index = get_focused_element_index(element_focused);

        // Focused on Start Backup label
        if (focused_index == START_INDEX) {
            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
            // Focused on Toggle Target label
        } else if (focused_index == MERGE_INDEX || focused_index == TARGET_INDEX) {
            lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
            // Focused on any other backup path label
        } else {
            lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);

            // Backup path label has "NONE" text
            if (strcasecmp(label_text, "NONE") == 0) {
                lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
            }
        }

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxbackup_main(void) {
    init_module("muxbackup");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXBACKUP.TITLE);
    init_muxbackup(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    update_backup_info();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_A] = handle_a,
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

    return 0;
}
