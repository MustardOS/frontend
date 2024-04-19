#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <linux/joystick.h>
#include <linux/rtc.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
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
mini_t *muos_config;

int nav_moved = 1;
char *current_wall = "";

// Place as many NULL as there are options!
lv_obj_t *labels[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int lblYearValue;
int lblMonthValue;
int lblDayValue;
int lblHourValue;
int lblMinuteValue;
int lblTimezoneValue = 0;
int lblNotationValue = 0;

char rtc_buffer[32];

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;

const char *notation[] = {
        "12 Hour", "24 Hour"
};

void show_help() {
    char *title = lv_label_get_text(ui_lblTitle);
    char *message = MUXRTC_GENERIC;

    if (strlen(message) <= 1) {
        message = NO_HELP_FOUND;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, title, message);
}

void confirm_rtc_config() {
    int idx_notation = 0;
    char* notation_type = lv_label_get_text(ui_lblNotationValue);

    if (strcmp(notation_type, notation[1]) == 0) {
        idx_notation = 1;
    }

    mini_set_int(muos_config, "clock", "notation", idx_notation);

    mini_save(muos_config, MINI_FLAGS_SKIP_EMPTY_GROUPS);
}

void read_rtc_hardware() {
    int rtc_fd = open(RTC_DEVICE, O_RDONLY);
    if (rtc_fd == -1) {
        perror("Failed to open RTC device");
        return;
    }

    struct rtc_time rtc_tm;
    int result = ioctl(rtc_fd, RTC_RD_TIME, &rtc_tm);
    if (result == -1) {
        perror("Failed to read hardware clock");
        close(rtc_fd);
        return;
    }

    lblYearValue = rtc_tm.tm_year;
    lblMonthValue = rtc_tm.tm_mon;
    lblDayValue = rtc_tm.tm_mday;
    lblHourValue = rtc_tm.tm_hour;
    lblMinuteValue = rtc_tm.tm_min;

    close(rtc_fd);
}

void restore_clock_settings() {
    read_rtc_hardware();

    if (lblYearValue < 110) {
        // We can assume that it is at least the year 2020
        lblYearValue = 120;
        snprintf(rtc_buffer, sizeof(rtc_buffer), "20%d", lblYearValue - 100);
        lv_label_set_text(ui_lblYearValue, rtc_buffer);
    } else {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "20%d", lblYearValue - 100);
    }
    lv_label_set_text(ui_lblYearValue, rtc_buffer);

    if (lblMonthValue < 9) {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMonthValue + 1);
    } else {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMonthValue + 1);
    }
    lv_label_set_text(ui_lblMonthValue, rtc_buffer);

    if (lblDayValue < 10) {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
    } else {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblDayValue);
    }
    lv_label_set_text(ui_lblDayValue, rtc_buffer);

    if (lblHourValue < 10) {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblHourValue);
    } else {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblHourValue);
    }
    lv_label_set_text(ui_lblHourValue, rtc_buffer);

    if (lblMinuteValue < 10) {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMinuteValue);
    } else {
        snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMinuteValue);
    }
    lv_label_set_text(ui_lblMinuteValue, rtc_buffer);

    lblNotationValue = get_ini_int(muos_config, "clock", "notation", 0);
    lv_label_set_text(ui_lblNotationValue, notation[lblNotationValue]);
}

void set_hardware_clock(struct rtc_time *rtc_tm) {
    int rtc_fd = open(RTC_DEVICE, O_RDWR);
    if (rtc_fd == -1) {
        perror("Failed to open RTC device");
        return;
    }

    int result = ioctl(rtc_fd, RTC_SET_TIME, rtc_tm);
    if (result == -1) {
        perror("Failed to set hardware clock");
        close(rtc_fd);
        return;
    }

    close(rtc_fd);
}

