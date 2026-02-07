#include "muxshare.h"
#include "ui/ui_muxstorage.h"

#define UI_COUNT 15

struct storage {
    const char *path_suffix;
    lv_obj_t *ui_label;
};

struct storage storage_path[UI_COUNT];

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    struct help_msg help_messages[] = {
#define STORAGE(NAME, ENUM, UDATA) { lang.MUXSTORAGE.HELP.ENUM },
            STORAGE_ELEMENTS
#undef STORAGE
    };

    gen_help(current_item_index, UI_COUNT, help_messages, ui_group, items);
}

static inline void add_storage(int *sp, const char *suffix, lv_obj_t *label) {
    storage_path[*sp].path_suffix = suffix;
    storage_path[*sp].ui_label = label;
    (*sp)++;
}

static void update_storage_info(void) {
    int sp = 0;

    add_storage(&sp, STORE_LOC_APPS, ui_lblAppsValue_storage);
    add_storage(&sp, STORE_LOC_BIOS, ui_lblBiosValue_storage);
    add_storage(&sp, STORE_LOC_CLOG, ui_lblCatalogueValue_storage);
    add_storage(&sp, STORE_LOC_COLL, ui_lblCollectionValue_storage);
    add_storage(&sp, STORE_LOC_HIST, ui_lblHistoryValue_storage);
    add_storage(&sp, STORE_LOC_INIT, ui_lblInitValue_storage);
    add_storage(&sp, STORE_LOC_MUSI, ui_lblMusicValue_storage);
    add_storage(&sp, STORE_LOC_NAME, ui_lblNameValue_storage);
    add_storage(&sp, STORE_LOC_NETW, ui_lblNetworkValue_storage);
    add_storage(&sp, STORE_LOC_PACK, ui_lblPackageValue_storage);
    add_storage(&sp, STORE_LOC_SAVE, ui_lblSaveValue_storage);
    add_storage(&sp, STORE_LOC_SCRS, ui_lblScreenshotValue_storage);
    add_storage(&sp, STORE_LOC_SYCT, ui_lblSyncthingValue_storage);
    add_storage(&sp, STORE_LOC_THEM, ui_lblThemeValue_storage);
    add_storage(&sp, STORE_LOC_TRAK, ui_lblTrackValue_storage);

    char dir[FILENAME_MAX];
    int on_sd2 = 0;

    for (int i = 0; i < sp; i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.STORAGE.SDCARD.MOUNT, storage_path[i].path_suffix);
        if (dir_exist(dir)) {
            lv_label_set_text(storage_path[i].ui_label, "SD2");
            on_sd2 = 1;
        } else {
            lv_label_set_text(storage_path[i].ui_label, "SD1");
        }
    }

    lv_label_set_text(ui_lblNavX, on_sd2 ? lang.GENERIC.SYNC : lang.GENERIC.MIGRATE);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, storage, Apps, lang.MUXSTORAGE.APPS, "apps", "");
    INIT_VALUE_ITEM(-1, storage, Bios, lang.MUXSTORAGE.BIOS, "bios", "");
    INIT_VALUE_ITEM(-1, storage, Catalogue, lang.MUXSTORAGE.CATALOGUE, "catalogue", "");
    INIT_VALUE_ITEM(-1, storage, Collection, lang.MUXSTORAGE.COLLECTION, "collection", "");
    INIT_VALUE_ITEM(-1, storage, History, lang.MUXSTORAGE.HISTORY, "history", "");
    INIT_VALUE_ITEM(-1, storage, Init, lang.MUXSTORAGE.INIT, "init", "");
    INIT_VALUE_ITEM(-1, storage, Music, lang.MUXSTORAGE.MUSIC, "music", "");
    INIT_VALUE_ITEM(-1, storage, Name, lang.MUXSTORAGE.NAME, "name", "");
    INIT_VALUE_ITEM(-1, storage, Network, lang.MUXSTORAGE.NETWORK, "network", "");
    INIT_VALUE_ITEM(-1, storage, Package, lang.MUXSTORAGE.PACKAGE, "package", "");
    INIT_VALUE_ITEM(-1, storage, Save, lang.MUXSTORAGE.SAVE, "save", "");
    INIT_VALUE_ITEM(-1, storage, Screenshot, lang.MUXSTORAGE.SCREENSHOT, "screenshot", "");
    INIT_VALUE_ITEM(-1, storage, Syncthing, lang.MUXSTORAGE.SYNCTHING, "syncthing", "");
    INIT_VALUE_ITEM(-1, storage, Theme, lang.MUXSTORAGE.THEME, "theme", "");
    INIT_VALUE_ITEM(-1, storage, Track, lang.MUXSTORAGE.TRACK, "track", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

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
    gen_step_movement(steps, direction, false, 0);
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

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);

    static char storage_script[MAX_BUFFER_SIZE];
    if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/sync.sh", OPT_PATH);
    } else {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/migrate.sh", OPT_PATH);
    }

    size_t exec_count;
    const char *args[] = {storage_script, storage_path[current_item_index].path_suffix, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);

    write_text_to_file(MUOS_SIN_LOAD, "w", INT, current_item_index);

    load_mux("storage");

    close_input();
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
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {ui_lblNavXGlyph, "",                0},
            {ui_lblNavX,      "",                0},
            {NULL, NULL,                         0}
    });

#define STORAGE(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_storage, UDATA);
    STORAGE_ELEMENTS
#undef STORAGE

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, WALL_GENERAL);
        adjust_gen_panel();

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

int muxstorage_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSTORAGE.TITLE);
    init_muxstorage(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    update_storage_info();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_a,
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
