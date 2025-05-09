#include "muxshare.h"
#include "ui/ui_muxcharge.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"

static int exit_status = -1;
static int blank = 0;

static char capacity_info[MAX_BUFFER_SIZE];
static char voltage_info[MAX_BUFFER_SIZE];

lv_timer_t *battery_timer;

#define CHARGER_EXIT "/tmp/charger_exit"

static void check_for_cable() {
    if (file_exist(device.BATTERY.CHARGER)) {
        if (read_int_from_file(device.BATTERY.CHARGER, 1) == 0) {
            exit_status = 1;
        }
    }
}

static void set_brightness(int brightness) {
    char bright_value[8];
    snprintf(bright_value, sizeof(bright_value), "%d", brightness);

    const char *args[] = {(INTERNAL_PATH "device/current/input/bright.sh"), bright_value, NULL};
    run_exec(args, A_SIZE(args), 1);
}

static void handle_power_short(void) {
    if (blank < 3) {
        lv_timer_pause(battery_timer);

        lv_obj_add_flag(ui_lblCapacity_charge, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblVoltage_charge, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

        lv_label_set_text(ui_lblBoot_charge, lang.MUXCHARGE.BOOT);
        refresh_screen(ui_scrCharge_charge);

        exit_status = 0;
        return;
    }

    blank = 0;
    set_brightness(read_int_from_file(INTERNAL_PATH "config/brightness.txt", 1));
}

static void handle_idle(void) {
    if (file_exist("/tmp/mux_blank")) {
        lv_obj_set_style_bg_opa(ui_blank_charge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(ui_blank_charge);
    } else {
        if (lv_obj_get_style_bg_opa(ui_blank_charge, LV_PART_MAIN | LV_STATE_DEFAULT) > 0) {
            lv_obj_set_style_bg_opa(ui_blank_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_move_background(ui_blank_charge);
        }
    }

    if (exit_status >= 0) {
        write_text_to_file(CHARGER_EXIT, "w", INT, exit_status);

        close_input();
        safe_quit(0);
        mux_input_stop();

        return;
    }

    refresh_screen(ui_scrCharge_charge);
}

static void battery_task_charge() {
    snprintf(capacity_info, sizeof(capacity_info), "%s: %d%%", lang.MUXCHARGE.CAPACITY, read_battery_capacity());
    snprintf(voltage_info, sizeof(voltage_info), "%s: %s", lang.MUXCHARGE.VOLTAGE, read_battery_voltage());

    lv_label_set_text(ui_lblCapacity_charge, capacity_info);
    lv_label_set_text(ui_lblVoltage_charge, voltage_info);

    if (blank == 3) set_brightness(0);
    check_for_cable();
    blank++;
}

int main() {
    load_device(&device);
    load_config(&config);

    init_module("muxcharge");
    setup_background_process();

    init_theme(0, 0);
    init_display();

    init_muxcharge();
    set_brightness(read_int_from_file(INTERNAL_PATH "config/brightness.txt", 1));

    lv_obj_set_user_data(ui_scrCharge_charge, mux_module);
    lv_label_set_text(ui_lblBoot_charge, lang.MUXCHARGE.POWER);

    load_wallpaper(ui_scrCharge_charge, NULL, ui_pnlWall_charge, ui_imgWall_charge, GENERAL);
    load_font_text(ui_scrCharge_charge);

#if TEST_IMAGE
    display_testing_message(ui_scrCharge_charge);
#endif

    overlay_image = lv_img_create(ui_scrCharge_charge);
    load_overlay_image(ui_scrCharge_charge, overlay_image);

    lv_obj_set_y(ui_pnlCharge_charge, theme.CHARGER.Y_POS);

    battery_task_charge();
    battery_timer = lv_timer_create(battery_task_charge, TIMER_BATTERY, NULL);

    refresh_screen(ui_scrCharge_charge);

    mux_input_options input_opts = {
            .press_handler = {[MUX_INPUT_POWER_SHORT] = handle_power_short},
            .idle_handler = handle_idle
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    return 0;
}
