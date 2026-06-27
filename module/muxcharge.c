#include "muxshare.h"
#include "ui/ui_muxcharge.h"
#include "../common/battery.h"

static int exit_status = -1;
static int blank_timeout = 3;
static int blank_status = 0;

static char capacity_info[MAX_BUFFER_SIZE];
static char voltage_info[MAX_BUFFER_SIZE];

#define CHARGER_BRIGHT "/tmp/charger_bright"
#define CHARGER_EXIT   "/tmp/charger_exit"

static void check_for_cable(void) {
    if (file_exist(device.battery.charger) && !read_line_int_from(device.battery.charger, 1)) exit_status = 1;
}

static int blank_check(void) {
    const int new_blank = file_exist(MUX_BLANK);
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

static void set_brightness(const int brightness) {
    char bright_value[8];
    snprintf(bright_value, sizeof(bright_value), "%d", brightness);

    const char *args[] = {DEV_SCRIPT "bright.sh", bright_value, NULL};
    run_exec(args, A_SIZE(args), 0, 0, NULL, NULL);

    load_config(&config);
}

static void wake_screen(void) {
    const int bright_value = read_line_int_from(CHARGER_BRIGHT, 1);

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

    lv_obj_add_flag(ui_lbl_capacity_charge, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_voltage_charge, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lbl_boot_charge, lang.muxcharge.boot);
    lv_refr_now(NULL);

    exit_status = 0;
}

static void wake_screen_on_input(void) {
    LOG_SUCCESS(mux_module, "Wake Button Pressed");

    if (blank_status) {
        wake_screen();
    }
}

static void wake_screen_on_input_lid_open(void) {
    if (device.board.has_lid && read_line_int_from(device.board.joy_hall, 1) != 1) return;
    wake_screen_on_input();
}

static void handle_idle(void) {
    refresh_screen(ui_scr_charge_charge, 1);

    if (exit_status >= 0) {
        write_text_to_file(CHARGER_EXIT, "w", INT, exit_status);
        if (file_exist(CHARGER_BRIGHT)) remove(CHARGER_BRIGHT);

        safe_quit(0);
        mux_input_stop();
    }
}

static void battery_task_charge(lv_timer_t *timer __attribute__((unused))) {
    check_for_cable();

    if (blank_check()) return;

    if (blank_timeout < 0) {
        LOG_INFO(mux_module, "Setting Brightness To: %d", 0);

        set_brightness(0);
        blank_status = 1;

        return;
    }

    battery_update();

    const int bat_cap = battery_get_capacity();
    const char *bat_vol = battery_get_voltage();

    LOG_INFO(mux_module, "Capacity: %d%%", bat_cap);
    LOG_INFO(mux_module, "Voltage: %s", bat_vol);

    snprintf(capacity_info, sizeof(capacity_info), "%s: %d%%", lang.muxcharge.capacity, bat_cap);
    snprintf(voltage_info, sizeof(voltage_info), "%s: %s", lang.muxcharge.voltage, bat_vol);

    lv_label_set_text(ui_lbl_capacity_charge, capacity_info);
    lv_label_set_text(ui_lbl_voltage_charge, voltage_info);

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

    LOG_INFO(mux_module, "Current Brightness: %d", config.settings.general.brightness);
    write_text_to_file(CHARGER_BRIGHT, "w", INT, config.settings.general.brightness);
    set_brightness(config.settings.general.brightness);

    lv_obj_set_user_data(ui_scr_charge_charge, mux_module);
    lv_label_set_text(ui_lbl_boot_charge, lang.muxcharge.start);

    load_wallpaper(ui_scr_charge_charge, NULL, ui_img_wall_charge, wall_general);
    if (theme.image_list.recolour_alpha > 0) {
        lv_obj_set_style_img_recolor(ui_img_wall_charge, lv_color_hex(theme.image_list.recolour), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_img_wall_charge, theme.image_list.recolour_alpha, MU_OBJ_MAIN_DEFAULT);
    }
    load_font_text(ui_scr_charge_charge);

    watermark(ui_scr_charge_charge);

    overlay_image = lv_img_create(ui_scr_charge_charge);
    load_overlay_image(ui_scr_charge_charge, overlay_image);

    LOG_INFO(mux_module, "Charging Statistics Y Position: %d", theme.charger.y_pos);
    lv_obj_set_y(ui_pnl_charge_charge, theme.charger.y_pos);

    check_for_cable();
    battery_task_charge(NULL);

    lv_timer_create(battery_task_charge, TIMER_BATTERY, NULL);

    refresh_screen(ui_scr_charge_charge, 1);

    mux_input_options input_opts = {
        .press_handler =
            {
                [mux_input_start] = handle_start,
                [mux_input_select] = wake_screen_on_input,
                [mux_input_a] = wake_screen_on_input,
                [mux_input_b] = wake_screen_on_input,
                [mux_input_x] = wake_screen_on_input,
                [mux_input_y] = wake_screen_on_input,
                [mux_input_menu] = wake_screen_on_input,
                [mux_input_dpad_down] = wake_screen_on_input,
                [mux_input_dpad_left] = wake_screen_on_input,
                [mux_input_dpad_right] = wake_screen_on_input,
                [mux_input_dpad_up] = wake_screen_on_input,
                [mux_input_ls_down] = wake_screen_on_input,
                [mux_input_ls_left] = wake_screen_on_input,
                [mux_input_ls_right] = wake_screen_on_input,
                [mux_input_ls_up] = wake_screen_on_input,
                [mux_input_l3] = wake_screen_on_input,
                [mux_input_rs_down] = wake_screen_on_input,
                [mux_input_rs_left] = wake_screen_on_input,
                [mux_input_rs_right] = wake_screen_on_input,
                [mux_input_rs_up] = wake_screen_on_input,
                [mux_input_r3] = wake_screen_on_input,
                [mux_input_l1] = wake_screen_on_input_lid_open,
                [mux_input_l2] = wake_screen_on_input_lid_open,
                [mux_input_r1] = wake_screen_on_input_lid_open,
                [mux_input_r2] = wake_screen_on_input_lid_open,
                [mux_input_vol_down] = wake_screen_on_input_lid_open,
                [mux_input_vol_up] = wake_screen_on_input_lid_open,
                [mux_input_power_short] = wake_screen_on_input_lid_open,
            },
        .idle_handler = handle_idle
    };

    init_input(&input_opts, 0);
    mux_input_task(&input_opts);

    sdl_cleanup();
    return 0;
}
