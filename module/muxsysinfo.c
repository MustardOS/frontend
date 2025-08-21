#include "muxshare.h"
#include "ui/ui_muxsysinfo.h"

#define UI_COUNT  11

static char hostname[32];
static int tap_count = 0;

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblVersion_sysinfo,  lang.MUXSYSINFO.HELP.VERSION},
            {ui_lblDevice_sysinfo,   lang.MUXSYSINFO.HELP.DEVICE},
            {ui_lblKernel_sysinfo,   lang.MUXSYSINFO.HELP.KERNEL},
            {ui_lblUptime_sysinfo,   lang.MUXSYSINFO.HELP.UPTIME},
            {ui_lblCpu_sysinfo,      lang.MUXSYSINFO.HELP.CPU.INFO},
            {ui_lblSpeed_sysinfo,    lang.MUXSYSINFO.HELP.CPU.SPEED},
            {ui_lblGovernor_sysinfo, lang.MUXSYSINFO.HELP.CPU.GOVERNOR},
            {ui_lblMemory_sysinfo,   lang.MUXSYSINFO.HELP.MEMORY},
            {ui_lblTemp_sysinfo,     lang.MUXSYSINFO.HELP.TEMP},
            {ui_lblCapacity_sysinfo, lang.MUXSYSINFO.HELP.CAPACITY},
            {ui_lblVoltage_sysinfo,  lang.MUXSYSINFO.HELP.VOLTAGE},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

const char *get_cpu_model(void) {
    static char cpu_model[48];
    snprintf(cpu_model, sizeof(cpu_model), "%s",
             get_execute_result("lscpu | grep 'Model name:' | awk -F: '{print $2}'"));

    static char cpu_cores[6];
    snprintf(cpu_cores, sizeof(cpu_cores), "%s",
             get_execute_result("lscpu | grep '^CPU(s):' | awk '{print $2}'"));

    str_remchar(cpu_model, ' ');
    str_remchar(cpu_cores, ' ');

    if (strlen(cpu_model) == 0) return lang.GENERIC.UNKNOWN;

    static char result[MAX_BUFFER_SIZE];
    if (strlen(cpu_cores) > 0) {
        snprintf(result, sizeof(result), "%s (%s)", cpu_model, cpu_cores);
    } else {
        snprintf(result, sizeof(result), "%s", cpu_model);
    }

    return result;
}

