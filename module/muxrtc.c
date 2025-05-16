#include "muxshare.h"
#include "muxrtc.h"
#include "ui/ui_muxrtc.h"
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/log.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int notation;
} rtc_state_t;
static rtc_state_t rtc = {
        2025, 1, 1, 0, 0, 0
};

#define UI_COUNT 7
#define UI_PANEL 5
static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_mux_panels[UI_PANEL];

const char *notation[] = {
        NULL, NULL
};

static void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXRTC.TITLE, lang.MUXRTC.HELP);
}

static void confirm_rtc_config() {
    const char *notation_type = lv_label_get_text(ui_lblNotationValue);
    int idx_notation = (notation_type && strcmp(notation_type, notation[1]) == TIME_12H) ? TIME_24H : TIME_12H;

    write_text_to_file((RUN_GLOBAL_PATH "clock/notation"), "w", INT, idx_notation);

    const char *args[] = {"hwclock", "-w", NULL};
    run_exec(args, A_SIZE(args), 1);
}

static void set_dt_label(lv_obj_t *label, const char *format, int value) {
    char rtc_buffer[8];
    snprintf(rtc_buffer, sizeof(rtc_buffer), format, value);
    lv_label_set_text(label, rtc_buffer);
}

static void restore_clock_settings() {
    int year, month, day, hour, minute;
    time_t now = time(NULL);
    if (now == (time_t) -1) {
        LOG_ERROR(mux_module, "Failed to retrieve current time")
        return;
    }

    struct tm *tm_now = localtime(&now);
    if (!tm_now) {
        LOG_ERROR(mux_module, "Failed to convert current time")
        return;
    }

    rtc.year = year = tm_now->tm_year + 1900;
    rtc.month = month = tm_now->tm_mon + 1;
    rtc.day = day = tm_now->tm_mday;
    rtc.hour = hour = tm_now->tm_hour;
    rtc.minute = minute = tm_now->tm_min;

    set_dt_label(ui_lblYearValue, "%04d", year);
    set_dt_label(ui_lblMonthValue, "%02d", month);
    set_dt_label(ui_lblDayValue, "%02d", day);
    set_dt_label(ui_lblHourValue, "%02d", hour);
    set_dt_label(ui_lblMinuteValue, "%02d", minute);

    rtc.notation = config.CLOCK.NOTATION;
    lv_label_set_text(ui_lblNotationValue, notation[rtc.notation]);
}

static void save_clock_settings(int year, int month, int day, int hour, int minute, int notation) {
    struct tm t = {
            .tm_year = year - 1900,
            .tm_mon  = month - 1,
            .tm_mday = day,
            .tm_hour = hour,
            .tm_min  = minute,
            .tm_sec  = 0,
            .tm_isdst = -1
    };

    int rtc_retry_attempt = 0;
    while (rtc_retry_attempt < RTC_MAX_RETRIES) {
        time_t time_seconds = mktime(&t);
        if (time_seconds == (time_t) -1) {
            LOG_ERROR(mux_module, "Invalid time retrieved")
            break;
        }

        struct timeval tv = {
                .tv_sec = time_seconds,
                .tv_usec = 0
        };

        if (settimeofday(&tv, NULL) == 0) {
            const struct tm *tm_check = localtime(&time_seconds);
            LOG_SUCCESS(mux_module, "Time successfully set to: %04d-%02d-%02d %02d:%02d:00 (DST %s) (%sH)",
                        year, month, day, hour, minute,
                        tm_check && tm_check->tm_isdst > 0 ? "YES" : "NO",
                        notation ? "12" : "24")

            confirm_rtc_config();
            refresh_config = 1;

            return;
        } else {
            perror(lang.SYSTEM.FAIL_RUN_COMMAND);
            rtc_retry_attempt++;
            sleep(RTC_RETRY_DELAY);
        }
    }

    LOG_ERROR(mux_module, "Attempt to set system date failed")
    refresh_config = 1;
}

