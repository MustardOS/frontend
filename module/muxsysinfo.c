#include "muxshare.h"
#include "ui/ui_muxsysinfo.h"

#define UI_COUNT 15
#define UI_BUFFER 128

static char hostname[32];
static int tap_count = 0;

static struct sysinfo sysinfo_cache;

static void show_help(void) {
    struct help_msg help_messages[] = {
#define SYSINFO(NAME, ENUM, UDATA) { UDATA, lang.MUXSYSINFO.HELP.ENUM },
            SYSINFO_ELEMENTS
#undef SYSINFO
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int read_file_trim(const char *path, char *out) {
    if (!path || !out) return -1;

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    if (!fgets(out, UI_BUFFER, fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    char *start = out;
    while (*start && isspace((unsigned char) *start)) start++;

    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char) *(end - 1))) --end;
    *end = '\0';

    if (start != out) memmove(out, start, end - start + 1);

    return (out[0] != '\0') ? 0 : -1;
}

static int read_ll_from_file(const char *path, unsigned long long *val) {
    char buffer[UI_BUFFER];

    if (read_file_trim(path, buffer) != 0) return -1;

    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(buffer, &end, 10);

    if (errno != 0 || end == buffer || *end != '\0') return -1;

    *val = v;
    return 0;
}

const char *get_cpu_model(void) {
    static char cached[UI_BUFFER];
    static int cached_ok = 0;

    if (cached_ok) return cached;

    FILE *fp = popen("lscpu", "r");
    if (!fp) return lang.GENERIC.UNKNOWN;

    char line[256];
    char cpu_model[128] = {0};
    char cpu_cores[16] = {0};

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Model name:", 11) == 0) {
            char *value = line + 11;
            while (*value && isspace((unsigned char) *value)) value++;

            char *end = value + strlen(value);
            while (end > value && isspace((unsigned char) *(end - 1))) --end;
            *end = '\0';

            snprintf(cpu_model, sizeof(cpu_model), "%s", value);
        } else if (strncmp(line, "CPU(s):", 7) == 0) {
            char *value = line + 7;

            while (*value && isspace((unsigned char) *value)) value++;

            char *end = value + strlen(value);
            while (end > value && isspace((unsigned char) *(end - 1))) --end;
            *end = '\0';

            snprintf(cpu_cores, sizeof(cpu_cores), "%s", value);
        }

        if (cpu_model[0] && cpu_cores[0]) break;
    }

    pclose(fp);

    if (cpu_model[0] == '\0') return lang.GENERIC.UNKNOWN;

    if (cpu_cores[0])
        snprintf(cached, sizeof(cached), "%s (%s)", cpu_model, cpu_cores);
    else
        snprintf(cached, sizeof(cached), "%s", cpu_model);

    cached_ok = 1;
    return cached;
}

const char *get_current_frequency(void) {
    static char buffer[UI_BUFFER];

    const char *paths[] = {
            "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq",
            "/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq",
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
            "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",
    };

    unsigned long long khz = 0;

    for (size_t i = 0; i < A_SIZE(paths); i++) {
        if (read_ll_from_file(paths[i], &khz) == 0) break;
    }

    if (khz == 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
        return buffer;
    }

    snprintf(buffer, sizeof(buffer), "%.2f MHz", (double) khz / 1000.0);
    return buffer;
}

const char *get_scaling_governor(void) {
    static char buffer[UI_BUFFER];

    if (read_file_trim(device.CPU.GOVERNOR, buffer) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    }

    return buffer;
}

const char *get_memory_usage(void) {
    static char buffer[UI_BUFFER];

    const unsigned long long unit = sysinfo_cache.mem_unit;
    const unsigned long long total = (unsigned long long) sysinfo_cache.totalram * unit;
    const unsigned long long free = (unsigned long long) sysinfo_cache.freeram * unit;
    const unsigned long long used = (total > free) ? (total - free) : 0;

    snprintf(buffer, sizeof(buffer), "%.2f MB / %.2f MB",
             (double) used / 1048576.0, (double) total / 1048576.0);

    return buffer;
}

