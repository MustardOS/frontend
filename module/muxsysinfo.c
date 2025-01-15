#include "../lvgl/lvgl.h"
#include "ui/ui_muxsysinfo.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 11
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_mux_panels[5];

static char hostname[32];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblVersion,  lang.MUXSYSINFO.HELP.VERSION},
            {ui_lblDevice,   lang.MUXSYSINFO.HELP.DEVICE},
            {ui_lblKernel,   lang.MUXSYSINFO.HELP.KERNEL},
            {ui_lblUptime,   lang.MUXSYSINFO.HELP.UPTIME},
            {ui_lblCPU,      lang.MUXSYSINFO.HELP.CPU.INFO},
            {ui_lblSpeed,    lang.MUXSYSINFO.HELP.CPU.SPEED},
            {ui_lblGovernor, lang.MUXSYSINFO.HELP.CPU.GOV},
            {ui_lblMemory,   lang.MUXSYSINFO.HELP.MEMORY},
            {ui_lblTemp,     lang.MUXSYSINFO.HELP.TEMP},
            {ui_lblCapacity, lang.MUXSYSINFO.HELP.CAPACITY},
            {ui_lblVoltage,  lang.MUXSYSINFO.HELP.VOLTAGE},
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
    char *result = get_execute_result("lscpu | grep 'Model name:' | awk -F: '{print $2}'");

    if (result == NULL || strlen(result) == 0) return lang.GENERIC.UNKNOWN;
    while (*result == ' ') result++;

    return result;
}

