#include "muxshare.h"
#include "ui/ui_muxbatinfo.h"
#include "../common/battery.h"

#define BATINFO(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(BATINFO_ELEMENTS) };
#undef BATINFO

#define UI_BUFFER 128
#define RUN_BATT  "/run/muos/battery"

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define BATINFO(NAME, UDATA) {UDATA, lang.muxbatinfo.help.NAME},
        BATINFO_ELEMENTS
#undef BATINFO
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void get_bat_base_dir(char *out) {
    snprintf(out, MAX_BUFFER_SIZE, "%s", device.battery.capacity);
    char *slash = strrchr(out, '/');
    if (slash) *slash = '\0';
}

static int read_bat_file_trim(const char *path, char *out, const size_t out_sz) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    if (!fgets(out, (int) out_sz, fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    const char *start = out;
    while (*start && isspace((unsigned char) *start))
        start++;

    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1]))
        end--;

    const size_t len = (size_t) (end - start);
    if (start != out) memmove(out, start, len);

    out[len] = '\0';

    return len > 0 ? 0 : -1;
}

static const char *get_bat_capacity(void) {
    static char buffer[UI_BUFFER];
    snprintf(buffer, sizeof(buffer), "%d%%", battery_get_capacity());

    return buffer;
}

static const char *get_bat_voltage(void) {
    return battery_get_voltage();
}

static const char *get_bat_status(void) {
    static char buffer[UI_BUFFER];
    char base[MAX_BUFFER_SIZE];
    char path[MAX_BUFFER_SIZE];

    get_bat_base_dir(base);
    snprintf(path, sizeof(path), "%s/status", base);

    if (read_bat_file_trim(path, buffer, sizeof(buffer)) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_bat_health(void) {
    static char buffer[UI_BUFFER];

    if (read_bat_file_trim(device.battery.health, buffer, sizeof(buffer)) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_bat_design_cap(void) {
    static char buffer[UI_BUFFER];

    if (device.battery.size > 0) {
        snprintf(buffer, sizeof(buffer), "%d mAh", device.battery.size);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_last_charged(void) {
    static char buffer[UI_BUFFER];
    char ts_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/last_charged", ts_str, sizeof(ts_str)) != 0 || ts_str[0] == '\0')
        return "-";

    const long ts = strtol(ts_str, NULL, 10);
    if (ts <= 0) return "-";

    const time_t t = ts;
    struct tm *tm_info = localtime(&t);
    if (!tm_info) return "-";

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm_info);
    return buffer;
}

static const char *get_time_on_battery(void) {
    static char buffer[UI_BUFFER];
    char secs_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/time_on_battery", secs_str, sizeof(secs_str)) != 0 || secs_str[0] == '\0')
        return "-";

    const long secs = strtol(secs_str, NULL, 10);
    if (secs <= 0) return "-";

    const long hours = secs / 3600;
    const long mins = secs % 3600 / 60;

    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%ldh %ldm", hours, mins);
    } else if (mins > 0) {
        snprintf(buffer, sizeof(buffer), "%ldm", mins);
    } else {
        snprintf(buffer, sizeof(buffer), "< 1m");
    }

    return buffer;
}

static const char *get_battery_used(void) {
    static char buffer[UI_BUFFER];
    char unplug_str[32];
    char curr_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/unplug_capacity", unplug_str, sizeof(unplug_str)) != 0
        || unplug_str[0] == '\0')
        return "-";
    if (read_bat_file_trim(RUN_BATT "/capacity", curr_str, sizeof(curr_str)) != 0 || curr_str[0] == '\0') return "-";

    const int unplug = (int) strtol(unplug_str, NULL, 10);
    const int curr = (int) strtol(curr_str, NULL, 10);

    if (unplug <= 0 || unplug > 100 || curr < 0 || curr > 100) return "-";

    int used = unplug - curr;
    if (used < 0) used = 0;

    snprintf(buffer, sizeof(buffer), "%d%%", used);
    return buffer;
}

static const char *get_bat_charger(void) {
    return battery_is_charging() ? lang.generic.online : lang.generic.offline;
}

static int battery_used_available(void) {
    char buf[32];
    return read_bat_file_trim(RUN_BATT "_usage/unplug_capacity", buf, sizeof(buf)) == 0 && buf[0] != '\0';
}

static void update_battery_info(const lv_timer_t *timer) {
    (void) timer;
    lv_label_set_text(ui_val_capacity_batinfo, get_bat_capacity());
    lv_label_set_text(ui_val_voltage_batinfo, get_bat_voltage());
    lv_label_set_text(ui_val_status_batinfo, get_bat_status());
    lv_label_set_text(ui_val_health_batinfo, get_bat_health());
    lv_label_set_text(ui_val_last_charged_batinfo, get_last_charged());
    lv_label_set_text(ui_val_time_on_battery_batinfo, get_time_on_battery());
    lv_label_set_text(ui_val_battery_used_batinfo, get_battery_used());
    lv_label_set_text(ui_val_charger_batinfo, get_bat_charger());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, batinfo, capacity, lang.muxbatinfo.capacity, "capacity", get_bat_capacity());
    INIT_VALUE_ITEM(-1, batinfo, voltage, lang.muxbatinfo.voltage, "voltage", get_bat_voltage());
    INIT_VALUE_ITEM(-1, batinfo, status, lang.muxbatinfo.status, "status", get_bat_status());
    INIT_VALUE_ITEM(-1, batinfo, health, lang.muxbatinfo.health, "health", get_bat_health());
    INIT_VALUE_ITEM(-1, batinfo, design_cap, lang.muxbatinfo.design_cap, "designcap", get_bat_design_cap());
    INIT_VALUE_ITEM(-1, batinfo, last_charged, lang.muxbatinfo.last_charged, "lastcharged", get_last_charged());
    INIT_VALUE_ITEM(
        -1, batinfo, time_on_battery, lang.muxbatinfo.time_on_battery, "timeonbattery", get_time_on_battery()
    );
    INIT_VALUE_ITEM(-1, batinfo, battery_used, lang.muxbatinfo.battery_used, "batteryused", get_battery_used());
    INIT_VALUE_ITEM(-1, batinfo, charger, lang.muxbatinfo.charger, "charger", get_bat_charger());

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!battery_used_available()) HIDE_VALUE_ITEM(batinfo, battery_used);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 0, 0, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_back(void) {
    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "batinfo");

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    handle_back();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});

#define BATINFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_batinfo, UDATA);
    BATINFO_ELEMENTS
#undef BATINFO

    overlay_display();
}

int muxbatinfo_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxbatinfo.title);
    init_muxbatinfo(ui_screen, ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, update_battery_info);
    list_nav_next(0);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
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

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