const char *get_current_frequency(void) {
    static char buffer[32];
    char *freq_str = read_all_char_from("/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq");

    if (!freq_str || freq_str[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
        free(freq_str);
        return buffer;
    }

    char *end;
    double frequency = strtod(freq_str, &end);

    errno = 0;
    if (errno != 0 || end == freq_str || *end != '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f MHz", frequency / 1000.0);
    }

    free(freq_str);
    return buffer;
}

const char *get_scaling_governor(void) {
    static char buffer[MAX_BUFFER_SIZE];
    char *governor_str = read_all_char_from("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor");

    if (!governor_str || governor_str[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
        free(governor_str);
        return buffer;
    }

    snprintf(buffer, sizeof(buffer), "%s", governor_str);
    free(governor_str);
    return buffer;
}

const char *get_memory_usage(void) {
    char *result = get_execute_result("free -m | awk '/^Mem:/ {printf \"%.2f MB / %.2f MB\", $3, $2}'");
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    return result;
}

const char *get_temperature(void) {
    static char buffer[32];
    char *temp_str = read_all_char_from("/sys/class/thermal/thermal_zone0/temp");

    if (!temp_str || temp_str[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
        free(temp_str);
        return buffer;
    }

    char *end;
    double temperature = strtod(temp_str, &end);

    errno = 0;
    if (errno != 0 || end == temp_str || *end != '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f°C", temperature / 1000.0);
    }

    free(temp_str);
    return buffer;
}

const char *get_uptime(void) {
    static char formatted_uptime[MAX_BUFFER_SIZE];
    struct sysinfo info;

    if (sysinfo(&info) == 0) {
        long total_minutes = info.uptime / 60;
        long days = total_minutes / (24 * 60);
        long hours = (total_minutes % (24 * 60)) / 60;
        long minutes = total_minutes % 60;

        if (days > 0) {
            snprintf(formatted_uptime, sizeof(formatted_uptime), "%ld %s%s %ld %s%s %ld %s%s",
                     days, lang.MUXRTC.DAY, (days == 1) ? "" : "s",
                     hours, lang.MUXRTC.HOUR, (hours == 1) ? "" : "s",
                     minutes, lang.MUXRTC.MINUTE, (minutes == 1) ? "" : "s");
        } else if (hours > 0) {
            snprintf(formatted_uptime, sizeof(formatted_uptime), "%ld %s%s %ld %s%s",
                     hours, lang.MUXRTC.HOUR, (hours == 1) ? "" : "s",
                     minutes, lang.MUXRTC.MINUTE, (minutes == 1) ? "" : "s");
        } else {
            snprintf(formatted_uptime, sizeof(formatted_uptime), "%ld %s%s",
                     minutes, lang.MUXRTC.MINUTE, (minutes == 1) ? "" : "s");
        }
    } else {
        snprintf(formatted_uptime, sizeof(formatted_uptime), "%s", lang.GENERIC.UNKNOWN);
    }

    return formatted_uptime;
}

const char *get_battery_cap(void) {
    static char battery_cap[32];
    snprintf(battery_cap, sizeof(battery_cap), "%d%% (Offset: %d)",
             read_battery_capacity(),
             config.SETTINGS.ADVANCED.OFFSET - 50);
    return battery_cap;
}

const char *get_device_info(void) {
    static char device_info[32];
    snprintf(device_info, sizeof(device_info), "%s",
             str_toupper(read_line_char_from((CONF_DEVICE_PATH "board/name"), 1)));
    return device_info;
}

const char *get_kernel_version(void) {
    static char buffer[128];
    struct utsname sys_info;

    if (uname(&sys_info) == 0) {
        snprintf(buffer, sizeof(buffer), "%s %s (%s)", sys_info.sysname, sys_info.release, sys_info.machine);
        snprintf(hostname, sizeof(hostname), "%s", sys_info.nodename);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
    }

    return buffer;
}

static void update_system_info() {
    lv_label_set_text(ui_lblVersionValue_sysinfo, get_build_version());
    lv_label_set_text(ui_lblDeviceValue_sysinfo, get_device_info());
    lv_label_set_text(ui_lblKernelValue_sysinfo, get_kernel_version());
    lv_label_set_text(ui_lblUptimeValue_sysinfo, get_uptime());
    lv_label_set_text(ui_lblCpuValue_sysinfo, get_cpu_model());
    lv_label_set_text(ui_lblSpeedValue_sysinfo, get_current_frequency());
    lv_label_set_text(ui_lblGovernorValue_sysinfo, get_scaling_governor());
    lv_label_set_text(ui_lblMemoryValue_sysinfo, get_memory_usage());
    lv_label_set_text(ui_lblTempValue_sysinfo, get_temperature());
    lv_label_set_text(ui_lblCapacityValue_sysinfo, get_battery_cap());
    lv_label_set_text(ui_lblVoltageValue_sysinfo, read_battery_voltage());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, sysinfo, Version, lang.MUXSYSINFO.VERSION, "version", get_build_version());
    INIT_VALUE_ITEM(-1, sysinfo, Device, lang.MUXSYSINFO.DEVICE, "device", get_device_info());
    INIT_VALUE_ITEM(-1, sysinfo, Kernel, lang.MUXSYSINFO.KERNEL, "kernel", get_kernel_version());
    INIT_VALUE_ITEM(-1, sysinfo, Uptime, lang.MUXSYSINFO.UPTIME, "uptime", get_uptime());
    INIT_VALUE_ITEM(-1, sysinfo, Cpu, lang.MUXSYSINFO.CPU.INFO, "cpu", get_cpu_model());
    INIT_VALUE_ITEM(-1, sysinfo, Speed, lang.MUXSYSINFO.CPU.SPEED, "speed", get_current_frequency());
    INIT_VALUE_ITEM(-1, sysinfo, Governor, lang.MUXSYSINFO.CPU.GOVERNOR, "governor", get_scaling_governor());
    INIT_VALUE_ITEM(-1, sysinfo, Memory, lang.MUXSYSINFO.MEMORY.INFO, "memory", get_memory_usage());
    INIT_VALUE_ITEM(-1, sysinfo, Temp, lang.MUXSYSINFO.TEMP, "temp", get_temperature());
    INIT_VALUE_ITEM(-1, sysinfo, Capacity, lang.MUXSYSINFO.CAPACITY, "capacity", get_battery_cap());
    INIT_VALUE_ITEM(-1, sysinfo, Voltage, lang.MUXSYSINFO.VOLTAGE, "voltage", read_battery_voltage());

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) == ui_lblVersion_sysinfo) {
        play_sound(SND_MUOS);

        switch (tap_count) {
            case 5:
                toast_message(
                        "\x57\x68\x61\x74\x20"
                        "\x64\x6F\x20\x79\x6F"
                        "\x75\x20\x77\x61\x6E"
                        "\x74\x3F",
                        1000
                );
                break;
            case 10:
                toast_message(
                        "\x59\x6F\x75\x20\x73"
                        "\x75\x72\x65\x20\x61"
                        "\x72\x65\x20\x70\x65"
                        "\x72\x73\x69\x73\x74"
                        "\x65\x6E\x74\x21",
                        1000
                );
                break;
            case 20:
                toast_message(
                        "\x57\x68\x61\x74\x20"
                        "\x61\x72\x65\x20\x79"
                        "\x6F\x75\x20\x65\x78"
                        "\x70\x65\x63\x74\x69"
                        "\x6E\x67\x3F",
                        1000
                );
                break;
            case 30:
                toast_message(
                        "\x4F\x6B\x61\x79\x20"
                        "\x6C\x69\x73\x74\x65"
                        "\x6E\x20\x68\x65\x72"
                        "\x65\x20\x79\x6F\x75"
                        "\x2E\x2E\x2E",
                        1000
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
                        1000
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
                        1000
                );
                break;
            default:
                toast_message(
                        "\x54\x68\x61\x6E\x6B"
                        "\x20\x79\x6F\x75\x20"
                        "\x66\x6F\x72\x20\x75"
                        "\x73\x69\x6E\x67\x20"
                        "\x6D\x75\x4F\x53\x21",
                        1000
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

            refresh_device = 1;

            load_mux("launcher");
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "");

            close_input();
            mux_input_stop();
        }

        tap_count++;
    }

    if (lv_group_get_focused(ui_group) == ui_lblMemory_sysinfo) {
        write_text_to_file("/proc/sys/vm/drop_caches", "w", INT, 3);
        toast_message(lang.MUXSYSINFO.MEMORY.DROP, 1000);
    }

    if (lv_group_get_focused(ui_group) == ui_lblKernel_sysinfo) {
        toast_message(hostname, 1000);
    }

    refresh_screen(ui_screen);
}

static void handle_b(void) {
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
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void launch_device(void) {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) == ui_lblDevice_sysinfo) {
        load_mux("device");

        close_input();
        mux_input_stop();
    }
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
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

#define SYSINFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_sysinfo, UDATA);
    SYSINFO_ELEMENTS
#undef SYSINFO

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

int muxsysinfo_main(void) {
    init_module("muxsysinfo");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSYSINFO.TITLE);
    init_muxsysinfo(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_refresh_task, update_system_info);

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
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
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
