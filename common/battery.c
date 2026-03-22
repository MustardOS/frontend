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

#define BATTERY_VOLT_MIN BATTERY_DEVICE_CONFIG "/volt_min"
#define BATTERY_VOLT_MAX BATTERY_DEVICE_CONFIG "/volt_max"

#define BATTERY_CURVE_FILE BATTERY_DEVICE_CONFIG "/curve"

#define BATTERY_CAPACITY_PREFIX "capacity_"
#define BATTERY_CHARGING_PREFIX "capacity_charging_"

#define BATTERY_CURVE_POINTS         10
#define BATTERY_LOAD_COMPENSATION_MV 20
#define BATTERY_VOLTAGE_DEADBAND_MV  8

#define BATTERY_ZERO_VOLTAGE "0.00 V"
#define BATTERY_GOOD_VOLTAGE "%.2f V"

#define BATTERY_PERCENT_JUMP      4
#define BATTERY_PERCENT_STEP_UP   2
#define BATTERY_PERCENT_STEP_DOWN 1

#define BATTERY_VOLTAGE_SAMPLES 5
#define BATTERY_WARMUP_CYCLES   3

typedef struct {
    int mv;
} battery_curve_point_t;

static int battery_daemon_mode = 0;

static char daemon_voltage_path[256];
static char daemon_capacity_path[256];
static char daemon_charger_path[256];

static int battery_volt_min = 0;
static int battery_volt_max = 0;

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

static int filtered_voltage_mv = 0;
static int last_percent_source_mv = -1;
static int battery_warmup = BATTERY_WARMUP_CYCLES;

static int voltage_samples[BATTERY_VOLTAGE_SAMPLES];
static int voltage_sample_index = 0;
static int voltage_sample_count = 0;

static battery_curve_point_t battery_curve[BATTERY_CURVE_POINTS];

static unsigned char *battery_lookup = NULL;
static int battery_lookup_size = 0;
static int battery_curve_valid = 0;

static int load_curve_file(const char *path, battery_curve_point_t *curve) {
    if (!*path) return 0;

    char *data = read_all_char_from(path);
    if (!data) return 0;

    char *p = data;
    int i = 0;

    while (*p && i < BATTERY_CURVE_POINTS) {
        char *end;
        long mv = strtol(p, &end, 10);

        if (p == end) {
            while (*p && *p != '\n') p++;
            if (*p) p++;
            continue;
        }

        curve[i].mv = (int) mv;
        i++;
        p = end;
    }

    free(data);

    if (i != BATTERY_CURVE_POINTS) return 0;
    for (i = 1; i < BATTERY_CURVE_POINTS; i++) if (curve[i].mv >= curve[i - 1].mv) return 0;

    return 1;
}

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
    battery_percent_valid = 1;
    set_voltage_string(mv);

    write_capacity_file(battery_percent);
    write_voltage_file(mv);
}

static void set_battery_state_unknown(void) {
    battery_percent = 0;
    battery_percent_valid = 0;

    filtered_voltage_mv = 0;
    last_percent_source_mv = -1;

    voltage_sample_index = 0;
    voltage_sample_count = 0;

    battery_warmup = BATTERY_WARMUP_CYCLES;

    set_voltage_string(0);

    write_capacity_file(0);
    write_voltage_file(0);
    write_charging_file(0);
}

static void build_battery_lookup(void) {
    free(battery_lookup);

    battery_lookup = NULL;
    battery_lookup_size = 0;

    if (!battery_curve_valid || battery_volt_max <= battery_volt_min) return;

    battery_lookup_size = battery_volt_max - battery_volt_min + 1;
    battery_lookup = malloc((size_t) battery_lookup_size);

    if (!battery_lookup) {
        LOG_ERROR("battery", "Failed to allocate battery lookup");
        battery_lookup_size = 0;
        return;
    }

    int mv;
    for (mv = battery_volt_min; mv <= battery_volt_max; mv++) {
        int percent = 0;
        int i;

        if (mv >= battery_curve[0].mv) {
            percent = 100;
        } else if (mv <= battery_curve[BATTERY_CURVE_POINTS - 1].mv) {
            percent = 0;
        } else {
            for (i = 0; i < BATTERY_CURVE_POINTS - 1; i++) {
                int high_mv = battery_curve[i].mv;
                int low_mv = battery_curve[i + 1].mv;

                if (mv <= high_mv && mv >= low_mv) {
                    int low_pct = 100 - ((i + 1) * 10);
                    int span_mv = high_mv - low_mv;

                    if (span_mv <= 0) {
                        percent = low_pct;
                    } else {
                        int high_pct = 100 - (i * 10);
                        int span_pct = high_pct - low_pct;
                        int pos_mv = mv - low_mv;

                        percent = low_pct + (pos_mv * span_pct + span_mv / 2) / span_mv;
                    }

                    break;
                }
            }
        }

        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;

        battery_lookup[mv - battery_volt_min] = (unsigned char) percent;
    }

    LOG_INFO("battery", "Battery lookup built (%d entries)", battery_lookup_size);
}

