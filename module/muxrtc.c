#include "muxshare.h"
#include "ui/ui_muxrtc.h"

#define RTC(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(RTC_ELEMENTS) };
#undef RTC

#define MIN_YEAR        1970
#define MAX_YEAR        2199
#define MONTHS_IN_YEAR  12
#define HOURS_IN_DAY    24
#define MINUTES_IN_HOUR 60
#define NOTATION_COUNT  7
#define NOTATION_CUSTOM 6

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int notation;
    char custom_fmt[MAX_BUFFER_SIZE];
} rtc_state_t;

static rtc_state_t rtc = {2025, 1, 1, 0, 0, 0, ""};
static rtc_state_t rtc_original;

static const char *notation[NOTATION_COUNT];

static int save_mode = 0;
static mux_dialogue save_dlg;

static void hide_save_dialog(void) {
    dialogue_dismiss(&save_mode, &save_dlg);
}

static int any_rtc_modified(void) {
    return rtc.year != rtc_original.year || rtc.month != rtc_original.month || rtc.day != rtc_original.day
           || rtc.hour != rtc_original.hour || rtc.minute != rtc_original.minute
           || rtc.notation != rtc_original.notation || strcmp(rtc.custom_fmt, rtc_original.custom_fmt) != 0;
}

static void list_nav_move(int steps, int direction);

static void handle_keyboard_ok_press(void);

static void show_help(void) {
    show_info_box(lang.muxrtc.title, lang.muxrtc.help, 0);
}

