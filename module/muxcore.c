#include "muxshare.h"
#include <common/ui/list_frame.h>
#include <common/content/core/retroarch.h>
#include <common/ui/notify.h>
#include <common/ui/orientation.h>
#include <common/storage/download.h>
#include <common/ui/task_progress.h>

static char remote_manifest_path[MAX_BUFFER_SIZE];

static int starter_image = 0;
static int extract_pending = 0;

typedef enum { core_state_current = 0, core_state_update, core_state_absent } core_state;

static struct json local_manifest;
static struct json remote_manifest;
static char *local_manifest_raw = NULL;
static char *remote_manifest_raw = NULL;

static struct json manifest_load(const char *path, char **raw) {
    struct json empty = {0};

    if (!file_exist(path)) return empty;

    const char *text = read_all_char_from(path);
    if (!text || !json_valid(text)) return empty;

    free(*raw);
    *raw = strdup(text);

    return json_parse(*raw);
}

static void manifest_field(const struct json manifest, const char *zip, const char *key, char *out, size_t out_size) {
    out[0] = '\0';
    if (!json_exists(manifest)) return;

    const struct json entry = json_object_get(manifest, zip);
    if (!json_exists(entry)) return;

    const struct json value = json_object_get(entry, key);
    if (json_type(value) != JSON_STRING) return;

    json_string_copy(value, out, out_size);
}

static core_state core_state_of(const char *zip) {
    char local_sha[MAX_BUFFER_SIZE];
    char remote_sha[MAX_BUFFER_SIZE];

    manifest_field(local_manifest, zip, "sha256", local_sha, sizeof(local_sha));
    manifest_field(remote_manifest, zip, "sha256", remote_sha, sizeof(remote_sha));

    if (!local_sha[0]) return core_state_absent;
    if (!remote_sha[0] || strcmp(local_sha, remote_sha) == 0) return core_state_current;

    return core_state_update;
}

static const char *core_state_glyph(const core_state state) {
    switch (state) {
        case core_state_update:
            return "download";
        case core_state_absent:
            return "download";
        default:
            return "downloaded";
    }
}

static void core_display_name(const char *zip, const char *fallback, char *out, const size_t out_size) {
    char key[MAX_BUFFER_SIZE];
    snprintf(key, sizeof(key), "%s", zip);

    char *so = strstr(key, "_libretro.so");
    if (so) *so = '\0';

    for (int i = 0; ra_core_names[i].core; i++) {
        if (strcmp(key, ra_core_names[i].core) == 0) {
            snprintf(out, out_size, "%s", ra_core_names[i].name);
            return;
        }
    }

    char name[MAX_BUFFER_SIZE];
    manifest_field(remote_manifest, zip, "core", name, sizeof(name));
    if (!name[0]) manifest_field(local_manifest, zip, "core", name, sizeof(name));

    snprintf(out, out_size, "%s", name[0] ? name : fallback);
}

static void core_asset_name(const char *zip, char *out, const size_t out_size) {
    snprintf(out, out_size, "%s", zip);

    const size_t len = strlen(out);
    if (len > 4 && strcmp(out + len - 4, ".zip") == 0) out[len - 4] = '\0';
}

static void resolve_muxzip_path(const char *zip, char *out) {
    char asset[MAX_BUFFER_SIZE];
    core_asset_name(zip, asset, sizeof(asset));

    snprintf(out, MAX_BUFFER_SIZE, "%s/%s/Core - %s.muxzip", device.storage.rom.mount, MUOS_ARCH_PATH, asset);
}

static void core_value(const char *zip, char *out, const size_t out_size) {
    switch (core_state_of(zip)) {
        case core_state_update:
            snprintf(out, out_size, "%s", lang.muxcore.update);
            break;
        case core_state_absent:
            snprintf(out, out_size, "%s", lang.muxcore.not_installed);
            break;
        default:
            snprintf(out, out_size, "%s", lang.muxcore.installed);
            break;
    }
}

static void show_help(void) {
    show_info_box(items[current_item_index].display_name, items[current_item_index].help, 0);
}

