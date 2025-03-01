#include "../lvgl/lvgl.h"
#include "ui/ui_muxcharge.h"
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"

char *mux_module;

int msgbox_active = 0;
int nav_sound = 0;
int exit_status = -1;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

int blank = 0;

char capacity_info[MAX_BUFFER_SIZE];
char voltage_info[MAX_BUFFER_SIZE];

lv_timer_t *battery_timer;

#define CHARGER_EXIT "/tmp/charger_exit"

void check_for_cable() {
    if (file_exist(device.BATTERY.CHARGER)) {
        if (read_int_from_file(device.BATTERY.CHARGER, 1) == 0) {
            exit_status = 1;
        }
    }
}

void set_brightness(int brightness) {
    char bright_value[8];
    snprintf(bright_value, sizeof(bright_value), "%d", brightness);
    run_exec((const char *[]) {(char *) INTERNAL_PATH "device/current/input/combo/bright.sh", bright_value, NULL});
}

void handle_power_short(void) {
    if (blank < 3) {
        lv_timer_pause(battery_timer);

        lv_obj_add_flag(ui_lblCapacity, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblVoltage, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_lblCapacity, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblVoltage, LV_OBJ_FLAG_FLOATING);

        lv_label_set_text(ui_lblBoot, lang.MUXCHARGE.BOOT);

        refresh_screen(ui_scrCharge);

        exit_status = 0;
        return;
    }

    blank = 0;
    set_brightness(read_int_from_file(INTERNAL_PATH "config/brightness.txt", 1));
}

void handle_idle(void) {
    if (file_exist("/tmp/mux_blank")) {
        lv_obj_set_style_bg_opa(ui_blank, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(ui_blank);
    } else {
        if (lv_obj_get_style_bg_opa(ui_blank, LV_PART_MAIN | LV_STATE_DEFAULT) > 0) {
            lv_obj_set_style_bg_opa(ui_blank, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_move_background(ui_blank);
        }
    }

    if (exit_status >= 0) {
        write_text_to_file(CHARGER_EXIT, "w", INT, exit_status);
        mux_input_stop();
        return;
    }

    refresh_screen(ui_scrCharge);
}

void battery_task() {
    snprintf(capacity_info, sizeof(capacity_info), "%s: %d%%", lang.MUXCHARGE.CAPACITY, read_battery_capacity());
    snprintf(voltage_info, sizeof(voltage_info), "%s: %s", lang.MUXCHARGE.VOLTAGE, read_battery_voltage());

    lv_label_set_text(ui_lblCapacity, capacity_info);
    lv_label_set_text(ui_lblVoltage, voltage_info);

    if (blank == 3) set_brightness(0);
    check_for_cable();
    blank++;
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_theme(0, 0);
    init_display();

    init_mux();
    set_brightness(read_int_from_file(INTERNAL_PATH "config/brightness.txt", 1));

    lv_obj_set_user_data(ui_scrCharge, mux_module);
    lv_label_set_text(ui_lblBoot, lang.MUXCHARGE.POWER);

    load_wallpaper(ui_scrCharge, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    load_font_text(ui_scrCharge);

#if TEST_IMAGE
    display_testing_message(ui_scrCharge);
#endif

    overlay_image = lv_img_create(ui_scrCharge);
    load_overlay_image(ui_scrCharge, overlay_image, theme.MISC.IMAGE_OVERLAY);

    init_navigation_sound(&nav_sound, mux_module);
    lv_obj_set_y(ui_pnlCharge, theme.CHARGER.Y_POS);

    battery_task();
    battery_timer = lv_timer_create(battery_task, TIMER_BATTERY, NULL);

    mux_input_options input_opts = {
            .press_handler = {[MUX_INPUT_POWER_SHORT] = handle_power_short},
            .idle_handler = handle_idle
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    safe_quit(0);
    return 0;
}
