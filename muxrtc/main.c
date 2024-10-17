#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int rtcYearValue;
int rtcMonthValue;
int rtcDayValue;
int rtcHourValue;
int rtcMinuteValue;
int rtcNotationValue = 0;

char rtc_buffer[32];

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 7
lv_obj_t *ui_objects[UI_COUNT];

const char *notation[] = {
        NULL, NULL
};

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(ui_lblTitle)), TS("Change your current date, time, and timezone"));
}

void confirm_rtc_config() {
    int idx_notation = 0;
    char *notation_type = lv_label_get_text(ui_lblNotationValue);

    if (strcmp(notation_type, notation[1]) == 0) {
        idx_notation = 1;
    }

    write_text_to_file("/run/muos/global/clock/notation", "w", INT, idx_notation);

    system("hwclock -w");
}

void restore_clock_settings() {
    FILE *fp;
    char date_output[100];
    int attempts = 0;

    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "date +\"%%Y %%m %%d %%H %%M\"");

    while (attempts < RTC_MAX_RETRIES) {
        fp = popen(command, "r");
        if (fp == NULL) {
            perror("Failed to run date command");
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }

        if (fgets(date_output, sizeof(date_output) - 1, fp) != NULL) {
            pclose(fp);
            break;
        } else {
            perror("Failed to read date command output");
            pclose(fp);
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }
    }

    if (attempts == RTC_MAX_RETRIES) {
        fprintf(stderr, "Attempts to read system date failed\n");
        return;
    }

    int year, month, day, hour, minute;
    if (sscanf(date_output, "%d %d %d %d %d", &year, &month, &day, &hour, &minute) != 5) {
        fprintf(stderr, "Failed to parse date command output\n");
        return;
    }

    rtcYearValue = year;
    rtcMonthValue = month;
    rtcDayValue = day;
    rtcHourValue = hour;
    rtcMinuteValue = minute;

    snprintf(rtc_buffer, sizeof(rtc_buffer), "%04d", year);
    lv_label_set_text(ui_lblYearValue, rtc_buffer);

    snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMonthValue);
    lv_label_set_text(ui_lblMonthValue, rtc_buffer);

    snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
    lv_label_set_text(ui_lblDayValue, rtc_buffer);

    snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcHourValue);
    lv_label_set_text(ui_lblHourValue, rtc_buffer);

    snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMinuteValue);
    lv_label_set_text(ui_lblMinuteValue, rtc_buffer);

    rtcNotationValue = config.CLOCK.NOTATION;
    lv_label_set_text(ui_lblNotationValue, notation[rtcNotationValue]);
}

void save_clock_settings(int year, int month, int day, int hour, int minute) {
    FILE *fp;
    int attempts = 0;

    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "date \"%04d-%02d-%02d %02d:%02d:00\"",
             year, month, day, hour, minute);

    printf("SETTING DATE TO: %s\n", command);

    while (attempts < RTC_MAX_RETRIES) {
        fp = popen(command, "r");
        if (fp == NULL) {
            perror("Failed to run date command");
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }

        if (pclose(fp) == -1) {
            perror("Failed to close date command stream");
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }

        confirm_rtc_config();

        return;
    }

    fprintf(stderr, "Attempts to set system date failed\n");
}

