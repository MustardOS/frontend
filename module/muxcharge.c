#include "muxshare.h"
#include "ui/ui_muxcharge.h"
#include "../common/battery.h"
#include "../common/display.h"

static int exit_status = -1;
static int blank_timeout = 3;
static int blank_status = 0;

static char capacity_info[MAX_BUFFER_SIZE];
static char voltage_info[MAX_BUFFER_SIZE];

#define CHARGER_BRIGHT "/tmp/charger_bright"
#define CHARGER_EXIT "/tmp/charger_exit"

static void check_for_cable(void) {
    if (file_exist(device.BATTERY.CHARGER) && !read_line_int_from(device.BATTERY.CHARGER, 1)) exit_status = 1;
}

static int blank_check(void) {
    int new_blank = file_exist(MUX_BLANK);
    if (new_blank == blank_status) return blank_status;

    blank_status = new_blank;
    if (blank_status) {
        lv_obj_set_style_bg_opa(ui_blank_charge, 255, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_foreground(ui_blank_charge);
    } else {
        lv_obj_set_style_bg_opa(ui_blank_charge, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_background(ui_blank_charge);
    }

    return blank_status;
}

static void set_brightness(int brightness) {
    char bright_value[8];
    snprintf(bright_value, sizeof(bright_value), "%d", brightness);

    const char *args[] = {(DEV_SCRIPT "bright.sh"), bright_value, NULL};
    run_exec(args, A_SIZE(args), 0, 0, NULL, NULL);

    load_config(&config);
}

static void wake_screen(void) {
    int bright_value = read_line_int_from(CHARGER_BRIGHT, 1);

    LOG_INFO(mux_module, "Setting Brightness To: %d", bright_value);
    set_brightness(bright_value);

    if (file_exist(MUX_BLANK)) remove(MUX_BLANK);
    blank_check();

    blank_timeout = 3;
}

static void handle_start(void) {
    LOG_SUCCESS(mux_module, "Start Button Pressed");

    if (blank_status) {
        wake_screen();
        return;
    }

    lv_obj_add_flag(ui_lblCapacity_charge, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblVoltage_charge, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lblBoot_charge, lang.MUXCHARGE.BOOT);
    lv_refr_now(NULL);

    exit_status = 0;
}

static void wake_screen_on_input(void) {
    LOG_SUCCESS(mux_module, "Wake Button Pressed");

    if (blank_status) {
        wake_screen();
        return;
    }
}

static void wake_screen_on_input_lid_open(void) {
    if (device.BOARD.HASLID && read_line_int_from(device.BOARD.JOY_HALL, 1) != 1) return;
    wake_screen_on_input();
}

static void handle_idle(void) {
    refresh_screen(ui_scrCharge_charge, 1);

    if (exit_status >= 0) {
        write_text_to_file(CHARGER_EXIT, "w", INT, exit_status);
        if (file_exist(CHARGER_BRIGHT)) remove(CHARGER_BRIGHT);

        safe_quit(0);
        mux_input_stop();

        return;
    }
}

static void battery_task_charge() {
    check_for_cable();

    if (blank_check()) return;

    if (blank_timeout < 0) {
        LOG_INFO(mux_module, "Setting Brightness To: %d", 0);

        set_brightness(0);
        blank_status = 1;

        return;
    }

    battery_update();

    int bat_cap = battery_get_capacity();
    const char *bat_vol = battery_get_voltage();

    LOG_INFO(mux_module, "Capacity: %d%%", bat_cap);
    LOG_INFO(mux_module, "Voltage: %s", bat_vol);

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

    init_display();
    init_muxcharge();

    battery_init();

    LOG_INFO(mux_module, "Current Brightness: %d", config.SETTINGS.GENERAL.BRIGHTNESS);
    write_text_to_file(CHARGER_BRIGHT, "w", INT, config.SETTINGS.GENERAL.BRIGHTNESS);
    set_brightness(config.SETTINGS.GENERAL.BRIGHTNESS);

    lv_obj_set_user_data(ui_scrCharge_charge, mux_module);
    lv_label_set_text(ui_lblBoot_charge, lang.MUXCHARGE.START);

    load_wallpaper(ui_scrCharge_charge, NULL, ui_pnlWall_charge, ui_imgWall_charge, WALL_GENERAL);
    if (theme.IMAGE_LIST.RECOLOUR_ALPHA > 0) {
        lv_obj_set_style_img_recolor(ui_imgWall_charge, lv_color_hex(theme.IMAGE_LIST.RECOLOUR), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_imgWall_charge, theme.IMAGE_LIST.RECOLOUR_ALPHA, MU_OBJ_MAIN_DEFAULT);
    }
    load_font_text(ui_scrCharge_charge);

    display_testing_message(ui_scrCharge_charge);

    overlay_image = lv_img_create(ui_scrCharge_charge);
    load_overlay_image(ui_scrCharge_charge, overlay_image);

    LOG_INFO(mux_module, "Charging Statistics Y Position: %d", theme.CHARGER.Y_POS);
    lv_obj_set_y(ui_pnlCharge_charge, theme.CHARGER.Y_POS);

    check_for_cable();
    battery_task_charge();

    lv_timer_create(battery_task_charge, TIMER_BATTERY, NULL);

    refresh_screen(ui_scrCharge_charge, 1);

    mux_input_options input_opts = {
            .press_handler = {
                    [MUX_INPUT_START] = handle_start,
                    [MUX_INPUT_SELECT] = wake_screen_on_input,
                    [MUX_INPUT_A] = wake_screen_on_input,
                    [MUX_INPUT_B] = wake_screen_on_input,
                    [MUX_INPUT_X] = wake_screen_on_input,
                    [MUX_INPUT_Y] = wake_screen_on_input,
                    [MUX_INPUT_MENU] = wake_screen_on_input,
                    [MUX_INPUT_DPAD_DOWN] = wake_screen_on_input,
                    [MUX_INPUT_DPAD_LEFT] = wake_screen_on_input,
                    [MUX_INPUT_DPAD_RIGHT] = wake_screen_on_input,
                    [MUX_INPUT_DPAD_UP] = wake_screen_on_input,
                    [MUX_INPUT_LS_DOWN] = wake_screen_on_input,
                    [MUX_INPUT_LS_LEFT] = wake_screen_on_input,
                    [MUX_INPUT_LS_RIGHT] = wake_screen_on_input,
                    [MUX_INPUT_LS_UP] = wake_screen_on_input,
                    [MUX_INPUT_L3] = wake_screen_on_input,
                    [MUX_INPUT_RS_DOWN] = wake_screen_on_input,
                    [MUX_INPUT_RS_LEFT] = wake_screen_on_input,
                    [MUX_INPUT_RS_RIGHT] = wake_screen_on_input,
                    [MUX_INPUT_RS_UP] = wake_screen_on_input,
                    [MUX_INPUT_R3] = wake_screen_on_input,
                    [MUX_INPUT_L1] = wake_screen_on_input_lid_open,
                    [MUX_INPUT_L2] = wake_screen_on_input_lid_open,
                    [MUX_INPUT_R1] = wake_screen_on_input_lid_open,
                    [MUX_INPUT_R2] = wake_screen_on_input_lid_open,
                    [MUX_INPUT_VOL_DOWN] = wake_screen_on_input_lid_open,
                    [MUX_INPUT_VOL_UP] = wake_screen_on_input_lid_open,
                    [MUX_INPUT_POWER_SHORT] = wake_screen_on_input_lid_open,
            },
            .idle_handler = handle_idle
    };

    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    sdl_cleanup();
    return 0;
}
