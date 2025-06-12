#include "muxshare.h"
#include "ui/ui_muxstorage.h"

#define UI_COUNT 19

struct storage {
    const char *path_suffix;
    lv_obj_t *ui_label;
};

struct storage storage_path[UI_COUNT];

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBios_storage,             lang.MUXSTORAGE.HELP.BIOS},
            {ui_lblCatalogue_storage,        lang.MUXSTORAGE.HELP.CATALOGUE},
            {ui_lblName_storage,             lang.MUXSTORAGE.HELP.FRIENDLY},
            {ui_lblRetroArch_storage,        lang.MUXSTORAGE.HELP.RA_SYSTEM},
            {ui_lblConfig_storage,           lang.MUXSTORAGE.HELP.RA_CONFIG},
            {ui_lblCore_storage,             lang.MUXSTORAGE.HELP.ASSIGNED},
            {ui_lblCollection_storage,       lang.MUXSTORAGE.HELP.COLLECTION},
            {ui_lblHistory_storage,          lang.MUXSTORAGE.HELP.HISTORY},
            {ui_lblMusic_storage,            lang.MUXSTORAGE.HELP.MUSIC},
            {ui_lblSave_storage,             lang.MUXSTORAGE.HELP.SAVE},
            {ui_lblScreenshot_storage,       lang.MUXSTORAGE.HELP.SCREENSHOT},
            {ui_lblTheme_storage,            lang.MUXSTORAGE.HELP.PACKAGE.THEME},
            {ui_lblCataloguePackage_storage, lang.MUXSTORAGE.HELP.PACKAGE.CATALOGUE},
            {ui_lblConfigPackage_storage,    lang.MUXSTORAGE.HELP.PACKAGE.RA_CONFIG},
            {ui_lblBootlogoPackage_storage,  lang.MUXSTORAGE.HELP.PACKAGE.BOOTLOGO},
            {ui_lblLanguage_storage,         lang.MUXSTORAGE.HELP.LANGUAGE},
            {ui_lblNetwork_storage,          lang.MUXSTORAGE.HELP.NET_PROFILE},
            {ui_lblSyncthing_storage,        lang.MUXSTORAGE.HELP.SYNCTHING},
            {ui_lblUserInit_storage,         lang.MUXSTORAGE.HELP.USER_INIT},
    };

    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);
    gen_help(element_focused, help_messages, num_messages);
}

static void update_storage_info() {
    /*
     * Check for SD2 pathing, otherwise it should be on SD1.
     * If it's not on SD1 then you have bigger problems!
    */
    storage_path[0].path_suffix = STORE_LOC_BIOS;
    storage_path[0].ui_label = ui_lblBiosValue_storage;

    storage_path[1].path_suffix = STORE_LOC_CLOG;
    storage_path[1].ui_label = ui_lblCatalogueValue_storage;

    storage_path[2].path_suffix = STORE_LOC_NAME;
    storage_path[2].ui_label = ui_lblNameValue_storage;

    storage_path[3].path_suffix = STORE_LOC_RARC;
    storage_path[3].ui_label = ui_lblRetroArchValue_storage;

    storage_path[4].path_suffix = STORE_LOC_CONF;
    storage_path[4].ui_label = ui_lblConfigValue_storage;

    storage_path[5].path_suffix = STORE_LOC_CORE;
    storage_path[5].ui_label = ui_lblCoreValue_storage;

    storage_path[6].path_suffix = STORE_LOC_COLL;
    storage_path[6].ui_label = ui_lblCollectionValue_storage;

    storage_path[7].path_suffix = STORE_LOC_HIST;
    storage_path[7].ui_label = ui_lblHistoryValue_storage;

    storage_path[8].path_suffix = STORE_LOC_MUSC;
    storage_path[8].ui_label = ui_lblMusicValue_storage;

    storage_path[9].path_suffix = STORE_LOC_SAVE;
    storage_path[9].ui_label = ui_lblSaveValue_storage;

    storage_path[10].path_suffix = STORE_LOC_SCRS;
    storage_path[10].ui_label = ui_lblScreenshotValue_storage;

    storage_path[11].path_suffix = STORE_LOC_THEM;
    storage_path[11].ui_label = ui_lblThemeValue_storage;

    storage_path[12].path_suffix = STORE_LOC_PCAT;
    storage_path[12].ui_label = ui_lblCataloguePackageValue_storage;

    storage_path[13].path_suffix = STORE_LOC_PCON;
    storage_path[13].ui_label = ui_lblConfigPackageValue_storage;

    storage_path[14].path_suffix = STORE_LOC_PLOG;
    storage_path[14].ui_label = ui_lblBootlogoPackageValue_storage;

    storage_path[15].path_suffix = STORE_LOC_LANG;
    storage_path[15].ui_label = ui_lblLanguageValue_storage;

    storage_path[16].path_suffix = STORE_LOC_NETW;
    storage_path[16].ui_label = ui_lblNetworkValue_storage;

    storage_path[17].path_suffix = STORE_LOC_SYCT;
    storage_path[17].ui_label = ui_lblSyncthingValue_storage;

    storage_path[18].path_suffix = STORE_LOC_INIT;
    storage_path[18].ui_label = ui_lblUserInitValue_storage;

    char dir[FILENAME_MAX];
    for (int i = 0; i < sizeof(storage_path) / sizeof(storage_path[0]); i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.STORAGE.SDCARD.MOUNT, storage_path[i].path_suffix);
        if (directory_exist(dir)) {
            lv_label_set_text(storage_path[i].ui_label, "SD2");
            lv_label_set_text(ui_lblNavX, lang.GENERIC.SYNC);
        } else {
            lv_label_set_text(storage_path[i].ui_label, "SD1");
            lv_label_set_text(ui_lblNavX, lang.GENERIC.MIGRATE);
        }
    }
}