int days_in_month(int year, int month) {
    int max_days;
    switch (month) {
        case 2:  // February
            if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
                max_days = 29;
            } else {
                max_days = 28;
            }
            break;
        case 4:  // April
        case 6:  // June
        case 9:  // September
        case 11: // November
            max_days = 30;
            break;
        default:
            max_days = 31;
            break;
    }
    return max_days;
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlYear,
            ui_pnlMonth,
            ui_pnlDay,
            ui_pnlHour,
            ui_pnlMinute,
            ui_pnlNotation,
            ui_pnlTimezone,
    };

    ui_objects[0] = ui_lblYear;
    ui_objects[1] = ui_lblMonth;
    ui_objects[2] = ui_lblDay;
    ui_objects[3] = ui_lblHour;
    ui_objects[4] = ui_lblMinute;
    ui_objects[5] = ui_lblNotation;
    ui_objects[6] = ui_lblTimezone;

    lv_obj_t *ui_objects_value[] = {
            ui_lblYearValue,
            ui_lblMonthValue,
            ui_lblDayValue,
            ui_lblHourValue,
            ui_lblMinuteValue,
            ui_lblNotationValue,
            ui_lblTimezoneValue
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoYear,
            ui_icoMonth,
            ui_icoDay,
            ui_icoHour,
            ui_icoMinute,
            ui_icoNotation,
            ui_icoTimezone
    };

    apply_theme_list_panel(&theme, &device, ui_pnlYear);
    apply_theme_list_panel(&theme, &device, ui_pnlMonth);
    apply_theme_list_panel(&theme, &device, ui_pnlDay);
    apply_theme_list_panel(&theme, &device, ui_pnlHour);
    apply_theme_list_panel(&theme, &device, ui_pnlMinute);
    apply_theme_list_panel(&theme, &device, ui_pnlNotation);
    apply_theme_list_panel(&theme, &device, ui_pnlTimezone);

    apply_theme_list_item(&theme, ui_lblYear, TS("Year"), false, false);
    apply_theme_list_item(&theme, ui_lblMonth, TS("Month"), false, false);
    apply_theme_list_item(&theme, ui_lblDay, TS("Day"), false, false);
    apply_theme_list_item(&theme, ui_lblHour, TS("Hour"), false, false);
    apply_theme_list_item(&theme, ui_lblMinute, TS("Minute"), false, false);
    apply_theme_list_item(&theme, ui_lblNotation, TS("Time Notation"), false, false);
    apply_theme_list_item(&theme, ui_lblTimezone, TS("Set Timezone"), false, false);

    apply_theme_list_glyph(&theme, ui_icoYear, mux_module, "year");
    apply_theme_list_glyph(&theme, ui_icoMonth, mux_module, "month");
    apply_theme_list_glyph(&theme, ui_icoDay, mux_module, "day");
    apply_theme_list_glyph(&theme, ui_icoHour, mux_module, "hour");
    apply_theme_list_glyph(&theme, ui_icoMinute, mux_module, "minute");
    apply_theme_list_glyph(&theme, ui_icoNotation, mux_module, "notation");
    apply_theme_list_glyph(&theme, ui_icoTimezone, mux_module, "timezone");

    apply_theme_list_value(&theme, ui_lblYearValue, "");
    apply_theme_list_value(&theme, ui_lblMonthValue, "");
    apply_theme_list_value(&theme, ui_lblDayValue, "");
    apply_theme_list_value(&theme, ui_lblHourValue, "");
    apply_theme_list_value(&theme, ui_lblMinuteValue, "");
    apply_theme_list_value(&theme, ui_lblNotationValue, "");
    apply_theme_list_value(&theme, ui_lblTimezoneValue, "");

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
    play_sound("navigate", nav_sound, 0);
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
    play_sound("navigate", nav_sound, 0);
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
    if (msgbox_active) {
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblTimezone) {
        play_sound("confirm", nav_sound, 1);
        input_disable = 1;
        save_clock_settings(rtcYearValue, rtcMonthValue, rtcDayValue,
                            rtcHourValue, rtcMinuteValue);
        load_mux("timezone");
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "timezone");
        mux_input_stop();
    } else {
        play_sound("navigate", nav_sound, 0);
        if (element_focused == ui_lblYear) {
            if (rtcYearValue >= 1969 && rtcYearValue < 2199) {
                rtcYearValue++;
                rtcDayValue = 1;
            }
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%04d", rtcYearValue);
            lv_label_set_text(ui_lblYearValue, rtc_buffer);
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
            lv_label_set_text(ui_lblDayValue, rtc_buffer);
        } else if (element_focused == ui_lblMonth) {
            if (rtcMonthValue < 12) {
                rtcMonthValue++;
                rtcDayValue = 1;
            } else {
                rtcMonthValue = 1;
                rtcDayValue = 1;
            }
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMonthValue);
            lv_label_set_text(ui_lblMonthValue, rtc_buffer);
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
            lv_label_set_text(ui_lblDayValue, rtc_buffer);
        } else if (element_focused == ui_lblDay) {
            if (rtcDayValue < days_in_month(rtcYearValue, rtcMonthValue)) {
                rtcDayValue++;
            } else {
                rtcDayValue = 1;
            }
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
            lv_label_set_text(ui_lblDayValue, rtc_buffer);
        } else if (element_focused == ui_lblHour) {
            if (rtcHourValue < 23) {
                rtcHourValue++;
            } else {
                rtcHourValue = 0;
            }
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcHourValue);
            lv_label_set_text(ui_lblHourValue, rtc_buffer);
        } else if (element_focused == ui_lblMinute) {
            if (rtcMinuteValue < 59) {
                rtcMinuteValue++;
            } else {
                rtcMinuteValue = 0;
            }
            snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMinuteValue);
            lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
        } else if (element_focused == ui_lblNotation) {
            rtcNotationValue++;
            if (rtcNotationValue < 0) {
                rtcNotationValue = 1;
            }
            if (rtcNotationValue > 1) {
                rtcNotationValue = 0;
            }
            lv_label_set_text(ui_lblNotationValue, notation[rtcNotationValue]);
        }
    }
}

