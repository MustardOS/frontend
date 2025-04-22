#include "muxshare.h"
#include "muxsysinfo.h"
#include "../lvgl/lvgl.h"
#include "ui/ui_muxsysinfo.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input/list_nav.h"



#define UI_COUNT 11
static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_mux_panels[5];

static char hostname[32];
static int tap_count = 0;

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblVersion_sysinfo,  lang.MUXSYSINFO.HELP.VERSION},
            {ui_lblDevice_sysinfo,   lang.MUXSYSINFO.HELP.DEVICE},
            {ui_lblKernel_sysinfo,   lang.MUXSYSINFO.HELP.KERNEL},
            {ui_lblUptime_sysinfo,   lang.MUXSYSINFO.HELP.UPTIME},
            {ui_lblCPU_sysinfo,      lang.MUXSYSINFO.HELP.CPU.INFO},
            {ui_lblSpeed_sysinfo,    lang.MUXSYSINFO.HELP.CPU.SPEED},
            {ui_lblGovernor_sysinfo, lang.MUXSYSINFO.HELP.CPU.GOV},
            {ui_lblMemory_sysinfo,   lang.MUXSYSINFO.HELP.MEMORY},
            {ui_lblTemp_sysinfo,     lang.MUXSYSINFO.HELP.TEMP},
            {ui_lblCapacity_sysinfo, lang.MUXSYSINFO.HELP.CAPACITY},
            {ui_lblVoltage_sysinfo,  lang.MUXSYSINFO.HELP.VOLTAGE},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

