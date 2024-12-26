#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxsysinfo.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
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
char *osd_message;

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

#define UI_COUNT 12
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblVersion,    lang.MUXSYSINFO.HELP.VERSION},
            {ui_lblDevice,     lang.MUXSYSINFO.HELP.DEVICE},
            {ui_lblKernel,     lang.MUXSYSINFO.HELP.KERNEL},
            {ui_lblUptime,     lang.MUXSYSINFO.HELP.UPTIME},
            {ui_lblCPU,        lang.MUXSYSINFO.HELP.CPU.INFO},
            {ui_lblSpeed,      lang.MUXSYSINFO.HELP.CPU.SPEED},
            {ui_lblGovernor,   lang.MUXSYSINFO.HELP.CPU.GOV},
            {ui_lblMemory,     lang.MUXSYSINFO.HELP.MEMORY},
            {ui_lblTemp,       lang.MUXSYSINFO.HELP.TEMP},
            {ui_lblService,    lang.MUXSYSINFO.HELP.SERVICE},
            {ui_lblBatteryCap, lang.MUXSYSINFO.HELP.CAPACITY},
            {ui_lblVoltage,    lang.MUXSYSINFO.HELP.VOLTAGE},
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

char *remove_comma(const char *str) {
    size_t len = strlen(str);
    char *result = malloc(len + 1);

    if (result == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }

    strcpy(result, str);
    if (len > 0 && result[len - 1] == ',')
        result[len - 1] = '\0';

    return result;
}

char *format_uptime(const char *uptime) {
    int hours = 0, minutes = 0;

    if (sscanf(uptime, "%d:%d", &hours, &minutes) < 2) {
        sscanf(uptime, "%d", &minutes);
        minutes = hours;
        hours = 0;
    }

    static char format_uptime[MAX_BUFFER_SIZE];

    if (hours > 0) {
        sprintf(format_uptime, "%d %s %d %s",
                hours, (hours == 1) ? "Hour" : "Hours",
                minutes, (minutes == 1) ? "Minute" : "Minutes");
    } else {
        sprintf(format_uptime, "%d %s",
                minutes, (minutes == 1) ? "Minute" : "Minutes");
    }

    return format_uptime;
}

void update_system_info() {
    char battery_cap[32];
    sprintf(battery_cap, "%d%% (Offset: %d)",
            read_battery_capacity(),
            config.SETTINGS.ADVANCED.OFFSET - 50);

    char build_version[MAX_BUFFER_SIZE];
    sprintf(build_version, "%s (%s)",
            str_replace(read_line_from_file((INTERNAL_PATH "config/version.txt"), 1), "_", " "),
            read_line_from_file((INTERNAL_PATH "config/version.txt"), 2));

    lv_label_set_text(ui_lblVersionValue,
                      build_version);
    lv_label_set_text(ui_lblDeviceValue,
                      read_line_from_file((INTERNAL_PATH "config/device.txt"), 1));
    lv_label_set_text(ui_lblKernelValue,
                      get_execute_result(
                              "uname -rs"
                      ));
    lv_label_set_text(ui_lblUptimeValue,
                      format_uptime(remove_comma(get_execute_result(
                              "uptime | awk '{print $3}'"
                      ))));
    lv_label_set_text(ui_lblCPUValue,
                      get_execute_result(
                              "lscpu | grep -i 'Model name' | awk -F: '{print $2}' | sed 's/^ *//'"));
    lv_label_set_text(ui_lblSpeedValue,
                      get_execute_result(
                              "cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq"
                              " | awk '{print $1/1000}'"));
    lv_label_set_text(ui_lblGovernorValue,
                      get_execute_result(
                              "cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor"));
    lv_label_set_text(ui_lblMemoryValue,
                      get_execute_result(
                              "free | awk '/Mem:/ {used = $3 / 1024; total = $2 / 1024; "
                              "printf \"%.2f MB / %.2f MB\", used, total}'"
                      ));
    lv_label_set_text(ui_lblTempValue,
                      get_execute_result(
                              "cat /sys/class/thermal/thermal_zone0/temp | awk '{printf \"%.2f\", $1/1000}'"
                      ));
    lv_label_set_text(ui_lblServiceValue,
                      get_execute_result(
                              "ps | grep -v 'COMMAND' | grep -v 'grep' | sed '/\\[/d' | wc -l"
                      ));
    lv_label_set_text(ui_lblBatteryCapValue, battery_cap);
    lv_label_set_text(ui_lblVoltageValue, read_battery_voltage());
}

