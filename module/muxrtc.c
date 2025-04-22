#include "muxshare.h"
#include "muxrtc.h"
#include "../lvgl/lvgl.h"
#include "ui/ui_muxrtc.h"
#include <unistd.h>
#include <stdio.h>
#include <libgen.h>
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


static int rtcYearValue;
static int rtcMonthValue;
static int rtcDayValue;
static int rtcHourValue;
static int rtcMinuteValue;
static int rtcNotationValue = 0;

static char rtc_buffer[32];


#define UI_COUNT 7
static lv_obj_t *ui_objects[UI_COUNT];

static lv_obj_t *ui_mux_panels[5];

const char *notation[] = {
        NULL, NULL
};

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXRTC.TITLE, lang.MUXRTC.HELP);
}

static void confirm_rtc_config() {
    int idx_notation = 0;
    char *notation_type = lv_label_get_text(ui_lblNotationValue);

    if (!strcmp(notation_type, notation[1])) idx_notation = 1;

    write_text_to_file((RUN_GLOBAL_PATH "clock/notation"), "w", INT, idx_notation);
    run_exec((const char *[]) {"hwclock", "-w", NULL});
}

static void restore_clock_settings() {
    FILE *fp;
    char date_output[100];
    int attempts = 0;

    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "date +\"%%Y %%m %%d %%H %%M\"");

    while (attempts < RTC_MAX_RETRIES) {
        fp = popen(command, "r");
        if (!fp) {
            perror(lang.SYSTEM.FAIL_RUN_COMMAND);
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }

        if (fgets(date_output, sizeof(date_output) - 1, fp)) {
            pclose(fp);
            break;
        } else {
            perror(lang.SYSTEM.FAIL_READ_COMMAND);
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

static void save_clock_settings(int year, int month, int day, int hour, int minute) {
    FILE *fp;
    int attempts = 0;

    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "date \"%04d-%02d-%02d %02d:%02d:00\"",
             year, month, day, hour, minute);

    printf("SETTING DATE TO: %s\n", command);

    while (attempts < RTC_MAX_RETRIES) {
        fp = popen(command, "r");
        if (!fp) {
            perror(lang.SYSTEM.FAIL_RUN_COMMAND);
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }

        if (pclose(fp) == -1) {
            perror(lang.SYSTEM.FAIL_CLOSE_COMMAND);
            attempts++;
            sleep(RTC_RETRY_DELAY);
            continue;
        }

        confirm_rtc_config();

        return;
    }

    fprintf(stderr, "Attempts to set system date failed\n");
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

static void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = !current_item_index ? ui_count - 1 : current_item_index - 1;
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

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblTimezone) {
        if (kiosk.DATETIME.TIMEZONE) return;

        play_sound("confirm", nav_sound, 0, 1);
        save_clock_settings(rtcYearValue, rtcMonthValue, rtcDayValue,
                            rtcHourValue, rtcMinuteValue);
        load_mux("timezone");
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "timezone");

        safe_quit(0);
        mux_input_stop();
    } else {
        play_sound("navigate", nav_sound, 0, 0);
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

static void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    save_clock_settings(rtcYearValue, rtcMonthValue, rtcDayValue,
                        rtcHourValue, rtcMinuteValue);

    usleep(device.SCREEN.WAIT);

    char config_file[MAX_BUFFER_SIZE];
    snprintf(config_file, sizeof(config_file),
             "%s/config/config.ini", INTERNAL_PATH);

    write_text_to_file((RUN_GLOBAL_PATH "boot/clock_setup"), "w", INT, 0);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "clock");

    safe_quit(0);
    mux_input_stop();
}

static void handle_left() {
    if (msgbox_active) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0, 0);
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

static void handle_right() {
    if (msgbox_active) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0, 0);
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

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
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
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxrtc_main() {
    
    snprintf(mux_module, sizeof(mux_module), "muxrtc");
    
            
    init_theme(1, 0);
    
    init_ui_common_screen(&theme, &device, &lang, lang.MUXRTC.TITLE);
    init_muxrtc(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_sound(&nav_sound, mux_module);

    notation[0] = lang.MUXRTC.F_12HR;
    notation[1] = lang.MUXRTC.F_24HR;

    init_navigation_group();
    restore_clock_settings();

    load_kiosk(&kiosk);
    list_nav_next(direct_to_previous(ui_objects, UI_COUNT, &nav_moved));

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
