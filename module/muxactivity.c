#include "muxshare.h"

typedef struct {
    char name[256]; // To be fair exFAT is max 255 :D

    char core[64];
    int core_count;

    int launch_count;

    char device[64];
    int device_count;

    char mode[16];
    int mode_count;

    int start_time;
    int average_time;
    int total_time;
    int last_session;
} activity_item_t;

// 0 = Time Played
// 1 = Launch Count
static int activity_display_mode = 0;
static int in_detail_view = 0;

static activity_item_t *activity_items = NULL;
static size_t activity_count = 0;
static size_t activity_capacity = 0;

static char *playtime_json_str = NULL;
static struct json playtime_json_root = {0};
static int playtime_json_loaded = 0;

static void show_help(void) {
    show_info_box(lang.MUXACTIVITY.TITLE, lang.MUXACTIVITY.HELP, 0);
}

static void ensure_activity_capacity(void) {
    if (activity_count < activity_capacity) return;

    size_t new_capacity = activity_capacity ? activity_capacity * 2 : 256;
    activity_item_t *items = realloc(activity_items, new_capacity * sizeof(activity_item_t));

    if (!items) return;

    activity_items = items;
    activity_capacity = new_capacity;
}

void free_activity_items(activity_item_t **activity_items, size_t *count) {
    free(*activity_items);
    *activity_items = NULL;
    *count = 0;
    activity_capacity = 0;
}

static int cmp_activity_time(const void *a, const void *b) {
    const activity_item_t *x = (const activity_item_t *) a;
    const activity_item_t *y = (const activity_item_t *) b;

    if (y->total_time < x->total_time) return -1;
    if (y->total_time > x->total_time) return 1;

    return 0;
}

static int cmp_activity_launch(const void *a, const void *b) {
    const activity_item_t *x = (const activity_item_t *) a;
    const activity_item_t *y = (const activity_item_t *) b;

    if (y->launch_count < x->launch_count) return -1;
    if (y->launch_count > x->launch_count) return 1;

    return 0;
}

static const char *format_timestamp(int epoch) {
    static char buf[64];

    if (epoch <= 0) {
        snprintf(buf, sizeof(buf), "%s", lang.GENERIC.UNKNOWN);
        return buf;
    }

    time_t t = (time_t) epoch;
    struct tm tm_buf;

    struct tm *tm = localtime_r(&t, &tm_buf);
    if (!tm) {
        snprintf(buf, sizeof(buf), "%s", lang.GENERIC.UNKNOWN);
        return buf;
    }

    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

static void load_playtime_json_once(void) {
    if (playtime_json_loaded) return;

    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), INFO_ACT_PATH "/" PLAYTIME_DATA);

    if (!file_exist(path)) {
        playtime_json_loaded = 1;
        return;
    }

    playtime_json_str = read_all_char_from(path);
    if (!playtime_json_str) {
        playtime_json_loaded = 1;
        return;
    }

    if (!json_valid(playtime_json_str)) {
        free(playtime_json_str);
        playtime_json_str = NULL;
        playtime_json_loaded = 1;
        return;
    }

    playtime_json_root = json_parse(playtime_json_str);
    playtime_json_loaded = 1;
}

static struct json get_playtime_json(void) {
    load_playtime_json_once();
    return playtime_json_root;
}

