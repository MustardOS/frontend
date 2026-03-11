#include <stdio.h>
#include <stdlib.h>

#include "battery.h"
#include "common.h"
#include "ui_common.h"
#include "init.h"
#include "config.h"
#include "device.h"
#include "theme.h"
#include "options.h"
#include "log.h"

#define BATTERY_DEVICE_CONFIG "/opt/muos/device/config/battery"
#define BATTERY_RUN_DIRECTORY "/run/muos/battery"

#define BATTERY_CAPACITY_FILE BATTERY_RUN_DIRECTORY "/capacity"
#define BATTERY_VOLTAGE_FILE  BATTERY_RUN_DIRECTORY "/voltage"
#define BATTERY_CHARGING_FILE BATTERY_RUN_DIRECTORY "/charging"

#define BATTERY_CAPACITY_PREFIX "capacity_"
#define BATTERY_CHARGING_PREFIX "capacity_charging_"

#define BATTERY_VOLTAGE_OFFSET 16

#define BATTERY_ZERO_VOLTAGE "0.00 V"
#define BATTERY_GOOD_VOLTAGE "%.2f V"

#define BATTERY_PERCENT_JUMP      4
#define BATTERY_PERCENT_STEP_UP   2
#define BATTERY_PERCENT_STEP_DOWN 1

static int battery_daemon_mode = 0;

static char daemon_voltage_path[256];
static char daemon_capacity_path[256];
static char daemon_charger_path[256];

static int daemon_volt_min = 0;
static int daemon_volt_max = 0;

static const char *voltage_path;
static const char *capacity_path;

static int battery_percent;
static int battery_percent_valid;

static char voltage_string[10] = BATTERY_ZERO_VOLTAGE;

static int last_ui_percent = -1;
static int last_ui_charging = -1;

static int last_written_capacity = -1;
static int last_written_voltage = -1;
static int last_written_charging = -1;

void battery_set_daemon_mode(int enable) {
    battery_daemon_mode = enable;
}

static void load_daemon_battery_config(void) {
    daemon_charger_path[0] = '\0';
    daemon_capacity_path[0] = '\0';
    daemon_voltage_path[0] = '\0';

    const char *battery_file;

    battery_file = read_line_char_from(BATTERY_DEVICE_CONFIG "/charger", 1);
    if (battery_file) snprintf(daemon_charger_path, sizeof(daemon_charger_path), "%s", battery_file);

    battery_file = read_line_char_from(BATTERY_DEVICE_CONFIG "/capacity", 1);
    if (battery_file) snprintf(daemon_capacity_path, sizeof(daemon_capacity_path), "%s", battery_file);

    battery_file = read_line_char_from(BATTERY_DEVICE_CONFIG "/voltage", 1);
    if (battery_file) snprintf(daemon_voltage_path, sizeof(daemon_voltage_path), "%s", battery_file);

    daemon_volt_min = read_line_int_from(BATTERY_DEVICE_CONFIG "/volt_min", 0);
    daemon_volt_max = read_line_int_from(BATTERY_DEVICE_CONFIG "/volt_max", 0);
}

static void write_capacity_file(int percent) {
    if (!battery_daemon_mode) return;
    if (percent == last_written_capacity) return;

    write_text_to_file(BATTERY_CAPACITY_FILE, "w", INT, percent);
    last_written_capacity = percent;
}

static void write_voltage_file(int mv) {
    if (!battery_daemon_mode) return;
    if (mv == last_written_voltage) return;

    write_text_to_file(BATTERY_VOLTAGE_FILE, "w", INT, mv);
    last_written_voltage = mv;
}

static void write_charging_file(int charging) {
    if (!battery_daemon_mode) return;
    if (charging == last_written_charging) return;

    write_text_to_file(BATTERY_CHARGING_FILE, "w", INT, charging);
    last_written_charging = charging;
}

static inline int clamp_percent(int p) {
    if (p < 0) return 0;
    if (p > 100) return 100;

    return p;
}

static inline int abs_int(int v) {
    return (v < 0) ? -v : v;
}

static inline int is_charging(void) {
    const char *path = battery_daemon_mode ? daemon_charger_path : device.BATTERY.CHARGER;
    return (path && *path && read_all_long_from(path)) ? 1 : 0;
}

static void set_voltage_string(int mv) {
    snprintf(voltage_string, sizeof(voltage_string), BATTERY_GOOD_VOLTAGE, (double) mv / 1000.0);
}

static inline int percent_to_glyph(int percent) {
    percent = clamp_percent(percent);
    if (percent >= 95) return 100;

    return (percent / 10) * 10;
}

static void set_battery_state(int mv, int percent) {
    battery_percent = clamp_percent(percent);
    set_voltage_string(mv);

    write_capacity_file(battery_percent);
    write_voltage_file(mv);
}

