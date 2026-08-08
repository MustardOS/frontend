#include "muxshare.h"
#include "../common/ui/notify.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxstorage.h"
#include "../common/ui/list_frame.h"
#include "../common/ui/task_progress.h"

#define STORAGE(NAME, UDATA) 1,
enum {
    ui_count_dynamic = E_SIZE(STORAGE_ELEMENTS),
    bulk_count = E_SIZE(STORAGE_BULK_ELEMENTS),
    storage_count = ui_count_dynamic - bulk_count
};
#undef STORAGE

typedef enum { bulk_migrate = 0, bulk_sync, bulk_purge } bulk_act;

typedef struct {
    const char *path_suffix;
    lv_obj_t *ui_label;
    int on_secondary;
} storage;

static storage storage_path[storage_count];

static void nav_refresh(void);

static void show_help(void) {
    if (list_frame_focused()) {
        list_frame_help();

        return;
    }

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

    add_storage(&sp, STORE_LOC_CLOG, ui_val_catalogue_storage);
    add_storage(&sp, STORE_LOC_COLL, ui_val_collection_storage);
    add_storage(&sp, STORE_LOC_HIST, ui_val_history_storage);
    add_storage(&sp, STORE_LOC_NAME, ui_val_name_storage);
    add_storage(&sp, STORE_LOC_SAVE, ui_val_save_storage);
    add_storage(&sp, STORE_LOC_SCRS, ui_val_screenshot_storage);
    add_storage(&sp, STORE_LOC_TRAK, ui_val_track_storage);

    add_storage(&sp, STORE_LOC_BIOS, ui_val_bios_storage);
    add_storage(&sp, STORE_LOC_INIT, ui_val_init_storage);
    add_storage(&sp, STORE_LOC_NETW, ui_val_network_storage);
    add_storage(&sp, STORE_LOC_SYCT, ui_val_syncthing_storage);

    add_storage(&sp, STORE_LOC_APPS, ui_val_apps_storage);
    add_storage(&sp, STORE_LOC_MUSI, ui_val_music_storage);
    add_storage(&sp, STORE_LOC_PACK, ui_val_package_storage);
    add_storage(&sp, STORE_LOC_THEM, ui_val_theme_storage);

    char dir[FILENAME_MAX];

    for (int i = 0; i < sp; i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.storage.sdcard.mount, storage_path[i].path_suffix);

        storage_path[i].on_secondary = dir_exist(dir);
        lv_label_set_text(
            storage_path[i].ui_label, storage_path[i].on_secondary ? lang.generic.secondary : lang.generic.primary
        );
    }
}

static int focused_storage(void) {
    if (list_frame_focused()) return -1;

    const int row = list_frame_current_row();

    return row >= 0 && row < storage_count ? row : -1;
}

static bulk_act focused_bulk(int *found) {
    const int row = list_frame_focused() ? -1 : list_frame_current_row();

    *found = row >= storage_count && row < ui_count_dynamic;

    return *found ? (bulk_act) (row - storage_count) : bulk_migrate;
}

static int on_secondary(void) {
    const int row = focused_storage();

    return row >= 0 && storage_path[row].on_secondary;
}

static int bulk_rows(const bulk_act act, int *rows) {
    const int wanted = act == bulk_migrate ? 0 : 1;

    int count = 0;
    for (int i = 0; i < storage_count; i++)
        if (storage_path[i].on_secondary == wanted) rows[count++] = i;

    return count;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

#define STORAGE(NAME, UDATA) INIT_VALUE_ITEM(-1, storage, NAME, lang.muxstorage.NAME, UDATA, "");
    STORAGE_ELEMENTS
#undef STORAGE

    reset_ui_groups();

    static const list_frame frames[] = {
        {lang.muxstorage.section.content, 0, 7},
        {lang.muxstorage.section.system, 7, 4},
        {lang.muxstorage.section.custom, 11, 4},
        {lang.muxstorage.section.action, 15, 3},
    };

    list_frame_init(
        &theme, ui_pnl_content, frames, A_SIZE(frames), ui_objects_panel, ui_objects, ui_objects_glyph,
        ui_objects_value, ui_count_dynamic
    );
    list_frame_apply();

    update_storage_info();

    gen_step_movement(list_frame_restore(), +1, 0, 0, 1);
    nav_refresh();
}

static int task_pending = 0;

typedef enum { purge_now = 0, purge_sync, purge_nah, purge_count } purge_opt;