const char *get_current_frequency() {
    static char buffer[32];
    char *freq_str = read_text_from_file("/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq");

    if (freq_str == NULL || freq_str[0] == '\0') {
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

    if (governor_str == NULL || governor_str[0] == '\0') {
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
    if (result == NULL || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    return result;
}

const char *get_temperature() {
    static char buffer[32];
    char *temp_str = read_text_from_file("/sys/class/thermal/thermal_zone0/temp");

    if (temp_str == NULL || temp_str[0] == '\0') {
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

void update_system_info() {
    lv_label_set_text(ui_lblVersionValue, get_build_version());
    lv_label_set_text(ui_lblDeviceValue, get_device_info());
    lv_label_set_text(ui_lblKernelValue, get_kernel_version());
    lv_label_set_text(ui_lblUptimeValue, get_uptime());
    lv_label_set_text(ui_lblCPUValue, get_cpu_model());
    lv_label_set_text(ui_lblSpeedValue, get_current_frequency());
    lv_label_set_text(ui_lblGovernorValue, get_scaling_governor());
    lv_label_set_text(ui_lblMemoryValue, get_memory_usage());
    lv_label_set_text(ui_lblTempValue, get_temperature());
    lv_label_set_text(ui_lblCapacityValue, get_battery_cap());
    lv_label_set_text(ui_lblVoltageValue, read_battery_voltage());
}

void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlVersion,
            ui_pnlDevice,
            ui_pnlKernel,
            ui_pnlUptime,
            ui_pnlCPU,
            ui_pnlSpeed,
            ui_pnlGovernor,
            ui_pnlMemory,
            ui_pnlTemp,
            ui_pnlCapacity,
            ui_pnlVoltage,
    };

    ui_objects[0] = ui_lblVersion;
    ui_objects[1] = ui_lblDevice;
    ui_objects[2] = ui_lblKernel;
    ui_objects[3] = ui_lblUptime;
    ui_objects[4] = ui_lblCPU;
    ui_objects[5] = ui_lblSpeed;
    ui_objects[6] = ui_lblGovernor;
    ui_objects[7] = ui_lblMemory;
    ui_objects[8] = ui_lblTemp;
    ui_objects[9] = ui_lblCapacity;
    ui_objects[10] = ui_lblVoltage;

    lv_obj_t *ui_objects_value[] = {
            ui_lblVersionValue,
            ui_lblDeviceValue,
            ui_lblKernelValue,
            ui_lblUptimeValue,
            ui_lblCPUValue,
            ui_lblSpeedValue,
            ui_lblGovernorValue,
            ui_lblMemoryValue,
            ui_lblTempValue,
            ui_lblCapacityValue,
            ui_lblVoltageValue
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoVersion,
            ui_icoDevice,
            ui_icoKernel,
            ui_icoUptime,
            ui_icoCPU,
            ui_icoSpeed,
            ui_icoGovernor,
            ui_icoMemory,
            ui_icoTemp,
            ui_icoCapacity,
            ui_icoVoltage
    };

    apply_theme_list_panel(ui_pnlVersion);
    apply_theme_list_panel(ui_pnlDevice);
    apply_theme_list_panel(ui_pnlKernel);
    apply_theme_list_panel(ui_pnlUptime);
    apply_theme_list_panel(ui_pnlCPU);
    apply_theme_list_panel(ui_pnlSpeed);
    apply_theme_list_panel(ui_pnlGovernor);
    apply_theme_list_panel(ui_pnlMemory);
    apply_theme_list_panel(ui_pnlTemp);
    apply_theme_list_panel(ui_pnlCapacity);
    apply_theme_list_panel(ui_pnlVoltage);

    apply_theme_list_item(&theme, ui_lblVersion, lang.MUXSYSINFO.VERSION);
    apply_theme_list_item(&theme, ui_lblDevice, lang.MUXSYSINFO.DEVICE);
    apply_theme_list_item(&theme, ui_lblKernel, lang.MUXSYSINFO.KERNEL);
    apply_theme_list_item(&theme, ui_lblUptime, lang.MUXSYSINFO.UPTIME);
    apply_theme_list_item(&theme, ui_lblCPU, lang.MUXSYSINFO.CPU.INFO);
    apply_theme_list_item(&theme, ui_lblSpeed, lang.MUXSYSINFO.CPU.SPEED);
    apply_theme_list_item(&theme, ui_lblGovernor, lang.MUXSYSINFO.CPU.GOV);
    apply_theme_list_item(&theme, ui_lblMemory, lang.MUXSYSINFO.MEMORY.INFO);
    apply_theme_list_item(&theme, ui_lblTemp, lang.MUXSYSINFO.TEMP);
    apply_theme_list_item(&theme, ui_lblCapacity, lang.MUXSYSINFO.CAPACITY);
    apply_theme_list_item(&theme, ui_lblVoltage, lang.MUXSYSINFO.VOLTAGE);

    apply_theme_list_glyph(&theme, ui_icoVersion, mux_module, "version");
    apply_theme_list_glyph(&theme, ui_icoDevice, mux_module, "device");
    apply_theme_list_glyph(&theme, ui_icoKernel, mux_module, "kernel");
    apply_theme_list_glyph(&theme, ui_icoUptime, mux_module, "uptime");
    apply_theme_list_glyph(&theme, ui_icoCPU, mux_module, "cpu");
    apply_theme_list_glyph(&theme, ui_icoSpeed, mux_module, "speed");
    apply_theme_list_glyph(&theme, ui_icoGovernor, mux_module, "governor");
    apply_theme_list_glyph(&theme, ui_icoMemory, mux_module, "memory");
    apply_theme_list_glyph(&theme, ui_icoTemp, mux_module, "temp");
    apply_theme_list_glyph(&theme, ui_icoCapacity, mux_module, "capacity");
    apply_theme_list_glyph(&theme, ui_icoVoltage, mux_module, "voltage");

    apply_theme_list_value(&theme, ui_lblVersionValue, "");
    apply_theme_list_value(&theme, ui_lblDeviceValue, "");
    apply_theme_list_value(&theme, ui_lblKernelValue, "");
    apply_theme_list_value(&theme, ui_lblUptimeValue, "");
    apply_theme_list_value(&theme, ui_lblCPUValue, "");
    apply_theme_list_value(&theme, ui_lblSpeedValue, "");
    apply_theme_list_value(&theme, ui_lblGovernorValue, "");
    apply_theme_list_value(&theme, ui_lblMemoryValue, "");
    apply_theme_list_value(&theme, ui_lblTempValue, "");
    apply_theme_list_value(&theme, ui_lblCapacityValue, "");
    apply_theme_list_value(&theme, ui_lblVoltageValue, "");

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

void list_nav_prev(int steps) {
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

void list_nav_next(int steps) {
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

void handle_a() {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) == ui_lblVersion) {
        play_sound("muos", nav_sound, 0, 0);
        toast_message( /* :) */
                "\x54\x68\x61\x6E\x6B"
                "\x20\x79\x6F\x75\x20"
                "\x66\x6F\x72\x20\x75"
                "\x73\x69\x6E\x67\x20"
                "\x6D\x75\x4F\x53\x21",
                1000, 1000
        );
    }

    if (lv_group_get_focused(ui_group) == ui_lblMemory) {
        write_text_to_file("/proc/sys/vm/drop_caches", "w", INT, 3);
        toast_message(lang.MUXSYSINFO.MEMORY.DROP, 1000, 1000);
    }

    if (lv_group_get_focused(ui_group) == ui_lblKernel) {
        toast_message(hostname, 1000, 1000);
    }
}

void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "system");
    mux_input_stop();
}

void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void init_elements() {
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
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu,
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblVersion, "version");
    lv_obj_set_user_data(ui_lblDevice, "device");
    lv_obj_set_user_data(ui_lblKernel, "kernel");
    lv_obj_set_user_data(ui_lblUptime, "uptime");
    lv_obj_set_user_data(ui_lblCPU, "cpu");
    lv_obj_set_user_data(ui_lblSpeed, "speed");
    lv_obj_set_user_data(ui_lblGovernor, "governor");
    lv_obj_set_user_data(ui_lblMemory, "memory");
    lv_obj_set_user_data(ui_lblTemp, "temp");
    lv_obj_set_user_data(ui_lblCapacity, "capacity");
    lv_obj_set_user_data(ui_lblVoltage, "voltage");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSYSINFO.TITLE);
    init_mux(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);

    update_system_info();

    init_input(&js_fd, &js_fd_sys);
    init_timer(glyph_task, ui_refresh_task, update_system_info);

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
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
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