const char *get_swap_usage(void) {
    static char buffer[UI_BUFFER];

    if (sysinfo_cache.totalswap == 0) {
        snprintf(buffer, sizeof(buffer), "0.00 MB / 0.00 MB");
        return buffer;
    }

    const unsigned long long unit = sysinfo_cache.mem_unit;
    const unsigned long long total = (unsigned long long) sysinfo_cache.totalswap * unit;
    const unsigned long long free = (unsigned long long) sysinfo_cache.freeswap * unit;
    const unsigned long long used = (total > free) ? (total - free) : 0;

    snprintf(buffer, sizeof(buffer), "%.2f MB / %.2f MB",
             (double) used / 1048576.0, (double) total / 1048576.0);

    return buffer;
}

const char *get_temperature(void) {
    static char buffer[UI_BUFFER];

    const char *paths[] = {
            "/sys/class/thermal/thermal_zone0/temp",
            "/sys/class/thermal/thermal_zone1/temp",
    };

    unsigned long long milli_c = 0;

    for (size_t i = 0; i < A_SIZE(paths); i++) {
        if (read_ll_from_file(paths[i], &milli_c) == 0) break;
    }

    if (milli_c == 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
        return buffer;
    }

    snprintf(buffer, sizeof(buffer), "%.2f°C", (double) milli_c / 1000.0);
    return buffer;
}

static const char *get_system_uptime(void) {
    static char buffer[UI_BUFFER];

    long total_minutes = sysinfo_cache.uptime / 60;
    long days = total_minutes / (24 * 60);
    long hours = (total_minutes % (24 * 60)) / 60;
    long minutes = total_minutes % 60;

    if (days > 0) {
        snprintf(buffer, sizeof(buffer), "%ld %s%s %ld %s%s %ld %s%s",
                 days, lang.MUXRTC.DAY, (days == 1) ? "" : "s",
                 hours, lang.MUXRTC.HOUR, (hours == 1) ? "" : "s",
                 minutes, lang.MUXRTC.MINUTE, (minutes == 1) ? "" : "s");
    } else if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%ld %s%s %ld %s%s",
                 hours, lang.MUXRTC.HOUR, (hours == 1) ? "" : "s",
                 minutes, lang.MUXRTC.MINUTE, (minutes == 1) ? "" : "s");
    } else {
        snprintf(buffer, sizeof(buffer), "%ld %s%s",
                 minutes, lang.MUXRTC.MINUTE, (minutes == 1) ? "" : "s");
    }

    return buffer;
}

const char *get_battery_cap(void) {
    static char battery_cap[UI_BUFFER];
    snprintf(battery_cap, sizeof(battery_cap), "%d%% (Offset: %d)",
             read_battery_capacity(), config.SETTINGS.ADVANCED.OFFSET);
    return battery_cap;
}

const char *get_device_info(void) {
    static char device_info[UI_BUFFER];
    snprintf(device_info, sizeof(device_info), "%s",
             str_toupper(read_line_char_from((CONF_DEVICE_PATH "board/name"), 1)));
    return device_info;
}

const char *get_kernel_version(void) {
    static char cached[UI_BUFFER];
    static int cached_ok = 0;

    if (cached_ok) return cached;

    struct utsname sys_info;

    if (uname(&sys_info) == 0) {
        snprintf(cached, sizeof(cached), "%s %s (%s)",
                 sys_info.sysname, sys_info.release, sys_info.machine);

        snprintf(hostname, sizeof(hostname), "%s", sys_info.nodename);
        cached_ok = 1;
    } else {
        snprintf(cached, sizeof(cached), "%s", lang.GENERIC.UNKNOWN);
    }

    return cached;
}

const char *get_charger_status(void) {
    static char buffer[UI_BUFFER];

    if (file_exist(device.BATTERY.CHARGER)) {
        const char *status = read_line_int_from(device.BATTERY.CHARGER, 1)
                             ? lang.GENERIC.ONLINE
                             : lang.GENERIC.OFFLINE;

        snprintf(buffer, sizeof(buffer), "%s", status);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    }

    return buffer;
}