static void create_content_items(void) {
    local_manifest = manifest_load(CORE_MANIFEST_LOCAL, &local_manifest_raw);
    remote_manifest = manifest_load(remote_manifest_path, &remote_manifest_raw);

    const struct json sources[] = {local_manifest, remote_manifest};

    for (size_t s = 0; s < A_SIZE(sources); s++) {
        if (!json_exists(sources[s])) continue;

        for (struct json key = json_first(sources[s]); json_exists(key); key = json_next(json_next(key))) {
            char zip[MAX_BUFFER_SIZE];
            json_string_copy(key, zip, sizeof(zip));

            if (zip[0] && get_item_index_by_name(items, item_count, zip, content_type_item) == -1) {
                char name[MAX_BUFFER_SIZE];
                core_display_name(zip, zip, name, sizeof(name));

                char display[MAX_BUFFER_SIZE];
                snprintf(display, sizeof(display), "%s", name);

                char help[MAX_BUFFER_SIZE];
                switch (core_state_of(zip)) {
                    case core_state_update:
                        snprintf(help, sizeof(help), "%s", lang.muxcore.help.update);
                        break;
                    case core_state_absent:
                        snprintf(help, sizeof(help), "%s", lang.muxcore.help.absent);
                        break;
                    default:
                        snprintf(help, sizeof(help), "%s", lang.muxcore.help.current);
                        break;
                }

                content_item *new_item = add_item(&items, &item_count, zip, display, zip, content_type_item);
                if (new_item) new_item->help = strdup(help);
            }
        }
    }

    sort_items(items, item_count);

    for (int i = 0; i < item_count; i++) {
        if (lv_obj_get_child_cnt(ui_pnl_content) >= theme.mux.item.count) break;

        char value[MAX_BUFFER_SIZE];
        core_value(items[i].name, value, sizeof(value));

        lv_obj_t *ui_pnl_item = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_item);

        lv_obj_t *ui_lbl_item = lv_label_create(ui_pnl_item);
        apply_theme_option_item_label(&theme, ui_lbl_item, items[i].display_name, 1);

        lv_obj_t *ui_lbl_item_glyph = lv_img_create(ui_pnl_item);
        apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, core_state_glyph(core_state_of(items[i].name)));

        lv_obj_t *ui_lbl_item_value = lv_label_create(ui_pnl_item);
        apply_theme_list_value(&theme, ui_lbl_item_value, value);

        lv_group_add_obj(ui_group, ui_lbl_item);
        lv_group_add_obj(ui_group_value, ui_lbl_item_value);
        lv_group_add_obj(ui_group_glyph, ui_lbl_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_item);

        apply_text_long_dot(&theme, ui_lbl_item);
    }
}

#define CORE_VALUE_CHILD 2

static lv_obj_t *row_value_label(const lv_obj_t *ui_lbl_item) {
    lv_obj_t *panel = lv_obj_get_parent(ui_lbl_item);
    if (!panel || lv_obj_get_child_cnt(panel) <= CORE_VALUE_CHILD) return NULL;

    return lv_obj_get_child(panel, CORE_VALUE_CHILD);
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, items[index].display_name);

    lv_obj_t *ui_lbl_item_value = row_value_label(ui_lbl_item);
    if (ui_lbl_item_value) {
        char value[MAX_BUFFER_SIZE];
        core_value(items[index].name, value, sizeof(value));
        lv_label_set_text(ui_lbl_item_value, value);
    }

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (theme.list_default.glyph_alpha > 0 && theme.list_focus.glyph_alpha > 0) {
        get_glyph_path(
            mux_module, core_state_glyph(core_state_of(items[index].name)), glyph_image_embed, MAX_BUFFER_SIZE
        );
        lv_img_set_src(ui_lbl_item_glyph, glyph_image_embed);
    }
}

static const char *action_label(const int index) {
    switch (core_state_of(items[index].name)) {
        case core_state_update:
            return lang.muxcore.update;
        case core_state_absent:
            return lang.generic.download;
        default:
            return lang.muxcore.reinstall;
    }
}

static void update_action(const int index) {
    const char *label = action_label(index);
    const int visible = label[0] != '\0';
    const struct nav_flag nav_e[] = {{ui_lbl_nav_a, visible}, {ui_lbl_nav_a_glyph, visible}};

    if (visible) lv_label_set_text(ui_lbl_nav_a, label);
    set_nav_flags(nav_e, A_SIZE(nav_e));
}

static void list_nav_move(const int steps, const int direction) {
    list_win_nav_move(steps, direction, update_list_item);

    lv_obj_t *focused_value = row_value_label(lv_group_get_focused(ui_group));
    if (focused_value) lv_group_focus_obj(focused_value);

    if (ui_count_static > 0) update_action(current_item_index);
}

