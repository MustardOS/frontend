#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxcharge.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int exit_status = -1;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

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
    if (blank < 5) {
        lv_timer_pause(battery_timer);

        lv_obj_add_flag(ui_lblCapacity, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblVoltage, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_lblCapacity, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblVoltage, LV_OBJ_FLAG_FLOATING);

        lv_label_set_text(ui_lblBoot, lang.MUXCHARGE.BOOT);

        refresh_screen(device.SCREEN.WAIT);

        exit_status = 0;
        return;
    }

    blank = 0;
    set_brightness(read_int_from_file(INTERNAL_PATH "config/brightness.txt", 1));
}

void handle_idle(void) {
    if (exit_status >= 0) {
        mux_input_stop();
        return;
    }

    refresh_screen(device.SCREEN.WAIT);
}

void battery_task() {
    snprintf(capacity_info, sizeof(capacity_info), "%s: %d%%", lang.MUXCHARGE.CAPACITY, read_battery_capacity());
    snprintf(voltage_info, sizeof(voltage_info), "%s: %s", lang.MUXCHARGE.VOLTAGE, read_battery_voltage());

    lv_label_set_text(ui_lblCapacity, capacity_info);
    lv_label_set_text(ui_lblVoltage, voltage_info);

    if (blank == 5) {
        set_brightness(0);
    }

    check_for_cable();

    blank++;
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t *buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_lang(&lang);

    load_theme(&theme, &config, &device, basename(argv[0]));

    ui_init();
    set_brightness(read_int_from_file(INTERNAL_PATH "config/brightness.txt", 1));

    lv_obj_set_user_data(ui_scrCharge, mux_module);
    lv_label_set_text(ui_lblBoot, lang.MUXCHARGE.POWER);

    load_wallpaper(ui_scrCharge, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_scrCharge);

    if (TEST_IMAGE) display_testing_message(ui_scrCharge);

    overlay_image = lv_img_create(ui_scrCharge);
    load_overlay_image(ui_scrCharge, overlay_image, theme.MISC.IMAGE_OVERLAY);

    nav_sound = init_nav_sound(mux_module);
    lv_obj_set_y(ui_pnlCharge, theme.CHARGER.Y_POS);

    js_fd = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    battery_timer = lv_timer_create(battery_task, UINT16_MAX / 32, NULL);
    lv_timer_ready(battery_timer);

    refresh_screen(device.SCREEN.WAIT);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .press_handler = {
                    [MUX_INPUT_POWER_SHORT] = handle_power_short,
            },
            .idle_handler = handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return exit_status;
}
