#include "muxshare.h"

#define ZONE_REGION_OTHER "Other"
#define ZONE_REGION_MAX   64
#define ZONE_LABEL_MAX    128

typedef enum {
    VIEW_REGION, // Region groups
    VIEW_CITY,   // Cities inside a chosen region
} zone_view_t;

typedef enum {
    ALIAS_REGION,
    ALIAS_CITY,
} alias_field_t;

static zone_view_t zone_view = VIEW_REGION;
static char zone_selected_region[ZONE_LABEL_MAX] = {0};
static int zone_region_index = 0;

static void show_help(void) {
    show_info_box(lang.MUXTIMEZONE.TITLE, lang.MUXTIMEZONE.HELP, 0);
}

// This is currently the best I can come up with considering timezones
// are god damn awful and the way we present timezones in muX is using
// regions and then cities within.  If this needs changes let us know!
static const struct {
    const char *alias;
    const char *region;
    const char *city;
} zone_alias_map[] = {
        {"Cuba",      "America",  "Havana"},
        {"Egypt",     "Africa",   "Cairo"},
        {"Eire",      "Europe",   "Dublin"},
        {"Hongkong",  "Asia",     "Hong_Kong"},
        {"Iceland",   "Atlantic", "Reykjavik"},
        {"Iran",      "Asia",     "Tehran"},
        {"Israel",    "Asia",     "Jerusalem"},
        {"Jamaica",   "America",  "Jamaica"},
        {"Japan",     "Asia",     "Tokyo"},
        {"Kwajalein", "Pacific",  "Kwajalein"},
        {"Libya",     "Africa",   "Tripoli"},
        {"Navajo",    "America",  "Denver"},
        {"Poland",    "Europe",   "Warsaw"},
        {"Portugal",  "Europe",   "Lisbon"},
        {"Singapore", "Asia",     "Singapore"},
        {"Turkey",    "Europe",   "Istanbul"},
        {NULL, NULL, NULL}
};

static const char *alias_lookup(const char *zone, alias_field_t field) {
    for (int i = 0; zone_alias_map[i].alias != NULL; i++) {
        if (strcmp(zone, zone_alias_map[i].alias) == 0) {
            return field == ALIAS_REGION ? zone_alias_map[i].region : zone_alias_map[i].city;
        }
    }

    return NULL;
}

static void alias_lookup_both(const char *zone, const char **region_out, const char **city_out) {
    for (int i = 0; zone_alias_map[i].alias != NULL; i++) {
        if (strcmp(zone, zone_alias_map[i].alias) == 0) {
            *region_out = zone_alias_map[i].region;
            *city_out = zone_alias_map[i].city;
            return;
        }
    }

    *region_out = NULL;
    *city_out = NULL;
}

static void create_list_item(const char *base_key, const char *label) {
    char *label_display = str_replace(label, "_", " ");

    ui_count++;

    lv_obj_t *ui_pnlTimezone = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(ui_pnlTimezone);
    lv_obj_set_user_data(ui_pnlTimezone, strdup(base_key));

    lv_obj_t *ui_lblTimezoneItem = lv_label_create(ui_pnlTimezone);
    apply_theme_list_item(&theme, ui_lblTimezoneItem, label_display);

    lv_obj_t *ui_lblTimezoneGlyph = lv_img_create(ui_pnlTimezone);
    apply_theme_list_glyph(&theme, ui_lblTimezoneGlyph, mux_module, "timezone");

    lv_group_add_obj(ui_group, ui_lblTimezoneItem);
    lv_group_add_obj(ui_group_glyph, ui_lblTimezoneGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlTimezone);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblTimezoneItem, ui_lblTimezoneGlyph, label_display);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblTimezoneItem);

    free(label_display);
}

static void get_region(const char *zone, char *dst) {
    const char *mapped = alias_lookup(zone, ALIAS_REGION);
    if (mapped) {
        snprintf(dst, ZONE_LABEL_MAX, "%s", mapped);
        return;
    }

    const char *slash = strchr(zone, '/');
    if (slash) {
        size_t len = (size_t) (slash - zone);
        if (len >= ZONE_LABEL_MAX) len = ZONE_LABEL_MAX - 1;
        memcpy(dst, zone, len);
        dst[len] = '\0';
    } else {
        snprintf(dst, ZONE_LABEL_MAX, "%s", ZONE_REGION_OTHER);
    }
}

static const char *get_city_label(const char *zone) {
    const char *mapped = alias_lookup(zone, ALIAS_CITY);
    if (mapped) return mapped;

    const char *slash = strchr(zone, '/');
    return slash ? slash + 1 : zone;
}