void set_new_time() {
    struct tm newTime;
    struct timeval tv;
    struct rtc_time rtc_tm;

    newTime.tm_year = lblYearValue;
    newTime.tm_mon = lblMonthValue;
    newTime.tm_mday = lblDayValue;
    newTime.tm_hour = lblHourValue;
    newTime.tm_min = lblMinuteValue;
    newTime.tm_sec = 0;
    newTime.tm_isdst = -1;

    time_t newTimeSeconds = mktime(&newTime);

    tv.tv_sec = newTimeSeconds;
    tv.tv_usec = 0;

    rtc_tm.tm_sec = 0;
    rtc_tm.tm_min = newTime.tm_min;
    rtc_tm.tm_hour = newTime.tm_hour;
    rtc_tm.tm_mday = newTime.tm_mday;
    rtc_tm.tm_mon = newTime.tm_mon;
    rtc_tm.tm_year = newTime.tm_year;
    rtc_tm.tm_wday = newTime.tm_wday;
    rtc_tm.tm_yday = newTime.tm_yday;
    rtc_tm.tm_isdst = newTime.tm_isdst;

    set_hardware_clock(&rtc_tm);
    settimeofday(&tv, NULL);

    confirm_rtc_config();
}

int days_in_month(int year, int month) {
    int max_days;
    int sel_year = 1900 + year;
    switch (month + 1) {
        case 4:  // April
        case 6:  // June
        case 9:  // September
        case 11: // November
            max_days = 30;
            break;
        case 2:  // February
            if ((sel_year % 4 == 0 && sel_year % 100 != 0) || sel_year % 400 == 0) {
                max_days = 29;
            } else {
                max_days = 28;
            }
            break;
        default:
            max_days = 31;
            break;
    }
    return max_days;
}