static void set_battery_state_unknown(void) {
    battery_percent = 0;
    battery_percent_valid = 0;

    set_voltage_string(0);

    write_capacity_file(0);
    write_voltage_file(0);
    write_charging_file(0);
}

static void init_battery_paths(void) {
    if (battery_daemon_mode) {
        load_daemon_battery_config();

        voltage_path = daemon_voltage_path;
        capacity_path = daemon_capacity_path;
    } else {
        voltage_path = device.BATTERY.VOLTAGE;
        capacity_path = device.BATTERY.CAPACITY;
    }

    LOG_INFO("battery", "Voltage Path: %s", voltage_path ? voltage_path : "Not Available");
    LOG_INFO("battery", "Capacity Path: %s", capacity_path ? capacity_path : "Not Available");
}

static int read_voltage_mv(void) {
    if (!voltage_path || !*voltage_path) return 0;

    unsigned long long raw = read_all_long_from(voltage_path);
    if (raw == 0) return 0;

    return (int) (raw / 1000ULL);
}

static int voltage_to_percent(int mv) {
    const int volt_min = battery_daemon_mode ? daemon_volt_min : device.BATTERY.VOLT_MIN;
    const int volt_max = battery_daemon_mode ? daemon_volt_max : device.BATTERY.VOLT_MAX;

    if (mv <= volt_min + BATTERY_VOLTAGE_OFFSET) return 0;
    if (mv >= volt_max - BATTERY_VOLTAGE_OFFSET) return 100;

    return ((mv - volt_min) * 100) / (volt_max - volt_min);
}

static int stabilise_percent(int raw_percent, int charging) {
    raw_percent = clamp_percent(raw_percent);

    if (!battery_percent_valid) {
        battery_percent_valid = 1;
        return raw_percent;
    }

    if (abs_int(raw_percent - battery_percent) <= BATTERY_PERCENT_JUMP) return battery_percent;

    if (charging) {
        if (raw_percent > battery_percent + BATTERY_PERCENT_STEP_UP) return battery_percent + BATTERY_PERCENT_STEP_UP;
        if (raw_percent < battery_percent - BATTERY_PERCENT_STEP_DOWN) return battery_percent - BATTERY_PERCENT_STEP_DOWN;

        return raw_percent;
    }

    if (raw_percent < battery_percent - BATTERY_PERCENT_STEP_DOWN) return battery_percent - BATTERY_PERCENT_STEP_DOWN;
    if (raw_percent > battery_percent + BATTERY_PERCENT_STEP_UP) return battery_percent + BATTERY_PERCENT_STEP_UP;

    return raw_percent;
}

void battery_reset(void) {
    battery_percent = 0;
    battery_percent_valid = 0;

    last_ui_percent = -1;
    last_ui_charging = -1;

    last_written_capacity = -1;
    last_written_voltage = -1;
    last_written_charging = -1;

    set_voltage_string(0);
}

void battery_init(void) {
    create_directories(BATTERY_RUN_DIRECTORY, 0);

    init_battery_paths();
    battery_reset();
    battery_update();
}

void battery_update(void) {
    const int charging = is_charging();
    write_charging_file(charging);

    int mv = read_voltage_mv();
    if (mv > 0) {
        write_voltage_file(mv);

        int percent = voltage_to_percent(mv);
        percent = stabilise_percent(percent, charging);

        set_battery_state(mv, percent);
        return;
    }

    set_battery_state_unknown();
}

const char *battery_get_voltage(void) {
    return voltage_string;
}

int battery_get_capacity(void) {
    unsigned long long val = read_all_long_from(BATTERY_CAPACITY_FILE);
    return clamp_percent((int) val);
}

int battery_get_low_threshold(void) {
    return clamp_percent(config.SETTINGS.POWER.LOW_BATTERY);
}

int battery_is_low(void) {
    return battery_get_capacity() <= battery_get_low_threshold();
}

int battery_is_charging(void) {
    return read_all_long_from(BATTERY_CHARGING_FILE) ? 1 : 0;
}

char *battery_get_capacity_glyph(void) {
    static char capacity[MAX_BUFFER_SIZE];

    int percent = battery_get_capacity();
    int glyph = percent_to_glyph(percent);

    const char *prefix = battery_is_charging() ? BATTERY_CHARGING_PREFIX : BATTERY_CAPACITY_PREFIX;

    snprintf(capacity, sizeof(capacity), "%s%d", prefix, glyph);
    return capacity;
}

void battery_capacity_task(lv_timer_t *timer) {
    LV_UNUSED(timer);

    if (!ui_staCapacity || !lv_obj_is_valid(ui_staCapacity)) return;

    const int percent = battery_get_capacity();
    const int charging = battery_is_charging();

    if (percent == last_ui_percent && charging == last_ui_charging) return;

    last_ui_percent = percent;
    last_ui_charging = charging;

    update_battery_capacity(ui_staCapacity, &theme);
}