static void init_battery_paths(void) {
    load_daemon_battery_config();

    if (battery_daemon_mode) {
        voltage_path = daemon_voltage_path;
        capacity_path = daemon_capacity_path;
    } else {
        voltage_path = BATTERY_VOLTAGE_FILE;
        capacity_path = BATTERY_CAPACITY_FILE;
    }

    LOG_INFO("battery", "Voltage Path: %s", voltage_path ? voltage_path : "Not Available");
    LOG_INFO("battery", "Capacity Path: %s", capacity_path ? capacity_path : "Not Available");
}

static void init_battery_curves(void) {
    battery_curve_valid = load_curve_file(BATTERY_CURVE_FILE, battery_curve);
    if (!battery_curve_valid) LOG_ERROR("battery", "Battery curve failed to load");

    battery_volt_min = read_line_int_from(BATTERY_VOLT_MIN, 1);
    battery_volt_max = read_line_int_from(BATTERY_VOLT_MAX, 1);

    if (battery_volt_min <= 0) battery_volt_min = 3300;
    if (battery_volt_max <= 0) battery_volt_max = 4100;

    LOG_INFO("battery", "Voltage Min: %d", battery_volt_min);
    LOG_INFO("battery", "Voltage Max: %d", battery_volt_max);
}

static int read_voltage_mv(void) {
    if (!voltage_path || !*voltage_path) return 0;

    unsigned long long raw = read_all_long_from(voltage_path);
    if (raw == 0) return 0;

    if (battery_daemon_mode) return (int) (raw / 1000ULL);
    return (int) raw;
}

static inline void swap_int(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

static int median_voltage(int mv) {
    int a, b, c, d, e; // Now I know my ABCs...

    voltage_samples[voltage_sample_index++] = mv;
    if (voltage_sample_index >= BATTERY_VOLTAGE_SAMPLES) voltage_sample_index = 0;
    if (voltage_sample_count < BATTERY_VOLTAGE_SAMPLES) voltage_sample_count++;
    if (voltage_sample_count < BATTERY_VOLTAGE_SAMPLES) return mv;

    a = voltage_samples[0];
    b = voltage_samples[1];
    c = voltage_samples[2];
    d = voltage_samples[3];
    e = voltage_samples[4];

    if (a > b) swap_int(&a, &b);
    if (d > e) swap_int(&d, &e);
    if (a > c) swap_int(&a, &c);
    if (b > c) swap_int(&b, &c);
    if (a > d) swap_int(&a, &d);
    if (c > d) swap_int(&c, &d);
    if (b > e) swap_int(&b, &e);
    if (b > c) swap_int(&b, &c);

    return c;
}

static int voltage_to_percent(int mv, int charging) {
    if (!charging) mv += BATTERY_LOAD_COMPENSATION_MV;
    if (!battery_lookup || battery_lookup_size <= 0) return 0;

    if (mv < battery_volt_min) mv = battery_volt_min;
    if (mv > battery_volt_max) mv = battery_volt_max;

    return battery_lookup[mv - battery_volt_min];
}

static int stabilise_percent(int raw_percent, int charging) {
    raw_percent = clamp_percent(raw_percent);

    if (!battery_percent_valid) {
        battery_percent_valid = 1;
        return raw_percent;
    }

    if (!charging && raw_percent > battery_percent) raw_percent = battery_percent;
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

    filtered_voltage_mv = 0;
    last_percent_source_mv = -1;
    battery_warmup = BATTERY_WARMUP_CYCLES;

    voltage_sample_index = 0;
    voltage_sample_count = 0;

    set_voltage_string(0);
}

void battery_init(void) {
    create_directories(BATTERY_RUN_DIRECTORY, 0);

    init_battery_paths();
    init_battery_curves();

    build_battery_lookup();

    battery_reset();
    battery_update();
}

void battery_update(void) {
    const int charging = is_charging();
    write_charging_file(charging);

    int mv = read_voltage_mv();
    int percent;

    if (mv <= 0) {
        set_battery_state_unknown();
        return;
    }

    write_voltage_file(mv);

    mv = median_voltage(mv);
    filtered_voltage_mv = mv;

    set_voltage_string(filtered_voltage_mv);

    if (battery_warmup > 0) {
        battery_warmup--;
        if (battery_percent_valid) {
            write_capacity_file(battery_percent);
            write_voltage_file(filtered_voltage_mv);
            return;
        }
    }

    if (last_percent_source_mv >= 0 &&
        abs_int(filtered_voltage_mv - last_percent_source_mv) < BATTERY_VOLTAGE_DEADBAND_MV) {
        if (battery_percent_valid) {
            write_capacity_file(battery_percent);
            write_voltage_file(filtered_voltage_mv);
            return;
        }
    }

    percent = voltage_to_percent(filtered_voltage_mv, charging);
    percent = stabilise_percent(percent, charging);

    last_percent_source_mv = filtered_voltage_mv;
    set_battery_state(filtered_voltage_mv, percent);
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