void init_navigation_groups() {
    lv_obj_t *ui_objects[] = {
            ui_lblYear,
            ui_lblMonth,
            ui_lblDay,
            ui_lblHour,
            ui_lblMinute,
            ui_lblNotation,
            ui_lblTimezone
    };

    lv_obj_t *ui_objects_value[] = {
            ui_lblYearValue,
            ui_lblMonthValue,
            ui_lblDayValue,
            ui_lblHourValue,
            ui_lblMinuteValue,
            ui_lblNotationValue,
            ui_lblTimezoneValue
    };

    lv_obj_t *ui_objects_icon[] = {
            ui_icoYear,
            ui_icoMonth,
            ui_icoDay,
            ui_icoHour,
            ui_icoMinute,
            ui_icoNotation,
            ui_icoTimezone
    };

    labels[0] = ui_lblYearValue;
    labels[1] = ui_lblMonthValue;
    labels[2] = ui_lblDayValue;
    labels[3] = ui_lblHourValue;
    labels[4] = ui_lblMinuteValue;
    labels[5] = ui_lblNotationValue;
    labels[6] = ui_lblTimezoneValue;

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
                                    if (element_focused == ui_lblTimezone) {
                                        play_sound("confirm", nav_sound);
                                        input_disable = 1;
                                        set_new_time();
                                        load_mux("timezone");
                                        safe_quit = 1;
                                    } else {
                                        play_sound("navigate", nav_sound);
                                        if (element_focused == ui_lblYear) {
                                            if (lblYearValue >= 110 && lblYearValue < 199) {
                                                lblYearValue++;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "20%d",
                                                         lblYearValue - 100);
                                                lv_label_set_text(ui_lblYearValue, rtc_buffer);
                                                lblDayValue = 1;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                                lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                            }
                                        } else if (element_focused == ui_lblMonth) {
                                            if (lblMonthValue < 11) {
                                                lblMonthValue++;
                                                if (lblMonthValue < 9) {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d",
                                                             lblMonthValue + 1);
                                                } else {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "%d",
                                                             lblMonthValue + 1);
                                                }
                                                lv_label_set_text(ui_lblMonthValue, rtc_buffer);
                                                lblDayValue = 1;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                                lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                            } else {
                                                lblMonthValue = 0;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMonthValue + 1);
                                                lv_label_set_text(ui_lblMonthValue, rtc_buffer);
                                                lblDayValue = 1;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                                lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                            }
                                        } else if (element_focused == ui_lblDay) {
                                            if (lblDayValue < days_in_month(lblYearValue, lblMonthValue)) {
                                                lblDayValue++;
                                                if (lblDayValue < 10) {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                                } else {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblDayValue);
                                                }
                                                lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                            } else {
                                                lblDayValue = 1;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                                lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                            }
                                        } else if (element_focused == ui_lblHour) {
                                            if (lblHourValue < 23) {
                                                lblHourValue++;
                                                if (lblHourValue < 10) {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblHourValue);
                                                } else {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblHourValue);
                                                }
                                                lv_label_set_text(ui_lblHourValue, rtc_buffer);
                                            } else {
                                                lblHourValue = 0;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblHourValue);
                                                lv_label_set_text(ui_lblHourValue, rtc_buffer);
                                            }
                                        } else if (element_focused == ui_lblMinute) {
                                            if (lblMinuteValue < 59) {
                                                lblMinuteValue++;
                                                if (lblMinuteValue < 10) {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMinuteValue);
                                                } else {
                                                    snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMinuteValue);
                                                }
                                                lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
                                            } else {
                                                lblMinuteValue = 0;
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMinuteValue);
                                                lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
                                            }
                                        } else if (element_focused == ui_lblNotation) {
                                            lblNotationValue++;
                                            if (lblNotationValue < 0) {
                                                lblNotationValue = 1;
                                            }
                                            if (lblNotationValue > 1) {
                                                lblNotationValue = 0;
                                            }
                                            lv_label_set_text(ui_lblNotationValue, notation[lblNotationValue]);
                                        }
                                    }
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);
                                    input_disable = 1;

                                    osd_message = "Saving Changes";
                                    lv_label_set_text(ui_lblMessage, osd_message);
                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                    set_new_time();

                                    char *cs = "/opt/muos/flag/ClockSetup";
                                    if (file_exist(cs)) {
                                        remove(cs);
                                    }

                                    usleep(100000);
                                    safe_quit = 1;
                                }
                            }
                        } else {
                            if (ev.code == JOY_MENU) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound);
                                    show_help();
                                }
                            }
                        }
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            switch (ev.value) {
                                case -4096:
                                case -1:
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_value, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    play_sound("navigate", nav_sound);
                                    nav_moved = 1;
                                    break;
                                case 1:
                                case 4096:
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_value, 1);
                                    nav_next(ui_group_glyph, 1);
                                    play_sound("navigate", nav_sound);
                                    nav_moved = 1;
                                    break;
                                default:
                                    break;
                            }
                        } else if (ev.code == NAV_DPAD_HOR || ev.code == NAV_ANLG_HOR) {
                            switch (ev.value) {
                                case -4096:
                                case -1:
                                    play_sound("navigate", nav_sound);
                                    if (element_focused == ui_lblYear) {
                                        if (lblYearValue > 110 && lblYearValue <= 199) {
                                            lblYearValue--;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "20%d", lblYearValue - 100);
                                            lv_label_set_text(ui_lblYearValue, rtc_buffer);
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblMonth) {
                                        if (lblMonthValue > 0) {
                                            lblMonthValue--;
                                            if (lblMonthValue < 9) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMonthValue + 1);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMonthValue + 1);
                                            }
                                            lv_label_set_text(ui_lblMonthValue, rtc_buffer);
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        } else {
                                            lblMonthValue = 11;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMonthValue + 1);
                                            lv_label_set_text(ui_lblMonthValue, rtc_buffer);
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblDay) {
                                        if (lblDayValue > 1) {
                                            lblDayValue--;
                                            if (lblDayValue < 10) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblDayValue);
                                            }
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        } else {
                                            lblDayValue = days_in_month(lblYearValue, lblMonthValue);
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblHour) {
                                        if (lblHourValue > 0) {
                                            lblHourValue--;
                                            if (lblHourValue < 10) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblHourValue);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblHourValue);
                                            }
                                            lv_label_set_text(ui_lblHourValue, rtc_buffer);
                                        } else {
                                            lblHourValue = 23;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblHourValue);
                                            lv_label_set_text(ui_lblHourValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblMinute) {
                                        if (lblMinuteValue > 0) {
                                            lblMinuteValue--;
                                            if (lblMinuteValue < 10) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMinuteValue);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMinuteValue);
                                            }
                                            lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
                                        } else {
                                            lblMinuteValue = 59;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMinuteValue);
                                            lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblNotation) {
                                        lblNotationValue--;
                                        if (lblNotationValue < 0) {
                                            lblNotationValue = 1;
                                        }
                                        if (lblNotationValue > 1) {
                                            lblNotationValue = 0;
                                        }

                                        lv_label_set_text(ui_lblNotationValue, notation[lblNotationValue]);
                                        break;
                                    }
                                    break;
                                case 1:
                                case 4096:
                                    play_sound("navigate", nav_sound);
                                    if (element_focused == ui_lblYear) {
                                        if (lblYearValue >= 110 && lblYearValue < 199) {
                                            lblYearValue++;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "20%d", lblYearValue - 100);
                                            lv_label_set_text(ui_lblYearValue, rtc_buffer);
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblMonth) {
                                        if (lblMonthValue < 11) {
                                            lblMonthValue++;
                                            if (lblMonthValue < 9) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMonthValue + 1);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMonthValue + 1);
                                            }
                                            lv_label_set_text(ui_lblMonthValue, rtc_buffer);
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        } else {
                                            lblMonthValue = 0;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMonthValue + 1);
                                            lv_label_set_text(ui_lblMonthValue, rtc_buffer);
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblDay) {
                                        if (lblDayValue < days_in_month(lblYearValue, lblMonthValue)) {
                                            lblDayValue++;
                                            if (lblDayValue < 10) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblDayValue);
                                            }
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        } else {
                                            lblDayValue = 1;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblDayValue);
                                            lv_label_set_text(ui_lblDayValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblHour) {
                                        if (lblHourValue < 23) {
                                            lblHourValue++;
                                            if (lblHourValue < 10) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblHourValue);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblHourValue);
                                            }
                                            lv_label_set_text(ui_lblHourValue, rtc_buffer);
                                        } else {
                                            lblHourValue = 0;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblHourValue);
                                            lv_label_set_text(ui_lblHourValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblMinute) {
                                        if (lblMinuteValue < 59) {
                                            lblMinuteValue++;
                                            if (lblMinuteValue < 10) {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMinuteValue);
                                            } else {
                                                snprintf(rtc_buffer, sizeof(rtc_buffer), "%d", lblMinuteValue);
                                            }
                                            lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
                                        } else {
                                            lblMinuteValue = 0;
                                            snprintf(rtc_buffer, sizeof(rtc_buffer), "0%d", lblMinuteValue);
                                            lv_label_set_text(ui_lblMinuteValue, rtc_buffer);
                                        }
                                        break;
                                    } else if (element_focused == ui_lblNotation) {
                                        lblNotationValue++;
                                        if (lblNotationValue < 0) {
                                            lblNotationValue = 1;
                                        }
                                        if (lblNotationValue > 1) {
                                            lblNotationValue = 0;
                                        }
                                        lv_label_set_text(ui_lblNotationValue, notation[lblNotationValue]);
                                        break;
                                    }
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

    process_visual_element("clock", ui_lblDatetime);
    process_visual_element("battery", ui_staCapacity);
    process_visual_element("network", ui_staNetwork);
    process_visual_element("bluetooth", ui_staBluetooth);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, "Save");
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

    lv_obj_set_user_data(ui_lblYear, "year");
    lv_obj_set_user_data(ui_lblMonth, "month");
    lv_obj_set_user_data(ui_lblDay, "day");
    lv_obj_set_user_data(ui_lblHour, "hour");
    lv_obj_set_user_data(ui_lblMinute, "minute");
    lv_obj_set_user_data(ui_lblNotation, "notation");
    lv_obj_set_user_data(ui_lblTimezone, "timezone");
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

/*
    if (is_network_connected() > 0) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.NETWORK.ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.NETWORK.NORMAL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
*/

    if (atoi(read_text_from_file(BATT_CHARGER))) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.BATTERY.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.BATTERY.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (read_battery_capacity() <= 15) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.BATTERY.LOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.BATTERY.LOW_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.BATTERY.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.BATTERY.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    ui_scrRTC, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    ui_init();
    muos_config = mini_try_load(MUOS_CONFIG_FILE);

    init_elements();

    lv_obj_set_user_data(ui_scrRTC, basename(argv[0]));

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

    switch (mini_get_int(muos_config, "settings.advanced", "swap", LABEL)) {
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

    current_wall = load_wallpaper(ui_scrRTC, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font(basename(argv[0]), ui_scrRTC);

    if (get_ini_int(muos_config, "settings.general", "sound", LABEL) == 2) {
        nav_sound = 1;
    }

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

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    init_elements();
    while (!safe_quit) {
        usleep(SCREEN_WAIT);
    }

    mini_free(muos_config);

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
