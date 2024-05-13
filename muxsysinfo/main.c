#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include <zlib.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/glyph.h"
#include "../common/mini/mini.h"

static int js_fd;

int NAV_DPAD_HOR;
int NAV_ANLG_HOR;
int NAV_DPAD_VER;
int NAV_ANLG_VER;
int NAV_A;
int NAV_B;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;
struct mux_config config;

int nav_moved = 1;
char *current_wall = "";

// Place as many NULL as there are options!
lv_obj_t *labels[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;

void show_help(lv_obj_t *element_focused) {
    char *message = NO_HELP_FOUND;

    if (element_focused == ui_lblVersion) {
        message = MUXSYSINFO_VERSION;
    } else if (element_focused == ui_lblRetro) {
        message = MUXSYSINFO_RETRO;
    } else if (element_focused == ui_lblKernel) {
        message = MUXSYSINFO_KERNEL;
    } else if (element_focused == ui_lblUptime) {
        message = MUXSYSINFO_UPTIME;
    } else if (element_focused == ui_lblCPU) {
        message = MUXSYSINFO_CPU;
    } else if (element_focused == ui_lblSpeed) {
        message = MUXSYSINFO_SPEED;
    } else if (element_focused == ui_lblGovernor) {
        message = MUXSYSINFO_GOVERNOR;
    } else if (element_focused == ui_lblMemory) {
        message = MUXSYSINFO_MEMORY;
    } else if (element_focused == ui_lblTemp) {
        message = MUXSYSINFO_TEMP;
    } else if (element_focused == ui_lblServices) {
        message = MUXSYSINFO_SERVICES;
    } else if (element_focused == ui_lblBatteryCap) {
        message = MUXSYSINFO_BATTCAP;
    } else if (element_focused == ui_lblVoltage) {
        message = MUXSYSINFO_VOLTAGE;
    }

    if (strlen(message) <= 1) {
        message = NO_HELP_FOUND;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, lv_label_get_text(element_focused), message);
}

void text_replace(char *str, const char *find, const char *replace) {
    char *pos = strstr(str, find);
    if (pos != NULL) {
        size_t findLen = strlen(find);
        size_t replaceLen = strlen(replace);

        memmove(pos + replaceLen, pos + findLen, strlen(pos + findLen) + 1);
        memcpy(pos, replace, replaceLen);
    }
}

int compare(const void *a, const void *b) {
    return strcasecmp(*(const char **) a, *(const char **) b);
}

const char *ra_config_c32_check() {
    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "crc32 %s", RA_CONFIG_FILE);

    FILE * pipe = popen(command, "r");
    if (!pipe) {
        perror("Error opening pipe");
        return "Unknown";
    }

    char crc[10];
    if (fgets(crc, sizeof(crc), pipe) == NULL) {
        perror("Error reading result");
        return "Unknown";
    }

    pclose(pipe);

    crc[strlen(crc) - 1] = '\0';
    if (strcmp(crc, RA_CONFIG_CRC) == 0) {
        return "Original";
    } else {
        return "Modified";
    }
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
            read_line_from_file("/opt/muos/config/version.txt", 1),
            read_line_from_file("/opt/muos/config/version.txt", 2));

    lv_label_set_text(ui_lblVersionValue, build_version);
    lv_label_set_text(ui_lblRetroValue, ra_config_c32_check());
    lv_label_set_text(ui_lblKernelValue, get_execute_result("uname -rs"));
    lv_label_set_text(ui_lblUptimeValue, format_uptime(remove_comma(get_execute_result("uptime | awk '{print $3}'"))));
    lv_label_set_text(ui_lblCPUValue, "Cortex A53");
    lv_label_set_text(ui_lblSpeedValue, get_execute_result(
            "cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq | awk '{print $1/1000}'"));
    lv_label_set_text(ui_lblGovernorValue,
                      get_execute_result("cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor"));
    lv_label_set_text(ui_lblMemoryValue, get_execute_result(
            "free | awk '/Mem:/ {used = $3 / 1024; total = $2 / 1024; printf \"%.2f MB / %.2f MB\", used, total}'"));
    lv_label_set_text(ui_lblTempValue, get_execute_result(
            "cat /sys/class/thermal/thermal_zone0/temp | awk '{printf \"%.2f\", $1/1000}'"));
    lv_label_set_text(ui_lblServicesValue,
                      get_execute_result("ps | grep -v 'COMMAND' | grep -v 'grep' | sed '/\\[/d' | wc -l"));
    lv_label_set_text(ui_lblBatteryCapValue, battery_cap);
    lv_label_set_text(ui_lblVoltageValue, read_battery_voltage());
}

