#include "muxshare.h"
#include "ui/ui_muxbackup.h"

#define UI_COUNT 21
#define STORAGE_COUNT UI_COUNT - 2
#define START_BACKUP_INDEX UI_COUNT - 1
#define BACKUP_TARGET_INDEX UI_COUNT - 2

struct backup {
    const char *path_suffix;
    const char *shortname;
    lv_obj_t *ui_label;
};

struct backup backup_path[STORAGE_COUNT];

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused)
{
    struct help_msg help_messages[] = {
        {ui_lblBiosValue_backup, lang.MUXBACKUP.HELP.BIOS},
        {ui_lblCatalogue_backup, lang.MUXBACKUP.HELP.CATALOGUE},
        {ui_lblName_backup, lang.MUXBACKUP.HELP.FRIENDLY},
        {ui_lblRetroArch_backup, lang.MUXBACKUP.HELP.RA_SYSTEM},
        {ui_lblConfig_backup, lang.MUXBACKUP.HELP.RA_CONFIG},
        {ui_lblCore_backup, lang.MUXBACKUP.HELP.ASSIGNED},
        {ui_lblCollection_backup, lang.MUXBACKUP.HELP.COLLECTION},
        {ui_lblHistory_backup, lang.MUXBACKUP.HELP.HISTORY},
        {ui_lblMusic_backup, lang.MUXBACKUP.HELP.MUSIC},
        {ui_lblSave_backup, lang.MUXBACKUP.HELP.SAVE},
        {ui_lblScreenshot_backup, lang.MUXBACKUP.HELP.SCREENSHOT},
        {ui_lblTheme_backup, lang.MUXBACKUP.HELP.PACKAGE.THEME},
        {ui_lblCataloguePackage_backup, lang.MUXBACKUP.HELP.PACKAGE.CATALOGUE},
        {ui_lblConfigPackage_backup, lang.MUXBACKUP.HELP.PACKAGE.RA_CONFIG},
        {ui_lblBootlogoPackage_backup, lang.MUXBACKUP.HELP.PACKAGE.BOOTLOGO},
        {ui_lblLanguage_backup, lang.MUXBACKUP.HELP.LANGUAGE},
        {ui_lblNetwork_backup, lang.MUXBACKUP.HELP.NET_PROFILE},
        {ui_lblSyncthing_backup, lang.MUXBACKUP.HELP.SYNCTHING},
        {ui_lblUserInit_backup, lang.MUXBACKUP.HELP.USER_INIT},
        {ui_lblBackupTarget_backup, lang.MUXBACKUP.HELP.BACKUP_TARGET},
        {ui_lblStartBackup_backup, lang.MUXBACKUP.HELP.START_BACKUP},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void update_backup_info() {
    /*
     * Check for SD2 pathing, otherwise it should be on SD1.
     * If it's not on SD1 then you have bigger problems!
    */
    backup_path[0].path_suffix = STORE_LOC_BIOS;
    backup_path[0].ui_label = ui_lblBiosValue_backup;
    backup_path[0].shortname = "BIOS";

    backup_path[1].path_suffix = STORE_LOC_CLOG;
    backup_path[1].ui_label = ui_lblCatalogueValue_backup;
    backup_path[1].shortname = "Catalogue";

    backup_path[2].path_suffix = STORE_LOC_NAME;
    backup_path[2].ui_label = ui_lblNameValue_backup;
    backup_path[2].shortname = "FriendlyName";

    backup_path[3].path_suffix = STORE_LOC_RARC;
    backup_path[3].ui_label = ui_lblRetroArchValue_backup;
    backup_path[3].shortname = "RetroArch";

    backup_path[4].path_suffix = STORE_LOC_CONF;
    backup_path[4].ui_label = ui_lblConfigValue_backup;
    backup_path[4].shortname = "RAConfig";

    backup_path[5].path_suffix = STORE_LOC_CORE;
    backup_path[5].ui_label = ui_lblCoreValue_backup;
    backup_path[5].shortname = "Core";

    backup_path[6].path_suffix = STORE_LOC_COLL;
    backup_path[6].ui_label = ui_lblCollectionValue_backup;
    backup_path[6].shortname = "Collection";

    backup_path[7].path_suffix = STORE_LOC_HIST;
    backup_path[7].ui_label = ui_lblHistoryValue_backup;
    backup_path[7].shortname = "History";

    backup_path[8].path_suffix = STORE_LOC_MUSC;
    backup_path[8].ui_label = ui_lblMusicValue_backup;
    backup_path[8].shortname = "Music";

    backup_path[9].path_suffix = STORE_LOC_SAVE;
    backup_path[9].ui_label = ui_lblSaveValue_backup;
    backup_path[9].shortname = "Save";

    backup_path[10].path_suffix = STORE_LOC_SCRS;
    backup_path[10].ui_label = ui_lblScreenshotValue_backup;
    backup_path[10].shortname = "Screenshot";

    backup_path[11].path_suffix = STORE_LOC_THEM;
    backup_path[11].ui_label = ui_lblThemeValue_backup;
    backup_path[11].shortname = "Theme";

    backup_path[12].path_suffix = STORE_LOC_PCAT;
    backup_path[12].ui_label = ui_lblCataloguePackageValue_backup;
    backup_path[12].shortname = "CataloguePkg";

    backup_path[13].path_suffix = STORE_LOC_PCON;
    backup_path[13].ui_label = ui_lblConfigPackageValue_backup;
    backup_path[13].shortname = "ConfigPkg";

    backup_path[14].path_suffix = STORE_LOC_PLOG;
    backup_path[14].ui_label = ui_lblBootlogoPackageValue_backup;
    backup_path[14].shortname = "BootlogoPkg";

    backup_path[15].path_suffix = STORE_LOC_LANG;
    backup_path[15].ui_label = ui_lblLanguageValue_backup;
    backup_path[15].shortname = "Language";

    backup_path[16].path_suffix = STORE_LOC_NETW;
    backup_path[16].ui_label = ui_lblNetworkValue_backup;
    backup_path[16].shortname = "Network";

    backup_path[17].path_suffix = STORE_LOC_SYCT;
    backup_path[17].ui_label = ui_lblSyncthingValue_backup;
    backup_path[17].shortname = "Syncthing";

    backup_path[18].path_suffix = STORE_LOC_INIT;
    backup_path[18].ui_label = ui_lblUserInitValue_backup;
    backup_path[18].shortname = "UserInit";

    char dir[FILENAME_MAX];
    for (int i = 0; i < A_SIZE(backup_path); i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.STORAGE.SDCARD.MOUNT, backup_path[i].path_suffix);
        if (directory_exist(dir)) {
            lv_label_set_text(backup_path[i].ui_label, "SD2");
        } else {
            lv_label_set_text(backup_path[i].ui_label, "SD1");
        }
    }

    // Set target to SD1 by default
    lv_label_set_text(ui_lblBackupTargetValue_backup, "SD1");
}

static void init_navigation_group()
{
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, backup, Bios, lang.MUXBACKUP.BIOS, "bios", "");
    INIT_VALUE_ITEM(-1, backup, Catalogue, lang.MUXBACKUP.CATALOGUE, "catalogue", "");
    INIT_VALUE_ITEM(-1, backup, Name, lang.MUXBACKUP.FRIENDLY, "name", "");
    INIT_VALUE_ITEM(-1, backup, RetroArch, lang.MUXBACKUP.RA_SYSTEM, "retroarch", "");
    INIT_VALUE_ITEM(-1, backup, Config, lang.MUXBACKUP.RA_CONFIG, "config", "");
    INIT_VALUE_ITEM(-1, backup, Core, lang.MUXBACKUP.ASSIGNED, "core", "");
    INIT_VALUE_ITEM(-1, backup, Collection, lang.MUXBACKUP.COLLECTION, "collection", "");
    INIT_VALUE_ITEM(-1, backup, History, lang.MUXBACKUP.HISTORY, "history", "");
    INIT_VALUE_ITEM(-1, backup, Music, lang.MUXBACKUP.MUSIC, "music", "");
    INIT_VALUE_ITEM(-1, backup, Save, lang.MUXBACKUP.SAVE, "save", "");
    INIT_VALUE_ITEM(-1, backup, Screenshot, lang.MUXBACKUP.SCREENSHOT, "screenshot", "");
    INIT_VALUE_ITEM(-1, backup, Theme, lang.MUXBACKUP.PACKAGE.THEME, "theme", "");
    INIT_VALUE_ITEM(-1, backup, CataloguePackage, lang.MUXBACKUP.PACKAGE.CATALOGUE, "pack-catalogue", "");
    INIT_VALUE_ITEM(-1, backup, ConfigPackage, lang.MUXBACKUP.PACKAGE.RA_CONFIG, "pack-config", "");
    INIT_VALUE_ITEM(-1, backup, BootlogoPackage, lang.MUXBACKUP.PACKAGE.BOOTLOGO, "pack-bootlogo", "");
    INIT_VALUE_ITEM(-1, backup, Language, lang.MUXBACKUP.LANGUAGE, "language", "");
    INIT_VALUE_ITEM(-1, backup, Network, lang.MUXBACKUP.NET_PROFILE, "network", "");
    INIT_VALUE_ITEM(-1, backup, Syncthing, lang.MUXBACKUP.SYNCTHING, "syncthing", "");
    INIT_VALUE_ITEM(-1, backup, UserInit, lang.MUXBACKUP.USER_INIT, "userinit", "");
    INIT_VALUE_ITEM(-1, backup, BackupTarget, lang.MUXBACKUP.BACKUP_TARGET, "backuptarget", "");
    INIT_VALUE_ITEM(-1, backup, StartBackup, lang.MUXBACKUP.START_BACKUP, "startbackup", "");

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
    if (file_exist(MUOS_DBI_LOAD))
    {
        dbi_index = read_line_int_from(MUOS_DBI_LOAD, 1);
        remove(MUOS_DBI_LOAD);
    }
    if (ui_count > 0 && 
        dbi_index >= 0 && 
        dbi_index < ui_count && 
        current_item_index < ui_count) {
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

static void handle_back(void) {
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

// Fuck you I'm a ukelele
static int get_focused_element_index(struct _lv_obj_t *element_focused)
{
    if (ui_lblStartBackupValue_backup == element_focused)
        return START_BACKUP_INDEX;
    else if (ui_lblBackupTargetValue_backup == element_focused)
        return BACKUP_TARGET_INDEX;
    else
        for (int i = 0; i < STORAGE_COUNT; i++)
            if (backup_path[i].ui_label == element_focused)
                return i;
    
    return -1; // Not found
}

static void handle_confirm(void) {
    if (msgbox_active)
        return;

    play_sound(SND_CONFIRM);

    lv_obj_t* element_focused = lv_group_get_focused(ui_group_value);
    int focused_index = get_focused_element_index(element_focused);
    char *label_value = lv_label_get_text(element_focused);
    char *target_value = lv_label_get_text(ui_lblBackupTargetValue_backup);

    // Return if backup set to NONE or if on Toggle Target Storage
    if (strcasecmp(label_value, "NONE") == 0 || focused_index == BACKUP_TARGET_INDEX)
        return;

    char datetime[64];

    strncpy(datetime, get_datetime(), sizeof(datetime) - 1);
    datetime[sizeof(datetime) - 1] = '\0';

    FILE *fp;
    char backup_script_path[FILENAME_MAX];
    fp = fopen("/tmp/muxbackup_manifest.txt", "w");
    if (!fp) {
        fprintf(stderr, "Failed to open /tmp/muxbackup_manifest.txt for writing\n");
        return;
    }

    // Write for batch backup
    if (focused_index == START_BACKUP_INDEX)
    {
        fprintf(fp, "%s %s\n", "BATCH", target_value);

        for (int i = 0; i < STORAGE_COUNT; i++) {
            label_value = lv_label_get_text(backup_path[i].ui_label);

            // Skip if set to NONE
            if (strcasecmp(label_value, "NONE") == 0) 
                continue; 
            
            fprintf(fp, 
                    "%s %s %s\n", 
                    label_value, 
                    backup_path[i].shortname, 
                    backup_path[i].path_suffix);
        }
    }
    // For other backup paths, write the focused label and its path suffix
    else
    {
        fprintf(fp, "%s %s\n", "INDIVIDUAL", target_value);

        label_value = lv_label_get_text(backup_path[focused_index].ui_label);
        if (strcasecmp(label_value, "NONE") != 0) {
            fprintf(fp, 
                    "%s %s %s\n", 
                    label_value,
                    backup_path[focused_index].shortname, 
                    backup_path[focused_index].path_suffix);
        }
    }
    fclose(fp);

    snprintf(backup_script_path, sizeof(backup_script_path), INTERNAL_PATH "script/mux/backup.sh");

    const char *args[] = {backup_script_path, NULL};
    size_t exec_count;
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0);
    }
    free(exec);

    write_text_to_file(MUOS_DBI_LOAD, "w", INT, current_item_index);

    load_mux("backup");

    close_input();
    mux_input_stop();
}

static void handle_toggle(void) {
    if (msgbox_active)
        return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
    const char *label_text = lv_label_get_text(element_focused);
    int focused_index = get_focused_element_index(element_focused);

    if (focused_index == BACKUP_TARGET_INDEX) {
        play_sound(SND_CONFIRM);

        if (strcasecmp(label_text, "SD2") == 0) {
            lv_label_set_text(element_focused, "USB");
        } else if (strcasecmp(label_text, "SD1") == 0) {
            lv_label_set_text(element_focused, "SD2");
        } else {
            lv_label_set_text(element_focused, "SD1");
        }

        return;
    } else if (focused_index == START_BACKUP_INDEX) {
        // If focused on Start Backup, just return
        return;
    } else {
        play_sound(SND_CONFIRM);

        // Toggle between SD1 and SD2 (or from NONE to SD1)
        if (strcasecmp(label_text, "SD2") == 0) {
            lv_label_set_text(element_focused, "SD1");
        } else if (strcasecmp(label_text, "SD1") == 0) {
            lv_label_set_text(element_focused, "SD2");
        } else if (strcasecmp(label_text, "NONE") == 0) {
            lv_label_set_text(element_focused, "SD1");
        }

        nav_moved = 1;
    }
}

static void handle_clear(void) {
    if (msgbox_active)
        return;

    play_sound(SND_CONFIRM);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
    const char *label_text = lv_label_get_text(element_focused);
    int focused_index = get_focused_element_index(element_focused);

    if (focused_index == BACKUP_TARGET_INDEX || focused_index == START_BACKUP_INDEX) {
        // If focused on Toggle Target or Start Backup, just return
        return;
    } 

    // If SD1 or SD2, set to NONE
    if (strcasecmp(label_text, "SD1") == 0 || strcasecmp(label_text, "SD2") == 0) {
        lv_label_set_text(element_focused, "NONE");
    }

    nav_moved = 1;
}

static void handle_help(void)
{
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]){
        ui_pnlFooter,
        ui_pnlHeader,
        ui_pnlHelp,
        ui_pnlProgressBrightness,
        ui_pnlProgressVolume,
        NULL});
}

static void init_elements() {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]){
        {ui_lblNavBGlyph, "", 0},
        {ui_lblNavB, lang.GENERIC.BACK, 0},
        {ui_lblNavXGlyph, "", 0},
        {ui_lblNavX, lang.GENERIC.CHANGE, 0},
        {ui_lblNavYGlyph, "", 0},
        {ui_lblNavY, lang.GENERIC.CLEAR, 0},
        {ui_lblNavAGlyph, "", 0},
        {ui_lblNavA, lang.GENERIC.LAUNCH, 0},
        {NULL, NULL, 0}});

#define BACKUP(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_backup, UDATA);
    BACKUP_ELEMENTS
#undef BACKUP

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) 
            adjust_wallpaper_element(ui_group, 0, GENERAL);
        
        adjust_panels();

        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
        const char *label_text = lv_label_get_text(element_focused);
        int focused_index = get_focused_element_index(element_focused);

        // Focused on Start Backup label
        if (focused_index == START_BACKUP_INDEX) {
            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        // Focused on Toggle Target label
        } else if (focused_index == BACKUP_TARGET_INDEX) {
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
            }
            else {
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

int muxbackup_main() {
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
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_toggle,
                    [MUX_INPUT_Y] = handle_clear,
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
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

    return 0;
}
