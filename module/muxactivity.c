#include "muxshare.h"

typedef struct {
    char name[256]; // To be fair exFAT is max 255 :D
    int total_time;
    int launch_count;
} activity_item_t;

// 0 = Time Played
// 1 = Launch Count
static int activity_display_mode = 0;

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

                json_string_copy(name_json, activity_items[activity_count].name,
                                 sizeof(activity_items[activity_count].name));

                activity_items[activity_count].total_time = json_int(time_json);
                activity_items[activity_count].launch_count = json_int(launches_json);

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

static void generate_activity_items(void) {
    load_activity_items();

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

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

//    close_input();
//    mux_input_stop();
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

    free_activity_items(&activity_items, &activity_count);

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call) return;

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