static mux_dialogue action_dlg;
static mux_dialogue purge_dlg;
static mux_dialogue bulk_dlg;

static bulk_act pending_bulk = bulk_migrate;

static void ask_purge(void);

static void finish_task(void) {
    task_pending = 0;

    load_mux("storage");
    mux_input_stop();
}

static mux_dialogue *active_dialogue(void) {
    if (dialogue_active(&action_dlg)) return &action_dlg;
    if (dialogue_active(&purge_dlg)) return &purge_dlg;
    if (dialogue_active(&bulk_dlg)) return &bulk_dlg;

    return NULL;
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (msgbox_active || hold_call || task_progress_active()) return;
    if (active_dialogue()) return;
    if (!on_secondary()) return;

    play_sound(snd_confirm);
    ask_purge();
}

static void handle_dpad_up(void) {
    if (task_progress_handle_dpad(-1)) return;

    mux_dialogue *dlg = active_dialogue();
    if (dlg) {
        dialogue_handle_dpad(dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (task_progress_handle_dpad(+1)) return;

    mux_dialogue *dlg = active_dialogue();
    if (dlg) {
        dialogue_handle_dpad(dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (task_progress_handle_dpad_hold(-1)) return;

    mux_dialogue *dlg = active_dialogue();
    if (dlg) {
        dialogue_handle_dpad_hold(dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (task_progress_handle_dpad_hold(+1)) return;

    mux_dialogue *dlg = active_dialogue();
    if (dlg) {
        dialogue_handle_dpad_hold(dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_section_step(const int direction) {
    if (msgbox_active || hold_call || task_progress_active() || active_dialogue()) return;

    if (list_frame_move(direction)) {
        play_sound(snd_navigate);
        gen_step_movement(0, +1, 0, 0, 0);

        nav_refresh();
        nav_moved = 1;
    }
}

static void handle_section_prev(void) {
    handle_section_step(-1);
}

static void handle_section_next(void) {
    handle_section_step(+1);
}

static void handle_dpad_left(void) {
    if (!list_frame_focused()) return;

    handle_section_prev();
}

static void handle_dpad_right(void) {
    if (!list_frame_focused()) return;

    handle_section_next();
}

static void handle_b(void) {
    mux_dialogue *dlg = active_dialogue();
    if (dlg) {
        dialogue_cancel(dlg);
        return;
    }

    if (task_progress_handle_b()) {
        if (task_pending && !task_progress_active()) finish_task();
        return;
    }

    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    list_frame_remember_section();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "storage");

    mux_input_stop();
}

static void run_storage_task(const char *script, const char *extra, const int *rows, const int count) {
    static char storage_script[MAX_BUFFER_SIZE];
    snprintf(storage_script, sizeof(storage_script), "%s/script/mux/%s", OPT_PATH, script);

    const char *argv[storage_count + 3];
    size_t argc = 0;

    argv[argc++] = storage_script;
    if (extra) argv[argc++] = extra;

    for (int i = 0; i < count; i++)
        argv[argc++] = storage_path[rows[i]].path_suffix;

    argv[argc] = NULL;

    list_frame_remember(lv_group_get_focused(ui_group));

    const task_exec_spec spec = {
        .argv = argv,
        .argc = argc,
        .mode = task_mode_progress,
        .can_cancel = 1,
        .turbo = 1,
        .title = lang.muxstorage.title,
    };

    if (task_exec_start(&spec) == 0) {
        task_pending = 1;
        task_progress_show();
    } else {
        notify_send(notify_warning, lang.generic.failed);
    }
}

static void run_single_task(const char *script, const char *extra) {
    const int row = focused_storage();
    if (row < 0) return;

    run_storage_task(script, extra, &row, 1);
}

static const char *focused_category(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    return e_focused ? lv_label_get_text(e_focused) : "";
}

static void ask_action(void) {
    char message[MAX_BUFFER_SIZE];
    snprintf(
        message, sizeof(message), on_secondary() ? lang.muxstorage.confirm.sync : lang.muxstorage.confirm.migrate,
        focused_category()
    );

    dialogue_init_confirm(
        &action_dlg, &theme, ui_screen, lang.muxstorage.title, message, lang.generic.yes, lang.generic.no,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_open(&action_dlg, &theme);
}

static void ask_purge(void) {
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), lang.muxstorage.confirm.purge, focused_category());

    const char *labels[purge_count] = {lang.generic.purge, lang.generic.sync_purge, lang.generic.cancel};

    dialogue_init(
        &purge_dlg, &theme, ui_screen, lang.muxstorage.title, message, labels, purge_count, lang.generic.select,
        lang.generic.cancel
    );

    dialogue_open(&purge_dlg, &theme);
}

static void ask_bulk(const bulk_act act) {
    int rows[storage_count];
    const int count = bulk_rows(act, rows);

    if (count == 0) {
        notify_send(notify_info, lang.muxstorage.nothing);
        return;
    }

    const char *format = act == bulk_migrate ? lang.muxstorage.confirm.migrate_all
                         : act == bulk_sync  ? lang.muxstorage.confirm.sync_all
                                             : lang.muxstorage.confirm.purge_all;

    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), format, count);

    pending_bulk = act;

    if (act == bulk_purge) {
        const char *labels[purge_count] = {lang.generic.purge, lang.generic.sync_purge, lang.generic.cancel};

        dialogue_init(
            &bulk_dlg, &theme, ui_screen, lang.muxstorage.title, message, labels, purge_count, lang.generic.select,
            lang.generic.cancel
        );
    } else {
        dialogue_init_confirm(
            &bulk_dlg, &theme, ui_screen, lang.muxstorage.title, message, lang.generic.yes, lang.generic.no,
            lang.generic.select, lang.generic.cancel
        );
    }

    dialogue_open(&bulk_dlg, &theme);
}

static void run_bulk(const bulk_act act, const char *extra) {
    int rows[storage_count];
    const int count = bulk_rows(act, rows);

    if (count == 0) {
        notify_send(notify_info, lang.muxstorage.nothing);
        return;
    }

    switch (act) {
        case bulk_migrate:
            run_storage_task("migrate.sh", NULL, rows, count);
            break;
        case bulk_sync:
            run_storage_task("sync.sh", NULL, rows, count);
            break;
        case bulk_purge:
            run_storage_task("purge.sh", extra, rows, count);
            break;
    }
}

static void handle_a(void) {
    if (task_progress_handle_a()) {
        if (task_pending && !task_progress_active()) finish_task();
        return;
    }

    if (dialogue_active(&action_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) action_dlg.selected;
        dialogue_dismiss(&action_dlg);

        if (opt == mux_confirm_yep) run_single_task(on_secondary() ? "sync.sh" : "migrate.sh", NULL);
        return;
    }

    if (dialogue_active(&bulk_dlg)) {
        const int selected = bulk_dlg.selected;
        dialogue_dismiss(&bulk_dlg);

        if (pending_bulk == bulk_purge) {
            if (selected == purge_now)
                run_bulk(bulk_purge, NULL);
            else if (selected == purge_sync)
                run_bulk(bulk_purge, "--sync");
        } else if (selected == mux_confirm_yep) {
            run_bulk(pending_bulk, NULL);
        }

        return;
    }

    if (dialogue_active(&purge_dlg)) {
        const purge_opt opt = (purge_opt) purge_dlg.selected;
        dialogue_dismiss(&purge_dlg);

        if (opt == purge_now) {
            run_single_task("purge.sh", NULL);
        } else if (opt == purge_sync) {
            run_single_task("purge.sh", "--sync");
        }

        return;
    }

    if (msgbox_active || hold_call) return;
    if (list_frame_focused()) return;

    int is_bulk = 0;
    const bulk_act act = focused_bulk(&is_bulk);

    play_sound(snd_confirm);

    if (is_bulk) {
        ask_bulk(act);
        return;
    }

    ask_action();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, "", 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.purge, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define STORAGE(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_storage, UDATA);
    STORAGE_ELEMENTS
#undef STORAGE

    overlay_display();
}

static void nav_refresh(void) {
    if (list_frame_focused()) {
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);

        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);

        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);

        return;
    }

    lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);

    int is_bulk = 0;
    focused_bulk(&is_bulk);

    lv_label_set_text(
        ui_lbl_nav_a, is_bulk          ? lang.generic.select
                      : on_secondary() ? lang.generic.sync
                                       : lang.generic.migrate
    );

    lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);

    if (on_secondary()) {
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    task_progress_tick();
    notify_tick();

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, wall_general);
        adjust_gen_panel();

        nav_refresh();

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

    task_progress_init(&theme, ui_screen);
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
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_l1] = handle_section_prev,
                [mux_input_r1] = handle_section_next,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_section_prev,
            [mux_input_r1] = handle_section_next,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxstorage.title, lang.muxstorage.overview);

    mux_input_task(&input_opts);

    return 0;
}