static void update_system_info() {
    sysinfo(&sysinfo_cache);

    lv_label_set_text(ui_lblUptimeValue_sysinfo, get_system_uptime());
    lv_label_set_text(ui_lblSpeedValue_sysinfo, get_current_frequency());
    lv_label_set_text(ui_lblGovernorValue_sysinfo, get_scaling_governor());
    lv_label_set_text(ui_lblMemoryValue_sysinfo, get_memory_usage());
    lv_label_set_text(ui_lblSwapValue_sysinfo, get_swap_usage());
    lv_label_set_text(ui_lblTempValue_sysinfo, get_temperature());
    lv_label_set_text(ui_lblCapacityValue_sysinfo, get_battery_cap());
    lv_label_set_text(ui_lblVoltageValue_sysinfo, read_battery_voltage());
    lv_label_set_text(ui_lblChargerValue_sysinfo, get_charger_status());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, sysinfo, Version, lang.MUXSYSINFO.VERSION, "version", get_version(verify_check));
    INIT_VALUE_ITEM(-1, sysinfo, Build, lang.MUXSYSINFO.BUILD, "build", get_build());
    INIT_VALUE_ITEM(-1, sysinfo, Device, lang.MUXSYSINFO.DEVICE, "device", get_device_info());
    INIT_VALUE_ITEM(-1, sysinfo, Kernel, lang.MUXSYSINFO.KERNEL, "kernel", get_kernel_version());
    INIT_VALUE_ITEM(-1, sysinfo, Uptime, lang.MUXSYSINFO.UPTIME, "uptime", get_system_uptime());
    INIT_VALUE_ITEM(-1, sysinfo, Cpu, lang.MUXSYSINFO.CPU.INFO, "cpu", get_cpu_model());
    INIT_VALUE_ITEM(-1, sysinfo, Speed, lang.MUXSYSINFO.CPU.SPEED, "speed", get_current_frequency());
    INIT_VALUE_ITEM(-1, sysinfo, Governor, lang.MUXSYSINFO.CPU.GOVERNOR, "governor", get_scaling_governor());
    INIT_VALUE_ITEM(-1, sysinfo, Memory, lang.MUXSYSINFO.MEMORY.INFO, "memory", get_memory_usage());
    INIT_VALUE_ITEM(-1, sysinfo, Swap, lang.MUXSYSINFO.SWAP, "swap", get_swap_usage());
    INIT_VALUE_ITEM(-1, sysinfo, Temp, lang.MUXSYSINFO.TEMP, "temp", get_temperature());
    INIT_VALUE_ITEM(-1, sysinfo, Capacity, lang.MUXSYSINFO.CAPACITY, "capacity", get_battery_cap());
    INIT_VALUE_ITEM(-1, sysinfo, Voltage, lang.MUXSYSINFO.VOLTAGE, "voltage", read_battery_voltage());
    INIT_VALUE_ITEM(-1, sysinfo, Charger, lang.MUXSYSINFO.CHARGER, "charger", get_charger_status());
    INIT_VALUE_ITEM(-1, sysinfo, Reload, lang.MUXSYSINFO.RELOAD, "reload", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblVersion_sysinfo) {
        toast_message(verify_check ? lang.GENERIC.MODIFIED : lang.GENERIC.CLEAN, SHORT);
    }

    if (element_focused == ui_lblBuild_sysinfo) {
        play_sound(SND_MUOS);

        switch (tap_count) {
            case 5:
                toast_message(
                        "\x57\x68\x61\x74\x20"
                        "\x64\x6F\x20\x79\x6F"
                        "\x75\x20\x77\x61\x6E"
                        "\x74\x3F",
                        SHORT
                );
                break;
            case 10:
                toast_message(
                        "\x59\x6F\x75\x20\x73"
                        "\x75\x72\x65\x20\x61"
                        "\x72\x65\x20\x70\x65"
                        "\x72\x73\x69\x73\x74"
                        "\x65\x6E\x74\x21",
                        SHORT
                );
                break;
            case 20:
                toast_message(
                        "\x57\x68\x61\x74\x20"
                        "\x61\x72\x65\x20\x79"
                        "\x6F\x75\x20\x65\x78"
                        "\x70\x65\x63\x74\x69"
                        "\x6E\x67\x3F",
                        SHORT
                );
                break;
            case 30:
                toast_message(
                        "\x4F\x6B\x61\x79\x20"
                        "\x6C\x69\x73\x74\x65"
                        "\x6E\x20\x68\x65\x72"
                        "\x65\x20\x79\x6F\x75"
                        "\x2E\x2E\x2E",
                        SHORT
                );
                break;
            case 40:
                toast_message(
                        "\x54\x68\x69\x73\x20"
                        "\x69\x73\x20\x79\x6F"
                        "\x75\x72\x20\x6C\x61"
                        "\x73\x74\x20\x77\x61"
                        "\x72\x6E\x69\x6E\x67"
                        "\x21",
                        SHORT
                );
                break;
            case 50:
                toast_message(
                        "\x4F\x6B\x61\x79\x20"
                        "\x77\x65\x6C\x6C\x20"
                        "\x79\x6F\x75\x20\x61"
                        "\x73\x6B\x65\x64\x20"
                        "\x66\x6F\x72\x20\x69"
                        "\x74",
                        SHORT
                );
                break;
            default:
                toast_message(
                        "\x54\x68\x61\x6E\x6B"
                        "\x20\x79\x6F\x75\x20"
                        "\x66\x6F\x72\x20\x75"
                        "\x73\x69\x6E\x67\x20"
                        "\x6D\x75\x4F\x53\x21",
                        SHORT
                );
                break;
        }

        if (tap_count > 50) {
            tap_count = 0;
            srandom(time(NULL));

            char s_rotate_str[8], s_zoom_str[8];
            snprintf(s_rotate_str, sizeof(s_rotate_str), "%d",
                     (int) (random() % 181) + 35);
            snprintf(s_zoom_str, sizeof(s_zoom_str), "%.2f",
                     (float) ((float[]) {0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f})[random() % 7]);

            write_text_to_file(CONF_DEVICE_PATH "screen/s_rotate", "w", CHAR, s_rotate_str);
            write_text_to_file(CONF_DEVICE_PATH "screen/s_zoom", "w", CHAR, s_zoom_str);

            refresh_config = 1;
            refresh_device = 1;
            refresh_kiosk = 1;
            refresh_resolution = 1;

            load_mux("launcher");
            if (file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);

            close_input();
            mux_input_stop();
        }

        tap_count++;
    }

    if (element_focused == ui_lblMemory_sysinfo) {
        write_text_to_file("/proc/sys/vm/drop_caches", "w", INT, 3);
        toast_message(lang.MUXSYSINFO.MEMORY.DROP, MEDIUM);
    }

    if (element_focused == ui_lblKernel_sysinfo) {
        toast_message(hostname, MEDIUM);
    }

    if (element_focused == ui_lblReload_sysinfo) {
        toast_message(lang.MUXSYSINFO.RELOAD_RUN, FOREVER);

        refresh_config = 1;
        refresh_device = 1;
        refresh_kiosk = 1;
        refresh_resolution = 1;

        if (file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);
        load_mux("launcher");

        close_input();
        mux_input_stop();
    }

    refresh_screen(ui_screen, 1);
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "sysinfo");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void launch_device(void) {
    if (msgbox_active || hold_call) return;

    if (lv_group_get_focused(ui_group) == ui_lblDevice_sysinfo) {
        load_mux("device");

        close_input();
        mux_input_stop();
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

#define SYSINFO(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_sysinfo, UDATA);
    SYSINFO_ELEMENTS
#undef SYSINFO

    overlay_display();
}

int muxsysinfo_main(void) {
    verify_check = script_hash_check();

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSYSINFO.TITLE);
    init_muxsysinfo(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, update_system_info);
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
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_L2) | BIT(MUX_INPUT_X),
                            .press_handler = launch_device,
                            .hold_handler = launch_device,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright_up,
                            .hold_handler = ui_common_handle_bright_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright_down,
                            .hold_handler = ui_common_handle_bright_down,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright_up,
                            .hold_handler = ui_common_handle_bright_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright_down,
                            .hold_handler = ui_common_handle_bright_down,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_volume_up,
                            .hold_handler = ui_common_handle_volume_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_volume_down,
                            .hold_handler = ui_common_handle_volume_down,
                    },
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    return 0;
}
