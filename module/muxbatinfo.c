#include "muxshare.h"
#include "ui/ui_muxbatinfo.h"
#include "../common/battery.h"

#define BATINFO(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(BATINFO_ELEMENTS)
};
#undef BATINFO

#define UI_BUFFER 128
#define RUN_BATT "/run/muos/battery"

static void show_help(void) {
    struct help_msg help_messages[] = {
#define BATINFO(NAME, ENUM, UDATA) { UDATA, lang.MUXBATINFO.HELP.ENUM },
            BATINFO_ELEMENTS
#undef BATINFO
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void get_bat_base_dir(char *out) {
    snprintf(out, MAX_BUFFER_SIZE, "%s", device.BATTERY.CAPACITY);
    char *slash = strrchr(out, '/');
    if (slash) *slash = '\0';
}

static int read_bat_file_trim(const char *path, char *out, size_t out_sz) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    if (!fgets(out, (int) out_sz, fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *start = out;
    while (*start && isspace((unsigned char) *start)) start++;

    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1])) end--;

    size_t len = (size_t) (end - start);
    if (start != out) memmove(out, start, len);

    out[len] = '\0';

    return (len > 0) ? 0 : -1;
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
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    }

    return buffer;
}

static const char *get_bat_health(void) {
    static char buffer[UI_BUFFER];

    if (read_bat_file_trim(device.BATTERY.HEALTH, buffer, sizeof(buffer)) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    }

    return buffer;
}

static const char *get_bat_design_cap(void) {
    static char buffer[UI_BUFFER];

    if (device.BATTERY.SIZE > 0) {
        snprintf(buffer, sizeof(buffer), "%d mAh", device.BATTERY.SIZE);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    }

    return buffer;
}

static const char *get_last_charged(void) {
    static char buffer[UI_BUFFER];
    char ts_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/last_charged", ts_str, sizeof(ts_str)) != 0 || ts_str[0] == '\0') return "-";

    long ts = strtol(ts_str, NULL, 10);
    if (ts <= 0) return "-";

    time_t t = (time_t) ts;
    struct tm *tm_info = localtime(&t);
    if (!tm_info) return "-";

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm_info);
    return buffer;
}

static const char *get_time_on_battery(void) {
    static char buffer[UI_BUFFER];
    char secs_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/time_on_battery", secs_str, sizeof(secs_str)) != 0 || secs_str[0] == '\0') return "-";

    long secs = strtol(secs_str, NULL, 10);
    if (secs <= 0) return "-";

    long hours = secs / 3600;
    long mins = (secs % 3600) / 60;

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

    if (read_bat_file_trim(RUN_BATT "_usage/unplug_capacity", unplug_str, sizeof(unplug_str)) != 0 || unplug_str[0] == '\0') return "-";
    if (read_bat_file_trim(RUN_BATT "/capacity", curr_str, sizeof(curr_str)) != 0 || curr_str[0] == '\0') return "-";

    int unplug = (int) strtol(unplug_str, NULL, 10);
    int curr = (int) strtol(curr_str, NULL, 10);

    if (unplug <= 0 || unplug > 100 || curr < 0 || curr > 100) return "-";

    int used = unplug - curr;
    if (used < 0) used = 0;

    snprintf(buffer, sizeof(buffer), "%d%%", used);
    return buffer;
}

static const char *get_bat_charger(void) {
    return battery_is_charging() ? lang.GENERIC.ONLINE : lang.GENERIC.OFFLINE;
}

static int battery_used_available(void) {
    char buf[32];
    return read_bat_file_trim(RUN_BATT "_usage/unplug_capacity", buf, sizeof(buf)) == 0
           && buf[0] != '\0';
}

static void update_battery_info() {
    lv_label_set_text(ui_lblCapacityValue_batinfo, get_bat_capacity());
    lv_label_set_text(ui_lblVoltageValue_batinfo, get_bat_voltage());
    lv_label_set_text(ui_lblStatusValue_batinfo, get_bat_status());
    lv_label_set_text(ui_lblHealthValue_batinfo, get_bat_health());
    lv_label_set_text(ui_lblLastChargedValue_batinfo, get_last_charged());
    lv_label_set_text(ui_lblTimeOnBatteryValue_batinfo, get_time_on_battery());
    lv_label_set_text(ui_lblBatteryUsedValue_batinfo, get_battery_used());
    lv_label_set_text(ui_lblChargerValue_batinfo, get_bat_charger());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, batinfo, Capacity, lang.MUXBATINFO.CAPACITY, "capacity", get_bat_capacity());
    INIT_VALUE_ITEM(-1, batinfo, Voltage, lang.MUXBATINFO.VOLTAGE, "voltage", get_bat_voltage());
    INIT_VALUE_ITEM(-1, batinfo, Status, lang.MUXBATINFO.STATUS, "status", get_bat_status());
    INIT_VALUE_ITEM(-1, batinfo, Health, lang.MUXBATINFO.HEALTH, "health", get_bat_health());
    INIT_VALUE_ITEM(-1, batinfo, DesignCap, lang.MUXBATINFO.DESIGN_CAP, "designcap", get_bat_design_cap());
    INIT_VALUE_ITEM(-1, batinfo, LastCharged, lang.MUXBATINFO.LAST_CHARGED, "lastcharged", get_last_charged());
    INIT_VALUE_ITEM(-1, batinfo, TimeOnBattery, lang.MUXBATINFO.TIME_ON_BATTERY, "timeonbattery", get_time_on_battery());
    INIT_VALUE_ITEM(-1, batinfo, BatteryUsed, lang.MUXBATINFO.BATTERY_USED, "batteryused", get_battery_used());
    INIT_VALUE_ITEM(-1, batinfo, Charger, lang.MUXBATINFO.CHARGER, "charger", get_bat_charger());

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!battery_used_available()) HIDE_VALUE_ITEM(batinfo, BatteryUsed);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, 0, 0, 1);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_back(void) {
    play_sound(SND_BACK);
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
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

#define BATINFO(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_batinfo, UDATA);
    BATINFO_ELEMENTS
#undef BATINFO

    overlay_display();
}

int muxbatinfo_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXBATINFO.TITLE);
    init_muxbatinfo(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, update_battery_info);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
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
