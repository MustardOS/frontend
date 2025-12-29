#include "muxshare.h"
#include "ui/ui_muxrtc.h"

#define UI_COUNT 7

#define MIN_YEAR 1970
#define MAX_YEAR 2199
#define MONTHS_IN_YEAR 12
#define HOURS_IN_DAY 24
#define MINUTES_IN_HOUR 60

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int notation;
} rtc_state_t;

static rtc_state_t rtc = {2025, 1, 1, 0, 0, 0};

const char *notation[] = {NULL, NULL};

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    show_info_box(lang.MUXRTC.TITLE, lang.MUXRTC.HELP, 0);
}

static void confirm_rtc_config(void) {
    const char *notation_type = lv_label_get_text(ui_lblNotationValue_rtc);
    int idx_notation = (notation_type && strcmp(notation_type, notation[1]) == TIME_12H) ? TIME_24H : TIME_12H;

    write_text_to_file(CONF_CONFIG_PATH
                       "clock/notation", "w", INT, idx_notation);

    const char *args[] = {"hwclock", "-w", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static int days_in_month(int year, int month) {
    int max_days;
    switch (month) {
        case 2: // February
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

static void restore_clock_settings(void) {
    time_t now = time(NULL);
    struct tm *tm_now;
    bool is_error = false;

    if (now == (time_t) -1) {
        is_error = true;
        LOG_ERROR(mux_module, "Failed to get current time")
    } else {
        tm_now = localtime(&now);
    }

    if (!is_error && !tm_now) {
        is_error = true;
        LOG_ERROR(mux_module, "Failed to convert time to local time")
    }

    if (is_error) {
        LOG_WARN(mux_module, "Using default date and time")
        rtc.year = 2025;
        rtc.month = 1;
        rtc.day = 1;
        rtc.hour = 0;
        rtc.minute = 0;
    } else {
        rtc.year = tm_now->tm_year + 1900;
        rtc.month = tm_now->tm_mon + 1;
        rtc.day = tm_now->tm_mday;
        rtc.hour = tm_now->tm_hour;
        rtc.minute = tm_now->tm_min;
    }

    lv_label_set_text_fmt(ui_lblYearValue_rtc, "%04d", rtc.year);
    lv_label_set_text_fmt(ui_lblMonthValue_rtc, "%02d", rtc.month);
    lv_label_set_text_fmt(ui_lblDayValue_rtc, "%02d", rtc.day);
    lv_label_set_text_fmt(ui_lblHourValue_rtc, "%02d", rtc.hour);
    lv_label_set_text_fmt(ui_lblMinuteValue_rtc, "%02d", rtc.minute);

    if (config.CLOCK.NOTATION < 0 || config.CLOCK.NOTATION > 1) {
        LOG_WARN(mux_module, "Invalid notation value, defaulting to 24-hour format")
        rtc.notation = TIME_24H;
    } else {
        rtc.notation = config.CLOCK.NOTATION;
    }

    lv_label_set_text(ui_lblNotationValue_rtc, notation[rtc.notation]);
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
            LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_RUN_COMMAND)
            rtc_retry_attempt++;
            sleep(RTC_RETRY_DELAY);
        }
    }

    LOG_ERROR(mux_module, "Attempt to set system date failed")
    refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, rtc, Timezone, lang.MUXRTC.TIMEZONE, "timezone", "");
    INIT_VALUE_ITEM(-1, rtc, Year, lang.MUXRTC.YEAR, "year", "");
    INIT_VALUE_ITEM(-1, rtc, Month, lang.MUXRTC.MONTH, "month", "");
    INIT_VALUE_ITEM(-1, rtc, Day, lang.MUXRTC.DAY, "day", "");
    INIT_VALUE_ITEM(-1, rtc, Hour, lang.MUXRTC.HOUR, "hour", "");
    INIT_VALUE_ITEM(-1, rtc, Minute, lang.MUXRTC.MINUTE, "minute", "");
    INIT_VALUE_ITEM(-1, rtc, Notation, lang.MUXRTC.NOTATION, "notation", "");

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

    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);
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

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblTimezone_rtc) {
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void validate_notation(void) {
    if (rtc.notation < 0)
        rtc.notation = 1;
    if (rtc.notation > 1)
        rtc.notation = 0;
}

static void validate_minute(void) {
    if (rtc.minute < 0)
        rtc.minute = MINUTES_IN_HOUR - 1;
    if (rtc.minute >= MINUTES_IN_HOUR)
        rtc.minute = 0;

    validate_notation();
}

static void validate_hour(void) {
    if (rtc.hour < 0)
        rtc.hour = HOURS_IN_DAY - 1;
    if (rtc.hour >= HOURS_IN_DAY)
        rtc.hour = 0;

    validate_minute();
}

static void validate_day(void) {
    int max_days = days_in_month(rtc.year, rtc.month);
    if (rtc.day < 1)
        rtc.day = max_days;
    if (rtc.day > max_days)
        rtc.day = 1;

    validate_hour();
}

static void validate_month(void) {
    if (rtc.month < 1)
        rtc.month = MONTHS_IN_YEAR;
    if (rtc.month > MONTHS_IN_YEAR)
        rtc.month = 1;

    validate_day();
}

static void validate_year(void) {
    if (rtc.year < MIN_YEAR)
        rtc.year = MIN_YEAR;
    if (rtc.year > MAX_YEAR)
        rtc.year = MAX_YEAR;

    validate_month();
}

static void adjust_year(int direction) {
    rtc.year += direction;
    validate_year();
}

static void adjust_month(int direction) {
    rtc.month += direction;
    validate_month();
}

static void adjust_day(int direction) {
    rtc.day += direction;
    validate_day();
}

static void adjust_hour(int direction) {
    rtc.hour += direction;
    validate_hour();
}

static void adjust_minute(int direction) {
    rtc.minute += direction;
    validate_minute();
}

static void adjust_notation(int direction) {
    rtc.notation += direction;
    validate_notation();
}

static void check_rtc_state(rtc_state_t *rtc, rtc_state_t *old_rtc) {
    if (rtc->year != old_rtc->year) lv_label_set_text_fmt(ui_lblYearValue_rtc, "%04d", rtc->year);
    if (rtc->month != old_rtc->month) lv_label_set_text_fmt(ui_lblMonthValue_rtc, "%02d", rtc->month);
    if (rtc->day != old_rtc->day) lv_label_set_text_fmt(ui_lblDayValue_rtc, "%02d", rtc->day);
    if (rtc->hour != old_rtc->hour) lv_label_set_text_fmt(ui_lblHourValue_rtc, "%02d", rtc->hour);
    if (rtc->minute != old_rtc->minute) lv_label_set_text_fmt(ui_lblMinuteValue_rtc, "%02d", rtc->minute);
    if (rtc->notation != old_rtc->notation) lv_label_set_text(ui_lblNotationValue_rtc, notation[rtc->notation]);
}

static void adjust_option(int direction) {
    if (msgbox_active || hold_call) return;

    rtc_state_t old_rtc = rtc;
    play_sound(SND_OPTION);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblYear_rtc) {
        adjust_year(direction);
    } else if (element_focused == ui_lblMonth_rtc) {
        adjust_month(direction);
    } else if (element_focused == ui_lblDay_rtc) {
        adjust_day(direction);
    } else if (element_focused == ui_lblHour_rtc) {
        adjust_hour(direction);
    } else if (element_focused == ui_lblMinute_rtc) {
        adjust_minute(direction);
    } else if (element_focused == ui_lblNotation_rtc) {
        adjust_notation(direction);
    }

    check_rtc_state(&rtc, &old_rtc);
}

static void save_and_exit(char *message) {
    toast_message(message, FOREVER);
    refresh_screen(ui_screen);

    // Validate the final RTC state before saving
    validate_year();
    save_clock_settings(rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.notation);

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (lv_group_get_focused(ui_group) == ui_lblTimezone_rtc) {
        if (is_ksk(kiosk.DATETIME.TIMEZONE)) return;

        play_sound(SND_CONFIRM);

        load_mux("timezone");
        save_and_exit(lang.GENERIC.LOADING);
    } else {
        adjust_option(+1);
    }
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

    if (config.BOOT.FACTORY_RESET) {
        write_text_to_file(CONF_CONFIG_PATH "boot/clock_setup", "w", INT, 0);
    } else {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");
    }

    save_and_exit(lang.GENERIC.SAVING);
}

static void handle_left(void) {
    adjust_option(-1);
}

static void handle_right(void) {
    adjust_option(+1);
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
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
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.SAVE,   0},
            {NULL, NULL,                            0}
    });

#define RTC(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_rtc, UDATA);
    RTC_ELEMENTS
#undef RTC

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

int muxrtc_main(void) {
    init_module(__func__);
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
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