void init_navigation_groups() {
    lv_obj_t *ui_objects[] = {
            ui_lblVersion,
            ui_lblRetro,
            ui_lblKernel,
            ui_lblUptime,
            ui_lblCPU,
            ui_lblSpeed,
            ui_lblGovernor,
            ui_lblMemory,
            ui_lblTemp,
            ui_lblServices,
            ui_lblBatteryCap,
            ui_lblVoltage
    };

    lv_obj_t *ui_objects_value[] = {
            ui_lblVersionValue,
            ui_lblRetroValue,
            ui_lblKernelValue,
            ui_lblUptimeValue,
            ui_lblCPUValue,
            ui_lblSpeedValue,
            ui_lblGovernorValue,
            ui_lblMemoryValue,
            ui_lblTempValue,
            ui_lblServicesValue,
            ui_lblBatteryCapValue,
            ui_lblVoltageValue
    };

    lv_obj_t *ui_objects_icon[] = {
            ui_icoVersion,
            ui_icoRetro,
            ui_icoKernel,
            ui_icoUptime,
            ui_icoCPU,
            ui_icoSpeed,
            ui_icoGovernor,
            ui_icoMemory,
            ui_icoTemp,
            ui_icoServices,
            ui_icoBatteryCap,
            ui_icoVoltage
    };

    labels[0] = ui_lblVersionValue;
    labels[1] = ui_lblRetroValue;
    labels[2] = ui_lblKernelValue;
    labels[3] = ui_lblUptimeValue;
    labels[4] = ui_lblCPUValue;
    labels[5] = ui_lblSpeedValue;
    labels[6] = ui_lblGovernorValue;
    labels[7] = ui_lblMemoryValue;
    labels[8] = ui_lblTempValue;
    labels[9] = ui_lblServicesValue;
    labels[10] = ui_lblBatteryCapValue;
    labels[11] = ui_lblVoltageValue;

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }

    for (unsigned int i = 0; i < sizeof(ui_objects_value) / sizeof(ui_objects_value[0]); i++) {
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
    }

    for (unsigned int i = 0; i < sizeof(ui_objects_icon) / sizeof(ui_objects_icon[0]); i++) {
        lv_group_add_obj(ui_group_glyph, ui_objects_icon[i]);
    }
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    int JOYHOTKEY_pressed = 0;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return NULL;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return NULL;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 64);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd) {
                int ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }

                struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B || ev.code == JOY_MENU) {
                                    play_sound("confirm", nav_sound);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                }
                            } else {
                                if (ev.code == JOY_MENU) {
                                    JOYHOTKEY_pressed = 1;
                                } else if (ev.code == NAV_A) {
                                    if (element_focused == ui_lblVersion) {
                                        osd_message = "Thank you for using muOS!";
                                        lv_label_set_text(ui_lblMessage, osd_message);
                                        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                    }
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);
                                    input_disable = 1;
                                    safe_quit = 1;
                                }
                            }
                        } else {
                            if (ev.code == JOY_MENU) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound);
                                    show_help(element_focused);
                                }
                            }
                        }
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            switch (ev.value) {
                                case -4100 ... -4000:
                                case -1:
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_value, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    play_sound("navigate", nav_sound);
                                    nav_moved = 1;
                                    break;
                                case 1:
                                case 4000 ... 4100:
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_value, 1);
                                    nav_next(ui_group_glyph, 1);
                                    play_sound("navigate", nav_sound);
                                    nav_moved = 1;
                                    break;
                                default:
                                    break;
                            }
                        }
                    default:
                        break;
                }
            }
        }

        if (ev.type == EV_KEY && ev.value == 1 && (ev.code == JOY_MINUS || ev.code == JOY_PLUS)) {
            progress_onscreen = 1;
            if (lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
            }
            if (JOYHOTKEY_pressed) {
                lv_label_set_text(ui_icoProgress, "\uF185");
                lv_bar_set_value(ui_barProgress, get_brightness_percentage(get_brightness()), LV_ANIM_OFF);
            } else {
                int volume = get_volume_percentage();
                switch (volume) {
                    case 0:
                        lv_label_set_text(ui_icoProgress, "\uF6A9");
                        break;
                    case 1 ... 46:
                        lv_label_set_text(ui_icoProgress, "\uF026");
                        break;
                    case 47 ... 71:
                        lv_label_set_text(ui_icoProgress, "\uF027");
                        break;
                    case 72 ... 100:
                        lv_label_set_text(ui_icoProgress, "\uF028");
                        break;
                }
                lv_bar_set_value(ui_barProgress, volume, LV_ANIM_OFF);
            }
        }

        lv_task_handler();
        usleep(SCREEN_WAIT);
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgress);

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, "Back");
    lv_label_set_text(ui_lblNavMenu, "Help");

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
            ui_lblNavZ
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_user_data(ui_lblVersion, "version");
    lv_obj_set_user_data(ui_lblRetro, "retroarch");
    lv_obj_set_user_data(ui_lblKernel, "kernel");
    lv_obj_set_user_data(ui_lblUptime, "uptime");
    lv_obj_set_user_data(ui_lblCPU, "cpu");
    lv_obj_set_user_data(ui_lblSpeed, "speed");
    lv_obj_set_user_data(ui_lblGovernor, "governor");
    lv_obj_set_user_data(ui_lblMemory, "memory");
    lv_obj_set_user_data(ui_lblTemp, "temp");
    lv_obj_set_user_data(ui_lblServices, "service");
    lv_obj_set_user_data(ui_lblBatteryCap, "capacity");
    lv_obj_set_user_data(ui_lblVoltage, "voltage");

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_scrSysInfo);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_scrSysInfo);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

    if (is_network_connected()) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.NORMAL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (atoi(read_text_from_file(BATT_CHARGER))) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (read_battery_capacity() <= 15) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.LOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.LOW_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_scrSysInfo, ui_group, theme.MISC.ANIMATED_BACKGROUND));

            if (strcmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    printf("LOADING WALLPAPER: %s\n", new_wall);
                    if (theme.MISC.ANIMATED_BACKGROUND) {
                        lv_obj_t * img = lv_gif_create(ui_pnlWall);
                        lv_gif_set_src(img, new_wall);
                    } else {
                        lv_img_set_src(ui_imgWall, new_wall);
                    }
                    lv_obj_invalidate(ui_pnlWall);
                } else {
                    lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
                }
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_scrSysInfo, ui_group));

            if (strlen(static_image) > 0) {
                printf("LOADING STATIC IMAGE: %s\n", static_image);

                switch (theme.MISC.STATIC_ALIGNMENT) {
                    case 0: // Bottom + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 1: // Middle + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 2: // Top + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 3: // Fullscreen + Behind
                        lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
            }
        }
        lv_obj_invalidate(ui_pnlContent);
        lv_task_handler();
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/system/bin", 1);
    setenv("NO_COLOR", "1", 1);

    lv_init();
    fbdev_init();

    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    if (strcasecmp(HARDWARE, "RG28XX") == 0) {
        disp_drv.hor_res = SCREEN_HEIGHT;
        disp_drv.ver_res = SCREEN_WIDTH;
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = LV_DISP_ROT_90;
    } else {
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
    }
    lv_disp_drv_register(&disp_drv);

    load_config(&config);

    ui_init();
    init_elements();

    lv_obj_set_user_data(ui_scrSysInfo, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, basename(argv[0]));
    apply_theme();

    switch (theme.MISC.NAVIGATION_TYPE) {
        case 1:
            NAV_DPAD_HOR = ABS_HAT0Y;
            NAV_ANLG_HOR = ABS_RX;
            NAV_DPAD_VER = ABS_HAT0X;
            NAV_ANLG_VER = ABS_Z;
            break;
        default:
            NAV_DPAD_HOR = ABS_HAT0X;
            NAV_ANLG_HOR = ABS_Z;
            NAV_DPAD_VER = ABS_HAT0Y;
            NAV_ANLG_VER = ABS_RX;
    }

    switch (config.SETTINGS.ADVANCED.SWAP) {
        case 1:
            NAV_A = JOY_B;
            NAV_B = JOY_A;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D2");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D3");
            break;
        default:
            NAV_A = JOY_A;
            NAV_B = JOY_B;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D3");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D2");
            break;
    }

    current_wall = load_wallpaper(ui_scrSysInfo, NULL, theme.MISC.ANIMATED_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.ANIMATED_BACKGROUND) {
            lv_obj_t * img = lv_gif_create(ui_pnlWall);
            lv_gif_set_src(img, current_wall);
        } else {
            lv_img_set_src(ui_imgWall, current_wall);
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
    }

    load_font_text(basename(argv[0]), ui_scrSysInfo);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
    }

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

    js_fd = open(JOY_DEVICE, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
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

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    init_elements();
    while (!safe_quit) {
        usleep(SCREEN_WAIT);
    }

    pthread_cancel(joystick_thread);

    close(js_fd);

    return 0;
}

uint32_t mux_tick(void) {
    static uint64_t start_ms = 0;

    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