static void toggle_custom_format(const int show) {
    if (show) {
        lv_obj_clear_flag(ui_pnl_custom_rtc, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_custom_rtc, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_pnl_custom_rtc, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_custom_rtc, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void confirm_rtc_config(void) {
    write_text_to_file(CONF_CONFIG_PATH "clock/notation", "w", INT, rtc.notation);

    if (rtc.notation == NOTATION_CUSTOM) {
        write_text_to_file(CONF_CONFIG_PATH "clock/custom", "w", CHAR, rtc.custom_fmt);
    }

    const char *args[] = {"hwclock", "-w", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static int days_in_month(const int year, const int month) {
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
    const time_t current_time = time(NULL);
    struct tm *tm_now = 0;
    int is_error = 0;

    if (current_time == (time_t) -1) {
        is_error = 1;
        LOG_ERROR(mux_module, "Failed to get current time");
    } else {
        tm_now = localtime(&current_time);
    }

    if (!is_error && !tm_now) {
        is_error = 1;
        LOG_ERROR(mux_module, "Failed to convert time to local time");
    }

    if (is_error) {
        LOG_WARN(mux_module, "Using default date and time");
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

    lv_label_set_text_fmt(ui_val_year_rtc, "%04d", rtc.year);
    lv_label_set_text_fmt(ui_val_month_rtc, "%02d", rtc.month);
    lv_label_set_text_fmt(ui_val_day_rtc, "%02d", rtc.day);
    lv_label_set_text_fmt(ui_val_hour_rtc, "%02d", rtc.hour);
    lv_label_set_text_fmt(ui_val_minute_rtc, "%02d", rtc.minute);

    if (config.clock.notation < 0 || config.clock.notation >= NOTATION_COUNT) {
        LOG_WARN(mux_module, "Invalid notation value, defaulting to 24-hour format");
        rtc.notation = time_24_h;
    } else {
        rtc.notation = config.clock.notation;
    }

    snprintf(rtc.custom_fmt, sizeof(rtc.custom_fmt), "%s", *config.clock.custom ? config.clock.custom : TIME_STRING_24);

    lv_label_set_text(ui_val_notation_rtc, notation[rtc.notation]);
    lv_label_set_text(ui_val_custom_rtc, rtc.custom_fmt);

    toggle_custom_format(rtc.notation == NOTATION_CUSTOM);
}

static void
save_clock_settings(const int year, const int month, const int day, const int hour, const int minute, const int n) {
    struct tm t = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = 0,
        .tm_isdst = -1
    };

    int rtc_retry_attempt = 0;
    while (rtc_retry_attempt < RTC_MAX_RETRIES) {
        const time_t time_seconds = mktime(&t);
        if (time_seconds == (time_t) -1) {
            LOG_ERROR(mux_module, "Invalid time retrieved");
            break;
        }

        struct timeval tv = {.tv_sec = time_seconds, .tv_usec = 0};

        if (settimeofday(&tv, NULL) == 0) {
            const struct tm *tm_check = localtime(&time_seconds);
            LOG_SUCCESS(
                mux_module, "Time set to: %04d-%02d-%02d %02d:%02d:00 (DST %s) (notation %d)", year, month, day, hour,
                minute, tm_check && tm_check->tm_isdst > 0 ? "YES" : "NO", n
            );

            confirm_rtc_config();
            refresh_config = 1;

            return;
        }
        LOG_ERROR(mux_module, "%s", lang.system.fail_run_command);
        rtc_retry_attempt++;
        sleep(RTC_RETRY_DELAY);
    }

    LOG_ERROR(mux_module, "Attempt to set system date failed");
    refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, rtc, timezone, lang.muxrtc.timezone, "timezone", "");
    INIT_VALUE_ITEM(-1, rtc, year, lang.muxrtc.year, "year", "");
    INIT_VALUE_ITEM(-1, rtc, month, lang.muxrtc.month, "month", "");
    INIT_VALUE_ITEM(-1, rtc, day, lang.muxrtc.day, "day", "");
    INIT_VALUE_ITEM(-1, rtc, hour, lang.muxrtc.hour, "hour", "");
    INIT_VALUE_ITEM(-1, rtc, minute, lang.muxrtc.minute, "minute", "");
    INIT_VALUE_ITEM(-1, rtc, notation, lang.muxrtc.notation, "notation", "");
    INIT_VALUE_ITEM(-1, rtc, custom, lang.muxrtc.custom, "custom", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    list_nav_move(direct_to_previous(ui_objects, ui_count_static, &nav_moved), +1);
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    const int show_a = e_focused == ui_lbl_timezone_rtc || e_focused == ui_lbl_custom_rtc;
    const int show_lr = !show_a;

    if (show_a) {
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else if (show_lr) {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 0, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void validate_notation(void) {
    if (rtc.notation < 0) rtc.notation = NOTATION_COUNT - 1;
    if (rtc.notation >= NOTATION_COUNT) rtc.notation = 0;
}

static void validate_minute(void) {
    if (rtc.minute < 0) rtc.minute = MINUTES_IN_HOUR - 1;
    if (rtc.minute >= MINUTES_IN_HOUR) rtc.minute = 0;

    validate_notation();
}

static void validate_hour(void) {
    if (rtc.hour < 0) rtc.hour = HOURS_IN_DAY - 1;
    if (rtc.hour >= HOURS_IN_DAY) rtc.hour = 0;

    validate_minute();
}

static void validate_day(void) {
    const int max_days = days_in_month(rtc.year, rtc.month);

    if (rtc.day < 1) rtc.day = max_days;
    if (rtc.day > max_days) rtc.day = 1;

    validate_hour();
}

static void validate_month(void) {
    if (rtc.month < 1) rtc.month = MONTHS_IN_YEAR;
    if (rtc.month > MONTHS_IN_YEAR) rtc.month = 1;

    validate_day();
}

static void validate_year(void) {
    if (rtc.year < MIN_YEAR) rtc.year = MIN_YEAR;
    if (rtc.year > MAX_YEAR) rtc.year = MAX_YEAR;

    validate_month();
}

static void adjust_year(const int direction) {
    rtc.year += direction;
    validate_year();
}

static void adjust_month(const int direction) {
    rtc.month += direction;
    validate_month();
}

static void adjust_day(const int direction) {
    rtc.day += direction;
    validate_day();
}

static void adjust_hour(const int direction) {
    rtc.hour += direction;
    validate_hour();
}

static void adjust_minute(const int direction) {
    rtc.minute += direction;
    validate_minute();
}

static void adjust_notation(const int direction) {
    rtc.notation += direction;
    validate_notation();
    toggle_custom_format(rtc.notation == NOTATION_CUSTOM);
}

static void check_rtc_state(const rtc_state_t *r, const rtc_state_t *old) {
    if (r->year != old->year) lv_label_set_text_fmt(ui_val_year_rtc, "%04d", r->year);
    if (r->month != old->month) lv_label_set_text_fmt(ui_val_month_rtc, "%02d", r->month);
    if (r->day != old->day) lv_label_set_text_fmt(ui_val_day_rtc, "%02d", r->day);
    if (r->hour != old->hour) lv_label_set_text_fmt(ui_val_hour_rtc, "%02d", r->hour);
    if (r->minute != old->minute) lv_label_set_text_fmt(ui_val_minute_rtc, "%02d", r->minute);
    if (r->notation != old->notation) lv_label_set_text(ui_val_notation_rtc, notation[r->notation]);
    if (strcmp(r->custom_fmt, old->custom_fmt) != 0) lv_label_set_text(ui_val_custom_rtc, r->custom_fmt);
}

static void adjust_option(const int direction) {
    if (msgbox_active || hold_call) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_timezone_rtc || e_focused == ui_lbl_custom_rtc) return;

    const rtc_state_t old_rtc = rtc;
    play_sound(snd_option);

    if (e_focused == ui_lbl_year_rtc) {
        adjust_year(direction);
    } else if (e_focused == ui_lbl_month_rtc) {
        adjust_month(direction);
    } else if (e_focused == ui_lbl_day_rtc) {
        adjust_day(direction);
    } else if (e_focused == ui_lbl_hour_rtc) {
        adjust_hour(direction);
    } else if (e_focused == ui_lbl_minute_rtc) {
        adjust_minute(direction);
    } else if (e_focused == ui_lbl_notation_rtc) {
        adjust_notation(direction);
    }

    check_rtc_state(&rtc, &old_rtc);
}

static int is_valid_date(void) {
    if (rtc.year < MIN_YEAR || rtc.year > MAX_YEAR) return 0;
    if (rtc.month < 1 || rtc.month > MONTHS_IN_YEAR) return 0;
    if (rtc.day < 1 || rtc.day > days_in_month(rtc.year, rtc.month)) return 0;
    if (rtc.hour < 0 || rtc.hour >= HOURS_IN_DAY) return 0;
    if (rtc.minute < 0 || rtc.minute >= MINUTES_IN_HOUR) return 0;

    return 1;
}

static void save_and_exit(const char *message) {
    validate_year();

    if (!is_valid_date()) {
        toast_message(lang.generic.invalid_time, tst_wait_m);
        return;
    }

    toast_message(message, tst_wait_f);
    save_clock_settings(rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.notation);

    mux_input_stop();
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_keyboard_ok_press(void) {
    const char *val = lv_textarea_get_text(ui_txt_entry_rtc);
    if (val && *val) {
        snprintf(rtc.custom_fmt, sizeof(rtc.custom_fmt), "%s", val);
        lv_label_set_text(ui_val_custom_rtc, rtc.custom_fmt);
    }

    key_show = 0;
    reset_osk(key_entry);
    lv_textarea_set_text(ui_txt_entry_rtc, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_rtc);
}

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) {
            if (config.boot.factory_reset) {
                write_text_to_file(CONF_CONFIG_PATH "boot/clock_setup", "w", INT, 0);
            } else {
                write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");
            }
            save_and_exit(lang.generic.saving);
        } else {
            play_sound(snd_back);
            if (config.boot.factory_reset) {
                write_text_to_file(CONF_CONFIG_PATH "boot/clock_setup", "w", INT, 0);
            } else {
                write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");
            }
            mux_input_stop();
        }
        return;
    }

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (msgbox_active || hold_call) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_timezone_rtc) {
        if (is_ksk(kiosk.datetime.timezone)) return;

        play_sound(snd_confirm);

        load_mux("timezone");
        save_and_exit(lang.generic.loading);
    } else if (e_focused == ui_lbl_custom_rtc) {
        play_sound(snd_confirm);
        key_curr = 0;
        first_open = 1;
        lv_textarea_set_text(ui_txt_entry_rtc, rtc.custom_fmt);
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);
        key_show = 1;
        osk_show(ui_pnl_entry_rtc);
        osk_refresh_labels();
    } else {
        adjust_option(+1);
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (key_show) {
        key_backspace(ui_txt_entry_rtc);
        return;
    }

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (dialogue_guard_unsaved(&save_mode, &save_dlg, &theme, any_rtc_modified())) return;

    play_sound(snd_back);

    if (config.boot.factory_reset) {
        write_text_to_file(CONF_CONFIG_PATH "boot/clock_setup", "w", INT, 0);
    } else {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");
    }

    save_and_exit(lang.generic.saving);
}

static void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }

    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    adjust_option(-1);
}

static void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    adjust_option(+1);
}

static void handle_dpad_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_l1(void) {
    if (key_show) {
        key_swap_back();
        return;
    }

    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) {
        key_swap();
        return;
    }

    handle_list_nav_page_down();
}