void init_navigation_groups() {
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
            ui_pnlService,
            ui_pnlBatteryCap,
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
    ui_objects[9] = ui_lblService;
    ui_objects[10] = ui_lblBatteryCap;
    ui_objects[11] = ui_lblVoltage;

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
            ui_lblServiceValue,
            ui_lblBatteryCapValue,
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
            ui_icoService,
            ui_icoBatteryCap,
            ui_icoVoltage
    };

    apply_theme_list_panel(&theme, &device, ui_pnlVersion);
    apply_theme_list_panel(&theme, &device, ui_pnlDevice);
    apply_theme_list_panel(&theme, &device, ui_pnlKernel);
    apply_theme_list_panel(&theme, &device, ui_pnlUptime);
    apply_theme_list_panel(&theme, &device, ui_pnlCPU);
    apply_theme_list_panel(&theme, &device, ui_pnlSpeed);
    apply_theme_list_panel(&theme, &device, ui_pnlGovernor);
    apply_theme_list_panel(&theme, &device, ui_pnlMemory);
    apply_theme_list_panel(&theme, &device, ui_pnlTemp);
    apply_theme_list_panel(&theme, &device, ui_pnlService);
    apply_theme_list_panel(&theme, &device, ui_pnlBatteryCap);
    apply_theme_list_panel(&theme, &device, ui_pnlVoltage);

    apply_theme_list_item(&theme, ui_lblVersion, lang.MUXSYSINFO.VERSION, false, true);
    apply_theme_list_item(&theme, ui_lblDevice, lang.MUXSYSINFO.DEVICE, false, true);
    apply_theme_list_item(&theme, ui_lblKernel, lang.MUXSYSINFO.KERNEL, false, true);
    apply_theme_list_item(&theme, ui_lblUptime, lang.MUXSYSINFO.UPTIME, false, true);
    apply_theme_list_item(&theme, ui_lblCPU, lang.MUXSYSINFO.CPU.INFO, false, true);
    apply_theme_list_item(&theme, ui_lblSpeed, lang.MUXSYSINFO.CPU.SPEED, false, true);
    apply_theme_list_item(&theme, ui_lblGovernor, lang.MUXSYSINFO.CPU.GOV, false, true);
    apply_theme_list_item(&theme, ui_lblMemory, lang.MUXSYSINFO.MEMORY, false, true);
    apply_theme_list_item(&theme, ui_lblTemp, lang.MUXSYSINFO.TEMP, false, true);
    apply_theme_list_item(&theme, ui_lblService, lang.MUXSYSINFO.SERVICE, false, true);
    apply_theme_list_item(&theme, ui_lblBatteryCap, lang.MUXSYSINFO.CAPACITY, false, true);
    apply_theme_list_item(&theme, ui_lblVoltage, lang.MUXSYSINFO.VOLTAGE, false, true);

    apply_theme_list_glyph(&theme, ui_icoVersion, mux_module, "version");
    apply_theme_list_glyph(&theme, ui_icoDevice, mux_module, "device");
    apply_theme_list_glyph(&theme, ui_icoKernel, mux_module, "kernel");
    apply_theme_list_glyph(&theme, ui_icoUptime, mux_module, "uptime");
    apply_theme_list_glyph(&theme, ui_icoCPU, mux_module, "cpu");
    apply_theme_list_glyph(&theme, ui_icoSpeed, mux_module, "speed");
    apply_theme_list_glyph(&theme, ui_icoGovernor, mux_module, "governor");
    apply_theme_list_glyph(&theme, ui_icoMemory, mux_module, "memory");
    apply_theme_list_glyph(&theme, ui_icoTemp, mux_module, "temp");
    apply_theme_list_glyph(&theme, ui_icoService, mux_module, "service");
    apply_theme_list_glyph(&theme, ui_icoBatteryCap, mux_module, "capacity");
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
    apply_theme_list_value(&theme, ui_lblServiceValue, "");
    apply_theme_list_value(&theme, ui_lblBatteryCapValue, "");
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
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
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

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

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
    lv_obj_set_user_data(ui_lblService, "service");
    lv_obj_set_user_data(ui_lblBatteryCap, "capacity");
    lv_obj_set_user_data(ui_lblVoltage, "voltage");

    if (TEST_IMAGE) display_testing_message(ui_screen);

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
    load_lang(&lang);

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t *buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, &lang, lang.MUXSYSINFO.TITLE);
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();

    update_system_info();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *sysinfo_timer = lv_timer_create(update_system_info, UINT16_MAX / 32, NULL);
    lv_timer_ready(sysinfo_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    refresh_screen(device.SCREEN.WAIT);
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
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    // List navigation:
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