const char *get_cpu_model() {
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

const char *get_current_frequency() {
    static char buffer[32];
    char *freq_str = read_text_from_file("/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq");

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

const char *get_scaling_governor() {
    static char buffer[MAX_BUFFER_SIZE];
    char *governor_str = read_text_from_file("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor");

    if (!governor_str || governor_str[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lang.GENERIC.UNKNOWN);
        free(governor_str);
        return buffer;
    }

    snprintf(buffer, sizeof(buffer), "%s", governor_str);
    free(governor_str);
    return buffer;
}

const char *get_memory_usage() {
    char *result = get_execute_result("free -m | awk '/^Mem:/ {printf \"%.2f MB / %.2f MB\", $3, $2}'");
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    return result;
}

const char *get_temperature() {
    static char buffer[32];
    char *temp_str = read_text_from_file("/sys/class/thermal/thermal_zone0/temp");

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

const char *get_uptime() {
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

const char *get_battery_cap() {
    static char battery_cap[32];
    snprintf(battery_cap, sizeof(battery_cap), "%d%% (Offset: %d)",
             read_battery_capacity(),
             config.SETTINGS.ADVANCED.OFFSET - 50);
    return battery_cap;
}

const char *get_build_version() {
    static char build_version[32];
    snprintf(build_version, sizeof(build_version), "%s (%s)",
             str_replace(read_line_from_file((INTERNAL_PATH "config/version.txt"), 1), "_", " "),
             read_line_from_file((INTERNAL_PATH "config/version.txt"), 2));
    return build_version;
}

const char *get_device_info() {
    static char device_info[32];
    snprintf(device_info, sizeof(device_info), "%s",
             read_line_from_file((INTERNAL_PATH "config/device.txt"), 1));
    return device_info;
}

const char *get_kernel_version() {
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
    lv_label_set_text(ui_lblCPUValue_sysinfo, get_cpu_model());
    lv_label_set_text(ui_lblSpeedValue_sysinfo, get_current_frequency());
    lv_label_set_text(ui_lblGovernorValue_sysinfo, get_scaling_governor());
    lv_label_set_text(ui_lblMemoryValue_sysinfo, get_memory_usage());
    lv_label_set_text(ui_lblTempValue_sysinfo, get_temperature());
    lv_label_set_text(ui_lblCapacityValue_sysinfo, get_battery_cap());
    lv_label_set_text(ui_lblVoltageValue_sysinfo, read_battery_voltage());
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlVersion_sysinfo,
            ui_pnlDevice_sysinfo,
            ui_pnlKernel_sysinfo,
            ui_pnlUptime_sysinfo,
            ui_pnlCPU_sysinfo,
            ui_pnlSpeed_sysinfo,
            ui_pnlGovernor_sysinfo,
            ui_pnlMemory_sysinfo,
            ui_pnlTemp_sysinfo,
            ui_pnlCapacity_sysinfo,
            ui_pnlVoltage_sysinfo,
    };

    ui_objects[0] = ui_lblVersion_sysinfo;
    ui_objects[1] = ui_lblDevice_sysinfo;
    ui_objects[2] = ui_lblKernel_sysinfo;
    ui_objects[3] = ui_lblUptime_sysinfo;
    ui_objects[4] = ui_lblCPU_sysinfo;
    ui_objects[5] = ui_lblSpeed_sysinfo;
    ui_objects[6] = ui_lblGovernor_sysinfo;
    ui_objects[7] = ui_lblMemory_sysinfo;
    ui_objects[8] = ui_lblTemp_sysinfo;
    ui_objects[9] = ui_lblCapacity_sysinfo;
    ui_objects[10] = ui_lblVoltage_sysinfo;

    lv_obj_t *ui_objects_value[] = {
            ui_lblVersionValue_sysinfo,
            ui_lblDeviceValue_sysinfo,
            ui_lblKernelValue_sysinfo,
            ui_lblUptimeValue_sysinfo,
            ui_lblCPUValue_sysinfo,
            ui_lblSpeedValue_sysinfo,
            ui_lblGovernorValue_sysinfo,
            ui_lblMemoryValue_sysinfo,
            ui_lblTempValue_sysinfo,
            ui_lblCapacityValue_sysinfo,
            ui_lblVoltageValue_sysinfo
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoVersion_sysinfo,
            ui_icoDevice_sysinfo,
            ui_icoKernel_sysinfo,
            ui_icoUptime_sysinfo,
            ui_icoCPU_sysinfo,
            ui_icoSpeed_sysinfo,
            ui_icoGovernor_sysinfo,
            ui_icoMemory_sysinfo,
            ui_icoTemp_sysinfo,
            ui_icoCapacity_sysinfo,
            ui_icoVoltage_sysinfo
    };

    apply_theme_list_panel(ui_pnlVersion_sysinfo);
    apply_theme_list_panel(ui_pnlDevice_sysinfo);
    apply_theme_list_panel(ui_pnlKernel_sysinfo);
    apply_theme_list_panel(ui_pnlUptime_sysinfo);
    apply_theme_list_panel(ui_pnlCPU_sysinfo);
    apply_theme_list_panel(ui_pnlSpeed_sysinfo);
    apply_theme_list_panel(ui_pnlGovernor_sysinfo);
    apply_theme_list_panel(ui_pnlMemory_sysinfo);
    apply_theme_list_panel(ui_pnlTemp_sysinfo);
    apply_theme_list_panel(ui_pnlCapacity_sysinfo);
    apply_theme_list_panel(ui_pnlVoltage_sysinfo);

    apply_theme_list_item(&theme, ui_lblVersion_sysinfo, lang.MUXSYSINFO.VERSION);
    apply_theme_list_item(&theme, ui_lblDevice_sysinfo, lang.MUXSYSINFO.DEVICE);
    apply_theme_list_item(&theme, ui_lblKernel_sysinfo, lang.MUXSYSINFO.KERNEL);
    apply_theme_list_item(&theme, ui_lblUptime_sysinfo, lang.MUXSYSINFO.UPTIME);
    apply_theme_list_item(&theme, ui_lblCPU_sysinfo, lang.MUXSYSINFO.CPU.INFO);
    apply_theme_list_item(&theme, ui_lblSpeed_sysinfo, lang.MUXSYSINFO.CPU.SPEED);
    apply_theme_list_item(&theme, ui_lblGovernor_sysinfo, lang.MUXSYSINFO.CPU.GOV);
    apply_theme_list_item(&theme, ui_lblMemory_sysinfo, lang.MUXSYSINFO.MEMORY.INFO);
    apply_theme_list_item(&theme, ui_lblTemp_sysinfo, lang.MUXSYSINFO.TEMP);
    apply_theme_list_item(&theme, ui_lblCapacity_sysinfo, lang.MUXSYSINFO.CAPACITY);
    apply_theme_list_item(&theme, ui_lblVoltage_sysinfo, lang.MUXSYSINFO.VOLTAGE);

    apply_theme_list_glyph(&theme, ui_icoVersion_sysinfo, mux_module, "version");
    apply_theme_list_glyph(&theme, ui_icoDevice_sysinfo, mux_module, "device");
    apply_theme_list_glyph(&theme, ui_icoKernel_sysinfo, mux_module, "kernel");
    apply_theme_list_glyph(&theme, ui_icoUptime_sysinfo, mux_module, "uptime");
    apply_theme_list_glyph(&theme, ui_icoCPU_sysinfo, mux_module, "cpu");
    apply_theme_list_glyph(&theme, ui_icoSpeed_sysinfo, mux_module, "speed");
    apply_theme_list_glyph(&theme, ui_icoGovernor_sysinfo, mux_module, "governor");
    apply_theme_list_glyph(&theme, ui_icoMemory_sysinfo, mux_module, "memory");
    apply_theme_list_glyph(&theme, ui_icoTemp_sysinfo, mux_module, "temp");
    apply_theme_list_glyph(&theme, ui_icoCapacity_sysinfo, mux_module, "capacity");
    apply_theme_list_glyph(&theme, ui_icoVoltage_sysinfo, mux_module, "voltage");

    apply_theme_list_value(&theme, ui_lblVersionValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblDeviceValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblKernelValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblUptimeValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblCPUValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblSpeedValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblGovernorValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblMemoryValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblTempValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblCapacityValue_sysinfo, "");
    apply_theme_list_value(&theme, ui_lblVoltageValue_sysinfo, "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (!current_item_index) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void handle_a() {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) == ui_lblVersion_sysinfo) {
        play_sound("muos", nav_sound, 0, 0);

        switch (tap_count) {
            case 5:
                toast_message(
                        "\x57\x68\x61\x74\x20"
                        "\x64\x6F\x20\x79\x6F"
                        "\x75\x20\x77\x61\x6E"
                        "\x74\x3F",
                        1000, 1000
                );
                break;
            case 10:
                toast_message(
                        "\x59\x6F\x75\x20\x73"
                        "\x75\x72\x65\x20\x61"
                        "\x72\x65\x20\x70\x65"
                        "\x72\x73\x69\x73\x74"
                        "\x65\x6E\x74\x21",
                        1000, 1000
                );
                break;
            case 20:
                toast_message(
                        "\x57\x68\x61\x74\x20"
                        "\x61\x72\x65\x20\x79"
                        "\x6F\x75\x20\x65\x78"
                        "\x70\x65\x63\x74\x69"
                        "\x6E\x67\x3F",
                        1000, 1000
                );
                break;
            case 30:
                toast_message(
                        "\x4F\x6B\x61\x79\x20"
                        "\x6C\x69\x73\x74\x65"
                        "\x6E\x20\x68\x65\x72"
                        "\x65\x20\x79\x6F\x75"
                        "\x2E\x2E\x2E",
                        1000, 1000
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
                        1000, 1000
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
                        1000, 1000
                );
                break;
            default:
                toast_message(
                        "\x54\x68\x61\x6E\x6B"
                        "\x20\x79\x6F\x75\x20"
                        "\x66\x6F\x72\x20\x75"
                        "\x73\x69\x6E\x67\x20"
                        "\x6D\x75\x4F\x53\x21",
                        1000, 1000
                );
                break;
        }

        if (tap_count > 50) {
            srandom(time(NULL));

            char s_rotate_str[8], s_zoom_str[8];
            snprintf(s_rotate_str, sizeof(s_rotate_str), "%d",
                     (int) (random() % 181) + 35);
            snprintf(s_zoom_str, sizeof(s_zoom_str), "%.2f",
                     (float) ((float[]) {0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f})[random() % 7]);

            write_text_to_file("/run/muos/device/screen/s_rotate", "w", CHAR, s_rotate_str);
            write_text_to_file("/run/muos/device/screen/s_zoom", "w", CHAR, s_zoom_str);

            load_mux("launcher");
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "");

            safe_quit(0);
            mux_input_stop();
        }

        tap_count++;
    }

    if (lv_group_get_focused(ui_group) == ui_lblMemory_sysinfo) {
        write_text_to_file("/proc/sys/vm/drop_caches", "w", INT, 3);
        toast_message(lang.MUXSYSINFO.MEMORY.DROP, 1000, 1000);
    }

    if (lv_group_get_focused(ui_group) == ui_lblKernel_sysinfo) {
        toast_message(hostname, 1000, 1000);
    }
}

static void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "system");

    safe_quit(0);
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblVersion_sysinfo, "version");
    lv_obj_set_user_data(ui_lblDevice_sysinfo, "device");
    lv_obj_set_user_data(ui_lblKernel_sysinfo, "kernel");
    lv_obj_set_user_data(ui_lblUptime_sysinfo, "uptime");
    lv_obj_set_user_data(ui_lblCPU_sysinfo, "cpu");
    lv_obj_set_user_data(ui_lblSpeed_sysinfo, "speed");
    lv_obj_set_user_data(ui_lblGovernor_sysinfo, "governor");
    lv_obj_set_user_data(ui_lblMemory_sysinfo, "memory");
    lv_obj_set_user_data(ui_lblTemp_sysinfo, "temp");
    lv_obj_set_user_data(ui_lblCapacity_sysinfo, "capacity");
    lv_obj_set_user_data(ui_lblVoltage_sysinfo, "voltage");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxsysinfo_main() {
    
    snprintf(mux_module, sizeof(mux_module), "muxsysinfo");
    
            
    init_theme(1, 0);
    
    init_ui_common_screen(&theme, &device, &lang, lang.MUXSYSINFO.TITLE);
    init_muxsysinfo(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);

    update_system_info();

    load_kiosk(&kiosk);

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
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
