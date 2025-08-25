#include "muxshare.h"
#include "ui/ui_muxcharge.h"
#include "../lvgl/src/drivers/display/sdl.h"

static int exit_status = -1;
static int blank_timeout = 3;

static char capacity_info[MAX_BUFFER_SIZE];
static char voltage_info[MAX_BUFFER_SIZE];

#define CHARGER_BRIGHT "/tmp/charger_bright"
#define CHARGER_EXIT "/tmp/charger_exit"

static void check_for_cable(void) {
    if (file_exist(device.BATTERY.CHARGER) && !read_line_int_from(device.BATTERY.CHARGER, 1)) exit_status = 1;
}

static int blank_check(void) {
    if (file_exist(MUX_BLANK)) {
        is_blank = 1;

        lv_obj_set_style_bg_opa(ui_blank_charge, 255, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_foreground(ui_blank_charge);
    } else {
        is_blank = 0;

        lv_obj_set_style_bg_opa(ui_blank_charge, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_background(ui_blank_charge);
    }

    return is_blank;
}

static void set_brightness(int brightness) {
    char bright_value[8];
    snprintf(bright_value, sizeof(bright_value), "%d", brightness);

    const char *args[] = {(INTERNAL_PATH "device/script/bright.sh"), bright_value, NULL};
    run_exec(args, A_SIZE(args), 0);

    load_config(&config);
}

static void handle_start(void) {
    LOG_SUCCESS(mux_module, "Start Button Pressed")

    if (is_blank) {
        int bright_value = read_line_int_from(CHARGER_BRIGHT, 1);

        LOG_INFO(mux_module, "Setting Brightness To: %d", bright_value)
        set_brightness(bright_value);

        if (file_exist(MUX_BLANK)) remove(MUX_BLANK);
        blank_check();

        blank_timeout = 4;

        return;
    }

    lv_obj_add_flag(ui_lblCapacity_charge, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblVoltage_charge, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lblBoot_charge, lang.MUXCHARGE.BOOT);
    lv_refr_now(NULL);

    exit_status = 0;
}

static void handle_idle(void) {
    if (exit_status >= 0) {
        write_text_to_file(CHARGER_EXIT, "w", INT, exit_status);
        if (file_exist(CHARGER_BRIGHT)) remove(CHARGER_BRIGHT);

        close_input();
        safe_quit(0);
        mux_input_stop();

        return;
    }

    if (!is_blank) refresh_screen(ui_scrCharge_charge);
}

static void battery_task_charge(void) {
    check_for_cable();

    if (blank_check()) return;

    if (blank_timeout < 0) {
        LOG_INFO(mux_module, "Setting Brightness To: %d", 0)

        set_brightness(0);
        is_blank = 1;

        return;
    }

    int bat_cap = read_battery_capacity();
    char *bat_vol = read_battery_voltage();

    LOG_INFO(mux_module, "Capacity: %d%%", bat_cap)
    LOG_INFO(mux_module, "Voltage: %s", bat_vol)

    snprintf(capacity_info, sizeof(capacity_info), "%s: %d%%", lang.MUXCHARGE.CAPACITY, bat_cap);
    snprintf(voltage_info, sizeof(voltage_info), "%s: %s", lang.MUXCHARGE.VOLTAGE, bat_vol);

    lv_label_set_text(ui_lblCapacity_charge, capacity_info);
    lv_label_set_text(ui_lblVoltage_charge, voltage_info);

    blank_timeout--;
}

int main(void) {
    load_device(&device);
    load_config(&config);

    init_module("muxcharge");

    init_theme(0, 0);
    init_display(1);

    init_muxcharge();

    LOG_INFO(mux_module, "Current Brightness: %d", config.SETTINGS.GENERAL.BRIGHTNESS)
    write_text_to_file(CHARGER_BRIGHT, "w", INT, config.SETTINGS.GENERAL.BRIGHTNESS);
    set_brightness(config.SETTINGS.GENERAL.BRIGHTNESS);

    lv_obj_set_user_data(ui_scrCharge_charge, mux_module);
    lv_label_set_text(ui_lblBoot_charge, lang.MUXCHARGE.START);

    load_wallpaper(ui_scrCharge_charge, NULL, ui_pnlWall_charge, ui_imgWall_charge, GENERAL);
    load_font_text(ui_scrCharge_charge);

#if TEST_IMAGE
    display_testing_message(ui_scrCharge_charge);
#endif

    overlay_image = lv_img_create(ui_scrCharge_charge);
    load_overlay_image(ui_scrCharge_charge, overlay_image);

    LOG_INFO(mux_module, "Charging Statistics Y Position: %d", theme.CHARGER.Y_POS)
    lv_obj_set_y(ui_pnlCharge_charge, theme.CHARGER.Y_POS);

    battery_task_charge();
    lv_timer_create(battery_task_charge, TIMER_BATTERY, NULL);

    refresh_screen(ui_scrCharge_charge);

    mux_input_options input_opts = {
            .press_handler = {[MUX_INPUT_START] = handle_start},
            .idle_handler = handle_idle
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    sdl_cleanup();
    return 0;
}