static void load_activity_items(void) {
    struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return;

    activity_count = 0;

    struct json child = json_first(playtime_json);
    while (json_exists(child)) {
        struct json key = child;
        struct json val = json_next(key);

        if (!json_exists(val)) break;

        if (json_type(val) == JSON_OBJECT) {
            struct json name_json = json_object_get(val, "name");
            struct json time_json = json_object_get(val, "total_time");
            struct json launches_json = json_object_get(val, "launches");

            if (json_exists(name_json) && json_exists(time_json) && json_exists(launches_json)) {
                ensure_activity_capacity();
                if (activity_count >= activity_capacity) break;

                activity_item_t *it = &activity_items[activity_count];
                json_string_copy(name_json, it->name, sizeof(it->name));

                it->total_time = json_int(time_json);
                it->launch_count = json_int(launches_json);

                struct json last_core_json = json_object_get(val, "last_core");
                struct json core_launch_json = json_object_get(val, "core_launches");

                if (json_exists(last_core_json)) {
                    json_string_copy(last_core_json, it->core, sizeof(it->core));
                } else {
                    it->core[0] = '\0';
                }

                it->core_count = 0;
                if (json_exists(core_launch_json) && json_exists(last_core_json)) {
                    char core_key[64];
                    json_string_copy(last_core_json, core_key, sizeof(core_key));

                    struct json core_count_json = json_object_get(core_launch_json, core_key);
                    if (json_exists(core_count_json)) it->core_count = json_int(core_count_json);
                }

                struct json last_device_json = json_object_get(val, "last_device");
                struct json device_launch_json = json_object_get(val, "device_launches");

                if (json_exists(last_device_json)) {
                    json_string_copy(last_device_json, it->device, sizeof(it->device));
                } else {
                    it->device[0] = '\0';
                }

                it->device_count = 0;
                if (json_exists(device_launch_json) && json_exists(last_device_json)) {
                    char dev_key[64];
                    json_string_copy(last_device_json, dev_key, sizeof(dev_key));

                    struct json dev_count_json = json_object_get(device_launch_json, dev_key);
                    if (json_exists(dev_count_json)) it->device_count = json_int(dev_count_json);
                }

                struct json last_mode_json = json_object_get(val, "last_mode");
                struct json mode_launch_json = json_object_get(val, "mode_launches");

                if (json_exists(last_mode_json)) {
                    json_string_copy(last_mode_json, it->mode, sizeof(it->mode));
                } else {
                    it->mode[0] = '\0';
                }

                it->mode_count = 0;
                if (json_exists(mode_launch_json) && json_exists(last_mode_json)) {
                    char mode_key[16];
                    json_string_copy(last_mode_json, mode_key, sizeof(mode_key));

                    struct json mode_count_json = json_object_get(mode_launch_json, mode_key);
                    if (json_exists(mode_count_json)) it->mode_count = json_int(mode_count_json);
                }

                it->start_time = json_int(json_object_get(val, "start_time"));
                it->average_time = json_int(json_object_get(val, "avg_time"));
                it->last_session = json_int(json_object_get(val, "last_session"));

                activity_count++;
            }
        }

        child = json_next(val);
    }
}

static char *format_total_time(int total_time) {
    static char time_buffer[MAX_BUFFER_SIZE] = "0m";

    int days = total_time / 86400;
    int hours = (total_time % 86400) / 3600;
    int minutes = (total_time % 3600) / 60;

    if (days > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dd %dh %dm",
                 days, hours, minutes);
    } else if (hours > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dh %dm",
                 hours, minutes);
    } else if (minutes > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dm",
                 minutes);
    }

    return time_buffer;
}

static void refresh_activity_labels(void) {
    char current_activity_mode[MAX_BUFFER_SIZE];
    snprintf(current_activity_mode, sizeof(current_activity_mode), "%s: %s",
             lang.MUXACTIVITY.MODE, activity_display_mode ? lang.MUXACTIVITY.LAUNCH : lang.MUXACTIVITY.TIME);

    lv_label_set_text(ui_lblNavY, current_activity_mode);

    lv_obj_clean(ui_pnlContent);
    ui_count = 0;

    if (activity_display_mode == 0) {
        qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_time);
    } else {
        qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_launch);
    }

    for (size_t i = 0; i < activity_count; ++i) {
        ui_count++;

        char label_buffer[MAX_BUFFER_SIZE];

        if (activity_display_mode == 0) {
            snprintf(label_buffer, sizeof(label_buffer), "[%s] %s",
                     format_total_time(activity_items[i].total_time),
                     activity_items[i].name);
        } else {
            snprintf(label_buffer, sizeof(label_buffer), "[%d] %s",
                     activity_items[i].launch_count,
                     activity_items[i].name);
        }

        lv_obj_t *ui_pnlAct = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlAct);

        lv_obj_set_user_data(ui_pnlAct, activity_items[i].name);

        lv_obj_t *ui_lblActItem = lv_label_create(ui_pnlAct);
        apply_theme_list_item(&theme, ui_lblActItem, label_buffer);

        lv_obj_t *ui_lblActItemGlyph = lv_img_create(ui_pnlAct);
        apply_theme_list_glyph(&theme, ui_lblActItemGlyph, mux_module, "rom");

        lv_group_add_obj(ui_group, ui_lblActItem);
        lv_group_add_obj(ui_group_glyph, ui_lblActItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlAct);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, label_buffer);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);
}