static void init_navigation_group() {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, storage, Bios, lang.MUXSTORAGE.BIOS, "bios", "");
    INIT_VALUE_ITEM(-1, storage, Catalogue, lang.MUXSTORAGE.CATALOGUE, "catalogue", "");
    INIT_VALUE_ITEM(-1, storage, Name, lang.MUXSTORAGE.FRIENDLY, "name", "");
    INIT_VALUE_ITEM(-1, storage, RetroArch, lang.MUXSTORAGE.RA_SYSTEM, "retroarch", "");
    INIT_VALUE_ITEM(-1, storage, Config, lang.MUXSTORAGE.RA_CONFIG, "config", "");
    INIT_VALUE_ITEM(-1, storage, Core, lang.MUXSTORAGE.ASSIGNED, "core", "");
    INIT_VALUE_ITEM(-1, storage, Collection, lang.MUXSTORAGE.COLLECTION, "collection", "");
    INIT_VALUE_ITEM(-1, storage, History, lang.MUXSTORAGE.HISTORY, "history", "");
    INIT_VALUE_ITEM(-1, storage, Music, lang.MUXSTORAGE.MUSIC, "music", "");
    INIT_VALUE_ITEM(-1, storage, Save, lang.MUXSTORAGE.SAVE, "save", "");
    INIT_VALUE_ITEM(-1, storage, Screenshot, lang.MUXSTORAGE.SCREENSHOT, "screenshot", "");
    INIT_VALUE_ITEM(-1, storage, Theme, lang.MUXSTORAGE.PACKAGE.THEME, "theme", "");
    INIT_VALUE_ITEM(-1, storage, CataloguePackage, lang.MUXSTORAGE.PACKAGE.CATALOGUE, "pack-catalogue", "");
    INIT_VALUE_ITEM(-1, storage, ConfigPackage, lang.MUXSTORAGE.PACKAGE.RA_CONFIG, "pack-config", "");
    INIT_VALUE_ITEM(-1, storage, BootlogoPackage, lang.MUXSTORAGE.PACKAGE.BOOTLOGO, "pack-bootlogo", "");
    INIT_VALUE_ITEM(-1, storage, Language, lang.MUXSTORAGE.LANGUAGE, "language", "");
    INIT_VALUE_ITEM(-1, storage, Network, lang.MUXSTORAGE.NET_PROFILE, "network", "");
    INIT_VALUE_ITEM(-1, storage, Syncthing, lang.MUXSTORAGE.SYNCTHING, "syncthing", "");
    INIT_VALUE_ITEM(-1, storage, UserInit, lang.MUXSTORAGE.USER_INIT, "userinit", "");

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

    int sin_index = 0;
    if (file_exist(MUOS_SIN_LOAD)) {
        sin_index = read_line_int_from(MUOS_SIN_LOAD, 1);
        remove(MUOS_SIN_LOAD);
    }

    if (ui_count > 0 && sin_index >= 0 && sin_index < ui_count && current_item_index < ui_count) {
        list_nav_move(sin_index, 1);
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
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "storage");

    close_input();
    mux_input_stop();
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    play_sound(SND_CONFIRM);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);

    static char storage_script[MAX_BUFFER_SIZE];
    if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/sync.sh", INTERNAL_PATH);
    } else {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/migrate.sh", INTERNAL_PATH);
    }

    size_t exec_count;
    const char *args[] = {storage_script, storage_path[current_item_index].path_suffix, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0);
    }
    free(exec);

    write_text_to_file(MUOS_SIN_LOAD, "w", INT, current_item_index);

    load_mux("storage");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
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
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {ui_lblNavXGlyph, "",                0},
            {ui_lblNavX,      "",                0},
            {NULL, NULL,                         0}
    });

#define STORAGE(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_storage, UDATA);
    STORAGE_ELEMENTS
#undef STORAGE

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
        if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
            lv_label_set_text(ui_lblNavX, lang.GENERIC.SYNC);
        } else {
            lv_label_set_text(ui_lblNavX, lang.GENERIC.MIGRATE);
        }

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxstorage_main() {
    init_module("muxstorage");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSTORAGE.TITLE);
    init_muxstorage(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    update_storage_info();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_confirm,
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