static void create_region_items(void) {
    lv_obj_clean(ui_pnlContent);
    reset_ui_groups();

    ui_count = 0;
    current_item_index = 0;

    char seen[ZONE_REGION_MAX][ZONE_LABEL_MAX];
    int seen_count = 0;

    for (size_t i = 0; timezone_location[i] != NULL; i++) {
        char region[ZONE_LABEL_MAX];
        get_region(timezone_location[i], region);

        int found = 0;
        for (int j = 0; j < seen_count; j++) {
            if (strcmp(seen[j], region) == 0) {
                found = 1;
                break;
            }
        }

        if (!found && seen_count < ZONE_REGION_MAX) snprintf(seen[seen_count++], ZONE_LABEL_MAX, "%s", region);
    }

    const char *sorted[ZONE_REGION_MAX];
    for (int i = 0; i < seen_count; i++) sorted[i] = seen[i];
    qsort(sorted, seen_count, sizeof(sorted[0]), str_compare);

    for (int i = 0; i < seen_count; i++) {
        const char *region = sorted[i];
        create_list_item(region, region);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    }
}

static void create_timezone_items(void) {
    lv_obj_clean(ui_pnlContent);
    reset_ui_groups();

    ui_count = 0;
    current_item_index = 0;

    const char *sorted[ZONE_REGION_MAX * 16];
    int sorted_count = 0;

    for (size_t i = 0; timezone_location[i] != NULL; i++) {
        const char *zone = timezone_location[i];

        const char *mapped_region = NULL;
        const char *mapped_city = NULL;
        alias_lookup_both(zone, &mapped_region, &mapped_city);

        char region[ZONE_LABEL_MAX];
        if (mapped_region) {
            snprintf(region, sizeof(region), "%s", mapped_region);
        } else {
            get_region(zone, region);
        }

        if (strcmp(region, zone_selected_region) != 0) continue;
        sorted[sorted_count++] = zone;
    }

    qsort(sorted, sorted_count, sizeof(sorted[0]), str_compare);

    for (int i = 0; i < sorted_count; i++) {
        const char *base_key = sorted[i];

        const char *mapped_region = NULL;
        const char *mapped_city = NULL;
        alias_lookup_both(base_key, &mapped_region, &mapped_city);

        const char *city_label = mapped_city ? mapped_city : get_city_label(base_key);
        create_list_item(base_key, city_label);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    }
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (zone_view == VIEW_REGION) {
        lv_obj_t *focused_panel = lv_group_get_focused(ui_group_panel);
        const char *region = (const char *) lv_obj_get_user_data(focused_panel);

        snprintf(zone_selected_region, sizeof(zone_selected_region), "%s", region);

        zone_region_index = current_item_index;
        zone_view = VIEW_CITY;

        play_sound(SND_CONFIRM);
        create_timezone_items();

        if (!ui_count) lv_label_set_text(ui_lblScreenMessage, lang.MUXTIMEZONE.NONE);

        first_open = 1;
        list_nav_next(0);

        return;
    }

    play_sound(SND_CONFIRM);
    toast_message(lang.MUXTIMEZONE.SAVE, FOREVER);

    lv_obj_t *focused_panel = lv_group_get_focused(ui_group_panel);
    const char *full_zone = (const char *) lv_obj_get_user_data(focused_panel);

    char zone_group[MAX_BUFFER_SIZE];
    snprintf(zone_group, sizeof(zone_group), "/usr/share/zoneinfo/%s", full_zone);

    unlink(LOCAL_TIME);
    if (symlink(zone_group, LOCAL_TIME) != 0) {
        LOG_ERROR(mux_module, "Failed to timezone symlink");
    }

    // Because weirdos live in different timezones...
    if (config.BOOT.FACTORY_RESET) {
        const char *args_date[] = {"date", "010100002026", NULL};
        run_exec(args_date, A_SIZE(args_date), 0, 1, NULL, NULL);

        const char *args_hw_clock[] = {"hwclock", "-w", NULL};
        run_exec(args_hw_clock, A_SIZE(args_hw_clock), 0, 1, NULL, NULL);
    }

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "timezone");
    refresh_config = 1;

    zone_view = VIEW_REGION;
    zone_selected_region[0] = '\0';

    close_input();
    mux_input_stop();
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

    if (zone_view == VIEW_CITY) {
        play_sound(SND_BACK);
        zone_view = VIEW_REGION;
        zone_selected_region[0] = '\0';
        create_region_items();
        first_open = 1;
        list_nav_next(zone_region_index);
        return;
    }

    play_sound(SND_BACK);

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

    overlay_display();
}

int muxtimezone_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTIMEZONE.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    create_region_items();

    if (!ui_count) lv_label_set_text(ui_lblScreenMessage, lang.MUXTIMEZONE.NONE);

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
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