static int days_in_month(int year, int month) {
    int max_days;
    switch (month) {
        case 2:  // February
            max_days = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 29 : 28;
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

static void init_navigation_group() {
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

    apply_theme_list_panel(ui_pnlYear);
    apply_theme_list_panel(ui_pnlMonth);
    apply_theme_list_panel(ui_pnlDay);
    apply_theme_list_panel(ui_pnlHour);
    apply_theme_list_panel(ui_pnlMinute);
    apply_theme_list_panel(ui_pnlNotation);
    apply_theme_list_panel(ui_pnlTimezone);

    apply_theme_list_item(&theme, ui_lblYear, lang.MUXRTC.YEAR);
    apply_theme_list_item(&theme, ui_lblMonth, lang.MUXRTC.MONTH);
    apply_theme_list_item(&theme, ui_lblDay, lang.MUXRTC.DAY);
    apply_theme_list_item(&theme, ui_lblHour, lang.MUXRTC.HOUR);
    apply_theme_list_item(&theme, ui_lblMinute, lang.MUXRTC.MINUTE);
    apply_theme_list_item(&theme, ui_lblNotation, lang.MUXRTC.NOTATION);
    apply_theme_list_item(&theme, ui_lblTimezone, lang.MUXRTC.TIMEZONE);

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

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

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

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblTimezone) {
        lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavLR, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavLRGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavLR, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavLRGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void adjust_option(int direction) {
    if (msgbox_active) return;
    play_sound(SND_OPTION, 0);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblYear) {
        if ((direction < 0 && rtc.year > 1970) ||
            (direction > 0 && rtc.year < 2199)) {
            rtc.year += direction;
        }
        set_dt_label(ui_lblYearValue, "%04d", rtc.year);
        set_dt_label(ui_lblDayValue, "%02d", 1);
    } else if (element_focused == ui_lblMonth) {
        rtc.month += direction;
        if (rtc.month > 12) rtc.month = 1;
        if (rtc.month < 1) rtc.month = 12;
        set_dt_label(ui_lblMonthValue, "%02d", rtc.month);
        set_dt_label(ui_lblDayValue, "%02d", 1);
    } else if (element_focused == ui_lblDay) {
        int max_days = days_in_month(rtc.year, rtc.month);
        rtc.day += direction;
        if (rtc.day > max_days) rtc.day = 1;
        if (rtc.day < 1) rtc.day = max_days;
        set_dt_label(ui_lblDayValue, "%02d", rtc.day);
    } else if (element_focused == ui_lblHour) {
        rtc.hour = (rtc.hour + direction + 24) % 24;
        set_dt_label(ui_lblHourValue, "%02d", rtc.hour);
    } else if (element_focused == ui_lblMinute) {
        rtc.minute = (rtc.minute + direction + 60) % 60;
        set_dt_label(ui_lblMinuteValue, "%02d", rtc.minute);
    } else if (element_focused == ui_lblNotation) {
        rtc.notation += direction;

        if (rtc.notation < 0) rtc.notation = 1;
        if (rtc.notation > 1) rtc.notation = 0;

        lv_label_set_text(ui_lblNotationValue, notation[rtc.notation]);
    }
}

static void save_and_exit(char *message) {
    toast_message(message, 0, 0);
    refresh_screen(ui_screen);

    save_clock_settings(rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.notation);

    close_input();
    mux_input_stop();
}

static void handle_a() {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) == ui_lblTimezone) {
        if (kiosk.DATETIME.TIMEZONE) return;

        play_sound(SND_CONFIRM, 0);

        load_mux("timezone");
        save_and_exit(lang.GENERIC.LOADING);
    } else {
        adjust_option(+1);
    }
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);

    if (config.BOOT.FACTORY_RESET) {
        write_text_to_file((RUN_GLOBAL_PATH "boot/clock_setup"), "w", INT, 0);
    } else {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");
    }

    save_and_exit(lang.GENERIC.SAVING);
}

static void handle_left() {
    adjust_option(-1);
}

static void handle_right() {
    adjust_option(+1);
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
        show_help();
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

    lv_label_set_text(ui_lblNavLR, lang.GENERIC.CHANGE);
    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    if (config.BOOT.FACTORY_RESET) {
        lv_label_set_text(ui_lblNavB, lang.GENERIC.INSTALL);
    } else {
        lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);
    }

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblYear, "year");
    lv_obj_set_user_data(ui_lblMonth, "month");
    lv_obj_set_user_data(ui_lblDay, "day");
    lv_obj_set_user_data(ui_lblHour, "hour");
    lv_obj_set_user_data(ui_lblMinute, "minute");
    lv_obj_set_user_data(ui_lblNotation, "notation");
    lv_obj_set_user_data(ui_lblTimezone, "timezone");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxrtc_main() {
    init_module("muxrtc");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXRTC.TITLE);
    init_muxrtc(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();

    notation[0] = lang.MUXRTC.F_12HR;
    notation[1] = lang.MUXRTC.F_24HR;

    init_navigation_group();
    restore_clock_settings();

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
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