static void handle_x(void) {
    if (!key_show) return;

    close_osk(key_entry, ui_group, ui_txt_entry_rtc, ui_pnl_entry_rtc);
}

static void handle_y(void) {
    if (key_show) {
        key_space(ui_txt_entry_rtc);
    }
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_rtc);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode || key_show) return;

    play_sound(snd_info_open);
    show_help();
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_ok_press();
    ev.code == KEY_ESC &&ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txt_entry_rtc);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define RTC(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_rtc, UDATA);
    RTC_ELEMENTS
#undef RTC

    overlay_display();
}

int muxrtc_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxrtc.title);
    init_muxrtc(ui_screen, ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();

    notation[0] = lang.muxrtc.f_12_hr;
    notation[1] = lang.muxrtc.f_24_hr;
    notation[2] = lang.muxrtc.f_dd_mm_12;
    notation[3] = lang.muxrtc.f_dd_mm_24;
    notation[4] = lang.muxrtc.f_mm_dd_12;
    notation[5] = lang.muxrtc.f_mm_dd_24;
    notation[6] = lang.muxrtc.f_custom;

    init_navigation_group();
    restore_clock_settings();
    rtc_original = rtc;

    init_osk(ui_pnl_entry_rtc, ui_txt_entry_rtc, 0, 0, OSK_MAX);
    register_key_event_callback(on_key_event);

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );
    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_left] = handle_left,
            [mux_input_dpad_right] = handle_right,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