void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 1);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 1);
    input_disable = 1;

    osd_message = TG("Saving Changes");
    lv_label_set_text(ui_lblMessage, osd_message);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    save_clock_settings(rtcYearValue, rtcMonthValue, rtcDayValue,
                        rtcHourValue, rtcMinuteValue);

    usleep(device.SCREEN.WAIT);

    char config_file[MAX_BUFFER_SIZE];
    snprintf(config_file, sizeof(config_file),
             "%s/config/config.ini", INTERNAL_PATH);

    write_text_to_file("/run/muos/global/boot/clock_setup", "w", INT, 0);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");
    mux_input_stop();
}

void handle_left() {
    if (msgbox_active) {
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0);
    if (element_focused == ui_lblYear) {
        if (rtcYearValue > 1970 && rtcYearValue <= 2199) {
            rtcYearValue--;
            rtcDayValue = 1;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%04d", rtcYearValue);
        lv_label_set_text(ui_lblYearValue, rtc_buffer);
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
        lv_label_set_text(ui_lblDayValue, rtc_buffer);
    } else if (element_focused == ui_lblMonth) {
        if (rtcMonthValue > 1) {
            rtcMonthValue--;
            rtcDayValue = 1;
        } else {
            rtcMonthValue = 12;
            rtcDayValue = 1;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMonthValue);
        lv_label_set_text(ui_lblMonthValue, rtc_buffer);
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
        lv_label_set_text(ui_lblDayValue, rtc_buffer);
    } else if (element_focused == ui_lblDay) {
        if (rtcDayValue > 1) {
            rtcDayValue--;
        } else {
            rtcDayValue = days_in_month(rtcYearValue, rtcMonthValue);
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
        lv_label_set_text(ui_lblDayValue, rtc_buffer);
    } else if (element_focused == ui_lblHour) {
        if (rtcHourValue > 0) {
            rtcHourValue--;
        } else {
            rtcHourValue = 23;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcHourValue);
        lv_label_set_text(ui_lblHourValue, rtc_buffer);
    } else if (element_focused == ui_lblMinute) {
        if (rtcMinuteValue > 0) {
            rtcMinuteValue--;
        } else {
            rtcMinuteValue = 59;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMinuteValue);
        lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
    } else if (element_focused == ui_lblNotation) {
        rtcNotationValue--;
        if (rtcNotationValue < 0) {
            rtcNotationValue = 1;
        }
        if (rtcNotationValue > 1) {
            rtcNotationValue = 0;
        }

        lv_label_set_text(ui_lblNotationValue, notation[rtcNotationValue]);
    }
}

void handle_right() {
    if (msgbox_active) {
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0);
    if (element_focused == ui_lblYear) {
        if (rtcYearValue >= 1970 && rtcYearValue < 2199) {
            rtcYearValue++;
            rtcDayValue = 1;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%04d", rtcYearValue);
        lv_label_set_text(ui_lblYearValue, rtc_buffer);
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
        lv_label_set_text(ui_lblDayValue, rtc_buffer);
    } else if (element_focused == ui_lblMonth) {
        if (rtcMonthValue < 12) {
            rtcMonthValue++;
            rtcDayValue = 1;
        } else {
            rtcMonthValue = 1;
            rtcDayValue = 1;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMonthValue);
        lv_label_set_text(ui_lblMonthValue, rtc_buffer);
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
        lv_label_set_text(ui_lblDayValue, rtc_buffer);
    } else if (element_focused == ui_lblDay) {
        if (rtcDayValue < days_in_month(rtcYearValue, rtcMonthValue)) {
            rtcDayValue++;
        } else {
            rtcDayValue = 1;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcDayValue);
        lv_label_set_text(ui_lblDayValue, rtc_buffer);
    } else if (element_focused == ui_lblHour) {
        if (rtcHourValue < 23) {
            rtcHourValue++;
        } else {
            rtcHourValue = 0;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcHourValue);
        lv_label_set_text(ui_lblHourValue, rtc_buffer);
    } else if (element_focused == ui_lblMinute) {
        if (rtcMinuteValue < 59) {
            rtcMinuteValue++;
        } else {
            rtcMinuteValue = 0;
        }
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%02d", rtcMinuteValue);
        lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
    } else if (element_focused == ui_lblNotation) {
        rtcNotationValue++;
        if (rtcNotationValue < 0) {
            rtcNotationValue = 1;
        }
        if (rtcNotationValue > 1) {
            rtcNotationValue = 0;
        }
        lv_label_set_text(ui_lblNotationValue, notation[rtcNotationValue]);
    }
}

void handle_menu() {
    if (msgbox_active) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 1);
        show_help();
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgressBrightness);
    lv_obj_move_foreground(ui_pnlProgressVolume);

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

    lv_label_set_text(ui_lblNavB, TG("Save"));

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
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblYear, "year");
    lv_obj_set_user_data(ui_lblMonth, "month");
    lv_obj_set_user_data(ui_lblDay, "day");
    lv_obj_set_user_data(ui_lblHour, "hour");
    lv_obj_set_user_data(ui_lblMinute, "minute");
    lv_obj_set_user_data(ui_lblNotation, "notation");
    lv_obj_set_user_data(ui_lblTimezone, "timezone");

    if (TEST_IMAGE) display_testing_message(ui_screen);
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
    update_bars(ui_barProgressBrightness, ui_barProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            if (config.BOOT.FACTORY_RESET) {
                char init_wall[MAX_BUFFER_SIZE];
                snprintf(init_wall, sizeof(init_wall), "M:%s/theme/image/wall/default.png", INTERNAL_PATH);
                lv_img_set_src(ui_imgWall, init_wall);
            } else {
                load_wallpaper(ui_screen, ui_group, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND, 
                        theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_screen, ui_group));

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
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
            }
        }
        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

void direct_to_previous() {
    if (file_exist(MUOS_PDI_LOAD)) {
        char *prev = read_text_from_file(MUOS_PDI_LOAD);
        int text_hit = 0;

        for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
            const char *u_data = lv_obj_get_user_data(ui_objects[i]);

            if (strcasecmp(u_data, prev) == 0) {
                text_hit = i;
                break;
            }
        }

        if (text_hit != 0) {
            list_nav_next(text_hit);
            nav_moved = 1;
        }
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);


    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

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

    ui_common_screen_init(&theme, &device, TS("DATE AND TIME"));
    ui_init(ui_pnlContent);
    init_elements();
    load_overlay_image(ui_screen, theme.MISC.IMAGE_OVERLAY);

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND, 
            theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    notation[0] = TS("12 Hour");
    notation[1] = TS("24 Hour");

    init_navigation_groups();
    restore_clock_settings();

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
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
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

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    init_elements();
    direct_to_previous();

    refresh_screen();

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
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
