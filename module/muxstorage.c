#include "muxshare.h"
#include "ui/ui_muxstorage.h"

#define STORAGE(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(STORAGE_ELEMENTS) };
#undef STORAGE

typedef struct {
    const char *path_suffix;
    lv_obj_t *ui_label;
} storage;

storage storage_path[ui_count_dynamic];

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define STORAGE(NAME, UDATA) {UDATA, lang.muxstorage.help.NAME},
        STORAGE_ELEMENTS
#undef STORAGE
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void add_storage(int *sp, const char *suffix, lv_obj_t *label) {
    storage_path[*sp].path_suffix = suffix;
    storage_path[*sp].ui_label = label;
    (*sp)++;
}

static void update_storage_info(void) {
    int sp = 0;

    add_storage(&sp, STORE_LOC_APPS, ui_val_apps_storage);
    add_storage(&sp, STORE_LOC_BIOS, ui_val_bios_storage);
    add_storage(&sp, STORE_LOC_CLOG, ui_val_catalogue_storage);
    add_storage(&sp, STORE_LOC_COLL, ui_val_collection_storage);
    add_storage(&sp, STORE_LOC_HIST, ui_val_history_storage);
    add_storage(&sp, STORE_LOC_INIT, ui_val_init_storage);
    add_storage(&sp, STORE_LOC_MUSI, ui_val_music_storage);
    add_storage(&sp, STORE_LOC_NAME, ui_val_name_storage);
    add_storage(&sp, STORE_LOC_NETW, ui_val_network_storage);
    add_storage(&sp, STORE_LOC_PACK, ui_val_package_storage);
    add_storage(&sp, STORE_LOC_SAVE, ui_val_save_storage);
    add_storage(&sp, STORE_LOC_SCRS, ui_val_screenshot_storage);
    add_storage(&sp, STORE_LOC_SYCT, ui_val_syncthing_storage);
    add_storage(&sp, STORE_LOC_THEM, ui_val_theme_storage);
    add_storage(&sp, STORE_LOC_TRAK, ui_val_track_storage);

    char dir[FILENAME_MAX];
    int on_sd2 = 0;

    for (int i = 0; i < sp; i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.storage.sdcard.mount, storage_path[i].path_suffix);
        if (dir_exist(dir)) {
            lv_label_set_text(storage_path[i].ui_label, "SD2");
            on_sd2 = 1;
        } else {
            lv_label_set_text(storage_path[i].ui_label, "SD1");
        }
    }

    lv_label_set_text(ui_lbl_nav_x, on_sd2 ? lang.generic.sync : lang.generic.migrate);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, storage, apps, lang.muxstorage.apps, "apps", "");
    INIT_VALUE_ITEM(-1, storage, bios, lang.muxstorage.bios, "bios", "");
    INIT_VALUE_ITEM(-1, storage, catalogue, lang.muxstorage.catalogue, "catalogue", "");
    INIT_VALUE_ITEM(-1, storage, collection, lang.muxstorage.collection, "collection", "");
    INIT_VALUE_ITEM(-1, storage, history, lang.muxstorage.history, "history", "");
    INIT_VALUE_ITEM(-1, storage, init, lang.muxstorage.init, "init", "");
    INIT_VALUE_ITEM(-1, storage, music, lang.muxstorage.music, "music", "");
    INIT_VALUE_ITEM(-1, storage, name, lang.muxstorage.name, "name", "");
    INIT_VALUE_ITEM(-1, storage, network, lang.muxstorage.network, "network", "");
    INIT_VALUE_ITEM(-1, storage, package, lang.muxstorage.package, "package", "");
    INIT_VALUE_ITEM(-1, storage, save, lang.muxstorage.save, "save", "");
    INIT_VALUE_ITEM(-1, storage, screenshot, lang.muxstorage.screenshot, "screenshot", "");
    INIT_VALUE_ITEM(-1, storage, syncthing, lang.muxstorage.syncthing, "syncthing", "");
    INIT_VALUE_ITEM(-1, storage, theme, lang.muxstorage.theme, "theme", "");
    INIT_VALUE_ITEM(-1, storage, track, lang.muxstorage.track, "track", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    int sin_index = 0;
    if (file_exist(MUOS_SIN_LOAD)) {
        sin_index = read_line_int_from(MUOS_SIN_LOAD, 1);
        remove(MUOS_SIN_LOAD);
    }

    if (ui_count_static > 0 && sin_index >= 0 && sin_index < ui_count_static && current_item_index < ui_count_static) {
        gen_step_movement(sin_index, 1, 0, 0, 1);
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "storage");

    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group_value);

    static char storage_script[MAX_BUFFER_SIZE];
    if (strcasecmp(lv_label_get_text(e_focused), "SD2") == 0) {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/sync.sh", OPT_PATH);
    } else {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/migrate.sh", OPT_PATH);
    }

    size_t exec_count;
    const char *args[] = {storage_script, storage_path[current_item_index].path_suffix, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        fade_out_screen();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);

    write_text_to_file(MUOS_SIN_LOAD, "w", INT, current_item_index);

    load_mux("storage");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, "", 0},
                                  {NULL, NULL, 0}});

#define STORAGE(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_storage, UDATA);
    STORAGE_ELEMENTS
#undef STORAGE

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, wall_general);
        adjust_gen_panel();

        const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group_value);
        if (strcasecmp(lv_label_get_text(e_focused), "SD2") == 0) {
            lv_label_set_text(ui_lbl_nav_x, lang.generic.sync);
        } else {
            lv_label_set_text(ui_lbl_nav_x, lang.generic.migrate);
        }

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxstorage_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxstorage.title);
    init_muxstorage(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    update_storage_info();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_a,
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
