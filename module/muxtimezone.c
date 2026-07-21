#include "muxshare.h"

#define ZONE_REGION_OTHER "Other"
#define ZONE_REGION_MAX   64
#define ZONE_LABEL_MAX    128

typedef enum {
    view_region, // Region groups
    view_city,   // Cities inside a chosen region
} zone_view_t;

typedef enum {
    alias_region,
    alias_city,
} alias_field_t;

static zone_view_t zone_view = view_region;
static char zone_selected_region[ZONE_LABEL_MAX] = {0};
static int zone_region_index = 0;

static void show_help(void) {
    show_info_box(lang.muxtimezone.title, lang.muxtimezone.help, 0);
}

// This is currently the best I can come up with considering timezones
// are god damn awful and the way we present timezones in muX is using
// regions and then cities within.  If this needs changes let us know!
static const struct {
    const char *alias;
    const char *region;
    const char *city;
} zone_alias_map[] = {
    {"Cuba", "America", "Havana"},
    {"Egypt", "Africa", "Cairo"},
    {"Eire", "Europe", "Dublin"},
    {"Hongkong", "Asia", "Hong_Kong"},
    {"Iceland", "Atlantic", "Reykjavik"},
    {"Iran", "Asia", "Tehran"},
    {"Israel", "Asia", "Jerusalem"},
    {"Jamaica", "America", "Jamaica"},
    {"Japan", "Asia", "Tokyo"},
    {"Kwajalein", "Pacific", "Kwajalein"},
    {"Libya", "Africa", "Tripoli"},
    {"Navajo", "America", "Denver"},
    {"Poland", "Europe", "Warsaw"},
    {"Portugal", "Europe", "Lisbon"},
    {"Singapore", "Asia", "Singapore"},
    {"Turkey", "Europe", "Istanbul"},
    {NULL, NULL, NULL}
};

static const char *alias_lookup(const char *zone, const alias_field_t field) {
    for (int i = 0; zone_alias_map[i].alias != NULL; i++) {
        if (strcmp(zone, zone_alias_map[i].alias) == 0) {
            return field == alias_region ? zone_alias_map[i].region : zone_alias_map[i].city;
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

    ui_count_static++;

    lv_obj_t *ui_pnl_timezone = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(ui_pnl_timezone);
    set_owned_user_data(ui_pnl_timezone, strdup(base_key));

    lv_obj_t *ui_lbl_timezone_item = lv_label_create(ui_pnl_timezone);
    apply_theme_list_item(&theme, ui_lbl_timezone_item, label_display);

    lv_obj_t *ui_lbl_timezone_glyph = lv_img_create(ui_pnl_timezone);
    apply_theme_list_glyph(&theme, ui_lbl_timezone_glyph, mux_module, "timezone");

    lv_group_add_obj(ui_group, ui_lbl_timezone_item);
    lv_group_add_obj(ui_group_glyph, ui_lbl_timezone_glyph);
    lv_group_add_obj(ui_group_panel, ui_pnl_timezone);

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_timezone_item, ui_lbl_timezone_glyph, label_display);
    apply_text_long_dot(&theme, ui_lbl_timezone_item);

    free(label_display);
}

static void get_region(const char *zone, char *dst) {
    const char *mapped = alias_lookup(zone, alias_region);
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
    const char *mapped = alias_lookup(zone, alias_city);
    if (mapped) return mapped;

    const char *slash = strchr(zone, '/');
    return slash ? slash + 1 : zone;
}

static void create_region_items(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
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
    for (int i = 0; i < seen_count; i++)
        sorted[i] = seen[i];
    qsort(sorted, seen_count, sizeof(sorted[0]), str_compare);

    for (int i = 0; i < seen_count; i++) {
        const char *region = sorted[i];
        create_list_item(region, region);
    }

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    }
}

static void create_timezone_items(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
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

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    }
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (zone_view == view_region) {
        lv_obj_t *e_focused = lv_group_get_focused(ui_group_panel);
        const char *region = lv_obj_get_user_data(e_focused);

        snprintf(zone_selected_region, sizeof(zone_selected_region), "%s", region);

        zone_region_index = current_item_index;
        zone_view = view_city;

        play_sound(snd_confirm);
        create_timezone_items();

        if (!ui_count_static) lv_label_set_text(ui_lbl_screen_message, lang.muxtimezone.none);

        first_open = 1;
        gen_step_movement(0, +1, 1, 0, 1);

        return;
    }

    play_sound(snd_confirm);
    toast_message(lang.muxtimezone.save, tst_wait_f);

    lv_obj_t *e_focused = lv_group_get_focused(ui_group_panel);
    const char *full_zone = lv_obj_get_user_data(e_focused);

    char zone_group[MAX_BUFFER_SIZE];
    snprintf(zone_group, sizeof(zone_group), "/usr/share/zoneinfo/%s", full_zone);

    unlink(LOCAL_TIME);
    if (symlink(zone_group, LOCAL_TIME) != 0) {
        LOG_ERROR(mux_module, "Failed to timezone symlink");
    }

    // Because weirdos live in different timezones...
    if (config.boot.factory_reset) {
        const char *args_date[] = {"date", "010100002026", NULL};
        run_exec(args_date, A_SIZE(args_date), 0, 1, NULL, NULL);

        const char *args_hw_clock[] = {"hwclock", "-w", NULL};
        run_exec(args_hw_clock, A_SIZE(args_hw_clock), 0, 1, NULL, NULL);
    }

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "timezone");
    refresh_config = 1;

    zone_view = view_region;
    zone_selected_region[0] = '\0';

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (zone_view == view_city) {
        play_sound(snd_back);
        zone_view = view_region;
        zone_selected_region[0] = '\0';
        create_region_items();
        first_open = 1;
        gen_step_movement(zone_region_index, +1, 1, 0, 1);
        return;
    }

    play_sound(snd_back);

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxtimezone_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxtimezone.title);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_region_items();

    if (!ui_count_static) lv_label_set_text(ui_lbl_screen_message, lang.muxtimezone.none);

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 1, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
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

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