static void list_nav_prev(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void finish_extract(void) {
    extract_pending = 0;

    load_mux("core");
    mux_input_stop();
}

static void download_finished(const int result) {
    if (result != 0) {
        play_sound(snd_error);
        const char *storage_message = download_storage_message(result);
        toast_message(storage_message ? storage_message : lang.muxcore.error_get_core, tst_wait_s);
        return;
    }

    char file_path[MAX_BUFFER_SIZE];
    resolve_muxzip_path(items[current_item_index].name, file_path);

    if (extract_archive(file_path, "core", lang.muxcore.title) != 0) {
        play_sound(snd_error);
        notify_send(notify_warning, lang.generic.failed);
        return;
    }

    extract_pending = 1;
    task_progress_show();
}

static void refresh_manifest_finished(const int result) {
    if (result == 0) {
        load_mux("core");
        mux_input_stop();
    } else {
        play_sound(snd_error);
        const char *storage_message = download_storage_message(result);
        toast_message(storage_message ? storage_message : lang.muxcore.error_get_data, tst_wait_f);
    }
}

static void refresh_manifest(void) {
    set_download_callbacks(refresh_manifest_finished);
    initiate_download(config.extra.core.data, remote_manifest_path, 1, lang.muxcore.down.data);
}

static void handle_a(void) {
    if (task_progress_handle_a()) {
        if (extract_pending && !task_progress_active()) finish_extract();
        return;
    }

    if (download_in_progress || !ui_count_static || hold_call) return;

    if (!json_exists(remote_manifest)) {
        play_sound(snd_error);
        toast_message(lang.muxcore.need_refresh, tst_wait_m);
        return;
    }

    if (!is_network_connected()) {
        play_sound(snd_error);
        toast_message(lang.generic.need_connect, tst_wait_m);
        return;
    }

    char remote_url[MAX_BUFFER_SIZE];
    manifest_field(remote_manifest, items[current_item_index].name, "sha256", remote_url, sizeof(remote_url));
    if (!remote_url[0]) {
        play_sound(snd_error);
        toast_message(lang.muxcore.not_published, tst_wait_m);
        return;
    }

    play_sound(snd_confirm);
    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    char file_path[MAX_BUFFER_SIZE];
    resolve_muxzip_path(items[current_item_index].name, file_path);

    char asset[MAX_BUFFER_SIZE];
    core_asset_name(items[current_item_index].name, asset, sizeof(asset));

    char url[MAX_BUFFER_SIZE];
    snprintf(url, sizeof(url), "https://github.com/MustardOS/extra/releases/latest/download/Core.-.%s.muxzip", asset);

    set_download_callbacks(download_finished);
    initiate_download(url, file_path, 1, lang.muxcore.down.core);
}

static void handle_b(void) {
    if (task_progress_handle_b()) {
        if (extract_pending && !task_progress_active()) finish_extract();
        return;
    }

    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    if (download_in_progress) {
        cancel_download = 1;

        char file_path[MAX_BUFFER_SIZE];
        resolve_muxzip_path(items[current_item_index].name, file_path);

        if (file_exist(file_path)) remove(file_path);

        return;
    }

    list_frame_remember_section();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "core");

    mux_input_stop();
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (download_in_progress || msgbox_active || hold_call || !device.board.has_network) return;

    if (is_network_connected()) {
        play_sound(snd_confirm);
        refresh_manifest();
    } else {
        play_sound(snd_error);
        toast_message(lang.generic.need_connect, tst_wait_m);
    }
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_progress_brightness,
                                          ui_pnl_progress_volume, ui_pnl_message, NULL});
    if (config.visual.box_art == 3) lv_obj_move_foreground(ui_pnl_box);
}

static void init_elements(void) {
    lv_obj_set_align(ui_img_box, config.visual.box_art_align);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.download, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.refresh, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    download_poll();
    task_progress_tick();

    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, wall_general);
        adjust_panels();

        if (!lv_obj_has_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
        }

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

int muxcore_main(void) {
    starter_image = 0;
    extract_pending = 0;

    snprintf(
        remote_manifest_path, sizeof(remote_manifest_path), "%s/%s", device.storage.rom.mount,
        MUOS_INFO_PATH "/" CORE_MANIFEST_REMOTE
    );

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxcore.title);

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lbl_datetime, get_datetime());
    init_fonts();
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    reset_ui_groups();

    create_content_items();
    ui_count_static = (int) item_count;

    init_elements();

    const struct nav_flag nav_e[] = {{ui_lbl_nav_a, 0},       {ui_lbl_nav_a_glyph, 0}, {ui_lbl_nav_y, 0},
                                     {ui_lbl_nav_y_glyph, 0}, {ui_lbl_nav_menu, 0},    {ui_lbl_nav_menu_glyph, 0}};

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (ui_count_static > 0) {
        update_action(0);
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxcore.no_cores);
        lv_obj_clear_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
    }

    int sys_index = -1;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    if (ui_count_static > 0 && sys_index > -1 && sys_index <= ui_count_static && current_item_index < ui_count_static) {
        list_nav_move(sys_index, +1);
    }

    task_progress_init(&theme, ui_screen);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_dpad_left] = handle_list_nav_left,
                [mux_input_dpad_right] = handle_list_nav_right,
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
            [mux_input_dpad_left] = handle_list_nav_left_hold,
            [mux_input_dpad_right] = handle_list_nav_right_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxcore.title, lang.muxcore.overview);

    mux_input_task(&input_opts);

    if (ui_count_static > 0) free_items(&items, &item_count);

    free(local_manifest_raw);
    free(remote_manifest_raw);
    local_manifest_raw = NULL;
    remote_manifest_raw = NULL;

    return 0;
}