static void show_detail_view(const activity_item_t *it) {
    lv_obj_clean(ui_pnlContent);

    ui_count = 0;
    current_item_index = 0;
    in_detail_view = 1;

    char detail_label[MAX_BUFFER_SIZE];
    char detail_value[MAX_BUFFER_SIZE];

    for (int i = 0; i < 9; ++i) {
        detail_label[0] = '\0';
        detail_value[0] = '\0';

        switch (i) {
            case 0:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.NAME);
                snprintf(detail_value, sizeof(detail_value), "%s", it->name);
                break;
            case 1:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.CORE);

                char core_tmp[64];
                snprintf(core_tmp, sizeof(core_tmp), "%s", it->core);

                snprintf(detail_value, sizeof(detail_value), "%s", str_replace(str_capital_all(core_tmp),
                                                                               "_libretro.so", " (RetroArch)"));
                break;
            case 2:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.LAUNCH);
                snprintf(detail_value, sizeof(detail_value), "%d", it->launch_count);
                break;
            case 3:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.DEVICE);

                char device_tmp[64];
                snprintf(device_tmp, sizeof(device_tmp), "%s", it->device);
                snprintf(detail_value, sizeof(detail_value), "%s", str_toupper(device_tmp));
                break;
            case 4:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.MODE);

                char mode_tmp[16];
                snprintf(mode_tmp, sizeof(mode_tmp), "%s", it->mode);
                snprintf(detail_value, sizeof(detail_value), "%s", str_capital(mode_tmp));
                break;
            case 5:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.START);
                snprintf(detail_value, sizeof(detail_value), "%s", format_timestamp(it->start_time));
                break;
            case 6:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.AVERAGE);
                snprintf(detail_value, sizeof(detail_value), "%s", format_total_time(it->average_time));
                break;
            case 7:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.TOTAL);
                snprintf(detail_value, sizeof(detail_value), "%s", format_total_time(it->total_time));
                break;
            case 8:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.LAST);
                snprintf(detail_value, sizeof(detail_value), "%s", format_total_time(it->last_session));
                break;
        }

        ui_count++;

        lv_obj_t *ui_pnlAct = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlAct);

        lv_obj_t *ui_lblActItem = lv_label_create(ui_pnlAct);
        apply_theme_list_item(&theme, ui_lblActItem, detail_label);

        lv_obj_t *ui_lblActItemValue = lv_label_create(ui_pnlAct);
        apply_theme_list_value(&theme, ui_lblActItemValue, detail_value);

        lv_obj_t *ui_lblActItemGlyph = lv_img_create(ui_pnlAct);
        apply_theme_list_glyph(&theme, ui_lblActItemGlyph, mux_module, "rom");

        lv_group_add_obj(ui_group, ui_lblActItem);
        lv_group_add_obj(ui_group_value, ui_lblActItemValue);
        lv_group_add_obj(ui_group_glyph, ui_lblActItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlAct);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, detail_label);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);
}

static void generate_activity_items(void) {
    load_activity_items();

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    in_detail_view = 0;
    refresh_activity_labels();
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
        nav_move(ui_group_value, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || hold_call) return;
    if (in_detail_view) return;

    play_sound(SND_CONFIRM);

    lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);

    size_t idx = current_item_index;
    if (idx >= activity_count) return;

    show_detail_view(&activity_items[idx]);
    nav_moved = 1;
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

    if (in_detail_view) {
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);

        in_detail_view = 0;
        refresh_activity_labels();
        nav_moved = 1;
        return;
    }

    free_activity_items(&activity_items, &activity_count);

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call || in_detail_view) return;

    activity_display_mode = !activity_display_mode;
    play_sound(SND_NAVIGATE);

    refresh_activity_labels();
    nav_moved = 1;
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
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
            {ui_lblNavAGlyph, "",                    0},
            {ui_lblNavA,      lang.MUXACTIVITY.INFO, 0},
            {ui_lblNavBGlyph, "",                    0},
            {ui_lblNavB,      lang.GENERIC.BACK,     0},
            {ui_lblNavYGlyph, "",                    0},
            {ui_lblNavY,      "",                    0},
            {NULL, NULL,                             0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxactivity_main() {
    const char *m = "muxactivity";
    set_process_name(m);
    init_module(m);

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_lblTitle, lang.MUXACTIVITY.TITLE);

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    init_fonts();

    generate_activity_items();

    if (ui_count == 0) lv_label_set_text(ui_lblScreenMessage, lang.MUXACTIVITY.NONE);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_Y] = handle_y,
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
